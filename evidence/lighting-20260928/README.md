# DX11 sun, clearcoat and solar lens flare — 2026-09-28

This milestone changes lighting/material shading in `MiniCity3D`; it preserves
the OpenGL fallback. See `graphics-upgrade-plan.md` for the full unfinished goal.

## Render checks

Run `./tools/verify_lighting.ps1`, then `python tests/lighting_evidence.py`.
The verifier accepts `-Executable` for copied-package checks. `captures.json`
records the tested executable and image SHA-256 values, exact smoke arguments
and process exit codes. All frames are 1920×1080 on the local Radeon 680M.

| Capture | Check |
| --- | --- |
| `sun.png` | Directional-light-aligned HDR solar disk and restrained ghosts. |
| `flare-visible.png` | Flare-only output for a visible daytime sun. |
| `flare-disabled.png` | Flare output disabled by `--no-lens-flare`. |
| `flare-occluded.png` | Actual city buildings hide the sun and its flare. |
| `flare-night.png` | Night produces no solar flare. |
| `facade.png` / `facade-no-sun.png` | Fixed-view glass highlight, with/without direct sun. |
| `car-coat.png` / `car-no-coat.png` | Same original sedan, with/without clearcoat. |
| `night.png` | Streetlight GGX reflections and retained emissive/light separation. |
| `rain.png` | Rain, wet surfaces and High TAA. |

The image check excludes the HUD. It requires a bright visible flare and zero
flare contribution for the three negative cases. It compares the facade glint
region against direct sunlight disabled and car pixels against clearcoat
disabled. `effect-checks.json` records the actual measured differences. These
checks verify contributions in fixed frames, not animated temporal stability.

Final image checks pass for all 11 captures. The facade sun contribution adds
103.281 average 8-bit RGB levels in the selected glint region. Clearcoat changes
the selected car pixels by 3.574 average levels. All three negative flare
regions are exactly black. The horizon regression region's maximum channel
mean is 167.204, below the 190-level stripe guard. Representative final day,
night, rain, car and effect/control frames were visually inspected.

## Reproduce and scope

All 18 CTest suites pass (`ctest.txt`). Original geometry/GLB checks pass for
28 meshes and 14 editable sources, including exported surface extensions; the
runtime asset suite checks car paint clearcoat and glazing IOR factors while
also loading the existing legacy material records. The 36 district HDR faces
were recaptured and recooked with the corrected sun/material shader; the
lighting manifest identifies the tested capture binary and output hashes.

`dist/MiniCity3D-modern-lighting-20260928-final.zip` contains the copied DX11 game,
original cooked assets, runtime lighting and attribution. The package workflow
passed its 11 graphical smoke runs, including night/ragdoll, regions, traversal
and the debug menu. The 268,444,616-byte archive has 817 entries, no raw model
sources, valid entry CRCs, and executable/probe hashes matching the tested
workspace files. The final lighting captures use this copied package. Editable
original sources are delivered separately in the modern asset archive.
`package-checks.json` records the final archive size, hashes and checks.

## Local performance

`benchmark-route.txt` records the final copied-package run with
`--smoke --benchmark-route --1080p --day --high-shadows --high-taa`: 120 warm-up
ticks, six 10-second segments, 3,600 measured frames, fixed 60 Hz simulation and
VSync disabled. On the Radeon 680M it measured 19.36 ms average (about 51.7 FPS),
46.83 ms p95, 48.23 ms p99 and 50.01 ms maximum. GPU shadow/scene/post averages
were 5.59/7.79/3.97 ms across 3,600 nonblocking samples; RAM was 685 MiB.
The route is not uniform in scene cost, and its high tail times remain visible
in this report. RTX 3060-class performance and extended motion inspection have
not been established.

```powershell
python tools/build_modern_assets.py
python tests/modern_assets.py
./build.ps1 -RunTests
./build-msvc-ninja/MiniCity3D.exe --smoke --bake-probes
python tools/cook_probes.py --captures build-msvc-ninja/probe-captures
./build.ps1
./tools/verify_lighting.ps1
python tests/lighting_evidence.py
```

Factor-driven clearcoat uses geometric normals, a separate GGX lobe and probe
lighting. The existing SSR path handles the base lobe; a separate clearcoat SSR
lobe, clearcoat textures, refractive glass, cloth/skin material models, bounded
exposure adaptation, full filmic grading and extended moving-scene inspection
remain open. The sun flare is analytic and screen-depth occluded; it has no
temporally filtered occlusion or local-light sources. Original asset textures
still use the first procedural collection's 256px maps; the requested higher
resolution art and texture residency work is not complete.

The subsequent 2K material/DDS milestone is recorded in
`../textures-20260928/README.md`; its new source assets and current editable
archive replace the 256px authoring collection. These lighting captures and
the named historical game ZIP retain the earlier verified state.
