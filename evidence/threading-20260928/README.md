# Direct3D 11 parallel loading and scene preparation — 2026-09-28

These measurements describe the loading/scene threading build before the subsequent [pickup hitch changes](../pickup-hitches-20260928/README.md). That later build adds weapon preload work, cached audio, and background autosaves; its executable hash and checks are recorded separately.

## Implementation

- Texture preparation runs in independent CPU jobs: GDI+ decode, pixel copying, and the existing color/normal/linear/cutout mip generation.
- A sliding window bounds outstanding texture jobs and CPU image results to the selected worker count. Images are released after upload, before scheduling the next job.
- The main thread creates GPU resources and publishes renderer caches. Regional textures are deduplicated by file and texture kind before submission; shared GPU textures own explicit references.
- The fifteen startup graphics shaders compile concurrently. Results and errors return to the main thread for shader creation, logging, or UI reporting.
- The default selects `min(4, hardware concurrency - 1)`, with a minimum of one. `--loader-workers=1` executes the same jobs inline for a serial reference; overrides accept 1–8.
- Progress represents completed work. Polling checks cancellation while jobs run; texture generation checks cancellation between rows. Workers are joined before GDI+, the renderer, or captured inputs are destroyed. An in-progress D3D compiler call finishes before cancellation can join it.
- Loading workers are scoped to startup batches. A separate persistent runtime pool handles large CPU deformation loops (4,096+ vertices) and grass-cache rebuild rows. Deformation jobs write separate vector elements; grass jobs build private rows that merge in original order. Every batch joins before captured state or output storage changes. Small loops remain inline. The main game loop, GPU uploads/submission, and Jolt job system remain on the main thread.

## Measurement method

Release MSVC build, AMD Radeon 680M, 1920 × 1080. Each comparison uses the same executable, assets, data, and settings (`settings-used.ini`). Performance runs execute sequentially; alternate runs reverse worker order. File caches are warm. Timings vary with system load and power/thermal state. Smoke startup skips saved-progress restoration; ordinary launch is checked separately. Worker CPU fields sum elapsed job durations, including scheduling/I/O waits; they are not OS CPU accounting and can exceed batch wall time.

`final/` holds measurements of the loading implementation before adding runtime scene jobs, with validation hashing disabled. `validation/` holds its separate checksum runs. The top-level startup files record the earlier texture-only experiment with validation hashing enabled, and `before-startup-*.txt` record the original executable. These are distinct experiments and are not pooled into the medians. `integrated-startup/` and `integrated-validation/` hold the final build's startup/checksum runs; `scene-travel/` and `scene-route/` compare runtime workers with loading fixed at four workers.

Median of three **final integrated build** runs per worker count, with validation hashing disabled:

| Work | 1 worker (serial) | 2 workers | 4 workers |
| --- | ---: | ---: | ---: |
| Startup to ready | 17.660 s | 11.253 s | 10.182 s |
| Regional texture preparation and upload | 12.544 s | 7.080 s | 6.323 s |
| Surface texture preparation and upload | 1.241 s | 0.790 s | 0.510 s |
| Graphics shader compilation | 1.621 s | 1.250 s | 1.177 s |

Four workers reduce matched startup by **42.3% (1.73× faster)**, regional texture preparation by **49.6%**, surface texture preparation by **58.9%**, and shader compilation by **27.4%**. Shader compilation scales less than texture preparation. Raw startup values range from 16.482–21.645 seconds with one worker and 8.380–10.646 seconds with four, illustrating the effect of system conditions. The default remains bounded at four workers rather than using every logical CPU.

The earlier loading-only build (`final/`) measured startup medians of 19.788/14.874/9.962 seconds for 1/2/4 workers, a 49.7% reduction with four. Its regional/surface/shader medians were 15.081/1.141/1.355 seconds serial and 6.336/0.464/1.146 seconds parallel. These provide separate supporting measurements; the integrated build above is the final result.

The original executable's earlier three smoke runs reached ready in 15.685/15.703/16.076 seconds. Those runs occurred under different system conditions, so the final same-executable serial/parallel comparison above is the primary speedup evidence.

## Runtime scene preparation

Three interleaved travel runs per scene-worker count, with loading fixed at four workers:

| Work | 1 scene worker | 4 scene workers |
| --- | ---: | ---: |
| CPU deformation | 10.417 ms/frame | 4.110 ms/frame |
| Whole scene preparation | 13.336 ms/frame | 7.356 ms/frame |
| Grass rebuild (amortized) | 0.016 ms/frame | 0.012 ms/frame |
| Average frame time, median of runs | 33.10 ms | 32.31 ms |

Deformation takes **60.5% less time**, and scene preparation takes **44.8% less time** in these samples. The median overall frame-time reduction is only **2.4%**: GPU shadow/scene/post work still dominates, and main-thread GPU/driver waits move between the measured stages. Do not interpret a CPU-stage improvement as an equivalent FPS gain. Every run submits the same 8,855,610 triangles/frame and deforms the same 77,311 CPU vertices/frame. RAM remains about 678–679 MiB. Grass work is a small component; character/occupant deformation is the material CPU improvement.

