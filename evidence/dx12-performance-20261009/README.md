# Native DX12 performance follow-up — 2026-10-09

The native renderer now overlaps CPU preparation and GPU execution. Present submits without draining the entire queue; two recording slots each own a command allocator, shader-visible descriptor heaps, and retained resources/uploads. A slot is recycled only after its fence completes. Readback, resizing, FSR2 reconfiguration and shutdown still drain the queue.

Upload pages remain mapped and are reused only when neither in-flight commands nor live dynamic buffers reference them. Idle reserve memory is bounded to 128 MiB. Texture descriptor tables are cached per recording, null texture descriptors are prebuilt, and resource retention is deduplicated. Scene vertices copy directly from material groups into disjoint upload ranges on the existing CPU worker pool; this removes an intermediate whole-scene copy. The HUD also avoids an intermediate texture copy.

## Matched measurements

AMD Radeon 680M, Release x64, 1920×1080 output, fixed seed 1, 120 measured frames after the benchmark's initial frame. All runs use the same saved `settings.ini` fixture in this directory: High graphics/shadows/vegetation/effects, draw distance 56, LOD 63, grass distance/LOD 94. VSync is disabled by the benchmark. Debug validation, captures, compilation and other game instances were excluded from timed runs. These are short local benchmark results, not a claim about every scene or GPU.

| Renderer | Mean frame times from three runs (ms) | Median mean (ms) | FPS from median |
| --- | --- | --- | --- |
| Original DX12, FSR2 Off | 62.67, 63.24, 64.57 | 63.24 | 15.8 |
| Optimized DX12, FSR2 Off | 37.46, 39.00, 35.94 | 37.46 | 26.7 |
| Retained DX11, native | 37.86, 36.56, 38.21 | 37.86 | 26.4 |
| Original DX12, FSR2 Quality | 80.01, 70.72, 67.51 | 70.72 | 14.1 |
| Optimized DX12, FSR2 Quality | 38.15, 38.79, 36.87 | 38.15 | 26.2 |

Native DX12's median frame time fell 40.8% (68.8% higher calculated FPS). Median CPU Present time fell from 28.000 to 1.077 ms. Native DX12 and DX11 are effectively level within local run variability. All native runs submit the same 1,243 draws and 4,952,015 triangles per frame; FSR2 uses 1,245 draws. No draw-distance or quality reduction was used. FSR2 Quality does not improve throughput in this particular workload after the fix; CPU preparation, shadows and other work outside the reduced-resolution scene remain substantial.

The final DX12/DX11/Quality runs were interleaved. Original executables were measured before and after that sequence. Raw logs retain every run, including the slower initial FSR2 result. `after-*` logs describe the intermediate frame-overlap version before upload recycling/copy improvements; `final-*` logs describe the delivered executable.

## Validation and scope

- Three CTest suites pass: `dx12_fsr2_smoke`, `scene_jobs_scenarios`, `grass_scenarios` (`regression-tests.txt`).
- GPU tests verify twelve asynchronous transient resource submissions, preservation of a long-lived constant buffer across upload recycling, all four active FSR2 modes at two output sizes, temporal resets, readback and resize.
- Game validation cycles all FSR2 modes, verifies 42 compressed texture mip chains, and captures the rendered frame with zero DX12 validation errors (`game-validation.txt`).
- Additional captures/checks cover sustained FSR2 Quality with GPU skinning validation and native rendering with one CPU worker (`quality-validation.*`, `serial-validation.*`). The existing optimized-clear-value performance warning remains; it is not a validation error.
- Root save and settings are preserved. The OpenGL renderer and target are unchanged.

CPU scene preparation and uploads use workers; native command-list recording is still on the main thread. Parallel command-list recording would require per-worker recording state and coordinated barriers. It is not claimed as implemented by this optimization. GPU frame overlap is also distinct from FSR frame generation.

## Reproduce

Configure `-DMINI_CITY_BUILD_DX11_BENCHMARK=ON` and build `MiniCity3D`, `MiniCity3D-DX11-benchmark` and `dx12_fsr2_smoke`. Use the fixture settings in an isolated runtime directory with the game's assets/data. Run:

```powershell
./MiniCity3D.exe --smoke --benchmark --fsr2=0
./MiniCity3D-DX11-benchmark.exe --smoke --benchmark
./MiniCity3D.exe --smoke --benchmark --fsr2=1
```

`--dx12-sync` restores synchronous Present for diagnosis. `--scene-workers=1` selects serial CPU preparation. Neither is the default. The original baseline executable is retained only in the ignored build directory as `MiniCity3D-before-performance.exe`; its SHA-256 is `EC9685E2542CBE855B418F827769FC19933D65B5B0980E579DEB6D005D6C656D`. Delivered executable hashes are in `delivered-build.txt`.
