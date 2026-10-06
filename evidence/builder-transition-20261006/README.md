# F5 outgoing-save checkpoint — 2026-10-06

F5 now confirms a durable outgoing save before changing world layers. This is one part of the Minecraft plan's failure-recovery requirement; the full plan remains incomplete.

## Behavior

- The writer returns a receipt for the exact submitted snapshot. Its atomic status is pending, succeeded, failed, or superseded. Replacing a queued autosave marks its old receipt as superseded; a later write cannot change an earlier receipt's result. The queue remains bounded to one active and one pending snapshot.
- The transition stays in “Saving outgoing layer” while its receipt is pending. Polling does not wait on disk I/O, so rendering can continue while ordinary simulation and harvesting remain paused.
- Success is reported only after writing the complete snapshot, flushing its file buffers and atomically replacing the save file through the existing Windows writer.
- A failed or superseded outgoing checkpoint cancels the switch before mode, camera, scenery, collision or actor recovery changes. A provisional baseline from a failed first entry is discarded. Input remains cleared, and an actionable message tells the player to press F5 to retry.
- The transition owns its failure notification, preventing the ordinary autosave warning from replacing the F5 retry message. Successful switching still queues its normal post-recovery snapshot.
- Losing window focus opens the existing pause menu. The main frame loop continues only the already-started transition in that paused state, leaving the ordinary simulation stopped. Once F5 finishes, Escape can close the menu normally.

## Verification

All **31 CTest suites passed in 176.34 seconds** after the save/transition core changes; see `ctest.log`. The subsequent main-loop-only focus fix was rebuilt and verified with the four DX11 fixtures below. Save-writing tests and previews ran sequentially in `build-msvc-ninja`, away from the user's root save.

`tests/save_jobs_smoke.cpp` blocks a real writer callback with a promise and checks pending receipts, 1,000 coalesced submissions, supersession, exact earlier results, exceptions, failure recovery and shutdown draining. Polling the receipt requires no worker mutex or disk wait.

`tests/builder_transition_scenarios.cpp` runs the regular F5 path through `game::update` with real Windows file handles that deny save replacement or temporary-file creation. It verifies:

- Failed first entry preserves the original mode, camera, crouch, actor positions, collision, clock and builder snapshot, then permits a successful retry.
- A populated builder layer keeps its mined terrain, building cut, destroyed tree, chest with an axe at 17 durability, inventory and dropped material after repeated failures.
- Previous save bytes remain exactly unchanged after failed creation or replacement, and the temporary file is cleaned up when the fixture releases its creation lock.
- Retry restores normal scenery and removes builder collision; re-entry restores builder edits. A real save/load regenerates the world and retains the full builder records, including chest durability and edits.

Existing builder, excavation, scenery, tool, contact-feedback, loose-actor and ordinary autosave scenarios also passed with the checkpoint gate.

## DX11 reproduction

Run `tools/verify_builder_transition.ps1`; `-CaptureOnly` uses an independently completed test run. Captures are generated sequentially from prepared block/item fixtures using the regular transition. They do not demonstrate a full resource-acquisition playthrough.

| Capture | State |
|---|---|
| `loading.png` | Saving stage before a world swap, with loading label and progress bar |
| `failure.png` | Real save-replacement denial cancels exit; blocks, equipped axe, builder HUD and retry message remain |
| `retry.png` | Releasing the lock permits F5 exit; blocks/collision and builder HUD disappear while stored blocks and axe durability remain |
| `focus.png` | F5 completes after the real input handler receives focus-loss and Escape messages; normal HUD resumes with no builder collision |

Matching logs record mode, transition flag, stored blocks, collider count and axe durability. The loading capture freezes the ordinary smoke fixture immediately after the checkpoint is queued, before polling its result; it is not a disk-latency benchmark.
All four final views were visually inspected. `focus.log` additionally confirms the pause-menu transition completed without advancing the game clock and that Escape resumes. Each fixture retains eight stored blocks and the axe at 77 durability; failure keeps one active builder collider chunk, and retry/focus leave zero.

Runtime SHA-256: `CBE01AAC8DECCB4B181DDD48CD54870371308BDAF784052E8E30C22B92CE79BC`.
Protected root save SHA-256: `5A4C87DB5DC9E23148622694D21056E0B18EC4FE45FE7B294C59826B289FC539`.

## Remaining work

Exceptional collision or actor-recovery failure still needs transactional rollback. Process interruption during writing, complete underground navigation, every tool's full acquisition/use/storage loop and appearance, dense-build merging/performance, and broader interactive checks remain open. The save gate does not claim to roll back the entire simulation or finish those requirements.
