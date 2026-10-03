# App ideas

[English](APP_IDEAS.md) · [Español](APP_IDEAS.es.md)

The current collection contains Conway, Fluid, Miso, Lumen, Dungeon, Maze,
Wayfarer, Three Body and CRT. New apps should add a different visual or interaction,
rather than duplicate an existing scene.

Possible additions:

- **Falling sand:** touch places materials; tilt moves granular particles through
  a small cellular grid. Start with sand, water and walls.
- **Pinball:** tilt nudges a ball around a procedurally arranged table. Touch
  operates flippers; unattended play keeps the scene active.
- **Circuit garden:** pulses travel through a bounded graph; taps reroute edges
  or switch gates. Draw the graph with clear signal states.
- **Transit map:** trains circulate on a generated rail network. Highlight
  arriving trains and transfer stations without relying on small text.
- **Magnetic field:** touch places poles; particles trace a bounded field. Clamp
  singularities and keep the integrator independent of the renderer.
- **Mechanical orrery:** gears and orbiting bodies form an animated clockwork
  scene. Use original geometry and a readable palette.

Design for the 536 × 240 display: a clear main subject, enough motion to read at
arm's length, no required network connection, and useful unattended behavior.
Reserve a 1.5-second BOOT hold for returning to the launcher. Keep buffers and
simulation state bounded; PSRAM is optional.

The existing apps offer portable renderers and examples of touch and IMU input.
The SD file service is available only in the launcher, so an app that loads
assets or saves state needs its own revision-validated SD integration. Adding a
folder alone does not register an app: follow the steps in
[CONTRIBUTING.md](../CONTRIBUTING.md).
