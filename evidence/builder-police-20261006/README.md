# Builder stealth, witnesses, and police support

The pre-correction helper scenarios reproduced three defects: an underground player could kill a surface victim through retained terrain, a surface witness could report a roof-hidden takedown, and dispatched officers used original surface height rather than a reachable excavated floor. `before-fix.log`, `unfixed.log`, and `before-source-hashes.json` retain that evidence.

Builder stealth now uses actual player/victim height, reach and obstruction. Witnesses use real 3D distance, height and line of sight while retaining the horizontal facing cone. Police dispatch selects a supported, unoccupied capsule-sized floor and a fully reachable destination, including dynamic obstruction rejection; it defers when geometry or the shared four-plan/18,000-query frame budget prevents dispatch. Officer corpse-slot reuse preserves carried-body ownership.

Exact voxel-edge floor rays could miss a valid underground floor. The corrected query samples tiny adjacent offsets only at grid seams and projects the hit plane back to the requested column. Full footprint and capsule validation remain in place. The dispatch reproduction changed from an exhausted 18,000-query search with no officer to a direct route using 2,874 queries. Raw floor samples at x=439.9, 440.0 and 440.1 now agree; gaps/headroom still reject support.

Budget-deferred armed AI route choices now retry promptly rather than taking the ordinary route cooldown and switching strafe intent. In the synchronized 12-actor scenario, progress changed from 4/12 to 12/12 over three seconds, with a final peak of 13,937 queries. This controlled result does not establish broad combat or dense-world performance.

## Verification

`tests/builder_police_scenarios.cpp` covers helper-level stealth reach, roof/block/facing witnesses, carrying rejection, elevated support, actual dispatch/pursuit/fire, dynamic obstruction/headroom, full-endpoint reachability, removal/retry, shared budget deferral, carried-corpse ownership, seam support, and the synchronized armed crowd. The final focused run is in `../builder-performance-20261006/focused.log` (3 suites passed, 24.55 seconds).

`tools/verify_builder_police.ps1` captures five native DX11 fixture views: same-floor target, roof-hidden witness, underground pursuit/fire, blocked-room deferral, and F5 normal-ground restoration. All five final PNGs were visually inspected; `captures.csv` records hashes and actor height. Images are 1600×900. The normal scene's test building was introduced after its normal-layer baseline was captured, so this view verifies restored ground/officer support, not persistence of that fixture building.

Initial capture attempts and assertion failures are retained. Dispatch preview was corrected to include the same original-building obstruction as the automated fixture and to advance 180 simulation ticks, matching its pursuit/fire scenario, rather than assuming fire within 90 ticks. `dispatch-capture-failure.log` records the earlier preview failure. Final capture run: `capture-final-180.log`.

## Remaining integration

These previews call game helpers directly. Actual builder F/G/K keyboard actions are still consumed by `builder::handleKey`, and knife equipment does not yet have a builder hotbar path. The existing interaction HUD labels therefore do not establish end-to-end keyboard integration. Static shop/house/ladder/mission/hideout heights, road pursuit, broader live play, dense navigation performance, and exceptional collision reconstruction recovery remain open. NPC/corpse AI state is not made persistent across restart by this work.
