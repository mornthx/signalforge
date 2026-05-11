"""Visual-baseline diff — V0.2 backward-compat + M16 contract API.

The V0.2 API `compare_baseline(actual, baseline, max_diff_percent,
channel_tolerance)` is preserved for existing tests until S7
baseline migration completes (per
`docs/v0.3/visual-diff-contract.md` §4).

The M16 canonical API `compare_with_contract(actual, baseline,
mask=None, pixel_threshold=4, cluster_threshold=200,
percent_threshold=1.0, emit_diff_image=False,
require_env_sidecar=True)` implements the full algorithm from
`docs/v0.3/visual-diff-contract.md` §1:

  Step 1 — env-sidecar pre-check (R14 / H10): both images must
          have `<image>.env.json` sidecars matching the required
          Tier 1+2+3 contract values from
          `docs/v0.3/rendering-environment-lock.md`. Drift =
          INVALID (not soft fail).
  Step 2 — image size match (immediate fail on mismatch).
  Step 3 — optional masking from `<state>.mask.json` next to
          the baseline.
  Step 4 — per-pixel max-channel delta with PIXEL_THRESHOLD=4
          (default).
  Step 5 — 4-connected clustering BFS; computes
          max_cluster_size (always computed for analytics;
          only gates if exceeds CLUSTER_THRESHOLD=200).
  Step 6 — acceptance: PASS iff (percent_differing <
          PERCENT_THRESHOLD AND max_cluster_size <=
          CLUSTER_THRESHOLD).

Diagnostic report optionally emitted to
`<state>.diff-report.json`; diff visualization optionally
emitted to `<state>.diff.png` (red overlay on greyscale
baseline).

Per M15-concerns C7: CI runs no vision LLM. Pixel-diff (this
module) is the only CI gate. Vision-LLM verdicts stay local-
only via CC's Read tool or operator-local MiMo benchmark.

Stdlib only — no PIL / Pillow / numpy deps (CLAUDE.md §1).
"""

from __future__ import annotations

import json
import struct
import zlib
from collections import deque
from dataclasses import dataclass, field
from pathlib import Path

# ---------------------------------------------------------------------------
# Public types
# ---------------------------------------------------------------------------


@dataclass
class CompareResult:
    """Outcome of pixel-level baseline diff (V0.2 + M16 shared shape)."""

    width: int
    height: int
    diff_percent: float
    differing_pixels: int
    total_pixels: int
    matched: bool
    note: str = ""

    # M16-additional fields (zero for V0.2 compare_baseline; populated for
    # compare_with_contract).
    invalid: bool = False                # True only on env-contract violation
    masked_pixels: int = 0
    max_cluster_size: int = 0
    cluster_count: int = 0
    env_drift: list[str] = field(default_factory=list)
    diff_image_path: Path | None = None
    diff_report_path: Path | None = None


# ---------------------------------------------------------------------------
# V0.2 backward-compat API (PRESERVED — DO NOT BREAK)
# ---------------------------------------------------------------------------


def compare_baseline(
    actual_path: Path | str,
    baseline_path: Path | str,
    max_diff_percent: float = 5.0,
    channel_tolerance: int = 8,
) -> CompareResult:
    """V0.2 API. Channel-tolerance based per-pixel diff.

    Treats absent baseline as a pass with `note="baseline-absent"`
    so new states can be introduced without immediately failing
    CI; operator approves via `scripts/accept-baseline.sh`.

    M16 retains this signature for V0.2 visual tests
    (test_states_empty.py / test_states_with_connection.py /
    test_states_chart_visible.py / test_states_production_fidelity.py)
    until S7 baseline migration. M16 canonical API is
    `compare_with_contract` below.
    """
    actual_path = Path(actual_path)
    baseline_path = Path(baseline_path)

    if not baseline_path.is_file():
        return CompareResult(
            width=0, height=0, diff_percent=0.0,
            differing_pixels=0, total_pixels=0, matched=True,
            note=f"baseline-absent: {baseline_path}",
        )

    a_w, a_h, a_pixels = _read_png_rgba(actual_path)
    b_w, b_h, b_pixels = _read_png_rgba(baseline_path)

    if (a_w, a_h) != (b_w, b_h):
        return CompareResult(
            width=a_w, height=a_h, diff_percent=100.0,
            differing_pixels=a_w * a_h, total_pixels=a_w * a_h,
            matched=False,
            note=f"dimension-mismatch: actual={a_w}×{a_h} baseline={b_w}×{b_h}",
        )

    differing = 0
    total = a_w * a_h
    for i in range(0, len(a_pixels), 4):
        if any(abs(a_pixels[i + ch] - b_pixels[i + ch]) > channel_tolerance for ch in range(4)):
            differing += 1

    diff_pct = (100.0 * differing) / total if total > 0 else 0.0
    matched = diff_pct <= max_diff_percent
    return CompareResult(
        width=a_w, height=a_h, diff_percent=diff_pct,
        differing_pixels=differing, total_pixels=total, matched=matched,
        note=f"diff={diff_pct:.2f}% threshold={max_diff_percent:.1f}%",
    )


