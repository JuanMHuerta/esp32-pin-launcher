# 3D MAZE

[English](README.md) · [Español](README.es.md)

An autoplaying, redrawn tribute to the classic Windows 3D Maze screensaver for the
Waveshare ESP32-S3 Touch AMOLED 1.91. It renders at 268×120 and scales to 536×240.
Short BOOT presses have no action; hold BOOT for 1.5 seconds to return to the launcher.

The maze uses a 10×10 grid of thin partition walls, with short branches, junctions,
and dead ends. The camera follows the right wall at three cells per second, with
quarter turns taking about 0.24 seconds. It explores branches and turns around at
dead ends; every generated maze is a connected tree, so the wall follower always
reaches the exit. Movement consumes the full frame time across cell and turn
boundaries to avoid a pause at every tile.

Red brick with pale mortar, stippled ceiling tiles, continuous wood grain, a low
viewpoint, and bright lighting follow the visual cues of the
[Windows 3D Maze reference](https://github.com/coco-monier/Windows-95-3D-Maze).
Textures and objects are redrawn in code. A yellow smiley with blue features marks
the exit; the walls lower and another maze rises. Occasional runs contain a brown
rat that independently follows passages, or a rotating gray octahedron made from
3D triangles. Objects occupy world coordinates and are hidden by walls. Encounters
alternate with empty runs. The original screen-roll effect is not implemented.

## Development

![Maze exploration](preview.gif)

From the repository root:

```sh
./tools/test.sh --app maze-pin
python3 tools/make_previews.py maze-pin
idf.py -C firmwares/maze-pin build
```

The tests check 64 seeds, reciprocal walls, connected mazes, camera and rat
collisions, repeatable runs, exits, frame-rate independence, occlusion and
rendering bounds. Use the root [build script](../../build-and-flash.sh) to install
the collection. Maze occupies `ota_5`; its address comes from `partitions.csv`.
Historical device results are in [app validation](../../documentation/APP_VALIDATION.md).

OTA subtypes above describe the full collection. Browser-selected installs assign
consecutive subtypes; USB shortcuts keep their app identities.
