# Tracy profiling and runnable benchmarks

Tracy v0.13.1 is integrated into both Direct3D game targets. The OpenGL renderer, textures and target block are unchanged. The implementation adds CPU/AI/physics/forest/grass/loading zones, named workers, native GPU pass zones, frame marks, and C++ allocation events with eight-frame callstacks. DX12 closes scopes before closing native lists and resumes them on primary/worker successors; submitted queries collect through Tracy's fence-backed queue context.

## Run

```powershell
./build.ps1 -DX12 -Tracy -DX11Benchmark
./tools/benchmark_game.ps1 -Executable @(
    './build-msvc-ninja/MiniCity3D.exe',
    './build-msvc-ninja/MiniCity3D-DX11-benchmark.exe'
) -Frames 120 -Warmup 30
```

The delivered root executables are `MiniCity3D.exe` (DX11/ImGui) and `MiniCity3D-DX12.exe` (native DX12/FSR2), both with Tracy enabled. `delivery-manifest.json` records their hashes and confirms the existing root save/settings stayed unchanged. PDB symbols and the BSD notice are available beside them. Normal builds keep Tracy off unless `-Tracy` is supplied.

The runner stages executables and read-only asset/data junctions under ignored `build-benchmarks/` directories, uses fresh settings/save state, and retains logs, per-frame CSV, run metadata and a summary at the selected output destination. It bounds run duration, checks exit status/sample counts/finite timings, and rejects recorded graphics validation errors. City, forest, travel and route are supported; `-Repeats` alternates backend order and `-RecordWorkers` controls native DX12 recording. Benchmarking remains available without a profiler connection.

## Verification

- Both Release games build through `build.ps1 -DX12 -Tracy -DX11Benchmark`. The first configuration pins upstream commit `05cceee0df3b8d7c6fa87e9638af311dbabc63cb`.
- All **11 affected CTest entries pass in 23.23 seconds**: benchmark options, allocation contracts, resource cache, GPU profiler, native DX12/FSR2, WARP, save jobs, startup, scene jobs, texture loading and shader loading. Exact output is in `tests.txt`.
- Allocation tests cover zero-sized/sized/array allocations, 32/64/256-byte alignment, cross-thread release, throwing/nothrow failure and null deletion.
- The DX11 frame and DX12 backend also compile with `TRACY_ENABLE` absent. `tracy-disabled-compile.txt` records the successful object checks; these are compile checks, not full additional disabled-game builds.
- Official Windows v0.13.1 tools saved and reopened actual forest traces. `tools/verify_tracy_capture.ps1` checks CPU zones on multiple threads, positive native GPU pass timings, absence of unresolved GPU durations, and substantial retained allocation data. Allocation data is verified by two identical-compression rewrites, with only the memory section stripped from the second; the upstream CSV exporter does not expose allocation counts.

| Trace | CPU events | Instrumented threads | GPU events | GPU passes | Memory strip reduction |
| --- | ---: | ---: | ---: | ---: | ---: |
| DX11 forest, 60 frames | 1,779 | 17 | 277 | 4 | 14,487,161 bytes |
| DX12 forest, 60 frames, recording workers | 3,901 | 21 | 1,210 | 10 | 10,173,591 bytes |
| Delivered DX12 forest, 20 frames, recording workers | 2,631 | 21 | 384 | 10 | 7,478,184 bytes |

The two worker-enabled DX12 captures use `--dx12-debug --validate-loading` and report **zero DX12 validation errors**. Their worker command recording CPU zones and GPU segment timings are present. `captured/` retains the longer DX11/DX12 checks; `delivery/` retains the delivered DX12 check after the final destructor lifetime guard. Each contains `trace-verification.json`, exported CPU/GPU CSV, memory-check output and game/capture logs. Large `.tracy` binaries remain locally available but are Git-ignored. The official matching tools are in ignored `build-tools/tracy-0.13.1/`.

To reproduce a trace and its checks:

```powershell
./tools/benchmark_game.ps1 -Scenarios forest -Frames 60 -Warmup 10 `
    -RecordWorkers 4 -Validate `
    -CaptureExecutable './build-tools/tracy-0.13.1/tracy-capture.exe'
./tools/verify_tracy_capture.ps1 -Trace '<run directory>/capture.tracy' -RequireRecordingWorkers
```

## Logged local diagnostics

`final/summary.csv` records one sequential 120-frame run per scene/API, with 30 warmup ticks, seed 1, 1080p, High shadows/TAA, native resolution, four scene workers, one recording worker, and no connected profiler or debug validation. Adapter: AMD Radeon 680M. Exact binaries/settings are recorded per run; the delivered DX12 hash changes for the final teardown guard and has the separate validated capture above.

| Backend | City mean / p95 | Forest mean / p95 | Travel mean / p95 |
| --- | --- | --- | --- |
| DX11 | 26.55 / 27.93 ms | 49.42 / 51.88 ms | 25.16 / 47.68 ms |
| DX12 | 23.92 / 25.29 ms | 39.88 / 41.67 ms | 23.27 / 40.00 ms |

These measurements demonstrate useful diagnostics, not a claimed optimization. The APIs submit different shadow/scene geometry, and these short single runs do not establish general API superiority. Travel exposes transition spikes; dense forest remains expensive. Capture/debug runs include substantial tracing overhead and are correctness evidence, not comparable performance samples. GPU CSV values are the latest completed asynchronous timings, may repeat, and use -1 when unavailable. CPU duration ends before CSV/memory telemetry bookkeeping.

Memory hooks cover C++ `new`/`delete` and STL ownership; C allocation, driver allocations, GPU VRAM and OS context switches are outside this milestone. Collection is on demand. Connect before startup via `--tracy-wait=15` for startup allocations; connecting later records subsequent events rather than an existing heap snapshot. Jolt, animation and instancing/LOD roadmap completion claims are unchanged.