# ---------------------------------------------------------------------------
# M16 canonical API
# ---------------------------------------------------------------------------


# Env-contract Tier 1+2+3 required fields per
# `docs/v0.3/rendering-environment-lock.md`. Tier 4 advisory keys
# are NOT in this list — they get recorded but never gate.
#
# At S3 this list reflects the contract documentation; S5's
# `dump_render_env.py` will produce sidecars matching this schema.
# Mismatch on any key here → INVALID per H10 / R14.
ENV_CONTRACT_REQUIRED_KEYS: tuple[tuple[str, str], ...] = (
    # (sidecar JSON path, contract source)
    ("tier_1_font_cascade.app_default_family",      "rendering-environment-lock.md §2.1"),
    ("tier_1_font_cascade.app_default_size_pt",     "rendering-environment-lock.md §2.1"),
    ("tier_1_font_cascade.app_mono_family",         "rendering-environment-lock.md §2.1"),
    ("tier_2_qt_rendering.qt_version_major_minor",  "rendering-environment-lock.md §3.1"),
    ("tier_2_qt_rendering.qpa_platform",            "rendering-environment-lock.md §3.1"),
    ("tier_2_qt_rendering.style_object_introspection", "rendering-environment-lock.md §3.1"),
    ("tier_2_qt_rendering.wayland_disallowed",      "rendering-environment-lock.md §3.1"),
    ("tier_2_qt_rendering.gpu_rasterization_disallowed", "rendering-environment-lock.md §3.1"),
    ("tier_3_geometry.device_pixel_ratio",          "rendering-environment-lock.md §4.1"),
    ("tier_3_geometry.screen_geometry",             "rendering-environment-lock.md §4.1"),
    ("tier_3_geometry.locale",                      "rendering-environment-lock.md §4.1"),
)


