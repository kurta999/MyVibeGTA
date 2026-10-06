# Builder F5 loose-actor recovery — 2026-10-06

F5 now reconciles nearby loose props, live ragdolls, and settled corpse poses against the incoming world layer, after rebuilding collision and recovering the player. This extends the existing player/vehicle/live-pedestrian recovery; it does not complete the whole Minecraft plan.

## Behavior

- Loose actors are checked using their actual rotated Jolt shapes against incoming static collision, including terrain, buildings, builder blocks, and streamed tree/rock geometry. A separate buried-center check handles objects completely below a restored heightfield. Water uses its existing lower physics bed rather than the nominal land height.
- Overlapping props move to a clear position and lose their penetration-induced velocity; unaffected props retain their position and momentum. Nearby dynamic bodies are activated so they can fall when a builder platform, pit roof, or other support disappears.
- Every live ragdoll is resolved as one group. A common translation preserves body rotations, internal joint anchors, rest positions and skinning origins. Only a moved body's velocity is reset. An external wall pin is removed when recovery moves the body or its incoming scenery support is gone; valid pins and the internal limb joints remain.
- Settled corpse snapshots have no active physics bodies. They receive the same rigid translation, then downward shape casts find support in the incoming world, including ordinary props and vehicles. This lets a corpse return to an excavated pit without changing its pose or dropping through a shared crate.
- Legacy corpse animations without a saved altitude find a clear ground origin. Their bounded search includes global building/block edits across streaming boundaries and does not pretend that their existing animation can store a roof height. Other groups can use a roof escape if the short local search is enclosed.
- Pose and prop publication occurs before the transition ends, without a physics tick, AI update, resource yield, death/loot reset, or time rollback. Recovery diagnostics count affected groups and unresolved searches; transactional handling of exceptional transition failures remains open.

## Verification

All **30 CTest suites passed in 86.84 seconds** after the final physics/runtime changes; see `ctest.log`. The new recovery suite's transition helper was then strengthened to run through `game::update`, rebuilt and checked separately; its output is retained in `recovery.log`.

`tests/builder_recovery_scenarios.cpp` checks the ordinary F5 transition and game-update path with real Jolt simulation:

- Prop falls into an 80-unit pit, immediately resurfaces on exit, and falls into it again on entry; unrelated momentum and distant positions stay unchanged.
- A submerged prop retains its existing water-bed position across both switches.
- A pinned live ragdoll resurfaces as a rigid group, preserving rotations, rest/skin origins, six body parts, internal joint behavior, death/loot/cash state and timers; the obsolete pin is released.
- A settled snapshot resurfaces and returns to pit support over repeated switches, with one retained pose and unchanged cuts/resources.
- A captured pose on an ordinary dynamic crate retains that support during switching.
- Reappearing saved block collision clears a prop and legacy corpse origin.
- Restored tree-trunk collision clears a prop from a mined root.
- A wide restored building uses a roof fallback for a prop and a ground escape for a legacy corpse; the prop falls normally after that builder roof is removed again.
- A valid wall pin survives an unchanged layer; a saved removed wall releases only its external constraint, and the six-part ragdoll continues to settle normally.

## Rendered evidence and reproduction

The three DX11 fixtures use a prepared 3×3 pit, an ordinary crate and a real Jolt ragdoll that becomes a settled snapshot. They do not represent a full harvesting/acquisition playthrough. Mode switches follow the regular F5 transition; Jolt supplies the falling simulation.

| Capture | Observed state |
|---|---|
| `pit.png` | Crate and textured settled corpse at the bottom of the excavation, with builder HUD |
| `normal.png` | Original ground restored; the crate and the same corpse pose visible above it; builder HUD hidden |
| `restored.png` | Excavation restored; the crate falls back and the corpse pose returns to its pit support |

The matching `.log` files record mode, crate bottom, corpse torso height and six published pose parts. `tools/verify_builder_recovery.ps1` reproduces six focused suites and the three captures. `-CaptureOnly` refreshes the views after an independently completed test run. All save-writing tests and previews run sequentially in `build-msvc-ninja`, away from the user's root save.

The runtime executable SHA-256 is `3E5179A7B899E8822F4A5C4876C4860BE04E418950F21877C8121DF59B50AD3E`. The user's root save retained SHA-256 `5A4C87DB5DC9E23148622694D21056E0B18EC4FE45FE7B294C59826B289FC539`.

## Remaining scope

Complete underground navigation and the remaining actor/height-query integration, transition/save failure recovery, progression and full item appearance/gameplay-loop review, merging, dense-world memory/performance checks, and interactive verification remain required. Recovery is bounded to nearby loose actors and a finite escape search. Exceptional unresolved searches still need integration with the planned transactional transition-failure path.
