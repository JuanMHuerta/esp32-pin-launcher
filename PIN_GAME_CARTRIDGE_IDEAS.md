# Backpack Pin — Game Cartridge Collection

## Direction

Treat the backpack pin as a collection of fictional, self-contained game
cartridges. Each cartridge is an attractive autoplaying scene: it needs no
phone, network, account, or companion device, and it begins from a clean,
interesting state after power-on or reset.

The existing launcher is a natural cartridge selector. Conway, Fluid, Miso,
and Lumen can be framed as the first four cartridges; Miso already fills the
mascot/living-badge category, so future cartridges should explore other kinds
of nerdy, videogame-like visual worlds.

## Design rules for a backpack-facing display

- Optimize for recognition from roughly 1–3 m away: one large subject,
  strong silhouette, black/near-black background, and 2–4 bright colors.
- The loop should make sense in under three seconds. Fine detail and text are
  optional close-up rewards, never the main visual.
- Default to an autonomous attract mode; touch, tilt, shake, and BOOT can add
  one simple, discoverable interaction.
- Avoid dependencies on external connections. Every cartridge must be usable
  by plugging the pin into power and resetting it at any time.
- Give cartridges a shared identity: short title-card boot animation,
  consistent palette/brightness conventions, and a single physical input.

## Candidate cartridges

### Dungeon Crawler Attract Mode

A large pixel hero crosses procedural rooms, opens chests, fights clear enemy
silhouettes, and occasionally reaches a boss. It is a convincing endless game
demo rather than a full game. Tap could trigger a dash, spell, or new room.

### Roguelike Battle Screen

An oversized turn-based encounter: knight vs. slime, wizard vs. skeleton, or
mech vs. alien. Tap advances the action; after a battle ends, a new matchup
and arena begin.

### Bullet-Hell Wallpaper

A tiny ship threads waves of bright geometric projectiles. Backpack motion
nudges the ship; it auto-dodges enough to retain a composed visual. A tap
fires a dramatic screen-clearing bomb.

### Arcade Attract-Mode Cabinet

Rotate among brief fictional arcade screens: space shooter, falling-block
puzzle, side-scrolling platformer, racing start grid, and dungeon map. Each
screen remains up for 20–40 seconds before a cartridge-style transition.

### CRT Boot Sequence

An old terminal comes alive with scan lines, memory blocks, wireframe globe,
pseudo-code, loading bars, and rare alerts. The amber or phosphor-green style
will be legible at distance even where text is not.

### Hacker Radar / Tactical Display

A sweeping radar with blips, lock-on brackets, a star map, waveforms, and an
occasional unknown-signal event. It is deliberately fictional, with no real
world data required.

### Spaceship Cockpit Window

Use a large cockpit frame around hyperspace, asteroid fields, nebulae,
docking, and pursuit sequences. Tilt can look around, building on Lumen's
camera/motion work.

### Mech Hangar

One bold mech silhouette cycles through hangar life: warning lights,
hydraulics, power-up, scan, launch, damaged return, and repair. This is a
particularly strong nerdy badge from a distance.

### Boss Health-Bar Loop

A giant cyber dragon, reactor core, alien eye, or enemy ship fills the screen.
The encounter moves through visual phases, ends explosively, then loads a new
boss.

### Procedural Pinball Table

A high-contrast miniature pinball board. Tilt influences the ball; hits make
large bursts and flashes; a new ball starts automatically. It makes physical
backpack movement part of the scene.

### Starship Warp Map

A ship follows a clean 2D galaxy map, hopping between systems and encountering
anomalies. It has the strategic-overworld feeling of a space game while
remaining readable as animated art.

### Circuit Board / Logic Simulation

Glowing pulses move through a large procedurally generated circuit. A tap
injects a pulse, and every boot can generate a distinct board. Ideal for the
AMOLED's black background.

### Cellular Automata Gallery

Expand Conway into named visual cartridges/rules: Life, Wireworld, Langton's
Ant, cyclic automata, reaction-diffusion, and maze generation. Use larger
cells and intentional palettes so each rule is obvious at a glance.

### Falling-Sand Alchemy

An autonomous pixel sandbox of colored sand, water, lava, acid, plants, and
fire. Tilt shifts the world. Each boot chooses a fresh terrain recipe. This
is distinct from the existing fluid simulation and very watchable.

### Voxel-Style Mining Scene

A drill or mining robot digs a procedural side-view cave, finds ore, avoids
lava, returns to base, and regenerates the world. The reference point is a
tiny endless mining game rather than a literal clone.

### Procedural Subway Map

Bright transit lines spread across a fictional city while tiny trains run
between stations. Congestion and route additions give it a satisfying systems
game feel without real transit data.

### Synthwave Racer

A single bold car races toward a sunset along a neon grid. Scenery shifts,
rivals appear, and boost effects punctuate the loop. Gentle tilt can steer
the camera or car.

### Portal Experiment

A stylized test chamber with portals, cubes, lasers, and switches. Keep it
abstract/original while retaining the immediately recognizable puzzle-game
language.

### Rare-Item Discovery

Large rotating loot appears with reveal effects: enchanted sword, potion,
floppy disk, keycard, or alien relic. Rarity colors and particle effects turn
each reveal into a clear little event.

## Strong first candidates

1. **Mech Hangar** — best short-distance backpack badge and fits the theme.
2. **Dungeon Crawler Attract Mode** — offers an easy-to-watch game narrative.
3. **Falling-Sand Alchemy** — highly reactive to physical movement.
4. **Bullet-Hell Wallpaper** — lively and visually distinct.
5. **Cellular Automata Gallery** — efficiently extends existing Conway work.

## Possible cartridge naming style

- `MECH//BAY-07`
- `DUNGEON//SEED`
- `VOID//PILOT`
- `CIRCUIT//GHOST`
- `SAND//ALCHEMY`
- `LIFE//LAB`

Use only original worlds, names, mechanics, and art direction; videogame
genre language is the inspiration, not a license to duplicate a specific game.