def compare_with_contract(
    actual_path: Path | str,
    baseline_path: Path | str,
    mask: Path | dict | None = None,
    pixel_threshold: int = 4,
    cluster_threshold: int = 200,
    percent_threshold: float = 1.0,
    emit_diff_image: bool = False,
    emit_diff_report: bool = True,
    require_env_sidecar: bool = True,
) -> CompareResult:
    """M16 canonical visual-diff per `docs/v0.3/visual-diff-contract.md`.

    Args:
        actual_path: PNG produced by current test capture.
        baseline_path: committed reference PNG at
            `tests/visual/baselines/<state>.png`.
        mask: optional. If `Path`, read mask JSON from that path.
            If `dict`, treat as inline mask spec. Else attempt
            `<baseline_path stem>.mask.json` auto-discovery.
        pixel_threshold: per-channel absolute delta threshold
            (out of 255). Default 4 per contract §2.1.
        cluster_threshold: max contiguous diff cluster size (px).
            Default 200 per contract §2.1.
        percent_threshold: max percent of non-masked pixels
            differing. Default 1.0 per contract §2.1.
        emit_diff_image: when True, write `<actual stem>.diff.png`
            (red overlay on greyscale baseline). Useful for
            operator review of borderline failures.
        emit_diff_report: when True (default), write structured
            `<actual stem>.diff-report.json` for CI forensic
            artifacts.
        require_env_sidecar: when True (default), Step 1 INVALID
            on missing sidecars per R14 / H10. Set False ONLY
            for unit-tests / pre-S5 transition; production use
            keeps True.

    Returns:
        CompareResult with M16 fields populated (`invalid`,
        `masked_pixels`, `max_cluster_size`, `cluster_count`,
        `env_drift`, `diff_image_path`, `diff_report_path`).

    Step 1 INVALID semantics: caller distinguishes via
    `result.invalid` from `result.matched`. INVALID means env
    drift (H10); test harness should HALT, not regress-report.
    Regular `matched=False` means visual regression (S4+ widget
    work changed something).
    """
    actual_path = Path(actual_path)
    baseline_path = Path(baseline_path)

    if not baseline_path.is_file():
        return CompareResult(
            width=0, height=0, diff_percent=0.0,
            differing_pixels=0, total_pixels=0, matched=True,
            note=f"baseline-absent: {baseline_path}",
        )

    # ── Step 1: env-sidecar pre-check (R14 / H10) ────────────────────────
    env_drift = _check_env_sidecars(actual_path, baseline_path, require_env_sidecar)
    if env_drift:
        result = CompareResult(
            width=0, height=0, diff_percent=0.0,
            differing_pixels=0, total_pixels=0, matched=False, invalid=True,
            env_drift=env_drift,
            note=f"env-contract-violation: {len(env_drift)} drift field(s)",
        )
        if emit_diff_report:
            result.diff_report_path = _write_diff_report(
                actual_path, baseline_path, result,
                pixel_threshold, cluster_threshold, percent_threshold,
            )
        return result

    # ── Step 2: image size pre-check ─────────────────────────────────────
    a_w, a_h, a_pixels = _read_png_rgba(actual_path)
    b_w, b_h, b_pixels = _read_png_rgba(baseline_path)
    if (a_w, a_h) != (b_w, b_h):
        result = CompareResult(
            width=a_w, height=a_h, diff_percent=100.0,
            differing_pixels=a_w * a_h, total_pixels=a_w * a_h, matched=False,
            note=f"dimension-mismatch: actual={a_w}×{a_h} baseline={b_w}×{b_h}",
        )
        if emit_diff_report:
            result.diff_report_path = _write_diff_report(
                actual_path, baseline_path, result,
                pixel_threshold, cluster_threshold, percent_threshold,
            )
        return result

    # ── Step 3: optional masking pass ────────────────────────────────────
    mask_regions: list[dict] = _resolve_mask(mask, baseline_path)
    mask_bitmap = _build_mask_bitmap(a_w, a_h, mask_regions) if mask_regions else None
    masked_pixel_count = sum(1 for v in (mask_bitmap or [])) if mask_bitmap else 0
    if mask_bitmap is not None:
        masked_pixel_count = sum(1 for v in mask_bitmap if v)

    # ── Step 4: per-pixel max-channel delta ──────────────────────────────
    differing_flags = bytearray(a_w * a_h)
    differing = 0
    total_unmasked = 0
    for y in range(a_h):
        row_start = y * a_w * 4
        for x in range(a_w):
            i = row_start + x * 4
            if mask_bitmap is not None and mask_bitmap[y * a_w + x]:
                continue
            total_unmasked += 1
            d = max(
                abs(a_pixels[i]     - b_pixels[i]),
                abs(a_pixels[i + 1] - b_pixels[i + 1]),
                abs(a_pixels[i + 2] - b_pixels[i + 2]),
            )
            if d > pixel_threshold:
                differing += 1
                differing_flags[y * a_w + x] = 1

    diff_pct = (100.0 * differing) / total_unmasked if total_unmasked > 0 else 0.0

    # ── Step 5: 4-connected clustering ───────────────────────────────────
    max_cluster, cluster_count = _max_cluster_4conn(differing_flags, a_w, a_h)

    # ── Step 6: acceptance ───────────────────────────────────────────────
    matched = (diff_pct < percent_threshold) and (max_cluster <= cluster_threshold)
    note = (
        f"diff={diff_pct:.3f}% threshold={percent_threshold:.1f}% "
        f"max_cluster={max_cluster}px threshold={cluster_threshold}px"
    )

    result = CompareResult(
        width=a_w, height=a_h, diff_percent=diff_pct,
        differing_pixels=differing, total_pixels=total_unmasked, matched=matched,
        masked_pixels=masked_pixel_count, max_cluster_size=max_cluster,
        cluster_count=cluster_count, note=note,
    )

    if emit_diff_image:
        result.diff_image_path = _write_diff_image(actual_path, b_pixels, differing_flags, a_w, a_h)
    if emit_diff_report:
        result.diff_report_path = _write_diff_report(
            actual_path, baseline_path, result,
            pixel_threshold, cluster_threshold, percent_threshold,
        )
    return result


