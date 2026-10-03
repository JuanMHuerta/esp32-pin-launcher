# SPDX-License-Identifier: GPL-3.0-only
"""Record a serial session; optional command file can be appended while running.

Use ESP-IDF's Python environment (pyserial). Example:
python tools/device.py --seconds 600 --commands artifacts/commands.txt
An append of 'capture\n' writes the exact next displayed logical frame to a PPM.
The captured framebuffer is evidence of rendering, not a photograph of the panel.
"""

import argparse
from pathlib import Path
import re
import time
import serial


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="/dev/ttyACM0")
    parser.add_argument("--seconds", type=float, default=600)
    parser.add_argument("--log", type=Path, default=Path("artifacts/hardware.log"))
    parser.add_argument("--commands", type=Path)
    parser.add_argument(
        "--exercise",
        action="store_true",
        help="exercise all states, capture them, then resume autonomous mode",
    )
    parser.add_argument(
        "--heap-after",
        type=float,
        help="dump a diagnostic build's heap trace after this many seconds",
    )
    args = parser.parse_args()
    args.log.parent.mkdir(parents=True, exist_ok=True)
    if args.commands:
        args.commands.touch(exist_ok=True)
    command_position = 0
    # Set explicit line states. Native USB Serial/JTAG may still reset on open;
    # preserve boot output and never assume this attaches without a reset.
    port = serial.Serial(port=None, baudrate=115200, timeout=0.05)
    port.dtr = False
    port.rts = False
    port.port = args.port
    port.open()
    started = time.monotonic()
    deadline = started + args.seconds
    scheduled = []
    if args.heap_after is not None:
        scheduled.append((args.heap_after, "heap\n"))
    if args.exercise:
        states = ["idle", "walk", "sniff", "eat", "sleep", "love", "play", "surprise", "wave"]
        for i, state in enumerate(states):
            scheduled.extend([(i * 4 + 1, f"state {state}\n"), (i * 4 + 3, "capture\n")])
        scheduled.extend(
            [
                (37, "tap 5 50\n"),
                (43, "swipe 110 30\n"),
                (47, "hold\n"),
                (49, "hold\n"),
                (52, "shake\n"),
                (56, "auto\n"),
                (57, "bars\n"),
                (59, "capture\n"),
                (60, "auto\n"),
                (61, "brightness 0\n"),
                (63, "brightness 2\n"),
                (65, "brightness 1\n"),
            ]
        )
    scheduled.sort()
    count = 0
    frame = None
    row_count = 0
    width = height = 0
    with port, args.log.open("w", buffering=1) as log:
        port.write(b"status\n")
        pending = b""
        while time.monotonic() < deadline:
            if scheduled and time.monotonic() - started >= scheduled[0][0]:
                _, cmd = scheduled.pop(0)
                port.write(cmd.encode())
                print(f"EXERCISE {cmd.strip()}", flush=True)
            if args.commands:
                with args.commands.open("r") as commands:
                    commands.seek(command_position)
                    data = commands.read()
                    command_position = commands.tell()
                if data:
                    port.write(data.encode())
                    print(f"SENT {data.strip()}", flush=True)
            data = port.read(max(1, port.in_waiting))
            if not data:
                continue
            pending += data
            while b"\n" in pending:
                raw, pending = pending.split(b"\n", 1)
                line = re.sub(r"\x1b\[[0-9;]*m", "", raw.decode(errors="replace")).strip()
                log.write(line + "\n")
                if line.startswith("FRAME "):
                    _, w, h = line.split()
                    width, height = int(w), int(h)
                    frame = bytearray()
                    row_count = 0
                    print(line, flush=True)
                elif frame is not None and re.fullmatch(r"[0-9a-f]{" + str(width * 4) + r"}", line):
                    for x in range(width):
                        pixel = int(line[x * 4 : x * 4 + 4], 16)
                        frame.extend(
                            (
                                ((pixel >> 11) & 31) * 255 // 31,
                                ((pixel >> 5) & 63) * 255 // 63,
                                (pixel & 31) * 255 // 31,
                            )
                        )
                    row_count += 1
                elif line == "END_FRAME" and frame is not None:
                    if row_count != height:
                        raise RuntimeError(f"incomplete capture: {row_count}/{height} rows")
                    count += 1
                    path = args.log.parent / f"device-frame-{count:02}.ppm"
                    path.write_bytes(f"P6\n{width} {height}\n255\n".encode() + frame)
                    frame = None
                    print(f"SAVED {path}", flush=True)
                elif line:
                    print(line, flush=True)
        print(f"Session complete: {args.log}, {count} framebuffer captures", flush=True)


if __name__ == "__main__":
    main()
