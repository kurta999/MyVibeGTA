# Traffic and civilian retaliation plan

The first milestone integrates traffic and retaliation into the DX11/Jolt simulation. The existing pedestrian weapons, damage, police reporting, animation, and vehicle physics remain the foundations.

## Behavior model

1. **Normal driving.** Split downtown streets, East City streets, and authored regional roads at intersections. Follow directional lane offsets, choose connected turns, avoid unnecessary dead ends, slow for junctions, and brake for pedestrians, the player, queued cars, and blocked road corridors. Give intersections a stable priority order and stop yielding cars outside the crossing path. Activate ordinary traffic within 1,000 units of the player.
2. **An incident.** Record a grievance against the player after a surviving assault, noticed pickpocket attempt, carjacking, player projectile hit, or physical collision with a moving player vehicle. Drivers pause for 0.8 seconds after initial vehicle damage before pursuing. Unrelated collision/fire damage does not automatically implicate the player. A successful unnoticed pickpocket remains unnoticed.
3. **Pedestrian vehicle acquisition.** A victim who cannot reach the player immediately searches within 360 units for a visible, stopped, unoccupied car. Exclude boats, motorcycles, owned vehicles, burning cars, wrecks, and cars already reserved or entered by someone else. Reserve one car, run to its door, spend 0.75 seconds boarding, then drive it. Fail or retry a blocked approach after ten seconds. Continue fighting on foot when no suitable car exists.
4. **Pursuit and combat.** Remember the last observed player position, using nearby unobstructed sight to refresh it. Route toward that position at junctions; use a direct local approach when clear. Pursuers drive faster and may ram the player's vehicle. They stop and get out near an on-foot target, then use existing melee or armed combat. Armed pedestrians retain cover behavior. A mechanically blocked pursuer gets out after six seconds. Civilians do not receive a gun merely because they were angered.
5. **De-escalation and cleanup.** Lose the target after twelve seconds without sight, with a maximum grievance duration of sixty seconds per incident. Death, vehicle destruction/fire, ownership changes, and contested player entry release the appropriate driver or reservation. Pursuit and reservation state are session-local; load preserves the existing saved money, ownership, death, and vehicle damage rules while clearing transient hostility.

## Implemented structure

- `src/traffic.cpp`: road graph, routing, driver ownership, braking decisions, intersection priority, pedestrian reservations, boarding, pursuit, cleanup.
- `src/ai.cpp`: grievance integration with existing hit recovery and on-foot combat; seated drivers do not run a second pedestrian controller.
- `src/game.cpp`: theft, carjacking, projectile/explosion attribution, and traffic updates.
- `src/jolt_world.cpp`: real chassis-contact attribution, explicit service braking, and removal of seated pedestrian capsules.
- `src/dx11_scene.cpp`: pedestrian entry clip selection and suppression of standing character meshes inside vehicles.

The graph is built once per populated world. Pursuit routing runs at junctions. Existing traffic cars and a subset of East City cars receive stable-ID drivers; other parked vehicles remain available to the player and retaliating pedestrians. Initial lane placement checks clearance from the player, buildings, and other cars.

## Verification

`ctest --test-dir build-codex --output-on-failure` runs the existing simulation and asset suites plus `traffic_scenarios` (also callable as `simulation_smoke.exe --traffic-only`). The traffic suite exercises:

- Moving traffic, stopping and holding for an on-foot player, resuming, and queuing behind a player vehicle.
- Multi-junction travel with bounded per-tick displacement and perpendicular intersection yielding.
- Actual Jolt vehicle-contact and projectile attribution; environmental damage remains neutral.
- An angry driver's ram causing physical damage to the player's vehicle.
- Last-seen memory and pursuit expiry.
- Two victims competing for one car, timed boarding, and pursuit movement.
- Nonlethal pedestrian impact, recovery, and car acquisition.
- Player entry interrupting an NPC reservation; noticed theft and carjacking retaliation.
- A pursuer exiting its car and damaging the player in melee.
- Dead driver, wreck, owned-car load cleanup, and circulation in the populated downtown.

For a reproducible graphical check, run `MiniCity3D.exe --smoke --traffic-preview --day --screenshot --1080p`. Add `--benchmark` to measure 120 rendered simulation frames. Screenshots are written beside the executable under `screenshots/`.

Validation on 2026-09-26: the DX11 build and all three CTest cases passed. The additional physical ramming assertion passed in the targeted traffic run. A 1920 × 1080 traffic preview was captured and visually inspected. The 120-frame preview benchmark averaged 1.01 ms simulation (including 0.86 ms physics), 25.94 ms total, and 27.95 ms p95 on the development machine; this is a scene-specific measurement, not a broad hardware performance claim.

## Pedestrian navigation follow-up

DX11 pedestrians now use A* when their destination cannot be reached directly. The search operates on a local 24-unit grid, expands buildings and oriented vehicle footprints by pedestrian clearance, and includes live props, water, and world boundaries. Grid diagonals and smoothed paths use swept clearance tests to prevent corner cutting. Cached routes are reconsidered when targets move, obstacles block a waypoint, or movement stalls. The target remains the AI's chosen destination or last-known player position; navigation does not reveal an unseen player.

A separate local steering layer selects a clear velocity, keeps walkers separated, chooses passing space for opposing walkers, predicts nearby moving-car crossings, and stops at melee distance from the player. This also applies to police, cover/investigation movement, and approaches to reserved vehicle doors. Jolt remains responsible for physical contact and knockback.

Searches are capped at four per AI tick and 3,000 expanded cells per search, with a 0.55-second retry interval. Destinations farther than 960 units are approached in local stages, with 288 units of detour space around each stage. An unreachable route stops or approaches a reachable point; it never falls back to moving through a wall. These are local navigation limits: large disconnected regions and very narrow passages can still require future navigation improvements.

The `navigation_scenarios` CTest case (`simulation_smoke.exe --navigation-only`) covers hostile pursuit around a wall, escaping a U-shaped obstacle, four walkers sharing a detour, opposing walkers passing, a newly placed prop, an angled parked car, enclosed destinations, and water boundaries. Routes are exercised through Jolt movement, with clearance and displacement assertions. Path caches clear on death and load.

## Later milestones

These are deliberately separate from the implemented milestone: authored traffic lights and signs, overtaking and lane changes, long-distance hierarchical pedestrian routing, stealing an already occupied NPC car, passenger/driver seated animation, reputation and personality-dependent escalation, richer pursuit search patterns, persistent grievances, and streamed regional population state. Current drivers wait for blocked traffic; they do not overtake a queue. Interactive play is still needed to tune traffic density, cornering, pursuit difficulty, and congestion under sustained player interference.
