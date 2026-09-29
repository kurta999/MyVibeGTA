# Original modern asset slice — 2026-09-28

The collection contains 14 original asset types, 28 indexed cooked meshes,
54 PBR maps and 14 self-contained editable GLBs. Original source generation,
integration, regeneration instructions and current limits are described in
`assets/models/MODERN_ASSETS.md`. Sources use no third-party meshes or textures.

These game captures record the initial asset integration before the later
clearcoat, Fresnel glass, corrected sunlight and solar flare milestone. The
new material sources/studio renders and archive include that later pass;
current engine captures and validation are in `evidence/lighting-20260928/`.

## Visual evidence

`modern-assets-overview.jpg` is a labelled sheet of Blender 4.5.14 LTS studio
renders made from the actual editable GLBs. `studio/` contains the 14 separate
renders. These are studio previews; the following files are actual 1920×1080
Direct3D 11 `MiniCity3D` frames captured on the local Radeon 680M:

| File | Additional smoke arguments | Inspection |
| --- | --- | --- |
| `dx11-day.png` | `--day` | Starting parcels, car, road furniture and single-arm lamps. |
| `dx11-street.png` | `--day --modern-street-preview` | Closer view of facades, starter sedan, bicycle rack and bin. |
| `dx11-night.png` | `--night --modern-street-preview --high-shadows` | New lamp geometry, LED emission and local illumination. |
| `dx11-car.png` | `--day --driver-preview --modern-car-preview` | Original sedan, glazing, wheel detail and existing seated driver. |
| `dx11-coupe.png` | `--day --driver-preview --modern-coupe-preview` | Original coupe and its separate car glazing. |
| `dx11-marina.png` | `--night --marina` | Twin-arm promenade lamp and wood bench. |
| `dx11-pistol.png` | `--day --weapon-preview` | New pistol in the existing hand/muzzle attachment path. |
| `dx11-carbine.png` | `--day --weapon-preview --rifle-preview` | New carbine and optic in the same gameplay attachment path. |

All eight runs exit 0, and the images were inspected. `captures.json` records
complete arguments, executable SHA-256 and image hashes. `dx11-*.txt` are the
associated startup/render logs. Reproduce with `tools/verify_modern_assets.ps1`.

## Validation

- `ctest.txt` records the final run: all 18 game suites pass, including asset,
  simulation, movement/vehicle/pedestrian, scene job, save, audio and loading
  checks. Run `./build.ps1 -RunTests`.
- `python tests/modern_assets.py` validates all 28 indexed files, 68,116
  triangles, indices, finite values, normals, winding, geometry hashes and the
  structure of all 14 embedded GLBs. The DX11 asset suite also checks contiguous
  PBR ranges/maps, wrap sampling, complete LOD chains, glazing bounds and safe
  temporal routing. Authored meshes disable generic tessellation/displacement
  and use their supplied normal maps without the legacy surface overlay.
- `python tests/probe_cooking.py` passes five math checks. The 36 district HDR
  faces were recaptured and `assets/lighting/showcase.mcpb` was recooked for the
  changed static buildings/lamps. Its companion JSON records source hashes and
  capture settings. This initial capture binary matches the tested initial
  asset slice; the later lighting milestone refreshes the same probe asset.
- The separate asset ZIP contains 14 GLBs, 28 M3D2 files and 54 texture maps.
  Its entries and CRCs were checked. Package again with
  `tools/package_modern_assets.ps1`.

## Remaining review

The matching 120-frame `--smoke --benchmark --1080p --day
--modern-street-preview` samples on the Radeon 680M measured 37.75 ms average
before disabling unnecessary authored-mesh tessellation and the legacy surface
overlay, and 28.71 ms afterward (39.42 ms p95, 50.30 ms p99, 52.89 ms maximum).
GPU shadow/scene/post averages were 6.91/11.90/5.30 ms in the final sample.
`benchmark-before.txt` and `benchmark.txt` preserve both logs. These are short
local samples, with no claim of sustained 60 FPS or discrete-GPU performance.

These frames verify this runnable asset integration. They do not establish
extended temporal/LOD stability or performance on RTX 3060-class hardware.
Car wheels/doors are static; new street furniture is decorative without new
collision. Existing parcel/vehicle physics and weapon behaviour are retained.
Advanced glass/clearcoat, broader lighting and lens flare remain open.
