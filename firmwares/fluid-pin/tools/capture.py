"""Capture device evidence without restarting work after an observation timeout."""
import argparse
import time
from pathlib import Path
import serial

p = argparse.ArgumentParser()
p.add_argument("--port", default="/dev/ttyACM0")
p.add_argument("--seconds", type=float, default=30)
p.add_argument("--out", type=Path, required=True)
args = p.parse_args()
with serial.Serial(args.port, 115200, timeout=0.25) as device, args.out.open("wb") as log:
    deadline = time.monotonic() + args.seconds
    while time.monotonic() < deadline:
        data = device.readline()
        if data:
            log.write(data)
            log.flush()
            print(data.decode(errors="replace"), end="", flush=True)
