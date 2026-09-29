# Physics review — 2026-09-28

Status: revised solver passes host validation and is flashed on the ESP32-S3
with diagnostics enabled. The initial checkpoint is commit `3328da6` in the
project repository. Device timing passed at rest and under an automated stress
sequence. The user confirmed the bulk wall-release revision was substantially
better, then reported a remaining single row at walls/ceiling during quick
rotations. The user also confirmed the interpolation correction in `2df7ab5`
was substantially better. The accepted motion checkpoint is `ea0b123`. The
user then reported a persistent V-shaped dark band despite the first idle
rendering correction. The color correction below is flashed for that check.

## Why water lingered at walls

Three effects reinforced each other in `main/fluid.c`:

1. Projection could create negative pressure beside a solid wall. The grid
   continued treating the wall-adjacent cell as full even when liquid should
   detach, cancelling outward acceleration until particle movement finally
   changed the cell classification.
2. Particle separation could push a centre outside the container. Collision
   handling then erased the normal velocity even when that velocity pointed
   back into the tank.
3. Tangential samples inside the solid border were zero. Gathering those
   samples introduced unwanted friction while water flowed down a side wall.

The previous horizontal centroid correction hid part of the first issue. It
could not handle a side column returning to upright with zero horizontal force,
or water falling away from the ceiling. In the checkpoint solver, a settled
ceiling pool moved only 0.141 cells in 200 ms under reversed gravity; its speed
fell from 0.091 to 0.048 cells/s instead of continuing to accelerate.

## Solver changes

Wall-adjacent pressure is now constrained to be nonnegative using projected
Gauss–Seidel updates. Accumulated pressure is clamped, so later iterations can
still undo an excessive earlier correction. Interior cells retain the ordinary
incompressibility solve. Positive divergence is allowed at a wall cell with
zero pressure: this is a coarse-grid approximation of a gap opening between the
water and the wall. The reported projection residual accounts for that
inequality instead of counting permitted separation as a solve error.