# ---------------------------------------------------------------------------
# Env-sidecar pre-check helpers
# ---------------------------------------------------------------------------


def _sidecar_path(image_path: Path) -> Path:
    """Resolve `<image>.env.json` sidecar path.

    Convention: sibling file with `.env.json` suffix. Examples:
      tests/visual/baselines/00-empty-launch.png
        → tests/visual/baselines/00-empty-launch.env.json
      tests/screenshots/m16-spike/24.png
        → tests/screenshots/m16-spike/24.env.json
    """
    return image_path.with_name(image_path.stem + ".env.json")


def _check_env_sidecars(
    actual_path: Path, baseline_path: Path, require: bool
) -> list[str]:
    """Compare env sidecars; return list of drift descriptors.

    Empty list = contract honored. Non-empty = INVALID (H10).
    """
    actual_sc = _sidecar_path(actual_path)
    baseline_sc = _sidecar_path(baseline_path)

    actual_present = actual_sc.is_file()
    baseline_present = baseline_sc.is_file()

    if not actual_present or not baseline_present:
        if require:
            drift = []
            if not actual_present:
                drift.append(f"missing-sidecar: {actual_sc}")
            if not baseline_present:
                drift.append(f"missing-sidecar: {baseline_sc}")
            return drift
        # require_env_sidecar=False — pre-S5 transition / unit tests
        return []

    try:
        actual_env = json.loads(actual_sc.read_text())
        baseline_env = json.loads(baseline_sc.read_text())
    except json.JSONDecodeError as exc:
        return [f"sidecar-json-decode-error: {exc}"]

    drift: list[str] = []
    for keypath, source in ENV_CONTRACT_REQUIRED_KEYS:
        a_val = _nested_get(actual_env, keypath)
        b_val = _nested_get(baseline_env, keypath)
        if a_val != b_val:
            drift.append(f"{keypath}: actual={a_val!r} baseline={b_val!r} (per {source})")
    return drift


def _nested_get(obj: dict, dotted_path: str):
    """Return obj['a']['b']['c'] for 'a.b.c'; missing → None."""
    cur = obj
    for part in dotted_path.split("."):
        if not isinstance(cur, dict) or part not in cur:
            return None
        cur = cur[part]
    return cur


# ---------------------------------------------------------------------------
# Masking helpers
# ---------------------------------------------------------------------------


def _resolve_mask(mask, baseline_path: Path) -> list[dict]:
    """Resolve `mask` parameter into a list of {x, y, w, h, ...} dicts."""
    if mask is None:
        # Auto-discover <baseline>.mask.json
        auto_path = baseline_path.with_name(baseline_path.stem + ".mask.json")
        if auto_path.is_file():
            return _parse_mask_json(json.loads(auto_path.read_text()))
        return []
    if isinstance(mask, Path):
        if not mask.is_file():
            return []
        return _parse_mask_json(json.loads(mask.read_text()))
    if isinstance(mask, dict):
        return _parse_mask_json(mask)
    raise TypeError(f"mask parameter must be Path|dict|None; got {type(mask).__name__}")


def _parse_mask_json(spec: dict) -> list[dict]:
    """Validate + extract region list from mask JSON spec."""
    regions = spec.get("regions", [])
    if not isinstance(regions, list):
        raise ValueError("mask.json: 'regions' must be a list")
    out: list[dict] = []
    for i, r in enumerate(regions):
        for k in ("x", "y", "w", "h"):
            if k not in r:
                raise ValueError(f"mask.json regions[{i}]: missing required '{k}'")
        rationale = r.get("rationale", "")
        if not rationale:
            # Per contract §1 Step 3: empty rationale emits warning;
            # for stdlib version, simply note it on the parsed region
            # so the diff report can flag it.
            r = {**r, "_warning": "empty-rationale: mask region needs human-audit note"}
        out.append(r)
    return out