One matched 3,600-frame route run per scene-worker count (`scene-route/`), with loading fixed at four workers:

| Work | 1 scene worker | 4 scene workers |
| --- | ---: | ---: |
| CPU deformation | 23.260 ms/frame | 10.051 ms/frame |
| Whole scene preparation | 27.517 ms/frame | 14.827 ms/frame |
| Average frame time | 67.63 ms | 64.83 ms |
| Frame time p95 | 274.08 ms | 210.53 ms |
| Frame time p99 | 586.73 ms | 573.88 ms |

This route uses high shadows and high temporal AA. Deformation takes **56.8% less time**, scene preparation takes **46.1% less time**, and average frame time falls **4.1%** in this pair. The route's large frame-time spikes remain; p95 improved 23.2% in this sample, but one pair does not establish a stable tail-latency gain. Both submit 8,892,238 triangles/frame, deform 144,134 CPU vertices/frame, and average 315 draws and 30 active AI. RAM is 681/682 MiB. GPU shadow, scene, and post/HUD timings total roughly 60–62 ms/frame, so more CPU workers alone cannot remove the main rendering cost.

The earlier `runtime/` route measured 52.08/52.12 ms/frame when changing only startup workers, with runtime deformation still serial. Startup concurrency alone is not a gameplay FPS improvement. Use the final runtime comparisons above to assess scene jobs.

## Ordinary launch and cancellation

The actual native loading window appeared after 39–79 ms and responded to a Windows message. A normal launch restored saved progress, prepared its first frame, reached ready in **13.984 seconds**, replaced the loading window with the game window, and closed cleanly. This is a separate functional check, not a matched startup speedup measurement.

Closing the native loading window during regional texture preparation exited cleanly in **86 ms** after cancellation. Closing it during graphics shader compilation exited cleanly in **1.042 seconds**, waiting for the active compiler call to finish. See `normal-launch-check.txt`, `cancel-textures-check.txt`, and `cancel-shaders-check.txt`.

## Verification

- All fifteen CTest suites passed; see `ctest.txt`.
- Texture tests compare every mip byte across serial/parallel preparation for all four texture kinds, check concurrency and queue bounds, invalid/missing/oversized images, caller-only publication, active/early cancellation, consumer exceptions, and reopening.
- Shader tests compare full serial/parallel bytecode, check concurrency and caller-only progress, invalid HLSL diagnostics, cancellation, and reopening.
- The worker pool transports job exceptions through futures and remains usable after a failing job.
- Runtime scene scenarios compare every generated vertex byte, ordered models, and GPU animation palettes across serial/parallel preparation for seats, pitched aiming, ragdolls, cold grass rebuilds, region changes, and repeated frames.
- Final integrated game runs produce identical shader checksum `9ac7cbd349871cbd`, surface mip checksum `c3f452d70d1b5bf2`, and regional mip checksum `f5339eebcd27685f` across 1/4 loading workers. They compile 15 shaders and prepare 18 surface plus 117 deduplicated regional textures; see `integrated-validation/`.
- Day, night/ragdoll, and forest 1080p captures were visually inspected with no visible regression. Forest PNGs match exactly. Day captures differ at 15 of 2,073,600 pixels (maximum channel delta 75); night captures differ at 3 pixels (maximum delta 24). A separate serial night repeat also differs at 3 pixels with maximum delta 24; a serial day repeat matches exactly. Full-frame pixel equality is therefore not asserted for day/night. Exact CPU geometry, shader, and texture comparisons provide the deterministic pipeline checks. Captures, logs, and pixel counts are preserved in `rendering/`; `verify_threaded_rendering.ps1 -CompareOnly` recomputes comparisons without launching the game.
- The root and tested executables used for this experiment had matching SHA-256 `58B9EB2B9CC977E487BCA5B8B46F567A891C856C52EBFC4421CCD7775D392C13`. The preceding loading-only build had SHA-256 `008824CC818860025DAEF984B271D7DBB34210DA2C2DCF57E95772615BB2E148`.

## Reproduction

```powershell
./build.ps1 -RunTests
./tools/measure_threading.ps1 -OutputDirectory evidence/threading-20260928/integrated-startup
./tools/measure_threading.ps1 -OutputDirectory evidence/threading-20260928/integrated-validation -Repeats 1 -Workers 1,4 -Validate
./tools/measure_threading.ps1 -OutputDirectory evidence/threading-20260928/scene-travel -Repeats 3 -Workers 1,4 -CompareScene -Travel
./tools/measure_threading.ps1 -OutputDirectory evidence/threading-20260928/scene-route -Repeats 1 -Workers 1,4 -CompareScene -Route
./tools/verify_threaded_rendering.ps1
./tools/verify_threaded_startup.ps1 -CancelAtStage 'Preparing regional textures'
./tools/verify_threaded_startup.ps1 -CancelAtStage 'Compiling graphics shaders'
./tools/verify_threaded_startup.ps1
```
