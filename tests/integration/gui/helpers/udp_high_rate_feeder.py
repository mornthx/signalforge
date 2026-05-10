#!/usr/bin/env python3
"""M14 S6 mechanical 18-test T11 — high-rate UDP feeder for backpressure.

Sends `temperature_sensor` schema frames at a configurable high rate
(default 5 kHz) into a local UDP port. The receiver-under-test
(SignalForge) is expected to either keep up or apply M5/M6/M10 back-
pressure (drop counters increment, file footer still valid).

Stdlib only — no PIL, no Pillow (CLAUDE.md §1).

Usage:
  udp_high_rate_feeder.py --host 127.0.0.1 --port 9998 \\
                          --frames 5000 --rate-hz 5000

Exits 0 on success.
"""

import argparse
import math
import socket
import struct
import sys
import time


def make_frame(seq: int, base_ns: int) -> bytes:
    timestamp_ns = base_ns + seq * 200_000  # 5 kHz cadence
    timestamp_ms = (timestamp_ns // 1_000_000) & 0xFFFFFFFF
    temperature_raw = int(2500 + 500 * math.sin(seq * 0.005))
    pressure_raw = int(1010 + 50 * math.sin(seq * 0.003))
    status = 0x02
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
    parser.add_argument("--frames", type=int, default=5000)
    parser.add_argument("--rate-hz", type=float, default=5000.0)
    parser.add_argument("--initial-delay-s", type=float, default=0.5)
    args = parser.parse_args()

    period = 1.0 / args.rate_hz
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    addr = (args.host, args.port)

    if args.initial_delay_s > 0:
        time.sleep(args.initial_delay_s)

    sent = 0
    start = time.monotonic()
    base_ns = time.time_ns()
    for seq in range(args.frames):
        try:
            sock.sendto(make_frame(seq, base_ns), addr)
        except OSError:
            # Buffer full / receiver overrun — back off briefly. The
            # receiver-under-test's behaviour is what we're measuring.
            time.sleep(0.0005)
            continue
        sent += 1
        deadline = start + (seq + 1) * period
        now = time.monotonic()
        if deadline > now:
            time.sleep(deadline - now)

    elapsed = time.monotonic() - start
    print(f"udp_high_rate_feeder: sent {sent} frames in {elapsed:.2f}s "
          f"({sent/elapsed:.1f} Hz)", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