def _build_mask_bitmap(width: int, height: int, regions: list[dict]) -> bytearray:
    """Build a 1-byte-per-pixel mask: 1 = masked (skip in diff), 0 = compare."""
    bmp = bytearray(width * height)
    for r in regions:
        x0 = max(0, int(r["x"]))
        y0 = max(0, int(r["y"]))
        x1 = min(width, x0 + int(r["w"]))
        y1 = min(height, y0 + int(r["h"]))
        for y in range(y0, y1):
            row_off = y * width
            for x in range(x0, x1):
                bmp[row_off + x] = 1
    return bmp


# ---------------------------------------------------------------------------
# Clustering (Step 5)
# ---------------------------------------------------------------------------


def _max_cluster_4conn(flags: bytearray, width: int, height: int) -> tuple[int, int]:
    """4-connected BFS over `flags` bitmap; return (max cluster size, cluster count).

    Iterates the bitmap once; each differing pixel visited once across
    all BFS expansions (visit-tracking via a second bitmap of same
    size). O(W*H) time, O(W*H) memory for the visited bitmap.
    """
    visited = bytearray(width * height)
    max_size = 0
    count = 0
    for y in range(height):
        for x in range(width):
            idx = y * width + x
            if flags[idx] != 1 or visited[idx]:
                continue
            # BFS from (x, y)
            size = 0
            queue = deque([(x, y)])
            visited[idx] = 1
            while queue:
                cx, cy = queue.popleft()
                size += 1
                # 4-neighborhood
                for nx, ny in ((cx + 1, cy), (cx - 1, cy), (cx, cy + 1), (cx, cy - 1)):
                    if 0 <= nx < width and 0 <= ny < height:
                        nidx = ny * width + nx
                        if flags[nidx] == 1 and not visited[nidx]:
                            visited[nidx] = 1
                            queue.append((nx, ny))
            count += 1
            if size > max_size:
                max_size = size
    return max_size, count


# ---------------------------------------------------------------------------
# Diff image emission (Step 4/5 visualisation)
# ---------------------------------------------------------------------------


def _write_diff_image(
    actual_path: Path, baseline_pixels: bytes, differing_flags: bytearray, width: int, height: int
) -> Path:
    """Emit `<actual stem>.diff.png` — red overlay on greyscale baseline."""
    out = bytearray(width * height * 4)
    for y in range(height):
        for x in range(width):
            idx = y * width + x
            i = idx * 4
            if differing_flags[idx]:
                # Pure red overlay.
                out[i] = 0xff
                out[i + 1] = 0x40
                out[i + 2] = 0x40
                out[i + 3] = 0xff
            else:
                # Greyscale baseline (luminance from baseline RGB).
                r, g, b = baseline_pixels[i], baseline_pixels[i + 1], baseline_pixels[i + 2]
                lum = (r * 30 + g * 59 + b * 11) // 100
                out[i] = lum
                out[i + 1] = lum
                out[i + 2] = lum
                out[i + 3] = 0xff
    diff_path = actual_path.with_name(actual_path.stem + ".diff.png")
    _write_png_rgba(diff_path, width, height, bytes(out))
    return diff_path


# ---------------------------------------------------------------------------
# Diff report emission
# ---------------------------------------------------------------------------


def _write_diff_report(
    actual_path: Path, baseline_path: Path, result: CompareResult,
    pixel_threshold: int, cluster_threshold: int, percent_threshold: float,
) -> Path:
    """Emit `<actual stem>.diff-report.json` per contract §3."""
    report = {
        "state": baseline_path.stem,
        "baseline": str(baseline_path),
        "actual": str(actual_path),
        "verdict": "INVALID" if result.invalid else ("PASS" if result.matched else "FAIL"),
        "metrics": {
            "percent_differing":  round(result.diff_percent, 4),
            "differing_pixels":   result.differing_pixels,
            "total_pixels":       result.total_pixels,
            "masked_pixels":      result.masked_pixels,
            "max_cluster_size":   result.max_cluster_size,
            "cluster_count":      result.cluster_count,
        },
        "thresholds": {
            "pixel_threshold":    pixel_threshold,
            "cluster_threshold":  cluster_threshold,
            "percent_threshold":  percent_threshold,
        },
        "env_drift": result.env_drift,
        "note": result.note,
    }
    if result.diff_image_path is not None:
        report["diff_image_path"] = str(result.diff_image_path)
    report_path = actual_path.with_name(actual_path.stem + ".diff-report.json")
    report_path.write_text(json.dumps(report, indent=2, sort_keys=True))
    return report_path


