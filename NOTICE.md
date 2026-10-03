# Licensing and asset sources

[English](NOTICE.md) · [Español](NOTICE.es.md)

ESP32 Pin Launcher code, documentation and repository artwork are distributed
under GPL-3.0-only. The full license is in [LICENSE](LICENSE). Source files carry
SPDX identifiers; generated asset tables inherit the license of their sources.

ESP-IDF and downloaded components are external dependencies. Their source and
license notices are supplied by their upstream distributions, not replaced by
this project's license:

| Dependency | Version used | License |
| --- | --- | --- |
| ESP-IDF | 5.5.1 | Apache-2.0, with separately licensed components |
| `espressif/esp_lcd_sh8601` | 2.0.1~1 | Apache-2.0 |
| `espressif/cmake_utilities` | 0.5.3 | Apache-2.0 |

The component manager downloads these from the manifests and lockfiles.
When distributing firmware, retain the applicable upstream license and notice
files alongside the corresponding source and build instructions.

The static web flasher bundles `esptool-js` 0.7.0 (Apache-2.0), `pako` 2.x
(MIT and Zlib), `atob-lite` 2.x (MIT), and `spark-md5` 3.0.2 (MIT).
The exact versions are recorded in [web/package-lock.json](web/package-lock.json).
The JavaScript bundle retains upstream license comments. The site build also
includes these license texts in `THIRD_PARTY_LICENSES.txt`.
The firmware packager includes upstream firmware license/notice files in
`FIRMWARE_LICENSES.txt`, the matching project source in `source.tar.gz`, and
dependency source locations in `SOURCE.txt`.

Conway's patterns and Three Body's figure-eight initial conditions are
mathematical data; their source references are retained in the code. Maze is
an independently drawn interpretation of a classic screensaver. Its textures
and objects are generated in C; the repository does not include Microsoft's
screensaver or assets.

Miso, Lumen, Conway, Fluid, Maze and CRT draw their graphics procedurally.
Dungeon and Wayfarer include AI-generated artwork processed by
the repository's asset tools, plus procedural art. Editable PNG sources are included; see the
[Dungeon](firmwares/dungeon-pin/assets/ART_DIRECTION.md) and
[Wayfarer](firmwares/wayfarer-pin/assets/ART_DIRECTION.md) asset notes.
