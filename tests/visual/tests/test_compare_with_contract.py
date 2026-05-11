"""M16 S3 — compare_with_contract() unit tests.

Synthetic test images (tiny, deterministic) exercise each of
the 6 steps from `docs/v0.3/visual-diff-contract.md` §1:

  Step 1 — env-sidecar pre-check (require_env_sidecar=False
           for unit tests; sidecar-presence behavior also
           exercised separately).
  Step 2 — image size match.
  Step 3 — mask handling.
  Step 4 — per-pixel delta (PIXEL_THRESHOLD=4).
  Step 5 — clustering (CLUSTER_THRESHOLD=200).
  Step 6 — acceptance.

Plus diff-image emission + diff-report emission + V0.2
compare_baseline() backward-compat preservation.

Stdlib-only — uses the same `lib/runner.py` test discovery
pattern as the V0.2 tests.
"""

from __future__ import annotations

import json
import struct
import sys
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from lib.compare import (  # noqa: E402
    CompareResult,
    compare_baseline,
    compare_with_contract,
)

REPO_ROOT = Path(__file__).resolve().parent.parent.parent.parent


# ---------------------------------------------------------------------------
# Helpers — synthesise tiny PNGs at runtime
# ---------------------------------------------------------------------------


def _write_png_solid(path: Path, width: int, height: int, rgba: tuple[int, int, int, int]) -> None:
    """Write a solid-color PNG via stdlib zlib + struct."""
    r, g, b, a = rgba
    pixel = bytes([r, g, b, a])
    stride = width * 4
    raw = bytearray()
    for _ in range(height):
        raw.append(0)  # filter type None
        raw.extend(pixel * width)
    idat = zlib.compress(bytes(raw), 6)

    def _chunk(tag: bytes, payload: bytes) -> bytes:
        crc = zlib.crc32(tag + payload)
        return struct.pack(">I", len(payload)) + tag + payload + struct.pack(">I", crc)

    sig = b"\x89PNG\r\n\x1a\n"
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    path.write_bytes(sig + _chunk(b"IHDR", ihdr) + _chunk(b"IDAT", idat) + _chunk(b"IEND", b""))


def _write_png_2color(
    path: Path, width: int, height: int,
    bg: tuple[int, int, int, int],
    fg: tuple[int, int, int, int],
    fg_region: tuple[int, int, int, int],  # x, y, w, h
) -> None:
    """Solid background + filled rectangle in fg color (for cluster tests)."""
    fx, fy, fw, fh = fg_region
    stride = width * 4
    raw = bytearray()
    for y in range(height):
        raw.append(0)
        row = bytearray()
        for x in range(width):
            if fx <= x < fx + fw and fy <= y < fy + fh:
                row.extend(bytes(fg))
            else:
                row.extend(bytes(bg))
        raw.extend(row)
    idat = zlib.compress(bytes(raw), 6)

    def _chunk(tag: bytes, payload: bytes) -> bytes:
        crc = zlib.crc32(tag + payload)
        return struct.pack(">I", len(payload)) + tag + payload + struct.pack(">I", crc)

    sig = b"\x89PNG\r\n\x1a\n"
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    path.write_bytes(sig + _chunk(b"IHDR", ihdr) + _chunk(b"IDAT", idat) + _chunk(b"IEND", b""))


def _new_tmp_dir() -> Path:
    """Return a per-test temp dir under tests/screenshots/ (gitignored)."""
    import tempfile
    return Path(tempfile.mkdtemp(prefix="m16-s3-test-",
                                  dir=str(REPO_ROOT / "tests" / "screenshots")))


# ---------------------------------------------------------------------------
# Step-1 tests: env-sidecar pre-check
# ---------------------------------------------------------------------------


