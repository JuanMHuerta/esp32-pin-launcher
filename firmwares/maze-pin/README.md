# 3D MAZE

An autoplaying, redrawn tribute to the classic Windows 3D Maze screensaver for the
Waveshare ESP32-S3 Touch AMOLED 1.91. It draws a textured first person maze at
268×120 and scales it to the 536×240 display. Each run builds a branching,
connected maze with intersections, walks to its exit, and generates another.
An occasional rat or floating shape occupies a fixed place in the maze and
passes out of view as the camera moves. Short BOOT presses have no action; hold
BOOT for 1.5 seconds to return to the launcher.

From an ESP-IDF 5.5 shell, run `idf.py build` here. The resulting
`build/maze_pin.bin` goes in the launcher's `ota_6` partition at `0x6a0000`.

For host checks, run `./tools/test.sh`. It writes PPM previews of the title,
maze, junction, rat encounter, floating shape, and exit. Convert them to PNG
with ImageMagick if desired, for example `magick preview.ppm preview.png`.
