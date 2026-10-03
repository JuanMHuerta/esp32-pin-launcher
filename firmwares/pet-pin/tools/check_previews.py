# SPDX-License-Identifier: GPL-3.0-only
"""Check exported animation dimensions, timing, variation and hard frame jumps.

This complements visual review. It cannot decide whether a mascot is appealing.
Small global palette changes are allowed; luminance changes >=16/255 are counted.
"""

import json
from pathlib import Path
from PIL import Image, ImageChops

from states import STATES

ROOT = Path(__file__).resolve().parents[1]


def main():
    result = {}
    for state in STATES:
        im = Image.open(ROOT / "artifacts" / f"{state}.gif")
        duration = 0
        max_change = 0
        unique = set()
        last = None
        for i in range(im.n_frames):
            im.seek(i)
            duration += im.info["duration"]
            frame = im.convert("RGB")
            unique.add(hash(frame.tobytes()))
            assert frame.size == (536, 240), state
            if last is not None:
                mask = (
                    ImageChops.difference(last, frame)
                    .convert("L")
                    .point(lambda v: 255 if v >= 16 else 0)
                )
                max_change = max(max_change, mask.histogram()[255] / (536 * 240))
            last = frame
        assert duration == 5000 and len(unique) > 50 and max_change < 0.25, (
            state,
            duration,
            len(unique),
            max_change,
        )
        result[state] = {
            "duration_ms": duration,
            "frames": im.n_frames,
            "unique": len(unique),
            "max_significant_pixel_change": round(max_change, 4),
        }
    (ROOT / "artifacts/animation-checks.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        "PASS: all exported animations have correct size/duration, vary, and contain no hard frame jumps"
    )


if __name__ == "__main__":
    main()