def test_compare_with_contract_env_sidecar_matched():
    """Step 1: matching sidecars → contract honored; algorithm proceeds."""
    tmp = _new_tmp_dir()
    actual = tmp / "actual.png"
    baseline = tmp / "baseline.png"
    _write_png_solid(actual, 32, 32, (200, 200, 200, 255))
    _write_png_solid(baseline, 32, 32, (200, 200, 200, 255))

    matching_env = {
        "tier_1_font_cascade": {
            "app_default_family": "Inter",
            "app_default_size_pt": 12,
            "app_mono_family": "JetBrains Mono",
        },
        "tier_2_qt_rendering": {
            "qt_version_major_minor": "6.10",
            "qpa_platform": "xcb",
            "style_object_introspection": "Fusion",
            "wayland_disallowed": True,
            "gpu_rasterization_disallowed": True,
        },
        "tier_3_geometry": {
            "device_pixel_ratio": 1.0,
            "screen_geometry": "1280x800",
            "locale": "C.UTF-8",
        },
    }
    (tmp / "actual.env.json").write_text(json.dumps(matching_env))
    (tmp / "baseline.env.json").write_text(json.dumps(matching_env))

    result = compare_with_contract(actual, baseline, emit_diff_report=False)
    assert not result.invalid, f"expected contract honored, got invalid={result.invalid} drift={result.env_drift}"
    assert result.matched, f"expected match, got {result.note}"
    assert result.env_drift == []


def test_compare_with_contract_env_sidecar_tier1_drift_invalid():
    """Step 1: Tier 1 font family drift → INVALID (H10), not soft fail."""
    tmp = _new_tmp_dir()
    actual = tmp / "actual.png"
    baseline = tmp / "baseline.png"
    _write_png_solid(actual, 32, 32, (200, 200, 200, 255))
    _write_png_solid(baseline, 32, 32, (200, 200, 200, 255))

    base_env = {
        "tier_1_font_cascade": {
            "app_default_family": "Inter",
            "app_default_size_pt": 12,
            "app_mono_family": "JetBrains Mono",
        },
        "tier_2_qt_rendering": {
            "qt_version_major_minor": "6.10", "qpa_platform": "xcb",
            "style_object_introspection": "Fusion",
            "wayland_disallowed": True, "gpu_rasterization_disallowed": True,
        },
        "tier_3_geometry": {
            "device_pixel_ratio": 1.0, "screen_geometry": "1280x800", "locale": "C.UTF-8",
        },
    }
    actual_env = json.loads(json.dumps(base_env))
    actual_env["tier_1_font_cascade"]["app_default_family"] = "Cantarell"  # drift!
    (tmp / "actual.env.json").write_text(json.dumps(actual_env))
    (tmp / "baseline.env.json").write_text(json.dumps(base_env))

    result = compare_with_contract(actual, baseline, emit_diff_report=False)
    assert result.invalid, "expected INVALID for Tier 1 drift"
    assert not result.matched
    assert any("app_default_family" in d for d in result.env_drift), result.env_drift


def test_compare_with_contract_env_sidecar_tier4_ignored():
    """Step 1: Tier 4 advisory drift is recorded but does NOT invalidate."""
    tmp = _new_tmp_dir()
    actual = tmp / "actual.png"
    baseline = tmp / "baseline.png"
    _write_png_solid(actual, 32, 32, (200, 200, 200, 255))
    _write_png_solid(baseline, 32, 32, (200, 200, 200, 255))

    base_env = {
        "tier_1_font_cascade": {
            "app_default_family": "Inter",
            "app_default_size_pt": 12,
            "app_mono_family": "JetBrains Mono",
        },
        "tier_2_qt_rendering": {
            "qt_version_major_minor": "6.10", "qpa_platform": "xcb",
            "style_object_introspection": "Fusion",
            "wayland_disallowed": True, "gpu_rasterization_disallowed": True,
        },
        "tier_3_geometry": {
            "device_pixel_ratio": 1.0, "screen_geometry": "1280x800", "locale": "C.UTF-8",
        },
        "tier_4_advisory": {"kernel": "6.8.0-111-generic"},
    }
    actual_env = json.loads(json.dumps(base_env))
    actual_env["tier_4_advisory"]["kernel"] = "6.17.0-1010-azure"  # CI drift
    (tmp / "actual.env.json").write_text(json.dumps(actual_env))
    (tmp / "baseline.env.json").write_text(json.dumps(base_env))

    result = compare_with_contract(actual, baseline, emit_diff_report=False)
    assert not result.invalid, f"Tier 4 drift must not invalidate, got drift={result.env_drift}"
    assert result.matched


