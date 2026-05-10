#!/usr/bin/env python3
"""M14 S1 CI smoke harness — UDP fixture frame sender.

Sends `temperature_sensor` schema frames (per
`examples/schemas/temperature_sensor.yaml`) to a local UDP port at a
fixed cadence. Stdlib only — no external deps (CLAUDE.md §1).

Frame layout (16 bytes, little-endian per the schema):

  [0]      0xAA            magic byte (layout match)
  [1..5]   uint32 LE       timestamp_ms (monotonic counter)
  [5..7]   int16  LE       temperature (raw; *0.01 = degC)
  [7..9]   uint16 LE       pressure (raw; *0.1 = kPa)
  [9]      uint8           status bits
  [10..12] uint16 LE       crc (not validated)
  [12..16] uint32 LE       padding

Usage:
  udp_fixture_sender.py --host 127.0.0.1 --port 9998 \\
                        --frames 200 --rate-hz 50

Exits 0 on success.
"""

import argparse
import math
import socket
import struct
import sys
import time


def make_frame(seq: int, base_ns: int) -> bytes:
    # M14 F4 Wave 1 (Path α): the temperature_sensor schema documents
    # `timestamp_ms` as "Device-local monotonic millisecond counter".
    # Production SchemaDecoder timestamps samples with `frame.recvAt`
    # (driver-set steady_clock::now()), not with this field — but real
    # hardware reports a `timestamp_ms` close to host wall-clock, so
    # we anchor the synthetic fixture's value the same way to keep
    # the smoke fixture indistinguishable from production data on
    # any consumer that does inspect the field.
    timestamp_ns = base_ns + seq * 20_000_000  # 50 Hz cadence in ns
    timestamp_ms = (timestamp_ns // 1_000_000) & 0xFFFFFFFF  # uint32 wrap
    # Sweep the temperature signal so the chart actually moves —
    # a flat-line signal won't produce non-clear pixels in many themes.
    temperature_raw = int(2500 + 500 * math.sin(seq * 0.05))  # raw int16
    pressure_raw = int(1010 + 50 * math.sin(seq * 0.03))      # raw uint16
    status = 0x02  # calibration_active bit
    crc = 0
    padding = 0
    return struct.pack(
        "<BIhHBHI",
        0xAA,
        timestamp_ms,
        temperature_raw,
        pressure_raw,
        status,
        crc,
        padding,
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=9998)
    parser.add_argument("--frames", type=int, default=200)
    parser.add_argument("--rate-hz", type=float, default=50.0)
    parser.add_argument("--initial-delay-s", type=float, default=0.5,
                        help="Wait this long before sending the first frame "
                             "(lets the binary finish auto-load + connect).")
    args = parser.parse_args()

    period = 1.0 / args.rate_hz
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    addr = (args.host, args.port)

    if args.initial_delay_s > 0:
        time.sleep(args.initial_delay_s)

    sent = 0
    start = time.monotonic()
    base_ns = time.time_ns()  # anchor `timestamp_ms` near host wall-clock
    for seq in range(args.frames):
        sock.sendto(make_frame(seq, base_ns), addr)
        sent += 1
        # Sleep until next deadline (drop drift if behind).
        deadline = start + (seq + 1) * period
        now = time.monotonic()
        if deadline > now:
            time.sleep(deadline - now)

    elapsed = time.monotonic() - start
    print(f"udp_fixture_sender: sent {sent} frames in {elapsed:.2f}s "
          f"({sent/elapsed:.1f} Hz)", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
