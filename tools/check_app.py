#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Launch a scene app from the running launcher and observe its serial health."""

import argparse
import re
import time
import serial


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("app", choices=["three-body-pin", "crt-pin"])
    parser.add_argument("--port", default="/dev/ttyACM0")
    parser.add_argument("--seconds", type=float, default=150)
    parser.add_argument(
        "--all-profiles", action="store_true", help="require runtime samples from every scenario"
    )
    args = parser.parse_args()
    if args.seconds < 10:
        parser.error("--seconds must be at least 10")
    tag = "three_body" if args.app == "three-body-pin" else "crt_pin"
    command = b"8" if args.app == "three-body-pin" else b"9"
    ready = False
    samples = []
    with serial.Serial(args.port, 115200, timeout=0.25, write_timeout=2) as port:
        # Opening this USB device can reset it. Start from the factory launcher.
        time.sleep(2)
        port.write(command)
        deadline = time.monotonic() + args.seconds
        while time.monotonic() < deadline:
            line = port.readline().decode("utf-8", errors="replace").strip()
            if not line:
                continue
            print(line, flush=True)
            if "Guru Meditation" in line or "abort()" in line or "panic" in line.lower():
                raise RuntimeError("Device reported a crash")
            if f"{tag}: READY:" in line:
                ready = True
            if f"{tag}: RUN " in line:
                samples.append(line)
    if not ready or len(samples) < 2:
        raise RuntimeError("Expected app READY and at least two runtime samples")
    heaps = [int(re.search(r"heap=(\d+)", line)[1]) for line in samples]
    if max(heaps) - min(heaps) > 2048:
        raise RuntimeError(f"Free heap changed by {max(heaps) - min(heaps)} bytes")
    fps = [float(re.search(r"fps=([\d.]+)", line)[1]) for line in samples]
    if min(fps) < 15:
        raise RuntimeError(f"Render rate below 15 fps: {min(fps)}")
    if args.app == "three-body-pin":
        drifts = [abs(float(re.search(r"energy_drift=([-\d.e+]+)", line)[1])) for line in samples]
        if max(drifts) > 0.0001:
            raise RuntimeError(f"Energy drift too large: {max(drifts)}")
    if args.all_profiles:
        if args.app == "three-body-pin":
            observed = {re.search(r"preset=(.*?) time=", line)[1] for line in samples}
            required = {"FIGURE EIGHT", "CHAOTIC SUNS", "BINARY VISITOR"}
        else:
            observed = {int(re.search(r"profile=(\d+)", line)[1]) for line in samples}
            required = set(range(6))
        if observed != required:
            raise RuntimeError(f"Missing scenarios: {required - observed}")
    print(
        f"PASS: {len(samples)} runtime samples, fps={min(fps):.1f}..{max(fps):.1f}, "
        f"heap={min(heaps)}..{max(heaps)}",
        flush=True,
    )


if __name__ == "__main__":
    main()
