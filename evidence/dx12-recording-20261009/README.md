# DX12 worker recording and post-pass profiling — 2026-10-09

Priority 3 is implemented and correctness-tested, but remains **opt-in** because these measurements do not establish a throughput improvement over milestone 1. Priority 5 now has separate pass measurements; its rendering optimization and priority 4's shadow/geometry work remain open. The optimization goal is not complete.

## Worker implementation

`--record-workers=4` splits shadow and opaque instance batches across native direct command lists. Each job has its own command allocator and applied-state cache. The owner thread prepares immutable draw packets, pipeline references, descriptors, constant-buffer addresses and resource barriers. Transparent geometry, small batches, skin draws, post-processing and HUD remain on the owner thread.

Primary and worker segments execute in original pass order in one `ExecuteCommandLists` call. This preserves resource transitions without introducing buffer decay between segments. Two frame slots fence-protect every allocator, command list, upload allocation and descriptor heap. RTV/DSV wrappers stay leased until recording finishes, including when a caller releases/replaces a view before submission. Clears, copies, dispatch, timestamps, target changes and state clearing finish an open captured batch.

The default is serial (`--record-workers=1`). Available worker counts are 1–8. This is native command-emission parallelism; descriptor and material preparation are still serial. It does not address the roughly 8 ms CPU HUD build identified in milestone 1.

## Matched recording measurements

Radeon 680M, Release x64, 1920×1080, seed 1, native resolution, the unchanged performance fixture, 120 measured frames/run. Baseline/candidate runs alternate, with no concurrent build, debug validation or screenshots. All configurations submit 4,952,015 triangles/frame and 1,239 draws. Runs are short and the integrated GPU's timing varies.

| Comparison | Mean frame times | Median mean |
| --- | --- | --- |
| Milestone 1 executable | 30.20, 31.21, 33.59 ms | 31.21 ms |
| Worker executable, four workers | 33.27, 31.85, 31.14 ms | 31.85 ms |
| Same worker executable, serial mode | 32.25, 34.20, 31.59 ms | 32.25 ms |
| Same executable, four workers, interleaved with serial | 31.02, 31.09, 31.43 ms | 31.09 ms |

The second comparison favors workers, while the first does not. CPU draw/HUD medians in the same-executable comparison are 14.319 ms serial and 14.079 ms with workers. This supports keeping the path available for further testing, not claiming a general FPS gain or enabling it by default.

Each worker run records 143,171 draws in 1,936 lists across 484 batches, with four workers observed concurrently. Worker CPU totals are about 68–72 ms for the entire 121-rendered-frame run; total join waits are about 1.9–2.2 ms. Additional list/state setup and GPU cost matter alongside CPU recording time.

The recording benchmark executable is preserved in ignored build output as `MiniCity3D-recording-workers4.exe`, SHA-256 `F21F1DF900CF67BCA755FE3AD957A8AB3F73343B5B1CCA6D664A91328FBCB4CF`. Milestone 1 is `MiniCity3D-before-recording.exe`, SHA-256 `873FAA9BCD1B56DC3EE3DF7F2DAFBA809C9D0EC04867EDD80D514319A24DAE44`.

## GPU post profiling

Nonblocking timestamps now divide post-processing into bloom, reflections, composition, tone mapping, temporal/display, and HUD upload/draw. Composition contains cloud marching and SSAO, so two diagnostic switches measure their incremental contribution: `--profile-no-clouds` bypasses volumetric cloud marching while retaining atmosphere/cirrus/stars, and `--profile-no-ssao` bypasses SSAO. These switches intentionally change the image and are profiling tools, not delivered quality reductions. No render resolution or normal quality setting changed.

