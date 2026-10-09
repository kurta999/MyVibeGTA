# Builder simulation performance follow-up

The reported builder slowdown had two remaining causes: repeated multi-floor navigation probes/searches, and collection visibility checks against distant weapon pickups.

## Implemented fix

- Cache exact static floor/capsule results across frames, invalidating them when Jolt static collision is created, removed, rebuilt, or reset. Separate route samples from moving footprint samples so footsteps cannot evict the useful route history. Histories are pruned at frame boundaries to 65,536 planning and 8,192 transient columns; incomplete physics-budget results are not retained.
- Reuse accepted clearance and eight-point support results within each navigation context. Nearby props/vehicles and pedestrian separation remain live checks. Distance rejection precedes other pedestrians' height queries.
- Check the shared planning budget before probing wander candidates. Casual wander targets follow the actor's current floor relative to terrain; incompatible roof/drop targets are rejected before route search. Failed/deferred wander requests retry after a staggered 0.25–0.495 seconds, and block/terrain edits bypass that delay. Combat and cover retain their multi-floor routing.
- Reject occupied-player and out-of-range pickups before builder column/visibility checks. Nearby height/obstruction tests and every pickup's respawn timer remain active.

Full route-search limits and sampling precision are retained. An experimental smaller search-work cutoff broke the pit detour regression and was removed before publication.

## Matched DX11 measurements

AMD Radeon 680M, 1920×1080, day, smoke seed 1, generated city retained through the real F5 transition. Each unprofiled sample measures 120 fixed-60-Hz updates with a stationary player and live actors. VSync is disabled. Startup/render-resource loading is outside the measured frames; navigation is cold at the start. Both executables use the exact same captured user graphics settings in `benchmark-settings.ini`; SHA-256 fingerprints are in the configuration manifests.

| Sample | Average frame | Simulation | Rendering | p95 | p99 | Draws | Active AI |
|---|---:|---:|---:|---:|---:|---:|---:|
| Before, normal | 37.25 ms | 2.44 ms | 34.82 ms | 46.40 ms | 48.51 ms | 1524 | 43 |
| Before, builder | 73.89 ms | 34.65 ms | 39.24 ms | 106.40 ms | 113.12 ms | 1590 | 43 |
| Published, normal | 33.98 ms | 2.22 ms | 31.76 ms | 43.28 ms | 45.28 ms | 1524 | 43 |
| Published, builder | 40.17 ms | 10.86 ms | 29.31 ms | 55.01 ms | 68.21 ms | 1590 | 44 |

Builder simulation time falls **68.7%**; average total frame time falls **45.6%**, from approximately **13.5 to 24.9 FPS**. Physics, included in simulation, measures 1.13 ms before and 1.12 ms after. Rendering/GPU timing varies between samples; the simulation reduction directly addresses the builder CPU bottleneck. The generated population and graphics settings were retained.

The published three-frame profile includes a cold 27.61 ms navigation frame, followed by 1.03 and 0.29 ms navigation frames (18,000 → 793 → 162 physics queries). It is a diagnostic, not the 120-frame average. Dense construction, repeated excavation, continuous travel and other hardware remain broader performance work; this short city sample does not establish those outcomes.

## Verification

- `published-ctest.log`: **9/9 final affected suites pass** (76.06 seconds): builder transitions, navigation, destinations, wildlife, interactions, police, ordinary pedestrians/navigation, and autosaves. Navigation checks cover pits, placed-block detours/removal, slopes, stacked tunnels, streaming/falling, and moving crowds. New pickup checks retain roof separation and close obstruction, allow nearby collection, and advance distant respawn timers.
- The unchanged-floor regression reuses nine columns across 60 frames, reducing a supported-location probe from 19 cold physics queries to one live dynamic-clearance query. Collision refresh and same-frame mining invalidate the samples.
- `ctest.log`: the first optimization's full 39-suite run passed 38 suites. Its old navigation fixture expected dispatch into a sealed tunnel without a legal concealed spawn. The fixture now supplies a reachable rear tunnel spawn while retaining its height assertions; `navigation-recheck.log` passes. The final source was rechecked with the nine affected suites above, not a second full 39-suite run.
- Intermediate builds/profiles and the rejected search-cutoff test are retained for diagnosis. `before-matched/` is the old executable reference; `published-matched/` contains the published measurements. Other benchmark folders are intermediate runs.
- `verification.json` records matching published root/runtime executable hashes and the preserved root/restored runtime save hashes. Benchmark settings were restored after verification. The old reference executable is retained locally as `build-msvc-ninja/MiniCity3D-before-navigation.exe`.

## Reproduction

Build the DX11 target with `build.ps1`. Run `tools/verify_builder_performance.ps1 -Evidence evidence/builder-performance-local` from the repository. For an old executable in the same runtime directory, pass `-Executable MiniCity3D-before-navigation.exe` and a separate evidence directory. Keep the runtime settings identical for comparisons. The verifier restores the runtime save and checks the root save hash.

This fix does not change rendering assets, graphics quality controls, or the OpenGL renderer/target.
