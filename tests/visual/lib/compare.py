"""M15 S1 — pixel-level baseline diff (CI's only visual gate).

Per M15-concerns C7: CI runs no vision LLM (public-repo
security). Pixel diff is the only deterministic regression
gate available to CI; vision-LLM verdicts stay local-only.

This module implements baseline comparison using only Python
stdlib (no PIL / Pillow dep). The PNG decoder reads IHDR +
IDAT chunks, decompresses via zlib, applies the row-filter
defilter (None / Sub / Up / Average / Paeth), and yields
RGBA bytes for byte-by-byte diff with a tolerance percentage.

Tolerance semantics: a pixel "matches" the baseline if every
RGBA channel differs by ≤ `channel_tolerance` (default 8 of
255) from the baseline. The diff score is the percentage of
pixels that fall outside that threshold; tests fail if score
> `max_diff_percent` (default 5%).

Aliased font rendering and minor RHI driver variation
typically produces a few-percent diff; theme / chart-line /
widget-position changes produce > 10%.
"""

from __future__ import annotations

import struct
import zlib
from dataclasses import dataclass
from pathlib import Path


@dataclass
class CompareResult:
    """Outcome of pixel-level baseline diff."""

    width: int
    height: int
    diff_percent: float
    differing_pixels: int
    total_pixels: int
    matched: bool
    note: str = ""


def compare_baseline(
    actual_path: Path | str,
    baseline_path: Path | str,
    max_diff_percent: float = 5.0,
    channel_tolerance: int = 8,
) -> CompareResult:
    """Compare two PNGs; return CompareResult with diff metrics.

    Treats absent baseline as a pass with `note="baseline-absent"`
    so new states can be introduced without immediately failing
    CI; operator approves via `scripts/accept-baseline.sh`.

    Treats different dimensions as a failure (> max_diff_percent).
    """
    actual_path = Path(actual_path)
    baseline_path = Path(baseline_path)

    if not baseline_path.is_file():
        # New state: pixel diff cannot apply yet. Surface clearly
        # so PR review can authorize the baseline via accept-
        # baseline.sh.
        return CompareResult(
            width=0,
            height=0,
            diff_percent=0.0,
            differing_pixels=0,
            total_pixels=0,
            matched=True,
            note=f"baseline-absent: {baseline_path}",
        )

    a_w, a_h, a_pixels = _read_png_rgba(actual_path)
    b_w, b_h, b_pixels = _read_png_rgba(baseline_path)

    if (a_w, a_h) != (b_w, b_h):
        return CompareResult(
            width=a_w,
            height=a_h,
            diff_percent=100.0,
            differing_pixels=a_w * a_h,
            total_pixels=a_w * a_h,
            matched=False,
            note=f"dimension-mismatch: actual={a_w}×{a_h} baseline={b_w}×{b_h}",
        )

    differing = 0
    total = a_w * a_h
    # Compare RGBA byte-by-byte; channel tolerance smooths font AA
    for i in range(0, len(a_pixels), 4):
        if any(abs(a_pixels[i + ch] - b_pixels[i + ch]) > channel_tolerance for ch in range(4)):
            differing += 1

    diff_pct = (100.0 * differing) / total if total > 0 else 0.0
    matched = diff_pct <= max_diff_percent
    return CompareResult(
        width=a_w,
        height=a_h,
        diff_percent=diff_pct,
        differing_pixels=differing,
        total_pixels=total,
        matched=matched,
        note=f"diff={diff_pct:.2f}% threshold={max_diff_percent:.1f}%",
    )


def _read_png_rgba(path: Path) -> tuple[int, int, bytes]:
    """Stdlib PNG → (width, height, RGBA bytes).

    Supports color-type 2 (RGB) + 6 (RGBA) at 8-bit depth, the
    only formats `QPixmap::save(..., "PNG")` produces in V0.2.
    Other formats raise NotImplementedError.
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
        # CRC (4 bytes) skipped
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

        # Convert RGB → RGBA on the fly so downstream comparison is uniform
        if bpp == 3:
            for x in range(width):
                src = x * 3
                dst = (y * width + x) * 4
                out[dst] = row[src]
                out[dst + 1] = row[src + 1]
                out[dst + 2] = row[src + 2]
                out[dst + 3] = 0xFF
        else:
            dst_row_start = y * width * 4
            out[dst_row_start:dst_row_start + stride] = row

    return width, height, bytes(out)
