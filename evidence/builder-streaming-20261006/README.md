# Builder actor streaming, vehicle height, and restart verification

This milestone continues `minecraft plan.md`; it does not complete the full plan.

## Verified changes

Pedestrian virtual capsules now retain their actual height and vertical velocity when streamed out. Builder AI uses Jolt movement within a 1,200-unit actor budget and freezes movement outside that area instead of using a flat surface shortcut. Returning capsules are recreated only after incoming terrain, building, scenery, and placed-block collision is ready. A valid underground pose or in-progress fall survives; a pose buried by an offscreen F5 layer change is reconciled against incoming floors before the actor next moves. Normal-mode restoration retains its existing 500-unit creation range. Unknown/replaced actors do not inherit an old indexed character's pose.

A settled capsule's bottom can coincide with the voxel floor boundary. The clearance check now permits a 0.05-unit terrain contact tolerance, avoiding a false burial classification and a visible upward shift during recreation.

Builder tree/rock collision now streams at 1,400 units. Buildings and placed-block collision stream at 1,900 units to cover the 1,200-unit actor area, capsule/block extents, and up to a full diagonal drift inside a cached 400-unit focus cell. Normal-mode building/tree ranges remain unchanged.

Vehicle impacts use the actual chassis vertical bounds and pedestrian height. NPC car selection, door approach, boarding, and exit use builder-layer heights and obstruction. Player entry and exit reject underground walls and prefer the unobstructed door. Normal-game interaction rules retain their existing branches.

## Focused scenario coverage

- Actual AI walking underground beyond the former 420-unit flat-movement boundary; streaming release, stationary offscreen pose, immediate capsule recreation, and resumed walking.
- An in-progress Jolt fall retaining its height and vertical velocity across unloading/recreation.
- Offscreen F5 normal collision restoration, with the returning actor repaired before movement; builder re-entry restores the excavation and falling floor.
- Replaced vector entries, including reused IDs, starting from their own generated floor rather than the previous actor's cached pose.
- Tree collision and actual capsule movement beyond the former scenery range; building and placed-block collision after both focus axes drift inside the same cached cell. The scenery suite separately checks an actual scanned-rock mesh near the actor-area edge.
- Surface-car rejection for underground impacts and NPC selection, real underground car exit/reboarding, placed-wall selection rejection, and player door obstruction.
- Two separate test processes: one saves 42 terrain cuts and a granite obstacle, exits, and the next loads those exact records. Newly created Jolt pedestrians then walk saved underground and surface detours. F5 restores the normal direct route and re-entry retains the saved edits.

The restart tests recreate controlled surroundings after loading but do not recreate the edited cells or placed block. This proves navigation through persisted geometry across process restart; it does not claim NPC positions or AI state are saved. Acquisition/progression has separate evidence in `evidence/builder-progression-20261006/`.

## Reproduction

1. Build the DX11 `MiniCity3D` and `simulation_smoke` targets.
2. Run CTest sequentially with `--test-dir build-msvc-ninja --output-on-failure -j 1`. The restart fixture runs its save process before its load process. For focused verification, select `^builder_navigation_(scenarios|restart_save|restart_load)$`.
3. Run `tools/verify_builder_navigation.ps1 -CaptureOnly -Evidence evidence/builder-streaming-20261006` after testing. It captures pit, roofed tunnel, restored normal route, and streamed underground actor views, preserving the runtime save and checking the root save hash.

All **35 CTest suites passed in 296.63 seconds** on the final code. The navigation suite passed in 9.17 seconds; the separate restart save/load processes passed in 0.61/2.95 seconds. `ctest.log` and `ctest-output.log` retain the complete result and scenario output. The four focused scenery/navigation/restart suites also passed; `focused.log` and `focused-output.log` retain that run. Earlier diagnostic/pre-margin logs are historical and do not replace the final results.

Four native 1600×900 DX11 captures were visually inspected: `pit.png` shows the actor outside the pit edge, `tunnel.png` shows the actor beneath the retained roof, `normal.png` shows restored terrain and the direct route, and `streaming.png` shows the returned actor on the same underground floor. The tunnel fixture now keeps its intended underground camera. The streamed fixture releases/recreates the capsule and checks its immediate height before resuming actual AI walking. `captures.csv` retains image hashes and actual Jolt positions; the screenshots complement those assertions rather than prove the entire movement sequence alone.

The verified runtime executable was copied to the root `MiniCity3D.exe`; their SHA-256 hashes match. The protected root save retains SHA-256 `5A4C87DB5DC9E23148622694D21056E0B18EC4FE45FE7B294C59826B289FC539`. `verification.json` records executable/source/image-related verification and the retained `restart-save.ini` fingerprint. OpenGL renderer files and the fallback target were not changed. No runtime captures were removed by the duplicate-capture audit because it found no retained builder-image duplicates.

The transition test helper bounds a stalled stage to five seconds; successful progress renews that bound so completed synchronous collision reconstruction cannot prevent the following safety stage from running. Production transition scheduling was not changed by this helper adjustment. Dense reconstruction responsiveness still needs the planned performance work.

## Rock appearance audit

`tools/build_builder_rock_review.py` produces `rocks.review.png` and `rock-assets.csv` from the current catalog and asset manifest. All 20 model, texture, icon, and OBJ source hashes matched. The sheet was visually inspected: the values are distinct, but several textures repeat the same morphology. Mudstone/shale/slate/travertine/schist/gneiss reuse wavy stripes, tuff/pumice/conglomerate reuse a checker pattern, and andesite/granite/diorite/gabbro/quartzite show diagonal bands rather than their specified grain/crystals. These materials need differentiated authored/procedural textures and native world review. This audit proves the appearance requirement is incomplete; unique PNG hashes alone cannot satisfy it.

## Remaining scope

Cover/wander target selection, vehicle road pursuit, wildlife and other height consumers still require integration. Actor behavior beyond streaming boundaries, larger construction/crowd combinations, and dense-world performance need broader checks. The full plan retains continuous exploration/balance, distinct rock-material appearance review, collider/face merging, exceptional reconstruction rollback, interrupted-process save recovery, and subjective audio review.