def test_compare_with_contract_env_sidecar_missing_strict():
    """Step 1: missing sidecar with require_env_sidecar=True → INVALID."""
    tmp = _new_tmp_dir()
    actual = tmp / "actual.png"
    baseline = tmp / "baseline.png"
    _write_png_solid(actual, 32, 32, (200, 200, 200, 255))
    _write_png_solid(baseline, 32, 32, (200, 200, 200, 255))
    # no sidecars

    result = compare_with_contract(actual, baseline, emit_diff_report=False,
                                   require_env_sidecar=True)
    assert result.invalid
    assert any("missing-sidecar" in d for d in result.env_drift), result.env_drift


def test_compare_with_contract_env_sidecar_missing_relaxed():
    """Step 1: missing sidecar with require_env_sidecar=False → proceed."""
    tmp = _new_tmp_dir()
    actual = tmp / "actual.png"
    baseline = tmp / "baseline.png"
    _write_png_solid(actual, 32, 32, (200, 200, 200, 255))
    _write_png_solid(baseline, 32, 32, (200, 200, 200, 255))

    result = compare_with_contract(actual, baseline, emit_diff_report=False,
                                   require_env_sidecar=False)
    assert not result.invalid
    assert result.matched


# ---------------------------------------------------------------------------
# Step-2 test: image size match
# ---------------------------------------------------------------------------


def test_compare_with_contract_size_mismatch_fails_fast():
    """Step 2: size mismatch → FAIL immediately."""
    tmp = _new_tmp_dir()
    actual = tmp / "actual.png"
    baseline = tmp / "baseline.png"
    _write_png_solid(actual, 32, 16, (200, 200, 200, 255))
    _write_png_solid(baseline, 32, 32, (200, 200, 200, 255))

    result = compare_with_contract(actual, baseline,
                                   require_env_sidecar=False, emit_diff_report=False)
    assert not result.matched
    assert not result.invalid
    assert "dimension-mismatch" in result.note


# ---------------------------------------------------------------------------
# Step-3 test: masking
# ---------------------------------------------------------------------------


def test_compare_with_contract_mask_excludes_pixels():
    """Step 3: mask region excludes its pixels from diff."""
    tmp = _new_tmp_dir()
    actual = tmp / "actual.png"
    baseline = tmp / "baseline.png"
    # Baseline: all grey. Actual: top-left 8x8 is red.
    _write_png_2color(actual, 32, 32,
                      bg=(200, 200, 200, 255), fg=(255, 0, 0, 255),
                      fg_region=(0, 0, 8, 8))
    _write_png_solid(baseline, 32, 32, (200, 200, 200, 255))

    # Without mask: 8*8 = 64 pixels differ → 64/(32*32) = 6.25 % → FAIL.
    no_mask = compare_with_contract(actual, baseline,
                                    require_env_sidecar=False, emit_diff_report=False)
    assert not no_mask.matched
    assert no_mask.differing_pixels == 64

    # With mask covering the red region: 0 pixels differ.
    mask_dict = {"regions": [{"x": 0, "y": 0, "w": 8, "h": 8, "rationale": "test"}]}
    with_mask = compare_with_contract(actual, baseline, mask=mask_dict,
                                      require_env_sidecar=False, emit_diff_report=False)
    assert with_mask.matched
    assert with_mask.differing_pixels == 0
    assert with_mask.masked_pixels == 64
    assert with_mask.total_pixels == 32 * 32 - 64


