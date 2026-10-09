# DX12 half-resolution cloud volume — 2026-10-09

This checkpoint implements priority 5's measured post-processing optimization. The preceding [pass profile](../dx12-recording-20261009/README.md) identified volumetric clouds as the largest post cost: about 7.12–7.28 ms, compared with roughly 0.37–0.58 ms incremental SSAO, 0.91 ms reflections and 0.52 ms GPU HUD upload/draw. The CPU HUD remains substantially more expensive than its GPU work.

## Implementation

Only volumetric cloud scattering/transmission renders at half width and half height, in an RGBA16F texture with an R32F termination-distance guide. Atmosphere, cirrus, stars, moon and sun remain at scene resolution. The scene/display resolutions and existing cloud march step count do not change. There is no cloud history reuse or new temporal lag.

Reconstruction accepts only samples from the same sky/surface class and, for surfaces, a compatible termination distance. It renormalizes accepted bilinear weights and falls back to the original full-resolution march when none match. Selecting the farthest depth in each footprint preserves thin sky openings. Below the cloud layer, foreground geometry skips the volume march. Odd dimensions round up; resource recreation and cleanup follow the existing post targets.

`--full-res-clouds` selects the reference path for comparisons. Shared sky math is factored into background and volume functions without changing the DX11 wrapper's composition. The OpenGL renderer, textures and target remain unchanged.

## Matched cloud-only performance

AMD Radeon 680M, Release x64, 1920×1080 native, seed 1, unchanged graphics fixture, serial recording, 120 measured frames/run. Three interleaved pairs compare the geometry checkpoint with this executable; no concurrent build, validation or capture runs occur during timing.

| Configuration | Mean frame times | Median mean | Median p95 |
| --- | --- | --- | --- |
| Geometry checkpoint, full-resolution clouds | 29.51, 28.07, 28.24 ms | 28.24 ms | 29.83 ms |
| Half-resolution volume | 28.12, 27.05, 26.13 ms | 27.05 ms | 29.65 ms |

Median GPU post/HUD falls **23.5%**, from **11.24 to 8.60 ms**. Median mean frame time falls **4.2%**; p95 is essentially unchanged in these short runs. The new volume pass measures 4.27–4.40 ms, with full-resolution composition/SSAO at 1.40–1.51 ms. Geometry remains 4,377,081 submitted triangles/frame; the extra cloud pass increases draws from 1,033 to 1,034. Individual CPU/GPU intervals overlap and must not be added to predict throughput.

The measured executable is SHA-256 `D54B28051B652AA88D718670BA83FD6BD275558502DD535E60BE2E55B3DB8E38`. The geometry baseline is preserved in ignored build output as `MiniCity3D-before-clouds.exe`, SHA-256 `A3453D0E1E0A03125BFBDC66BC476FE9934259F488CEAE33D867D6A012A9AF68`.

## Verification

Eight rebuilt affected suites pass in `final-tests.txt`: native DX12/FSR2, sky, assets, scene jobs, equipment, drivers, vehicle collision and pedestrians. The native suite includes:

- 21 production cloud volume cases across 1×1, odd and even dimensions; clear/overcast weather, drift/night, cameras inside/above/below the layer, and geometry termination. Split volume error is at most 0.0004882 against complete sky composition.
- Synthetic contrasting sky/surface colors and incompatible termination distances to verify reconstruction rejection and the full-resolution fallback.
- All procedural skin modes, aiming, ragdolls and history; cached-state pixel comparisons; 12 exact serial/worker color/depth frame pairs; 32,000 known shadow receiver/caster pairs; asynchronous lifetimes and all four FSR2 modes at two sizes. Native validation reports zero errors.

`final-validation/` contains native and FSR2 Quality full-game captures, both visually reviewed. Both pass GPU skin readback at frames 1, 30 and 60 and report zero DX12 errors. These debug/readback runs are excluded from timing claims.

Five paired full/half-resolution sky captures were visually reviewed. All ten application runs report zero DX12 errors. Differences are small but nonzero, as expected for reduced cloud sampling; these checks do not establish equivalence for every possible camera/weather state. Existing inside-layer horizon banding is visible in both reference and candidate.

| View | RGB MAE (out of 255) | Pixels differing by >2/255 | Maximum channel difference |
| --- | --- | --- | --- |
| Overcast, below layer with foliage silhouettes | 0.026186 | 0.0047% | 28 |
| Inside layer | 0.067263 | 0.1410% | 19 |
| Above layer | 0.069736 | 0.1915% | 14 |
| Night, zenith/stars | 0.002191 | 0% | 1 |
| Wind and 180-second cloud drift | 0.026486 | 0.0067% | 15 |

Raw pixel metrics are retained in `capture-comparisons.txt`. The city comparison against the preceding geometry checkpoint is also included. Native FSR2 tests exercise render-target resizing and odd internal sizes; game captures cover native and Quality modes. This is a bounded automated/capture validation, not a claim of exhaustive interactive flight testing.

Two additional affected suites, grass and builder recovery, pass in `additional-tests.txt`, bringing this final regression selection to ten suites. `final-workers/` verifies the integrated cloud/geometry path with four native recorders: 118,192 worker draws, all three GPU pose checks and zero DX12 errors. Worker timing from this debug run is not a performance result.

## Final end-to-end result and delivery

After the final quality and regression checks, three fresh interleaved native-resolution pairs compare the original executable from the start of this goal with the complete default implementation. The hardware, settings fixture, 120-frame sampling and isolation match the earlier timing protocol. Raw logs are in `end-to-end/`.

