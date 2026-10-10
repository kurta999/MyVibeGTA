# Renderer architecture checkpoint — 2026-10-09

This is a verified intermediate step in the **unfinished codebase rewrite**.
`ARCHITECTURE.md` records the remaining ownership, application, simulation, and
naming work. OpenGL files and target are unchanged.

## Changes

- DX11 and DX12 sessions own their former renderer globals. Session classes are
  noncopyable, clean up on destruction, and are published by the application
  facade only after successful initialization.
- Backend implementation files are organized by responsibility. This is a first
  decomposition; the backend classes still need smaller resource owners and
  frame-pass components.
- Eight identical shader blocks, instance/scene/probe buffer layouts, loading
  helpers, and PNG encoding have shared implementations.
- One resource-owning GPU profiler serves both APIs through small timing adapters.
  Pending query slots remain protected; profiling skips saturated frames. Named
  stage markers replace direct renderer access to query internals.
- New classes/functions use UpperCase names and members use `m_`. Legacy
  application-facing entry points and the rest of the codebase still require the
  naming migration.
- Direct3D asset deployment has one shared CMake target, removing concurrent
  writes from the two executable targets. The initial parallel build reached
  linking but failed in those duplicate copy operations; the final build passes.

## Builds and automated tests

Both `MiniCity3D` (DX12) and `MiniCity3D-DX11-benchmark` build in Release x64 with
Visual Studio 18/MSVC. `build.txt` records the final build.

All **40 pre-change baseline CTest entries passed** in 523.11 seconds; this run
overlapped compilation and is not a performance measurement. See
`baseline-tests.txt`.

All **six selected post-change tests passed** in 6.81 seconds:
`gpu_profiler_scenarios`, `dx12_fsr2_smoke`, `sky_smoke`, `asset_smoke`,
`texture_loading_smoke`, and `shader_loading_smoke`. See `affected-tests.txt`.
Simulation sources were not changed in this checkpoint.

The new profiler test covers both four- and twelve-timestamp layouts, every
partial query-creation failure, ring saturation, unavailable/partial results,
invalid frequencies, query reuse, statistics, repeated reset, reinitialization,
and destructor cleanup. The native DX12 suite also exercises actual GPU skinning,
state caching, worker recording, cloud passes, visibility, and FSR2.

## Full-game checks

Original renderer source from the starting Git revision was separately compiled
with the same build flags and linked against the same current non-renderer object
files. This avoids relying on the older preexisting DX11 executable. Original and
candidate runs use the same assets, settings fixture, seeded simulation, camera,
and 1920×1080 output. They run in an isolated runtime under `build-codex/`.
Executable hashes and arguments are in `runs.json`.

All eight initial runs exit successfully. All four candidates pass GPU skin
readback at frames 1, 30, and 60. Native DX12, FSR2 Quality, and four-worker DX12
report **zero validation errors**. The worker candidate records 111,461 draws
across 1,452 command lists, with a peak of four workers. FSR2 Quality explicitly
reports 1280×720 input to 1920×1080 output. Logs are retained as `.txt` files.

Candidate views were visually inspected along with the DX12 reference. Decoded
RGB comparisons are in `pixel-comparisons.json`:

| Mode | Different pixels | Mean absolute channel difference (0–255) | Maximum channel difference |
| --- | ---: | ---: | ---: |
| DX11 native | 5 / 2,073,600 | 0.00001238 | 14 |
| DX12 native | 5 / 2,073,600 | 0.00001206 | 13 |
| DX12 FSR2 Quality | 0 | 0 | 0 |
| DX12 four workers | 0 | 0 | 0 |

The five native differences are confined to x=973, y=318–325 near distant vehicle
geometry. A second DX12 reference/candidate pair is **pixel-identical**. The second
reference also matches the first reference; the two candidate runs differ at
those same five pixels. See `repeat-comparisons.json` and `repeat/`. This establishes
run-to-run variation in the candidate, not its exact cause or universal pixel
equivalence. No performance improvement is claimed from these validation runs.

## Reproduction and limits

```powershell
cmake --build build-msvc-ninja --target MiniCity3D MiniCity3D-DX11-benchmark gpu_profiler_scenarios --parallel 6
ctest --test-dir build-msvc-ninja -R 'gpu_profiler_scenarios|dx12_fsr2_smoke|sky_smoke|asset_smoke|texture_loading_smoke|shader_loading_smoke' --output-on-failure
./tools/verify_rendering_refactor.ps1
# With separately rebuilt references named baseline-dx11.exe / baseline-dx12.exe:
./tools/verify_rendering_refactor.ps1 -BaselineDirectory build-codex/refactor
./tools/compare_refactor_captures.ps1
```

These checks cover this renderer checkpoint, not all gameplay, devices, or failure
paths. The full rewrite remains active. Root save/settings are not used by the
capture fixtures; the existing modified root save was preserved.
