# Pickup and sound hitch investigation — 2026-09-28

Weapon collection called `savegame::save()` inside `game::update()`, immediately after playing the pickup sound. That save wrote the full world through hundreds of `WritePrivateProfileStringA` calls on the gameplay thread. It includes weapon progression, missions, ownership, corpse poses, vehicles, trees, wildlife, and birds. The timing can make a save hitch appear to be a sound hitch.

The sound path synthesized every waveform during `audio::play()`. The renderer also prepared held-weapon GPU resources on their first use; the regional preload list did not include weapon meshes.

## Changes

- Autosaves capture immutable serialized state on the main thread. A dedicated worker owns disk writing; it reads no game, physics, renderer, or audio state. One active write and at most one pending snapshot bound memory and disk work. The latest pending request supersedes older pending requests.
- The writer emits a complete INI with one file write, flushes it, closes it, and replaces the destination. Failure leaves the previous destination intact. Manual Save waits for its result; Load and normal shutdown wait for ordering. Load can still recover the previous save after a failed autosave. Background errors surface through the main-thread UI/logging.
- Playback reuses startup PCM rather than synthesizing or resizing a waveform during gameplay. Four prepared takes rotate for noise-based effects. Buffers remain immutable until voices are destroyed, following [Microsoft's XAudio2 buffer lifetime requirement](https://learn.microsoft.com/en-us/windows/win32/api/xaudio2/nf-xaudio2-ixaudio2sourcevoice-submitsourcebuffer). Mixing/playback already uses XAudio2's audio engine; a second sound-synthesis worker is unnecessary for these bounded effects.
- Pistol, AK, and pump-action weapon meshes, textures, and static GPU buffers are prepared during loading. GPU publication stays on the main thread.
- DX11 uses these changes; the OpenGL audio/save paths keep their original synchronous behavior.

## Measurements

Release MSVC build on the same machine as the earlier threading experiment. The save fixture resets the full configured world, including wildlife, and performs three saves. Audio uses a live output device muted to zero volume, with 100 calls per effect and enough overlap to exercise voice stealing. Before/after runs are separate sequential invocations; these are local microbenchmarks, not proof that every frame hitch is removed.

| Work on caller thread | Before | After |
| --- | ---: | ---: |
| Gameplay autosave, median of three | 206.221 ms | 0.285 ms |
| Explicit synchronous save, median of three | 206.221 ms | 8.904 ms |
| Pickup sound call, median | 116.8 µs | 1.7 µs |
| Shot sound call, median | 71.5 µs | 1.7 µs |
| Surf sound call, median | 430.0 µs | 1.4 µs |

Background request caller times were 0.296/0.271/0.285 ms; completion including disk write was 3.554/3.375/3.421 ms. The caller does not wait for that completion during gameplay. Explicit Save retains completion semantics, so it includes file/flush/worker waits and varies more (8.904/18.643/4.223 ms). The old synchronous save ranged from 203.671–219.918 ms. The measured pickup stall is dominated by saving; existing audio synthesis cost was much smaller.

Raw timings are in `save-before.txt`, `save-after.txt`, `audio-before.txt`, and `audio-after.txt`. Repeated saves use the fixture's save beside the test executable. Do not run this benchmark concurrently with simulation suites, because they share that test save path. An initial overlapping verification invocation was discarded; final measurements were run after the suites finished.

## Verification

- All eighteen CTest suites pass; `ctest.txt` records the final run. Existing persistence tests continue to exercise health/version import, inventory, ownership, wildlife, corpse poses, and other save fields.
- `save_jobs_smoke` deliberately blocks an active write while submitting 1,000 new snapshots. Submissions complete and the worker writes only the first and final pending snapshots. It checks caller/worker separation, destructor draining, false/throwing write failures, and recovery.
- `autosave_scenarios` checks captured state survives subsequent game mutations, the latest snapshot wins, Load drains requests, a locked destination survives replacement failure, the previous save can load, a later save recovers, and crossing a weapon pickup persists the equipped weapon.
- Live muted audio checks all effects and variant ranges, positioned playback, overlap/voice stealing, invalid effects, shutdown during playback, and reinitialization.
- Actual native launch restored saved progress, prepared its first frame, reached ready in 12.994 seconds, and closed cleanly. The loading stage completed 129 deduplicated regional/weapon textures; see `normal-launch-check.txt`. This functional launch is not a matched loading benchmark.
- Rifle and shotgun graphical smoke runs completed and their 1080p captures were visually inspected; see `rifle.png`, `shotgun.png`, and accompanying logs.
- Root and tested executables match SHA-256 `EE5AF8B3D2A04FD15F5177707B1DDA8844A9C84B4E49FD065C7485DC8D40E0A7`. The earlier startup/scene experiment used the preceding build documented separately in `../threading-20260928/README.md`.

## Reproduction

```powershell
./build.ps1 -RunTests
./build-msvc-ninja/simulation_smoke.exe --save-benchmark-only
./build-msvc-ninja/simulation_smoke.exe --audio-benchmark-only
./tools/verify_threaded_startup.ps1
./MiniCity3D.exe --smoke --day --1080p --weapon-preview --rifle-preview --screenshot
./MiniCity3D.exe --smoke --day --1080p --weapon-preview --shotgun-preview --screenshot
```
