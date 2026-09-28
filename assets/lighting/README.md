# Original district lighting

`showcase.mcpb` contains uncompressed linear HDR lighting derived from this
project's existing static scene at two positions near the starting area. It
contains two local day/dusk/night sets, three analytic game-sky fallback cubes,
SH diffuse coefficients, and a split-sum BRDF lookup. It contains no third-party
HDRI, raw model source, or newly restricted model.
The scene's existing model/material attributions remain in the packaged asset
license records; these captures do not replace those credits.

`showcase.json` records the capture hashes, executable/cooker hashes, settings,
dependency version, positions, times, filtering math and output hash. Runtime
uses the backward-compatible legacy ambient path if this file cannot load.

This is a single static radiance capture using the former ambient/direct model,
not a path-traced multi-bounce bake. Moving objects/lights are excluded. Local
influence spheres and time/weather blending are approximations; there are no
interior visibility volumes or box parallax correction. Recapture when the
static scene changes, especially after integrating new showcase assets.

Source workflow: `tools/PROBE_LIGHTING.md` in the repository.
