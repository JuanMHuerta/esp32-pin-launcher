# WAYFARER

[English](README.md) · [Español](README.es.md)

A pixel-art take on Wayfarer for the Waveshare ESP32-S3-Touch-AMOLED-1.91
(SKU 28596). The cozy hauler cockpit has patched cream and olive-grey
panels, warm cabin lights, and green instruments. Four amber fixtures have
bright cores, soft bloom, and gently breathing light pools. Moving cool,
event-tinted reflections cast soft rib shadows across the console, and the
main screen spills green light onto nearby panels. Material shading preserves
the pixel-art scratches and detail. Freighters, couriers, and tankers use
weathered grey hulls with small faded orange and teal details. Exterior hulls
have crisp panel seams, vents, access hatches, directional shade, and wear.

Outside, six detailed worlds approach and pass at different depths: ocean, desert,
ice, ringed gas giant, volcanic, and cratered moon. Their dark night sides,
textured terrain, fine clouds, fractures, crater rims, and small surface lights
use a subdued palette. Five ship designs form
passing traffic and convoys, with flickering engines and blinking running
lights. Orbital stations, comets, eclipses, asteroid alerts, and colorful
nebulae join the encounter cycle. Comets have textured nuclei and curved dust
streams; eclipses use a textured sun, cratered moon, pale magnetic streamers,
and small solar prominences. Sunlight on the cabin falls during totality.

Each journey follows arrival, a local encounter, and departure. Worlds begin
small near the flight direction, grow as the ship approaches, and pass out of
the canopy along different bearings. Ships fly diagonally with hulls and
exhaust aligned to their paths. All objects persist across encounter boundaries
and sort by distance, so nearby traffic can pass in front of a world. Jump
acceleration leaves existing scenery behind before revealing a new destination.
Local encounters use a shuffled cycle. Slowly changing sky colors reflect inside the cabin, while
layered stars twinkle and stretch into trails as travel accelerates into a jump.

See the [continuous journey](preview-journey.gif),
[journey contact sheet](preview-journey-frames.png),
[eclipse animation](preview-eclipse.gif), and
[pixel-art asset sheet](preview-exterior-assets.png).
The [world contact sheet](preview-worlds.png) shows all six detailed surfaces
and their night sides through the cockpit.
The [planet flyby preview](preview.gif) and [scene still](preview-scene.png)
show the updated scenery during travel.
The [lighting contact sheet](preview-lighting.png) shows the light and shadows
at several points in the animation.
The app runs in `ota_6`; press `7` over USB Serial/JTAG or select WAYFARER
in the launcher. Hold BOOT for 1.5 seconds and release to return to the launcher.

Artwork sources and rebuild instructions are in
[`assets/ART_DIRECTION.md`](assets/ART_DIRECTION.md). Rebuild the embedded
artwork with:

```sh
python3 tools/prepare_background.py assets/source/masters/cockpit-generated-v2.png main/assets/cockpit.rgb565 preview-cockpit.png
python3 tools/compile_assets.py
python3 tools/make_exterior_assets.py
```

To embed hand-edited exterior PNGs without regenerating their artwork, use
`python3 tools/make_exterior_assets.py --compile-only`.

Render a native-resolution preview using the same C scene renderer:

```sh
python3 tools/make_preview.py --seconds 8 --fps 12
python3 tools/make_preview.py --showcase --seed 25 --seconds 96 --fps 10 --output preview-journey.gif --still preview-journey.png
python3 tools/make_preview.py --event 10 --seed 25 --seconds 22 --fps 12 --output preview-eclipse.gif --still preview-eclipse.png
```

Host scene checks are available with `bash tools/test.sh`.

OTA subtypes above describe the full collection. Browser-selected installs assign
consecutive subtypes; USB shortcuts keep their app identities.