Two interleaved profiling rounds and their full logs are in `post-profile-1/` and `post-profile-2/`. The profiling executable has SHA-256 `B8595E65A51360EFB249ACF10DFACEB3088E845F98104CEB8AD972DCB009F9B8`; these runs explicitly used the then-default four-worker mode. Exact pass results are recorded in each log's `DX12 GPU post stages` line. Normal composition takes about 8.4–8.6 ms; bypassing cloud marching is the clear leading opportunity for a later reduced-resolution or temporal solution. This ablation is not a visual-quality-preserving optimization and must not be reported as one.

| GPU measurement | Round 1 | Round 2 |
| --- | --- | --- |
| Full composition | 8.441 ms | 8.586 ms |
| Composition without volumetric cloud marching | 1.318 ms | 1.303 ms |
| Composition without SSAO | 8.076 ms | 8.008 ms |
| Reflections, full settings | 0.910 ms | 0.926 ms |
| HUD upload/draw, full settings | 0.529 ms | 0.516 ms |

The incremental cloud-march cost is **7.12–7.28 ms**; SSAO is approximately **0.37–0.58 ms** in this view. Bloom is about 0.22 ms, tone mapping 0.65–0.69 ms, and temporal/display work 0.34–0.36 ms. These are pass-level measurements of this fixture, not additive independent costs on every scene. The cloud bypass lowers total mean frame time to 24.50/25.20 ms from 33.38/32.10 ms, at deliberately reduced visual output.

## Verification

- The native GPU regression compares exact serial/worker color and shadow-depth pixels over 12 frame pairs. It exercises indexed and instanced draws, per-draw constant remapping, a following pass sampling a worker-rendered texture, repeated frame-slot/allocator reuse, released RTV descriptor leases and implicit capture completion on `ClearState`.
- Existing state-cache, GPU deformation, asynchronous lifetime and FSR2 regression scenarios continue to run in `dx12_fsr2_smoke`.
- Native and FSR2 Quality full-game worker runs report zero DX12 errors and pass GPU skin readback at frames 1, 30 and 60. Screenshots were reviewed. Debug runs are excluded from the timing comparisons.
- The sky GPU test checks that the zero-step profiling bypass remains finite and fully transmissive; ordinary weather, night, layer crossings and noise checks remain in place.
- Final test/build and save/settings hashes accompany the delivered checkpoint. OpenGL sources and its target are unchanged.

`delivered-gpu-tests.txt` confirms the final DX12 and sky suites pass, including the final `ClearState` and descriptor-lifetime regressions. `final-validation/` confirms the delivered serial default in both native and FSR2 Quality modes with zero DX12 errors. Its native screenshot was reviewed; the FSR2 screenshot is byte-identical to the reviewed worker capture. The final default benchmark is **31.70 ms mean / 37.24 ms p95**, with 1.132 ms GPU deformation and no worker lists. This single confirmation is not an additional claimed performance gain. Root/runtime executables match SHA-256 `FF19F06EB440195CA8F1C9EFE2D69FE0427AEDA5D2BAD65B1FA941D863EAC31D`; the existing root save/settings hashes are unchanged (`delivered-build.txt`).

## Reproduce

```powershell
./tools/verify_dx12_optimization.ps1 -Evidence evidence/local-worker-validation -ExtraArgs '--record-workers=4'
./tools/verify_dx12_optimization.ps1 -Benchmark -Baseline MiniCity3D-before-recording.exe -Label workers4 -ExtraArgs '--record-workers=4'
./tools/verify_dx12_optimization.ps1 -Benchmark -CandidateOnly -Repeats 1 -Label full -ExtraArgs '--record-workers=4'
./tools/verify_dx12_optimization.ps1 -Benchmark -CandidateOnly -Repeats 1 -Label no-clouds -ExtraArgs '--record-workers=4','--profile-no-clouds'
./tools/verify_dx12_optimization.ps1 -Benchmark -CandidateOnly -Repeats 1 -Label no-ssao -ExtraArgs '--record-workers=4','--profile-no-ssao'
```

The verification script uses the separate build runtime, restores its save/settings, and extracts only the last session from the append-only log. Use a fresh evidence directory to preserve prior logs.