def test_compare_with_contract_mask_auto_discovered_from_baseline_sibling():
    """Step 3: auto-discover `<baseline>.mask.json` sibling."""
    tmp = _new_tmp_dir()
    actual = tmp / "actual.png"
    baseline = tmp / "baseline.png"
    _write_png_2color(actual, 32, 32,
                      bg=(200, 200, 200, 255), fg=(255, 0, 0, 255),
                      fg_region=(0, 0, 8, 8))
    _write_png_solid(baseline, 32, 32, (200, 200, 200, 255))

    # Drop a mask.json sibling to the baseline.
    mask_path = baseline.with_name(baseline.stem + ".mask.json")
    mask_path.write_text(json.dumps({
        "regions": [{"x": 0, "y": 0, "w": 8, "h": 8, "rationale": "auto-discover test"}]
    }))

    result = compare_with_contract(actual, baseline,
                                   require_env_sidecar=False, emit_diff_report=False)
    assert result.matched
    assert result.masked_pixels == 64


# ---------------------------------------------------------------------------
# Step-4 test: per-pixel max-channel delta with PIXEL_THRESHOLD
# ---------------------------------------------------------------------------


def test_compare_with_contract_pixel_threshold_absorbs_small_delta():
    """Step 4: delta ≤ PIXEL_THRESHOLD → not differing."""
    tmp = _new_tmp_dir()
    actual = tmp / "actual.png"
    baseline = tmp / "baseline.png"
    _write_png_solid(actual,   32, 32, (200, 200, 200, 255))
    _write_png_solid(baseline, 32, 32, (203, 203, 203, 255))  # delta = 3 (under 4)

    result = compare_with_contract(actual, baseline,
                                   require_env_sidecar=False, emit_diff_report=False)
    assert result.matched
    assert result.differing_pixels == 0


def test_compare_with_contract_pixel_threshold_catches_large_delta():
    """Step 4: delta > PIXEL_THRESHOLD → differing."""
    tmp = _new_tmp_dir()
    actual = tmp / "actual.png"
    baseline = tmp / "baseline.png"
    _write_png_solid(actual,   32, 32, (200, 200, 200, 255))
    _write_png_solid(baseline, 32, 32, (210, 210, 210, 255))  # delta = 10 (over 4)

    result = compare_with_contract(actual, baseline,
                                   require_env_sidecar=False, emit_diff_report=False)
    assert not result.matched, f"expected fail on 10-delta, got {result.note}"
    assert result.differing_pixels == 32 * 32


# ---------------------------------------------------------------------------
# Step-5 test: clustering
# ---------------------------------------------------------------------------


def test_compare_with_contract_clustering_max_cluster_size():
    """Step 5: 16x16 (256 px) cluster exceeds CLUSTER_THRESHOLD=200."""
    tmp = _new_tmp_dir()
    actual = tmp / "actual.png"
    baseline = tmp / "baseline.png"
    _write_png_2color(actual, 32, 32,
                      bg=(200, 200, 200, 255), fg=(50, 50, 50, 255),
                      fg_region=(8, 8, 16, 16))  # 256 px contiguous
    _write_png_solid(baseline, 32, 32, (200, 200, 200, 255))

    result = compare_with_contract(actual, baseline,
                                   require_env_sidecar=False, emit_diff_report=False)
    assert result.max_cluster_size == 256
    assert result.cluster_count == 1
    # 256 / 1024 = 25 % differing > 1 % AND cluster 256 > 200 → FAIL on both
    assert not result.matched