This follows the separating-boundary principle described by
[Batty, Bertails and Bridson, section 4](https://www.cs.ubc.ca/~rbridson/docs/batty-siggraph2007-variationalcoupling.pdf).
The implementation uses the existing cell pressure stencil; it does not
implement that paper's full variational solid coupling or a sub-cell interface.

Collision handling removes only velocity into the wall. Tangential ghost
samples copy the adjacent fluid velocity. The horizontal centroid servo and its
five tuning constants have been removed.

The step order is now advection, separation/collision, particle-to-grid transfer,
save the unforced grid, apply external force on grid velocities, pressure solve,
and PIC/FLIP gathering. Gravity and the pressure supporting a resting pool act
before its next advection. This avoids repeatedly moving a supported pool into
the floor before correcting it.

Gravity increased from 11.5 to 30 cells/s² with the existing 1.6 time scale;
an unconstrained fall across the short axis takes about 0.63 real seconds.
Per-step damping is 0.990, allowing faster motion to settle. These are display
animation scales, not a claim that the pin reproduces water at physical scale.

### Remaining thin wall layer

The bulk separation fix in `7e01f20` still gathered zero normal velocity from
the wall into departing particles. A particle centre 0.34 cells from a wall
sampled 66% of that fixed zero and only 34% of the interior velocity. This
reduced its acceleration and PIC velocity even after pressure allowed release.
The renderer's short persistence made the lagging particle row more visible.

After projection, zero-pressure wall cells with velocity pointing into the
tank now extend that interior velocity to the wall sample for particle
gathering. The saved previous grid uses the same extension, preserving the
FLIP velocity change. Pressure projection still uses impermeable faces;
supported cells and velocities pointing into the wall keep their normal
boundary constraint. Corner tangential ghosts are refreshed consistently.
This visits only the perimeter and adds no storage or tuning constants.

In a settled ceiling reversal, the closest particle row cleared in 133 ms
instead of 217 ms. The new rendered regression covers all four walls: edge
density is zero by 167 ms and the rendered edge is black by 200 ms. Five rapid
rotations in each direction also clear ceiling density by 200 ms and keep its
rendered row black from 250 ms onward. The test fails on `7e01f20`, which still
has 7.32 particle-equivalents of density in the first edge column at 167 ms.
Gravity, sensor gains, and visual persistence are unchanged in this revision.

## Idle black spots

The point-to-pixel bilinear splat was too sparse to reconstruct a continuous
liquid surface. Small gaps between particles became black logical pixels as
motion stopped and the visual trail faded. A one-minute upright host trace
reproduced up to 31 enclosed black pixels after settling, despite all 573
particles and total density being conserved.

The renderer now applies a separable `[1, 2, 1]/4` density filter before its
existing surface threshold and persistence. The intermediate uses the existing
scratch array, with no extra allocation. Each axis preserves summed density.
The wall's contact row/column is filtered only tangentially, with reflected
inboard samples, so smoothing cannot paint liquid back onto a vacated wall.
That isolation could leave one empty logical cell between a wall and a sparse
pool while force pushed into the wall. The renderer now restores the normal
reflected stencil only when acceleration exceeds 1.5 cells/s² toward that wall.
It returns to the isolated rule as soon as force turns away, preserving the
wall-release timing. The regression checks both continuous pushed contact and
its clearance after an opposing force.
The redundant second brightness cutoff was removed: the density threshold
already distinguishes air, and the extra cutoff reopened faint interior gaps.
Native 8 × 8 pixel shapes retain their sharp edges.

`test-render` shakes then settles the pool for one minute in eight poses:
upright, inverted, both sides, two diagonals, shallow pitch, and face-up.
The face-up case first settles under gravity before the zero-gravity hold.
Flood-filling exterior air in actual RGB565 output detects enclosed black
logical pixels. All 21,600 checked frames after settling have zero holes.
The test also checks density conservation at corners, genuinely separated
pools, clearing after liquid leaves, and immutable snapshot input. The original
renderer fails the same upright test. Undefined-behavior trap instrumentation
passed the seven gravity-bearing poses.

The unchanged wall tests still require empty edge density by 167 ms and black
edge pixels by 200 ms, plus ceiling release after rapid rotation. Walking
changes 19% of rendered occupancy on average, peaking at 33%. Short-slide tests
now require movement of the actual occupied shape's centroid by over one
logical pixel, as well as their existing particle-density displacement gates;
the old percentage-only gate counted interior hole flicker as visible movement.
Measured rendered shifts are 1.83 pixels horizontally upright, 2.00 vertically
upright, and 1.59 face-up. The solver and sensor response are unchanged.

### Residual V-shaped dark band

The user's photo exposed a gap in the previous validation: a connected channel
of very dark blue pixels passed a test that only counted enclosed, fully black
pixels. Long host holds reproduced broad density seams, including from cold
startup without the pre-shake used by the earlier idle tests. Most of the seam
still had visible particle coverage. Mapping marker density to opacity made
these sampling variations look like gaps in the liquid.

Density now determines coverage using the same 0.035 threshold and persistence.
Covered pixels use the established full-water palette, with velocity highlights
and the same three native pixel layers. Color no longer varies with the packing
density of simulation markers. This preserves the outline and departure timing.
The amount dimension of the color lookup and the unused speed scratch array
were removed, saving 11,016 bytes of renderer storage and per-pixel opacity work.

`test-render` now includes three five-minute holds: cold upright startup, cold
startup at a 0.3 horizontal/vertical force ratio, and upright rest after shaking.
At 10 Hz after the first 15 seconds, it measures actual RGB565 brightness for
lit cores surrounded by water. The previous firmware reaches only 15% of the
brightest water pixel's green channel in the upright case and fails the new
65% minimum. The correction passes at 79%, 69%, and 73% minima, respectively;
the remaining variation comes from velocity highlights. Existing eight-pose
hole, mass, immutability, air-gap, walking, slide, and wall-release checks pass.
The complete renderer suite also passes undefined-behavior trap instrumentation.

## Sensor interaction

Gravity estimation, the 15 ms linear filter, axis mapping, and the pitch guard
retain their tested behavior. The solver now releases water
under gravity alone, without depending on a sensor translation target.

During a later left-bias investigation, live telemetry found the gyro bias still
uncalibrated after 18 minutes. The stationary QMI8658 reported about 13.4
degrees/s on one gyro axis, while the filter estimated a persistent 0.93 m/s²
sideways and 3.25 m/s² vertical linear acceleration. Its leftward translation
target was about 4.4 cells/s. A reboot while stationary calibrated the gyro
within seconds; both linear estimates then approached zero and the translation
target became zero. A first accelerometer sample taken during motion could
leave the stored rest magnitude more than 0.75 m/s² from the real resting
value and permanently block stationary calibration. Initial calibration now
uses stability within the candidate still window and sets the rest magnitude
once that window passes. A host regression starts with a 13.5 m/s² shove and
checks that calibration and zero-motion estimates recover after settling.

The bounded translation and roll assists remain to make small walking motions
visible. They deliberately emphasize motion; the leaky velocity estimate is
not a measurement of long-term enclosure velocity. Face-up translation scale
increased from 10 to 35 cells/m so a short slide remains visible after the
boundary correction. Upright scale remains 18 cells/m, with the existing
vertical assist. Pitch tests still check against spurious upward launches.

The six-axis IMU cannot uniquely distinguish all combinations of slow tilt and
translation. Real backpack walking and the user's wall-release motions remain
required device checks. Synthetic traces verify direction, response, and bounds;
they cannot establish the feel on the worn pin.

## Performance

The 38 × 18 grid, 573 particles, two separation passes, and 22 pressure passes
are retained. Solver storage is 43,876 bytes, an increase of 2,752 bytes for
pressure and motion diagnostics. There are no allocations during updates.

Boundary enforcement now visits the rectangle's edges instead of scanning the
whole grid. Hash coordinates multiply by a precomputed reciprocal, eliminating
6,876 software floating-point divisions per step on the ESP32-S3. PIC/FLIP
normalization shares a reciprocal between its two sums, saving another 1,146
divisions per step. The ESP32 object code was inspected to check the software
divide call sites. Direct neighbor indexing also simplifies divergence.

Device captures measured the current firmware directly:

| Capture | Physics Hz | Display FPS | Maximum solver step | Resets |
| --- | ---: | ---: | ---: | ---: |
| Bulk-release revision, 45 seconds mostly stationary | 60.04 | 58.75 | 10.55 ms | 0 |
| Bulk-release revision, 35 seconds repeating directional impulses | 60.02 | 58.73 | 10.83 ms | 0 |
| Thin-row correction, 50 seconds mostly stationary | 60.02 | 58.74 | 10.69 ms | 0 |
| Idle density reconstruction, 60 seconds mostly stationary | 60.03 | 57.55 | 10.70 ms | 0 |
| Interior color correction, 60 seconds mostly stationary | 60.03 | 59.84 | 10.66 ms | 0 |

The stress build repeated twenty directional impulses over two seconds,
followed by four seconds of recovery. Internal free memory stayed at 135,548
bytes, the sensor task ran at about 125 Hz, all 573 particles remained present,
and every reported render mass ratio was 1.00000. Maximum solver time leaves
about 5.8 ms of the 16.67 ms physics period before snapshot publication and
scheduling overhead. The sensor-controlled build was restored after this test.

After the thin-row correction, a fresh sensor-controlled capture again kept all
573 particles, mass ratio 1.00000, and zero resets. Internal free memory stayed
at 134,780 bytes. That capture was mostly stationary; it establishes timing for
the revised gather but does not confirm the user's rapid-rotation experience.

The idle rendering correction's capture contains 59 complete performance
windows. Physics stayed at 60.03 Hz and the IMU at 125 Hz; all particles and
render mass remained conserved, with zero resets and 134,268 bytes of free
internal memory. Display throughput is about 1.2 FPS lower than the preceding
capture because of the extra reconstruction pass. Its visual improvement on
the physical display still needs user confirmation.

The subsequent interior color correction recovered display throughput to
59.84 FPS. Its 60 performance windows kept physics at 60.03 Hz, all 573 particles,
mass ratio 1.00000, and zero resets. Free internal memory stayed at 145,788 bytes.
The capture verifies device scheduling and memory after simplifying the color
lookup; the user's check of the photographed dark band remains pending.

Telemetry logs solver phase costs, maximum step time, memory, resets, sensor
values, fluid centroid, and mean fluid velocity. The autonomous stress sequence
establishes performance under load; the user's physical walking and wall-release
check is still required to establish the intended feel.

## Validation and remaining gates

The latest host runs measured 96–99% of the expected `g*dt` impulse on the first
step away from each wall. Displacement at 200 ms was 1.33–1.35 cells, and speed
continued increasing throughout that interval. After a side column returned
upright, the upper-half fluid fraction fell from about 0.51 to 0.27–0.29 at
500 ms and 0.066–0.073 at one second. The walking trace changed 22% of occupied
display pixels on average, peaking at 37%, relative to a stationary control.

- `test-boundaries`: settle against each of four walls, reverse gravity, require
  an immediate impulse of 65–115% of `g*dt`, continuing acceleration over 200 ms,
  and displacement consistent with the force. Also return both side columns
  upright with zero x force, check drainage at 0.5 and 1 second, then settling.
  Measure edge density and actual RGB565 edge pixels after departure and rapid
  rotation to catch a thin lagging layer that centroid checks miss.
- `test-walk`: 1.7 Hz gait, 3.4-degree sway and 0.08 g translation on each axis;
  checks actual rendered pixel occupancy as well as speed and rest recovery.
- Existing tilt, pitch, motion-filter, short-slide, hard-shake, rotation,
  conservation, invalid-state recovery, and renderer-band tests.
- Two 30-minute simulated soaks: mixed axis translation and repeated strong
  impulses, with mass and reset checks.
- Undefined-behavior instrumentation with trap mode passed boundary, shake, and
  core solver tests. AddressSanitizer could not link because this environment
  lacks its runtime library.
- ESP-IDF firmware build and image-size checks.

The settled-wall visual tests now start from a genuinely settled pool and use
acceleration-based bounds. The former early-displacement assertions required
the artificial instantaneous centroid kick; they were replaced alongside the
stronger four-wall impulse/acceleration tests.

Outstanding: confirm the idle rendering correction and visible backpack walking
on the physical pin. Production release remains pending those checks.
