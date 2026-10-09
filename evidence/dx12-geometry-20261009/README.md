# DX12 shadow/geometry optimization — 2026-10-09

This checkpoint implements priority 4 of the active optimization goal: receiver-aware sun-caster culling, shadow-map-footprint LOD selection, and regrouping compatible shadow instances. It also eliminates repeated per-frame mesh-cache work. The cloud optimization identified by the preceding post-pass profile remains open; the overall goal is not complete.

## Implementation

- `dx12::ShadowReceivers` sweeps each caster's bounding sphere away from the sun through the expanded camera frustum. A caster is rejected only when that sweep cannot reach visible receivers. It preserves off-screen and behind-camera casters that can cast into view. Near/middle cascade bounds use the shader's radial split distances plus their blend bands. The last cascade retains its uncapped radial range, matching the shader. Four shadow texels pad the receiver volume for filtering and boundary tolerance; the prior light-volume test still applies.
- Shadow LOD selection uses the object's diameter in shadow-map texels. Validated authored LOD chains and compatible existing coarse meshes supply lower-detail shadow geometry. A coarse replacement must preserve normalization bounds, cast shadows, remain opaque and contain no more geometry. Existing special shadow proxies take precedence. Camera meshes and LOD settings remain unchanged.
- Shadow instances regroup by selected shadow mesh and material, so different camera LOD batches can merge when they share a shadow representation. Transparent scene ordering is unchanged.
- Bounds are computed once per instance, and each unique mesh is checked/cached once per frame. The latter removes repeated texture/material-cache lookups for instances repeated across cascades. The cache resets each frame, preserving live mesh-revision checks.
- `--legacy-shadow-culling` and `--legacy-shadow-lod` independently restore the previous decisions for comparisons. `--no-frustum-cull` also bypasses receiver culling. Worker recording remains opt-in and works with the new batches.

## Matched performance

AMD Radeon 680M, Release x64, 1920×1080, seed 1, native resolution, unchanged saved graphics fixture, serial command recording, 120 measured frames per run. Three baseline/candidate pairs run interleaved with no compilation, validation, screenshots or concurrent game instances during timing.

| Configuration | Mean frame times | Median mean | Median p95 |
| --- | --- | --- | --- |
| Previous delivered checkpoint | 30.71, 32.32, 31.53 ms | 31.53 ms | 36.89 ms |
| Geometry/cache optimization | 28.41, 28.83, 28.36 ms | 28.41 ms | 30.27 ms |

Median mean frame time improves **9.9%** (31.7 → 35.2 FPS), and median p95 improves **17.9%**. CPU upload/culling falls from **8.046 to 2.028 ms**; GPU shadows fall from **5.93 to 5.03 ms**. Other CPU/GPU intervals vary with the shared CPU/GPU workload, so the individual savings should not be added to predict frame time.

Submitted triangles fall **11.6%**, from **4,952,015 to 4,377,081/frame**, and draws fall from **1,239 to 1,033**. Camera geometry is unchanged. The separate all-rendered-frame geometry counter reports roughly 3.74 → 3.17 million shadow triangles plus 1.21 million scene triangles; its denominator includes the initial frame, unlike the benchmark mean. Culling alone accounts for about 524,742 fewer triangles in the fixture, and shadow LOD for another 50,192. Around 375 of 3,138 sun-caster candidates per frame are rejected by the receiver test.

These are local short-run measurements, not a general performance guarantee. The low-sun forest validation still submits 25.62 million triangles after culling, versus 27.98 million before it. It remains a much heavier GPU workload; this checkpoint does not claim to solve dense-forest geometry cost.

## Verification

- `dx12_fsr2_smoke` and `asset_smoke` pass (`initial-tests.txt`). The new receiver test checks 32,000 known visible receiver/caster pairs, varied camera rotations/aspect ratios, low and overhead sun, behind-eye/off-screen casters, radius expansion and cascade radial bounds. Existing native state, skinning, lifetime, worker and FSR2 GPU scenarios still run.
- Asset checks verify lower/equal element counts, equal normalization bounds, opaque shadow routing, correct authored-chain selection and no replacement of an already-coarse source with finer geometry.
- Native and FSR2 Quality full-game runs pass skin readback at frames 1, 30 and 60 and report zero DX12 validation errors. A separate four-worker integration run also passes and records 118,192 draws on worker lists.
- Three scene conditions compare legacy, culling-only and fully optimized rendering: the city, the woods at sunrise, and a rotated street view with medium/single-cascade shadows. All nine runs report zero DX12 errors. These debug/validation runs are excluded from performance claims.
- Culling-only city and forest captures are **pixel-identical** to the legacy captures. The medium-shadow street comparison differs in 0.0001% of pixels by more than 2/255, despite rejecting zero additional casters there. This bounds the small cross-run raster/order variation.
- Full optimization differs by more than 2/255 in **0.0100%** of city pixels, **0%** of forest pixels and **0.0809%** of street pixels. Maximum RGB differences are 21, 0 and 36/255 respectively. The native city, low-sun forest and medium-shadow street images were visually reviewed; the FSR2 image was also reviewed. Raw comparisons are in `capture-comparisons.txt`.
- Root save/settings are preserved, and the OpenGL sources/target remain untouched.

The measured and delivered executable is SHA-256 `A3453D0E1E0A03125BFBDC66BC476FE9934259F488CEAE33D867D6A012A9AF68`. The previous checkpoint remains in ignored build output as `MiniCity3D-before-geometry.exe`, SHA-256 `FF19F06EB440195CA8F1C9EFE2D69FE0427AEDA5D2BAD65B1FA941D863EAC31D`. Delivery hashes are in `delivered-build.txt`.

## Reproduce

```powershell
./tools/verify_dx12_optimization.ps1 -Evidence evidence/local-geometry-validation
./tools/verify_dx12_optimization.ps1 -Evidence evidence/local-geometry-benchmark -Benchmark -Baseline MiniCity3D-before-geometry.exe -Label geometry
./tools/verify_dx12_optimization.ps1 -Evidence evidence/local-legacy -NativeOnly -ExtraArgs '--legacy-shadow-culling','--legacy-shadow-lod'
./tools/verify_dx12_optimization.ps1 -Evidence evidence/local-cull -NativeOnly -ExtraArgs '--legacy-shadow-lod'
./tools/compare_render_captures.ps1 -Reference evidence/local-legacy/native-validation.png -Candidate evidence/local-cull/native-validation.png
```

The scripts use the separate runtime, restore its save/settings, and retain only the latest session from the append-only application log. Use fresh evidence directories to preserve earlier runs.
