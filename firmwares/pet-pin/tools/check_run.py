# SPDX-License-Identifier: GPL-3.0-only
"""Check a completed on-device exercise/soak, without inferring physical tests."""

import argparse
import hashlib
import json
from pathlib import Path
import re

from states import STATES


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    text = args.log.read_text()
    perf = []
    for line in text.splitlines():
        m = re.search(
            r"PERF fps=([\d.]+) avg_us=(\d+) max_us=(\d+) late=(\d+) "
            r"period_us=(\d+)\.\.(\d+) heap=(\d+) state=(\w+)",
            line,
        )
        if m:
            perf.append(
                dict(
                    zip(
                        [
                            "fps",
                            "avg_us",
                            "max_us",
                            "late",
                            "period_min_us",
                            "period_max_us",
                            "heap",
                            "state",
                        ],
                        [float(m[1])] + [int(m[i]) for i in range(2, 8)] + [m[8]],
                    )
                )
            )
    assert len(perf) >= 36, (
        f"need at least six minutes of frame statistics; found {len(perf)} windows"
    )
    assert all(29.8 <= x["fps"] <= 30.2 for x in perf), "FPS outside budget"
    assert all(x["max_us"] < 33333 and x["late"] == 0 for x in perf), "frame deadline miss"
    assert all(30000 <= x["period_min_us"] <= x["period_max_us"] <= 36000 for x in perf), (
        "frame cadence jitter"
    )
    assert len({x["heap"] for x in perf}) == 1, "heap changed during steady rendering"
    assert not re.search(r"^E \(|Guru Meditation|assert failed|CORRUPT|watchdog", text, re.M), (
        "firmware error"
    )
    assert text.count("Miso 1.0.0 reset=") == 1, "unexpected reset or missing boot evidence"
    states = STATES
    for state in states:
        assert f"COMMAND state {state} -> {state}" in text, f"unexercised state {state}"
    assert text.count("END_FRAME") == len(states) + 1, (
        "missing animation/color-bar framebuffer captures"
    )
    for level in range(3):
        assert f"COMMAND brightness {level} ->" in text, "untested brightness level"
    # Check the actual captured color-bar payload, not merely its command ACK.
    bars = text.split("COMMAND bars ->", 1)[1].split("END_FRAME", 1)[0]
    rows = re.findall(r"^[0-9a-f]{536}$", bars, re.M)
    assert len(rows) == 60, "incomplete diagnostic color bars"
    colors = [0xF800, 0x07E0, 0x001F, 0xFFFF, 0]
    expected = "".join(f"{colors[x * 5 // 134]:04x}" for x in range(134))
    assert all(row == expected for row in rows), "color or framebuffer order incorrect"
    statuses = [line for line in text.splitlines() if "STATUS " in line]
    final = statuses[-1]
    assert "touch=1 imu=1" in final and "errors=0 dropped=0" in final, "input hardware error"
    assert f"visits=0x{(1 << len(states)) - 1:x}" in final, "not all behaviors visited"
    assert int(re.search(r"samples=(\d+)", final)[1]) > 15000, "insufficient motion samples"
    assert int(re.search(r"uptime=(\d+)", final)[1]) >= 420000, "insufficient runtime"
    root = Path(__file__).resolve().parents[1]
    elf = root / "build/miso_pin.elf"
    prefix = re.search(r"ELF file SHA256:\s+([0-9a-f]+)", text)[1]
    elf_hash = hashlib.sha256(elf.read_bytes()).hexdigest()
    assert elf_hash.startswith(prefix), "running firmware differs from the current build"
    report = {
        "result": "PASS",
        "windows": len(perf),
        "fps_min": min(x["fps"] for x in perf),
        "fps_max": max(x["fps"] for x in perf),
        "max_work_us": max(x["max_us"] for x in perf),
        "max_period_us": max(x["period_max_us"] for x in perf),
        "min_period_us": min(x["period_min_us"] for x in perf),
        "late_frames": sum(x["late"] for x in perf),
        "steady_heap_bytes": perf[-1]["heap"],
        "elf_sha256": elf_hash,
        "log": str(args.log),
        "final_status": final,
        "scope": "USB hardware exercise and soak; historical physical observations in documentation/APP_VALIDATION.md",
    }
    output = args.log.with_suffix(".json")
    output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
