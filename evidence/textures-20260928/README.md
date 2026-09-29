# Original 2K materials and DX11 DDS milestone

The preceding original collection's 256px maps have been replaced with 54
2048×2048 source PNGs across 18 shared materials. These are newly authored
periodic surfaces, with coherent colour, height-derived normals, roughness and
restrained cavity occlusion. No downloaded texture images are used. Geometry
remains 14 moderate-detail original asset types and 28 cooked mesh/LOD variants.
This does not establish GTA VI asset quality or finish the showcase roadmap.

## Assets and runtime

- `materials.jpg`: actual colour/normal/roughness map crops.
- `modern-assets-overview.jpg` and `studio/`: actual editable GLBs rendered in
  pinned Blender 4.5.14 LTS, using the new embedded maps. All 14 were rendered;
  the review sheet and representative close surfaces were inspected.
- `game/`: eight 1080p development asset captures with logs and binary/image
  hashes, taken before the glass correction. Day/night street, sedan/coupe,
  Marina lamps, pistol and carbine were checked. The later glass regression
  capture verifies all 54 final compressed resources again.
- `quality.json`: decoded quality for every mip of every compressed map, plus
  linear-light averaging and opposing-normal coherence checks.
- `ctest.txt`: all 18 suites pass in 67.13 seconds. The simulation suites retain
  gameplay checks; this milestone does not replace extended interactive play.

The runtime uses BC7 sRGB colour, BC5 normal and BC7 linear ORM DDS with 12
mips through 1×1. Colour mips are filtered in linear light; normal mips average
directions and renormalize, storing coherence in ORM alpha. DX11 reconstructs
positive Z from BC5 X/Y and uses coherence to broaden roughness at distance.
ORM red is also used for occlusion, sharing its cached resource. Legacy
PNG/JPEG loading retains generated mips. DDS parsing rejects incomplete,
truncated, oversized, unsupported-format and unsupported-surface files.

The pinned Microsoft DirectXTex May 2026 compressor is installed by
`tools/bootstrap_texconv.ps1` and SHA256 checked against published release
digests. NumPy/Pillow versions and source hashes are in `materials.json`;
cooked hashes, mip counts, formats, bytes and compressor settings are in
`textures.json`. Offline compression uses DirectCompute when available, with
the tool's CPU fallback. Runtime does not require these authoring dependencies.

Every compressed mip was decoded and compared with its authored reference.
Worst per-mip RGB RMSE is 1.225/255, worst per-mip p99 normal angular error is
1.272 degrees, and maximum normal-coherence channel error is 3/255. The eight
development asset runs verify 54 GPU resources each: every block of every mip is copied
back and compared exactly with its DDS payload, for 432 resource checks.
`game/gpu-checks.json` records these 432 checks; the glass regression adds 54
exact resource checks with the corrected final maps, for 486 total.

The modern chains use 288.00 MiB of GPU payload instead of 1152.00 MiB of
equivalent RGBA8. Total loaded image textures currently use 1224.72 MiB,
excluding HDR probes, geometry, render targets and driver overhead. All mips
remain resident together; streaming and bounded incremental uploads are open.

The 36 district day/dusk/night HDR faces were recaptured with the new materials
and recooked into `assets/lighting/showcase.mcpb`. The probe manifest records
the final capture binary and face hashes. The game ZIP excludes duplicate
modern source PNGs and raw GLBs; the separate editable archive retains them.

## Glass highlight correction

Close-up review found glass highlight grain from sub-8-bit normal perturbations
quantizing to alternating 127/128 values. Glass normal fields are now flat.
`glass-fix-after.png` and `lighting-before-glass-fix/facade.png` preserve the
observed correction; `tests/glass_evidence.py` checks the isolated highlight
crop. High-frequency residual drops from 3.7434 to 0.0000 average 8-bit levels.
The glass map quality checks require a uniform optical normal field. Both
changed materials were recooked, their source previews rerendered, and the
HDR faces recaptured before final delivery.

## Final packages and local performance

`dist/MiniCity3D-modern-2k-20260928-final.zip` is the tested game delivery:
360,311,482 bytes and 819 entries. The editable asset ZIP is
`dist/MiniCity3D-original-modern-assets-20260928.zip`: 384,313,748 bytes and 196
entries, including 14 GLBs, source PNGs, cooked meshes/DDS and authoring tools.
`package-checks.json` records hashes, matching executable/probes/DDS payloads,
valid entry CRCs and excluded raw game sources. The earlier game ZIP without
the `-final` suffix is a draft from before the glass correction.

The copied final game passed the package workflow's 11 smoke runs, including
night/ragdoll, regions, traversal and the debug menu. `lighting/` contains 11
final copied-package captures and logs. Image checks verify a 102.542-level
facade sun contribution and 3.553-level car coat difference in their selected
regions. Three negative flare cases are exactly black; the horizon regression
region's maximum channel mean is 167.204. Final facade, car, night and rain
frames were inspected. These remain fixed-camera checks.

`benchmark-route.txt` records the final copied-package 1080p High-shadow and
High-TAA run: VSync off, 120 warm-up ticks, six 10-second simulation segments,
3,600 measured frames. Radeon 680M results are **19.04 ms average** (about 52.5
FPS), **43.94 ms p95**, 49.01 ms p99 and 58.59 ms maximum. GPU shadow/scene/
post-HUD averages are 5.58/7.50/3.77 ms over 3,600 samples; process RAM is 666
MiB. `benchmark.json` confirms no other game instances before or after this
run. The route has slow stretches, and these local numbers do not verify the
RTX 3060-class target. Benchmark execution is included in the log's first-frame
startup stage; that stage's timing is not ordinary game loading time.

## Reproduce

```powershell
python -m pip install numpy pillow
./tools/bootstrap_texconv.ps1
python tools/build_modern_assets.py
python tools/cook_modern_textures.py
python tests/modern_assets.py
python tests/modern_texture_quality.py
./build.ps1 -RunTests
./tools/verify_modern_assets.ps1 -OutputDirectory evidence/textures-20260928/game -ExtraArguments '--validate-loading'
./build-msvc-ninja/MiniCity3D.exe --smoke --bake-probes
python tools/cook_probes.py --captures build-msvc-ninja/probe-captures
./build.ps1
```

The asset/source checks verify geometry, source maps embedded in GLBs,
surface extensions and cooked textures by recorded hashes. The runtime checks
retain original legacy assets and materials. Source previews use
`MODERN_STUDIO_OUTPUT` with `tools/render_modern_assets.py`, then
`tools/compose_modern_assets.py --folder evidence/textures-20260928`.

Remaining work includes finer showcase geometry/UV details, a third building
variation, richer characters and animal art, texture streaming, selective 4K
surfaces, exposure adaptation, fuller glTF materials/attachments and vehicle
and animal animation. Extended moving-scene and RTX 3060-class validation
remain open in `graphics-upgrade-plan.md`.