# ---------------------------------------------------------------------------
# Stdlib PNG read + write
# ---------------------------------------------------------------------------


def _read_png_rgba(path: Path) -> tuple[int, int, bytes]:
    """Stdlib PNG → (width, height, RGBA bytes).

    Supports color-type 2 (RGB) + 6 (RGBA) at 8-bit depth.
    """
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"{path}: not a PNG (signature mismatch)")
    pos = 8
    width = height = depth = color_type = 0
    idat = bytearray()
    while pos < len(data):
        length = struct.unpack(">I", data[pos:pos + 4])[0]
        chunk_type = data[pos + 4:pos + 8]
        chunk_data = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if chunk_type == b"IHDR":
            (width, height, depth, color_type, _, _, _) = struct.unpack(">IIBBBBB", chunk_data)
        elif chunk_type == b"IDAT":
            idat.extend(chunk_data)
        elif chunk_type == b"IEND":
            break

    if depth != 8 or color_type not in (2, 6):
        raise NotImplementedError(
            f"{path}: depth={depth} color_type={color_type} not supported "
            f"(V0.2 expects 8-bit RGB or RGBA)."
        )
    bpp = 3 if color_type == 2 else 4
    raw = zlib.decompress(bytes(idat))
    stride = width * bpp
    expected = height * (1 + stride)
    if len(raw) != expected:
        raise ValueError(f"{path}: IDAT decompressed size {len(raw)} != expected {expected}")

    out = bytearray(width * height * 4)
    prev_row = bytes(stride)
    for y in range(height):
        offset = y * (1 + stride)
        filt = raw[offset]
        row = bytearray(raw[offset + 1:offset + 1 + stride])
        if filt == 0:
            pass
        elif filt == 1:
            for x in range(bpp, stride):
                row[x] = (row[x] + row[x - bpp]) & 0xFF
        elif filt == 2:
            for x in range(stride):
                row[x] = (row[x] + prev_row[x]) & 0xFF
        elif filt == 3:
            for x in range(stride):
                left = row[x - bpp] if x >= bpp else 0
                up = prev_row[x]
                row[x] = (row[x] + ((left + up) // 2)) & 0xFF
        elif filt == 4:
            for x in range(stride):
                left = row[x - bpp] if x >= bpp else 0
                up = prev_row[x]
                up_left = prev_row[x - bpp] if x >= bpp else 0
                p = left + up - up_left
                pa = abs(p - left)
                pb = abs(p - up)
                pc = abs(p - up_left)
                if pa <= pb and pa <= pc:
                    pred = left
                elif pb <= pc:
                    pred = up
                else:
                    pred = up_left
                row[x] = (row[x] + pred) & 0xFF
        else:
            raise ValueError(f"{path}: unknown PNG filter {filt} on row {y}")
        prev_row = bytes(row)

        if bpp == 3:
            for x in range(width):
                src = x * 3
                dst = (y * width + x) * 4
                out[dst]     = row[src]
                out[dst + 1] = row[src + 1]
                out[dst + 2] = row[src + 2]
                out[dst + 3] = 0xFF
        else:
            dst_row_start = y * width * 4
            out[dst_row_start:dst_row_start + stride] = row

    return width, height, bytes(out)


def _write_png_rgba(path: Path, width: int, height: int, rgba: bytes) -> None:
    """Stdlib RGBA → PNG. Filter type 0 (None) on every row; zlib compress."""

    def _chunk(tag: bytes, payload: bytes) -> bytes:
        crc = zlib.crc32(tag + payload)
        return struct.pack(">I", len(payload)) + tag + payload + struct.pack(">I", crc)

    sig = b"\x89PNG\r\n\x1a\n"
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    stride = width * 4
    raw = bytearray()
    for y in range(height):
        raw.append(0)  # filter type None
        raw.extend(rgba[y * stride:(y + 1) * stride])
    idat = zlib.compress(bytes(raw), 6)
    iend = b""
    path.write_bytes(sig + _chunk(b"IHDR", ihdr) + _chunk(b"IDAT", idat) + _chunk(b"IEND", iend))
