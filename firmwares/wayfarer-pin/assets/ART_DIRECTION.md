# Wayfarer artwork

[English](ART_DIRECTION.md) · [Español](ART_DIRECTION.es.md)

Wayfarer uses a cozy, repaired hauler cockpit with warm cream,
olive-grey, and amber materials. A cool event-colored spill sweeps across the
canopy frame and projects soft rib shadows onto the console. Four amber
fixtures have bright cores and additive bloom, with gently breathing pools of
warm light. The main terminal spills green light onto its surrounding panels.
Multiplicative material shading keeps the scratches and panel contrast visible.
The exterior uses subdued slate, deep blue, and umber with selective lit edges.
Traffic ships have weathered grey paint, recessed access panels and vents,
scars, and worn orange and teal markings. Night sides occupy much of each
world, while coastlines, cloud filaments, fractured ice, eroded ridges, storm
bands, crater rims, and small emissive details provide surface texture.
All cutouts and the dust field use a limited pixel-art palette.

The editable masters live in `source/`:

- `masters/cockpit-generated-v2.png`: brighter pixel-art cockpit plate; the
  first pass, `cockpit-generated.png`, is retained for comparison.
- `masters/cockpit-window-mask.png`: source alpha mask that keeps the original
  canopy openings transparent for passing traffic.
- `space-sprites-pixel.png`: three ships, a planet, moon, and asteroid in a
  three-column, two-row atlas.
- `space-pixel.png`: the scrolling nebula dust field.
- `exterior/world_0.png` through `world_5.png`: ocean, desert, ice, ringed gas
  giant, volcanic, and cratered worlds, with surface texture, atmospheric
  edges, clouds, and night-side shading.
- `exterior/shuttle.png`, `tug.png`, and `station.png`: compact working ships
  and a ring habitat with solar arrays, windows, and navigation beacons.
- `exterior/ship_freighter.png`, `ship_courier.png`, and `ship_tanker.png`:
  detailed grey hulls derived from the original atlas with corrected cutouts,
  directional shade, recessed machinery, and wear at their native sprite size.

The cockpit is reduced to a 268 × 120 logical canvas and enlarged with nearest
neighbor sampling for crisp 2 × 2 pixel clusters at the native 536 × 240 panel
size. Sprite cutouts are alpha-trimmed, quantized, and packed as RGB565. The
space field is indexed into 48 colors with 17 lighting palettes.
The exterior cutouts use 63 RGB565 colors plus transparent index zero.
The worlds have 96 × 88 pixel canvases. All five ships retain detail at their
native sprite size instead of reducing the larger hulls to half-resolution.
They are enlarged with nearest-neighbor
sampling. The deterministic Python generator supplies the initial PNG masters;
those PNGs can also be edited directly and compiled without regenerating them.
The compiler trims transparent margins in the embedded cutouts to save flash
while keeping the editable canvases intact.

Run asset preparation from `firmwares/wayfarer-pin`:

```sh
python3 tools/prepare_background.py assets/source/masters/cockpit-generated-v2.png main/assets/cockpit.rgb565 preview-cockpit.png
python3 tools/compile_assets.py
python3 tools/make_exterior_assets.py
# Compile edited exterior masters instead of regenerating them:
python3 tools/make_exterior_assets.py --compile-only
```

The scene renderer adds animated exhaust, running lights, instruments,
breathing fixtures with bloom, screen spill, moving exterior reflections, and
soft cast shadows over these assets. Lighting motion interpolates the small
sine table, and shades the logical 2 × 2 tiles consistently across DMA strips.
Outside, stars use multiple depths and color temperatures, with acceleration
stretching the nearby stars into jump trails. Slow sector-color changes and
nebula encounters tint the sky and the cabin reflections together. Stations,
convoys, comets, eclipses, and asteroid flybys have distinct motion and lights.
Comets have curved, fading dust streams, individual debris particles, and a
textured nucleus. The eclipse has a cached 128 × 128 procedural pixel sun,
limb-darkened granulation, uneven pale magnetic streamers, and a small flare
loop. A detailed cratered moon crosses diagonally with faint earthshine;
the corona remains visible at totality, and cabin sunlight dims as it is blocked.
The sky and cabin ambient light are dimmer, preserving pools of
warm cabin light and isolated exterior highlights.

Objects retain their three-dimensional positions across encounter boundaries.
A shared forward camera movement projects distance into size and radial motion,
with nearer bodies occluding farther ones. Ships and their engine trails rotate
with their trajectories using nearest-neighbor pixel sampling. Planets approach
from varied bearings; jumps accelerate past the same existing traffic and
landmarks. A fresh destination appears after the departure has cleared the view.
