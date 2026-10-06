# Builder wildlife and pickup integration

This milestone integrates wildlife height with edited geometry. The full Minecraft plan remains in progress.

## Behavior

- Builder wildlife has an actual runtime floor height and gravity velocity. Species-sized Jolt virtual characters sweep static collision, including retained terrain roofs, excavations, buildings, trees, scanned rocks and placed blocks. Existing kinematic animal bodies use the same height for vehicle contacts. Normal-mode locomotion keeps its previous behavior.
- DX11 animal models, melee selection, predation, player retaliation, projectile bounds/hits, reticle obstruction, corpse interaction and rider placement use that shared height. Actor avoidance distinguishes vertically separated bodies. Block placement checks the animal's oriented footprint and current vertical extent.
- Mounted movement sweeps a compound animal/rider shape. Mounting checks the standing player's capsule headroom; dismounting requires a nearby supported floor and actual clearance. Underground corpse drops choose an unobstructed nearby floor rather than the original surface.
- F5 reconciles nearby wildlife against the incoming geometry. Restored normal ground resurfaces an underground animal; re-entry removes its support and lets gravity produce the fall. Health and loot flags survive these transitions. Runtime support is not a new claim that animal AI/falling state persists across restart.
- Original weapon pickup markers share a builder support-height helper with collection. They follow excavation floors and connected placed blocks at their column. Collection checks vertical overlap and an unobstructed 3D line; normal mode retains its original marker altitude and collection behavior.

## Focused verification

`builder_wildlife_scenarios` passes in 9.03 seconds on the implementation used for the full regression run. Its controlled fixtures use real Jolt collision and actual F5 transitions:

1. A retained roof prevents surface-animal melee selection, mounting, projectile hit acceptance and predator damage against an underground player. Attacks on a shared underground floor work.
2. A tiger and its rider move underground, render at the supported height and dismount onto the same floor. Both tiger and elephant mounting are rejected when their standing rider cannot fit beneath the retained roof.
3. An actual pig falls into an 80-unit excavation. F5 immediately restores normal support; re-entry preserves an initially elevated pose and gravity velocity before settling back on the excavated floor.
4. A placed block supports an animal; removing it causes a fall. A walking pig detours a placed underground block over 20 seconds, ending at X 420.7, Y -80.3, Z 142.7.
5. Prey/corpses separated by a roof do not interact across levels. Underground carry/drop keeps the corpse on the underground floor.
6. All 15 species settle through actual gravity and render on the excavated floor with species-sized clearance. A small underground animal is hit by the live game's swept projectile loop.
7. Placement rejects a block intersecting an animal on its current floor, permits the separate surface cell above the roof, and reticle/edit targeting stops on the underground body.
8. The actual game update loop rejects weapon collection through retained terrain and a placed block, permits collection at the matching surface, and restores normal marker height on exit.

All 37 CTest suites passed in 281.19 seconds, including the existing normal wildlife regression and the separate-process navigation save/load fixtures. The new suite took 7.61 seconds in that full run. `ctest.log` records the complete result; `captures.csv` and the five final image/log pairs record native DX11 review of the current executable. Every final view was visually inspected: underground walking, pit support, an animal/pickup on a placed block, a mounted tiger below a retained roof, and normal F5 restoration. The corrected tunnel/block views no longer select scenery through the animal's body. Captures prepare controlled geometry and animals, rather than an exploration or resource-acquisition playthrough.

The verified runtime executable was copied to the root. Its SHA-256 is `9B634A489A721611D864E0561D9998975D7D3BF4244EE3DD05DC298160D4CACA`; both copies match. The protected root save retains SHA-256 `5A4C87DB5DC9E23148622694D21056E0B18EC4FE45FE7B294C59826B289FC539`. `verification.json` records current source/executable/image hashes and this verification scope.

## Reproduction

Build `MiniCity3D` and `simulation_smoke` in `build-msvc-ninja` using the Visual Studio developer environment. Run tests from that separate runtime:

```powershell
ctest --test-dir build-msvc-ninja -R '^builder_wildlife_scenarios$' --output-on-failure -V
ctest --test-dir build-msvc-ninja --output-on-failure
./tools/verify_builder_wildlife.ps1
```

The native verifier launches five sequential processes with a hidden window, retains screenshots/logs and backs up/restores the runtime save. It verifies the root save hash throughout.

## Limits and remaining work

These controlled scenarios do not prove dense-world performance, behavior beyond streaming budgets, complex animal path planning, continuous exploration or progression balance. The existing approximate animal hit/reticle silhouettes remain approximations; this milestone fixes their floor height. Exceptional reconstruction rollback, interrupted saves, geometry/collider merging and the other integration requirements remain open. See `remaining-height-audit.md` for concrete source-level follow-ups.
