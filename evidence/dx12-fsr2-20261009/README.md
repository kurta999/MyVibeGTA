# Native DX12 and AMD FSR2 verification — 2026-10-09

The main `MiniCity3D` executable uses a native D3D12 device, direct queue, command lists, descriptor heaps, graphics/compute pipeline state objects, explicit resource barriers, timestamp queries and fence synchronization. `graphics-imports.txt` confirms D3D12/DXGI/D3DCompiler imports and no D3D11 import. There is no D3D11On12 integration. Historical `dx11_` CPU scene, asset and shader modules remain shared; the OpenGL target and its renderer/textures sources were not edited.

AMD FSR2 is the official MIT-licensed 2.2.1 source at commit `1680d1edd5c034f88ebbbb793d8b88f8842cf804`. CMake restores and statically builds its DX12 backend and shader permutations. Graphics settings persist `FSR2=0..4` (Off, Quality, Balanced, Performance, Ultra Performance) and `FSR2Sharpness=0..100`. HUD/menu composition occurs at display resolution after reconstruction. The menu uses 17 rows in a 704-pixel panel.

## Automated checks

- `regression-tests.txt`: all eight selected CTest entries passed in 57.66 seconds: `dx12_fsr2_smoke`, `simulation_smoke`, `scene_jobs_scenarios`, `grass_scenarios`, `terrain_scenarios`, `asset_smoke`, `texture_mips_smoke`, and `texture_loading_smoke`. The simulation suite checks FSR2/sharpness save/load alongside existing graphics settings.
- `fsr2-final-test.txt`: final GPU test passed after the mask changes. It uses actual AMD compute dispatches in every quality mode at 640×360 and 800×450, verifies render dimensions, jitter bounds, reset behavior, resource recreation and swapchain resizing, then reads GPU output and checks reconstruction of a known RGB signal. It reports zero D3D12 validation errors.
- A game run with `--smoke --benchmark --benchmark-short --dx12-debug --fsr2-cycle --capture-converged` switched all five modes live and reported zero D3D12 validation errors.
- `quality-final.txt`: all 42 checked compressed DDS textures passed GPU mip readback. `temporal-motion.txt`: a 60-frame camera-motion run passed current/previous skeletal GPU-vs-CPU comparisons at frames 1, 30 and 60 (maximum error 0.00006104) with zero D3D12 validation errors.

## Captures and game checks

`native-day.png`, `quality-day.png`, `performance-night.png`, and `settings.png` were visually inspected. Matching `.txt` files contain run logs and zero D3D12 validation errors. Scene geometry, lighting, shadows, textured models, animated pedestrians and display-resolution HUD/menu are present. `quality-final.png` and `quality-final.txt` are the final Quality capture and additional GPU texture/skinning validation run after reactive/composition-mask refinement.

Commands use `build-msvc-ninja/MiniCity3D.exe` with its own runtime settings/save paths. Root user settings and save were preserved.

```powershell
./build-msvc-ninja/MiniCity3D.exe --smoke --benchmark --benchmark-short --fsr2=0 --capture-converged --dx12-debug
./build-msvc-ninja/MiniCity3D.exe --smoke --benchmark --benchmark-short --fsr2=1 --capture-converged --dx12-debug
./build-msvc-ninja/MiniCity3D.exe --smoke --benchmark --benchmark-short --fsr2=3 --capture-converged --dx12-debug --night
./build-msvc-ninja/MiniCity3D.exe --smoke --fsr2=1 --graphics-menu --screenshot --dx12-debug
./build-msvc-ninja/MiniCity3D.exe --smoke --benchmark --benchmark-short --fsr2=1 --capture-converged --dx12-debug --validate-loading --validate-gpu-skinning
```

## Scope and remaining limitations

FSR2 receives post-tonemap color, depth, current-to-previous motion converted to pixel units, AMD Halton jitter, reactive/composition masks, frame time and camera parameters. It replaces the previous FXAA/TAA resolve while enabled. Opaque camera motion and GPU skeletal motion have temporal vectors. Other moving rigid/procedural surfaces are conservatively rejected from history, and the pre/post-transparency color difference contributes to the mask. Per-object rigid motion vectors and broader temporal-artifact tuning remain improvements.

The backend currently waits at submission each frame. CPU/GPU frame overlap, PSO prewarming and wider performance tuning remain open. The short debug-layer runs are functional evidence, not representative performance benchmarks. The debug layer reports a missing optimized color clear value and intentionally discarded extra MRT outputs during transparent color-only draws; no validation errors were reported. GPU-based validation and full gameplay coverage on other GPU vendors were not performed.
