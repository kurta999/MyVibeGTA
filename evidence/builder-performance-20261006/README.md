# Immediate F5 performance regression

The user reported an unusable frame rate immediately after F5, before further mining or placement. The native DX11 benchmark retains the generated city, uses smoke seed 1, and enters the builder layer through its real transition. It does not load the user's save or reproduce their exact current location.

## Cause and correction

Builder wildlife interaction queries used real-geometry visibility before rejecting living, unmountable, or distant animals. HUD loot, carry, and mount prompts therefore scanned the whole world's scenery repeatedly. The completed three-frame 1080p diagnosis measured HUD work at 4,130–4,229 ms per frame (`hud-probe-before.log`), with an average total frame time of 4,238.01 ms.

`src/wildlife.cpp` now filters corpse eligibility, mount species, and interaction distance before height/visibility queries. Wildlife friend/prey selection also filters eligibility and range before visibility. Nearby candidates retain the height and obstruction checks. No physics or rendering quality settings were reduced.

The final 120-frame 1080p sample (`builder-after-120.log`) averaged 40.76 ms (24.5 FPS): simulation 33.27 ms, physics within simulation 1.11 ms, rendering 7.49 ms. p95 was 51.37 ms, p99 53.27 ms, and maximum 66.05 ms. It retained 43 active AI actors and averaged 350 draws. Hardware: AMD Radeon 680M. These short stationary measurements do not establish continuous travel, dense construction performance, or 60 FPS.

`builder-probe-final.log` is a separate **1600×900** diagnostic and must not be used as a same-resolution comparison. Its HUD work was 2.21–2.91 ms. `tools/verify_builder_performance.ps1` repeats normal 120-frame, builder three-frame with CPU profiling, and builder 120-frame runs at explicit 1080p, with runtime-save restoration.

## Verification and retained runs

- `focused.log`: builder wildlife, builder police, and normal wildlife suites passed (3/3, 24.55 s). The wildlife suite makes 600 interaction probes against 160 generated living animals under a generous 1-second bound, plus existing close-range, roof, mount, corpse, and F5 correctness checks. The measured probe total was 0.277 ms. This is a gross-stall guard, not a frame-rate promise.
- `normal-before.log`: completed normal-layer 120-frame baseline, 25.47 ms average at 1080p.
- `builder-probe-before.log` and `hud-probe-before.log`: completed pre-correction three-frame 1080p diagnoses.
- `builder-before.log` and `builder-profile-before.log`: deliberately interrupted long builder runs; not completed benchmarks. Host load varied during the latter.
- `builder-probe-after.log`: intermediate correction before mount filtering, still about 1.2 seconds/frame; not the final fix.
- `build*.log`: build outputs; final executable and source hashes are recorded separately in the verification manifest.

Pedestrian navigation remains the largest builder simulation cost (about 30–40 ms per frame in this location). Further navigation/dense-world optimization remains open.

The final executable recheck (`*-published.log`) measured normal 120 frames at 25.29 ms average and builder 120 frames at 45.95 ms (21.8 FPS), p95 57.27 ms, p99 70.46 ms. The same-resolution three-frame builder probe averaged 51.66 ms, with HUD work 2.91–3.61 ms. These repeat runs show timing variance; the observed final 120-frame builder range is 40.76–45.95 ms, not a guaranteed fixed frame rate. The native five-view police verifier also completed and its images were visually inspected. The user requested a commit and stop before the full 39-suite run; only the focused 3-suite run is claimed for this final code state.

## User saves

`user-root-save-backup.ini` preserves the user's current root save, SHA-256 `A4C391FFC48A3615E1FE0CC658B3C112D121ADB0ED68287F2B4E96DA861F8CF9`. This is their newer play state and supersedes the older save hash in historical milestones. `user-runtime-save-backup.ini` preserves their runtime-directory play state, SHA-256 `E85EF5016D16CD146B64A694A603F4011D970402DADACFE6ABFDA5C0A7E51033`. Runtime benchmarks/tests use the separate build directory; its user save must be restored after verification. Do not replace either with an older fixture save.