def test_compare_with_contract_clustering_below_threshold():
    """Step 5: cluster smaller than 200 px and percent < 1 % → PASS."""
    tmp = _new_tmp_dir()
    actual = tmp / "actual.png"
    baseline = tmp / "baseline.png"
    # 60-pixel cluster (smaller than 200; 0.058 % of 1024 — under percent_threshold=1.0)
    _write_png_2color(actual, 100, 100,
                      bg=(200, 200, 200, 255), fg=(50, 50, 50, 255),
                      fg_region=(0, 0, 6, 10))  # 60 px contiguous
    _write_png_solid(baseline, 100, 100, (200, 200, 200, 255))

    result = compare_with_contract(actual, baseline,
                                   require_env_sidecar=False, emit_diff_report=False)
    assert result.max_cluster_size == 60
    assert result.cluster_count == 1
    assert result.differing_pixels == 60
    assert result.diff_percent < 1.0
    assert result.matched, f"expected PASS, got {result.note}"


# ---------------------------------------------------------------------------
# Step-6 + diagnostic emission tests
# ---------------------------------------------------------------------------


def test_compare_with_contract_emit_diff_report():
    """`emit_diff_report=True` writes <state>.diff-report.json with structured fields."""
    tmp = _new_tmp_dir()
    actual = tmp / "actual.png"
    baseline = tmp / "baseline.png"
    _write_png_solid(actual, 32, 32, (200, 200, 200, 255))
    _write_png_solid(baseline, 32, 32, (200, 200, 200, 255))

    result = compare_with_contract(actual, baseline,
                                   require_env_sidecar=False, emit_diff_report=True)
    assert result.diff_report_path is not None
    assert result.diff_report_path.is_file()
    report = json.loads(result.diff_report_path.read_text())
    assert report["verdict"] == "PASS"
    assert "metrics" in report and "thresholds" in report


def test_compare_with_contract_emit_diff_image():
    """`emit_diff_image=True` writes <state>.diff.png with red-overlay visualisation."""
    tmp = _new_tmp_dir()
    actual = tmp / "actual.png"
    baseline = tmp / "baseline.png"
    _write_png_2color(actual, 32, 32,
                      bg=(200, 200, 200, 255), fg=(50, 50, 50, 255),
                      fg_region=(0, 0, 8, 8))
    _write_png_solid(baseline, 32, 32, (200, 200, 200, 255))

    result = compare_with_contract(actual, baseline,
                                   require_env_sidecar=False,
                                   emit_diff_image=True, emit_diff_report=False)
    assert result.diff_image_path is not None
    assert result.diff_image_path.is_file()
    # Sanity check: the PNG is non-empty
    assert result.diff_image_path.stat().st_size > 100


# ---------------------------------------------------------------------------
# V0.2 backward-compat
# ---------------------------------------------------------------------------


def test_compare_baseline_v02_api_preserved():
    """V0.2 compare_baseline() API must be unchanged for existing tests."""
    tmp = _new_tmp_dir()
    actual = tmp / "actual.png"
    baseline = tmp / "baseline.png"
    _write_png_solid(actual,   32, 32, (200, 200, 200, 255))
    _write_png_solid(baseline, 32, 32, (200, 200, 200, 255))

    result = compare_baseline(actual, baseline, max_diff_percent=5.0, channel_tolerance=8)
    assert isinstance(result, CompareResult)
    assert result.matched
    assert result.diff_percent == 0.0
    # M16 fields default to neutral values for V0.2 path
    assert not result.invalid
    assert result.masked_pixels == 0
    assert result.max_cluster_size == 0
    assert result.env_drift == []


def test_compare_baseline_absent_baseline():
    """V0.2 absent-baseline semantics preserved."""
    tmp = _new_tmp_dir()
    actual = tmp / "actual.png"
    _write_png_solid(actual, 32, 32, (200, 200, 200, 255))

    result = compare_baseline(actual, tmp / "missing.png")
    assert result.matched
    assert "baseline-absent" in result.note


if __name__ == "__main__":
    from lib.runner import run_tests
    run_tests(globals())
