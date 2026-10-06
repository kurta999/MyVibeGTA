# Builder actor interactions and tactical movement

This milestone integrates pedestrian interactions, physical corpse release and armed tactical destinations with the edited builder floor. The complete Minecraft plan remains in progress.

## Behavior and evidence

`before-fix.log` retains a real runtime reproduction: an underground player could loot and carry a surface corpse through the retained roof, received a pickpocket prompt for a surface pedestrian, and punched that pedestrian from 100 health to 78. `before-source-hashes.json` identifies the production sources used for that reproduction. That diagnostic deliberately asserted the old faulty behavior; its passing exit code is not correctness evidence.

The corrected `builder_interaction_scenarios` passes in 6.70 seconds on the final implementation:

1. Loot and carry selection use the current live-ragdoll or captured-corpse bounds/contact, vertical overlap and an actual 3D sight line. A stale death-capsule height cannot prevent interaction after a body falls into an excavation. Surface and captured corpses cannot be reached through a retained roof; same-floor cash is awarded once.
2. Builder drops check a lying-body volume against real static and dynamic Jolt collision and an unobstructed release path. A successful release creates a fallen ragdoll at the player's actual height, preserving loot, cash, health and respawn state. Bodies settle on excavated and placed-block floors. An obstructed release keeps the carried state and permits retry after space opens.
3. Actual F5 transitions preserve carried state and edited geometry. Captured bodies can be picked up, then released back into Jolt physics. Session corpses and AI state are not newly saved across process restart.
4. Pickpocket/talk prompts, their selected actions, regular player melee and vehicle-repair selection respect actual actor floors and 3D obstruction. Direct vehicle repair uses the same builder reach check. The separate stealth-knife path remains part of the remaining audit.
5. Armed Attack destinations use the shared supported-floor route selector for retreat, approach and strafing, including an opposite-side strafe fallback. Edited selected endpoints invalidate immediately. Movement retains a selected endpoint's floor instead of replacing it with an elevated player's height. Actual AI movement/firing beneath an original building and an elevated-threat case execute within the existing four-plan/18,000-query/12,000-expansion budgets. These single-actor cases do not prove dense combat performance or fairness.
6. Interaction and carry hints now appear above the builder hotbar/message. Carrying a body hides held builder equipment in first and third person and cancels mining/placement until release; inventories remain intact. Both rendered views and the unchanged edit snapshot are checked in the focused scenario. The existing carried-body animation is retained.

Five final native DX11 views were visually inspected: captured underground corpse with loot/carry hints, carried looted body with a visible drop hint and no overlapping held block, released ragdoll on the excavated floor, an armed actor beneath the retained roof, and restored normal ground/corpse support. `captures.csv` and the image/log pairs record those views. They prepare controlled geometry and actors rather than an exploratory acquisition playthrough.

## Regression investigation

`ctest-initial.log` records an initial full run with 35 passes and three failures: a builder transition completion assertion, a tool-work timeout, and an outdated cover-route assertion. The cover test now validates the immediate supported Attack route after cover removal, including current revision, actual floor, clearance and departure from the former cover target. The first transition failure did not recur in `recheck.log` (builder suite 15.06 seconds); the unchanged tool suite passed in `tool-work-recheck.log` (49.42 seconds). No test deadlines were increased, and no gameplay or save-success gate was weakened. The timing failures' underlying cause is unproven; retain them when evaluating performance/reliability.

The final complete regression run passed all **38 suites in 284.57 seconds**, including tool progression, saved navigation restart, normal actor regressions and the new interaction scenario (6.63 seconds within that run). `ctest.log` retains the complete result. The verified runtime executable was copied to the root; both copies have SHA-256 `E246A44FD02D2F2C50CF10FB6EAC1FF9F46B2619D8008826479A5EEC6A4D64E0`. The protected root save retains SHA-256 `5A4C87DB5DC9E23148622694D21056E0B18EC4FE45FE7B294C59826B289FC539`. `verification.json` records current source, executable and image hashes and this verification scope.

## Reproduction

Build `MiniCity3D` and `simulation_smoke` in `build-msvc-ninja` using the Visual Studio developer environment. Run save-writing tests and native processes sequentially in that separate runtime:

```powershell
ctest --test-dir build-msvc-ninja -R '^builder_interaction_scenarios$' --output-on-failure -V
ctest --test-dir build-msvc-ninja --output-on-failure -j 1
./tools/verify_builder_interactions.ps1
```

The native verifier backs up/restores the runtime save, launches five sequential hidden-window processes and checks the root save hash. The OpenGL fallback files/target receive no implementation changes.

## Remaining scope

Continuous exploratory progression/balance, vehicle road pursuit, separate stealth/witness and static-world height consumers, complex wildlife targeting/routing, behavior beyond streaming budgets, dense-world navigation/physics/memory/performance, face/collider merging, exceptional reconstruction rollback and interrupted-process save/version-import recovery remain incomplete. See `remaining-height-audit.md` for concrete source-derived follow-ups. Native review proves the pictured controlled states, rather than all those broader requirements.
