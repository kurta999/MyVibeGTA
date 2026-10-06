# Builder pedestrian navigation — 2026-10-06

This milestone adds support-aware pedestrian routes in the persistent builder layer. The full Minecraft plan remains in progress.

## Implementation

- A navigation column can contain multiple walkable heights. Static Jolt ray hits and the actual pedestrian capsule validate floors, slopes and headroom, including retained tunnel roofs and stacked passages.
- Route edges sample continuous support and eight points around the capsule footprint. They reject unsupported pit shortcuts and unsafe floor changes. A pedestrian already near an edge may retreat toward better support.
- Bounded lazy A* searches distinguish floors at identical XZ coordinates. Static builder/terrain edits invalidate cached routes. Short capsule steering checks moving obstacles and same-floor actors each frame; a stuck walker replans.
- Per-frame shared floor queries and cached routes avoid rescanning entire paths on every tick. Intended safe velocity is tested before alternate steering velocities. Limits are four plans, 18,000 counted physics queries, 3,000 expanded nodes per search and 30,000 nodes per search.
- Normal mode retains the existing pedestrian planner. F5 deactivates excavations/blocks and their builder routes; re-entry reconstructs the edited walk surfaces.
- AI sight, talk and melee use actual pedestrian/player heights. Gunfire uses a 3D range and occupied vehicle height. Police witnesses check a 3D line and reports retain the target height. NPC boarding checks the pedestrian and vehicle heights.

## Gameplay evidence

`tests/builder_navigation_scenarios.cpp` moves actual Jolt pedestrian capsules through these controlled geometry fixtures:

1. A roofed tunnel beneath a player and another pedestrian at the same XZ position.
2. A placed underground block requiring a detour; mining it restores the straight route.
3. Independent opposing walkers crossing in two stacked passages.
4. A surface pit detour, normal-mode straight route after F5, and restored builder detour on re-entry.
5. A ceiling with insufficient capsule headroom, rejected without climbing through its roof.
6. A natural slope rising 40.1 units; the walker reaches the destination under Jolt movement.
7. Twelve nearby walkers, all making progress within the shared query budget. The observed final steady frame uses 888 counted physics queries.
8. Retained-roof rejection of talk, melee, gunfire and a silent police sight report; underground talk, shooting, melee and height-preserving police response work.

The fixtures prepare excavations directly and clear unrelated source scenery/traffic. They validate navigation and height interaction, not resource acquisition or a continuous exploration playthrough. `navigation.log` records the focused scenario result. All **33 CTest suites passed in 280.94 seconds**, including the navigation scenario in 5.43 seconds; `ctest.log` retains the complete result. Existing normal navigation, pedestrians, traffic, progression, excavation, save-failure and actor-recovery suites passed.

Three native 1600×900 DX11 captures were visually inspected: `pit.png` shows the pedestrian beyond the pit edge, `tunnel.png` shows the pedestrian beneath a retained ceiling, and `normal.png` shows restored terrain/road geometry and the direct walking route after F5 exit. `captures.csv` records image hashes and actual Jolt positions. Render fixtures move the capsule first, then freeze simulation for capture; screenshots complement the assertions rather than prove the complete movement sequence by themselves.

The verified runtime executable is copied to the root `MiniCity3D.exe`; `verification.json` records matching hashes and the unchanged root save hash. OpenGL renderer files and the fallback target were not modified.

## Reproduction

1. Build the DX11 `MiniCity3D` and `simulation_smoke` targets.
2. Run CTest sequentially in `build-msvc-ninja`, or run `simulation_smoke.exe --builder-navigation-only` with that directory as the working directory.
3. Run `tools/verify_builder_navigation.ps1` for focused verification. The current script also covers the restart fixture and streamed-actor view added in the later milestone; this folder retains the original three views. Pass `-Evidence evidence/builder-streaming-20261006` to reproduce the later four-view set. `-CaptureOnly` reuses completed test evidence. The verifier preserves the runtime save and checks the root save hash; it refuses the root as a runtime directory.

## Remaining work

The later `evidence/builder-streaming-20261006/` milestone verifies capsule pose retention/recreation, boundary collision margins, underground vehicle entry/exit/seeking and navigation through edited geometry after a separate-process save/load. Its 35-suite result supersedes the test count here; it does not claim completion of all actor/height consumers or the full plan.

Complete actor/height integration remains open: far pedestrian streaming/recreation, cover and wandering destination selection, vehicle exit/seeking and road behavior, wildlife, pickups/loose actors, and other consumers of a single terrain height need broader integration review. Crowds with dense construction and many simultaneous searches still need performance benchmarks. Navigation in edited geometry after a full save/restart needs a dedicated moving-actor scenario beyond the existing world-edit persistence coverage.

The full plan also retains its open reconstruction rollback/interrupted-save recovery, merging/dense-world performance, continuous exploration/balance, complete rock appearance and subjective audio checks.
