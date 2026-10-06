# Builder pedestrian destinations — 2026-10-06

Builder movement already followed Jolt walk surfaces, but wandering/fleeing/cover selection still used flat building footprints and ground-height sight lines. A legal underground destination could therefore be rejected, and a cover point could be inaccessible across an excavation. The builder destination selector now uses the same live capsule clearance, eight-point support and multi-floor routes as movement. It retains the selected floor and route rather than immediately doing a second search.

An inaccessible intended point may resolve to a supported, reachable approach. The actual AI target becomes that endpoint, so the actor never claims to have a route to the inaccessible point. Route selection and movement share the existing four-plan/18,000-query frame budget. An actor waits if the budget is exhausted. Edits invalidate both routes and cached floor samples; edits after `beginFrame` also synchronize live scenery collision before a threat callback selects a new target.

Cover candidates include nearby original buildings and placed blocks, with side centers as well as corners. The selector checks sight lines to two body heights at the actual destination floor and validates its approach. A defender preserves its cover target even after sight memory expires, then investigates when the tactic timer expires. Ordinary gunshot responses receive a 2.5-second hold period on reaching cover; already sheltered actors can keep nearby cover. Removed cover invalidates the selected route. Normal-mode AI retains its prior behavior.

## Focused evidence

All 36 CTest suites passed in 272.10 seconds after the final cover-lifetime/default-timer changes. The destination suite passed in 5.32 seconds within that run. The verified runtime/root executable hashes match, and the protected root save remains unchanged; exact identities are retained in `verification.json`.

`focused.log` and `focused-detail.log` retain the final standalone scenario pass (5.18 seconds). These are moving-actor checks against actual Jolt collision, not only sampled destination coordinates.

| Scenario | Evidence |
|---|---|
| Underground wandering beneath an original building | Actual AI chooses its targets and travels up to 127.8 units; 178 ticks have selected supported lower-floor targets despite a solid original surface footprint. Capsule height remains on the lower floor. |
| Fleeing from a real threat notification | The actor routes around the pit, passes its far side and remains above ground; its final X is 532.3 rather than falling into the excavation. |
| Same-frame terrain edit | A previously cached floor at (260,140) is removed without another `beginFrame`; selection updates collision and chooses a supported approach at (220,140), within the 18,000-query limit. |
| Underground block cover | Hit response chooses and reaches cover behind the placed granite block; the misleading original-height line is clear while actual-height body lines are blocked. |
| Cover lifetime and invalidation | Six seconds of actual AI/capsule updates retain cover beyond sight-memory expiry. Timer expiry starts investigation, a new threat retains nearby existing cover, and mining the cover resumes attack with the old route invalidated. |
| Ordinary gunshot response | A default zero tactic timer enters cover, receives its hold interval on arrival and subsequently investigates. |
| Twelve wandering actors | All twelve choose and execute their own destinations. Every tick stays within four plans, 12,000 expansions and 18,000 floor/clearance queries; observed crowd peak is 4,790 queries. |
| F5 isolation/re-entry | Normal mode rejects the builder destination cache and resumes its original planner; re-entry selects valid builder destinations again. |

The controlled fixtures clear unrelated actors/scenery and prepare excavations through test helpers. They verify AI integration, not resource acquisition or continuous exploration. Twelve walkers on this fixture do not establish dense-world performance. Other combat destinations, vehicle road pursuit, wildlife and remaining height consumers still require review.

## Native DX11 review

`tools/verify_builder_destinations.ps1` runs the native executable sequentially in the separate runtime, restores its previous save, and verifies the protected root save. It retains four full PNGs, per-view logs, and capture hashes/actor positions in `captures.csv`:

- `wander.png`: a real AI-selected walking pose on the underground floor beneath retained terrain and an original building.
- `flee.png`: the fleeing actor on the supported pit detour.
- `cover.png`: the defender at its selected underground granite cover after six seconds of AI updates.
- `normal.png`: F5 restores the original ground and removes builder visuals/collision. The behavioral suite also advances normal AI and checks its planner; the frozen native view shows the restored geometry.

The `selected` value in the final native report uses the live destination-validity predicate, including active mode and edit revision. A retained inactive route is not reported as an active builder destination.

## Reproduction

Run from the repository root, using Visual Studio's amd64 developer environment and its bundled CMake/CTest tools. Keep builds, tests and native save writers sequential.

```powershell
cmake --build build-msvc-ninja --target MiniCity3D simulation_smoke --parallel 3
ctest --test-dir build-msvc-ninja -R '^builder_destination_scenarios$' --output-on-failure -j 1
.\tools\verify_builder_destinations.ps1
ctest --test-dir build-msvc-ninja --output-on-failure -j 1
```

`build.log`, `ctest.log` and `verification.json` retain the final build and regression identity. `ctest-before-extended-hold.log` is the earlier full pass before the additional sight-memory/default-timer checks; the final result is recorded separately. The source/UI changes are confined to DX11/Jolt builder integration; OpenGL renderer files and target are unchanged.

The full Minecraft plan remains active. Continuous exploration/balance, other actor/height consumers, reconstruction rollback and interrupted-process recovery, face/collider merging, dense-world performance and subjective audio review remain open.