| Configuration | Mean frame times | Median mean | Median p95 |
| --- | --- | --- | --- |
| Original executable | 35.28, 36.97, 35.78 ms | 35.78 ms | 45.01 ms |
| Final default executable | 26.44, 27.68, 26.29 ms | 26.44 ms | 29.08 ms |

The combined changes reduce median mean frame time **26.1%** (approximately **27.9 → 37.8 FPS**) and median p95 **35.4%** on this fixture. This is a fresh comparison, not a multiplication of milestone percentages.

| Measured interval/count | Original median | Final median |
| --- | --- | --- |
| CPU scene preparation | 12.784 ms | 1.747 ms |
| CPU vertex deformation (part of scene preparation) | 8.169 ms | 0 ms |
| CPU upload/culling | 6.629 ms | 1.866 ms |
| CPU draw/HUD | 12.806 ms | 14.305 ms |
| GPU shadows | 6.77 ms | 5.27 ms |
| GPU scene | 12.57 ms | 10.77 ms |
| GPU post/HUD | 11.30 ms | 8.27 ms |
| Submitted triangles/frame | 4,952,015 | 4,377,081 |
| Draws/frame | 1,243 | 1,034 |

The final separately measured GPU deformation is 0.955–1.019 ms, outside shadow/scene/post intervals. The original executable lacks that separate timer, so its complete GPU total is not directly comparable by summing these columns. CPU draw/HUD does not improve in this final comparison despite reduced redundant state operations; HUD construction remains a meaningful remaining cost. The integrated GPU shares resources with CPU work, and these short local runs are not a guarantee for other hardware or scenes.

The root `MiniCity3D.exe` is updated to the tested/measured binary, matching the runtime SHA-256 `D54B28051B652AA88D718670BA83FD6BD275558502DD535E60BE2E55B3DB8E38`. `delivered-build.txt` records both executable hashes, the original baseline hash, and the unchanged root save/settings hashes. All five priorities below are implemented and verified within their stated scope; this optimization goal is complete. Broader roadmap work remains open.

## Five-priority implementation audit

| Requested priority | Integrated implementation and verification |
| --- | --- |
| Remaining character deformation on GPU | `dx12_skin_shader.h`, extended asset deformation metadata, scene emission and renderer compute dispatch cover procedural/aimed/seated/ragdoll vertices and prior poses. Independent CPU scene comparisons and GPU readback pass; measured CPU vertex deformation is zero in the benchmark. CPU joint/socket preparation remains. [Milestone 1](../dx12-optimization-20261009/README.md) |
| Reduce repeated draw-state setup | Compact stable pipeline keys and applied-state tracking skip repeated native binds. Constant remapping, recycled RTV identity, frame recycling and FSR2 invalidation have pixel regressions. The retained binding profile records 33 graphics pipeline lookups and one heap bind for 1,239 draws. [Milestone 1](../dx12-optimization-20261009/README.md) |
| Parallel command recording | Shadow/opaque batches use separate native worker lists, allocators and applied-state caches, ordered with primary segments and protected by frame fences. Twelve serial/worker frame pairs match exactly. Opt-in `--record-workers=4`; repeated measurements do not justify enabling it by default. Descriptor/material preparation and HUD remain serial. [Recording milestone](../dx12-recording-20261009/README.md) |
| Reduce shadows and geometry | Receiver-aware caster sweeps, cascade bounds, shadow-footprint LOD and regrouped instances reduce fixture submission by 574,934 triangles/frame. Per-frame mesh/bounds caching removes repeated upload/culling work. Math, asset and image comparisons pass; off-screen shadow casters are retained. Dense forests remain expensive. [Geometry milestone](../dx12-geometry-20261009/README.md) |
| Profile and optimize expensive post passes | Separate timers and cloud/SSAO ablations identify the leading cost. Half-resolution cloud volume with guided reconstruction lowers median post/HUD GPU cost by 2.64 ms in matched runs; the preceding quality and GPU tests cover the implementation. SSAO, reflections and HUD retain their current quality. |

These are the bounded implementations of the requested optimization pass. They do not imply that the wider `idea.md` roadmap, all hardware tuning, CPU HUD optimization or exhaustive interactive validation are complete. Earlier evidence documents intentionally describe the status at their checkpoints; this audit supersedes their then-open items for these five priorities.

The initial `sky-full/` run passed native DX12 validation but the general harness rejected its absent skin-readback messages: the sky preview has no visible character actors. The explicit `-SkyOnly` harness option is restricted to `--sky-preview`; ordinary fixture runs continue to require all three pose checks. The application did not fail that run.

## Reproduce

```powershell
./tools/verify_dx12_optimization.ps1 -Evidence evidence/local-cloud-validation
./tools/verify_dx12_optimization.ps1 -Evidence evidence/local-cloud-benchmark -Benchmark -Baseline MiniCity3D-before-clouds.exe -Label clouds
./tools/verify_dx12_optimization.ps1 -Evidence evidence/local-cloud-full -NativeOnly -SkyOnly -ExtraArgs '--sky-preview','--overcast','--full-res-clouds'
./tools/verify_dx12_optimization.ps1 -Evidence evidence/local-cloud-half -NativeOnly -SkyOnly -ExtraArgs '--sky-preview','--overcast'
./tools/compare_render_captures.ps1 -Reference evidence/local-cloud-full/native-validation.png -Candidate evidence/local-cloud-half/native-validation.png
```

Add `--sky-inside`, `--sky-above`, `--night --sky-zenith`, or `--sky-drift --windy` to both sky comparison runs. Use fresh evidence directories. The harness preserves runtime saves/settings and isolates the latest application session from the append-only log.
