# DX12 optimization milestone 1 — 2026-10-09

This checkpoint implements priorities 1 and 2 of the CPU/GPU optimization goal. Parallel command recording, further shadow/geometry reduction, and separate cloud/reflection/SSAO/HUD profiling remain open. The goal is not complete.

## Changes

- Native DX12 compute now handles aiming, all existing procedural character motion, seated vehicle pitch/roll attachments, and six-body Jolt ragdoll deformation. Hand/socket sampling and joint-palette preparation remain on the CPU. Existing CPU deformation remains available as fallback and as an independent scene reference; the DX11 renderer keeps its previous opt-in boundary.
- Current and valid prior procedural poses share the compute path. Actors without a persistent history identity, including ragdolls and vehicle occupants, retain conservative temporal rejection. They reuse the current output for invalid history instead of deforming the same pose twice.
- Draw recording tracks applied heaps, roots, pipelines, constant-buffer GPU addresses, texture/sampler tables, render targets, viewport/scissor, topology, and vertex/index buffers. Compact stable object identities replace full graphics PSO descriptor construction on each draw. Repeated state skips native binds. Submission and external FSR2 dispatch invalidate the native cache; recycled RTV handles also invalidate it by view identity.
- A separate GPU timestamp measures deformation, which the existing shadow/scene/post timestamps exclude. Compute dispatch remains on the direct queue. No async-compute or worker command recording is claimed.

## Matched measurements

AMD Radeon 680M, Release x64, 1920×1080, seed 1, native resolution, 120 measured frames after the initial frame. The prior performance milestone's saved settings are used unchanged. Baseline and candidate runs are interleaved; no compilation, debug validation, screenshots, or concurrent game instances run during timing.

| Configuration | Mean frame times (ms) | Median mean | Median p95 |
| --- | --- | --- | --- |
| Previous delivered DX12 | 35.76, 37.45, 36.82 | 36.82 ms | 48.25 ms |
| GPU deformation + state cache | 30.99, 31.46, 31.11 | 31.11 ms | 36.23 ms |

The median mean falls **15.5%**, equivalent to **27.2 → 32.1 FPS**. The candidate retains **4,952,015 submitted triangles/frame** and reduces draws from 1,243 to 1,239 by combining the former CPU character group with the shared GPU-skinned buffer. There is no draw-distance, render-resolution, or quality reduction.

Median CPU scene preparation falls from **13.224 to 1.561 ms**. CPU deformation falls from **8.515 ms / 247,656 vertices** to **zero** in this fixture. Candidate GPU deformation averages **1.120–1.158 ms** and must be added to the separately reported shadow/scene/post timings. CPU upload/culling remains about 7.6 ms, draw/HUD about 13.7 ms, and GPU post/HUD about 11 ms. These are local short-run measurements; they do not establish a general hardware or gameplay speedup.

The separate instrumented `binding-profile-native-1.txt` run shows **33 graphics pipeline lookups, 34 pipeline binds including compute, and one heap bind for 1,239 draws** in its last frame. That frame spends 8.453 ms building the CPU HUD within 13.803 ms of total draw/HUD preparation. Parallel recording therefore cannot remove all of the reported draw/HUD CPU cost. The instrumented run is excluded from the timing table.

The interleaved candidate series was measured before the final recycled-RTV identity guard. Its executable hash is in `measured-build.txt`; the guard has its own pixel regression. The subsequent delivered-build confirmation (`final-check-native-1.txt`) measures **32.11 ms mean / 37.09 ms p95**, with **1.166 ms GPU deformation**. Root/runtime executable hashes and preserved save/settings hashes are in `delivered-build.txt`. Debug/validation runs are not performance measurements, especially because GPU readback includes CPU comparison gaps in the deformation interval.

## Verification

- Six affected suites pass: DX12/FSR2, scene jobs, grass, equipment, drivers, and builder recovery (`regression-tests.txt`). The final DX12 GPU suite is rechecked after the shader history optimization and RTV guard (`final-gpu-tests.txt`).
- GPU readback covers all nine procedural modes, aiming, nonuniform scales, mixed joint weights, ragdolls, a rigid attachment rotation, and both valid and invalid prior poses. Shader reflection verifies the CPU/constant-buffer layout. Current position/normal/UV/color and prior positions are compared with CPU output.
- Real scene comparisons verify the original CPU implementation against extended skin instances, including four character styles, seated actors, aiming, Jolt ragdolls and region changes. Serial/parallel CPU scene checks remain in place.
- Pixel readback verifies repeated state, shader changes, remapping a live constant buffer, recycling a CPU RTV descriptor within a recording, external state invalidation, and frame recycling. Existing asynchronous upload/lifetime and all-mode/two-size FSR2 tests still run.
- Native and FSR2 Quality game runs pass GPU pose comparisons at frames 1, 30 and 60 and report zero DX12 validation errors. Maximum reported pose error is 0.00024414 world units. The two captured views were visually reviewed. Existing optimized-clear and unused-render-target warnings are not validation errors.
- Root save/settings are preserved. The OpenGL sources and target are unchanged.

`initial-validation.log` retains the initial shader compile failure, before correction. During development, a dynamic shader weight-array issue and the omitted seated-actor attachment transform were caught and corrected by the new comparisons. An intermediate unordered vertex comparison exceeded its 60-second test deadline; the final test compares deterministic emission order and passes with the original deadline.

## Reproduce

Build `MiniCity3D`, `simulation_smoke`, and `dx12_fsr2_smoke` in the configured Release build directory. The previous executable is retained as `build-msvc-ninja/MiniCity3D-before-optimization.exe` (ignored build output), SHA-256 `195BADE7D082F2C829FCED82CB885ACD6CA1E46C8ABB2ADF480A2F07FD0A86B0`.

```powershell
./tools/verify_dx12_optimization.ps1
./tools/verify_dx12_optimization.ps1 -Benchmark
./tools/verify_dx12_optimization.ps1 -Benchmark -CandidateOnly -Repeats 1 -Label final-check
```

The script uses an isolated runtime, restores its save/settings in `finally`, and extracts only the latest application session from the append-only game log. `MINICITY_CPU_PROFILE=1` enables native binding counters; profile logs should not be included in headline timing comparisons.
