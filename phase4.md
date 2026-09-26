# Mini City 3D — phase 4 ideas: a reactive city and reusable activities

**Status:** proposed design direction. None of the work below is claimed as implemented. Existing mechanics are foundations to extend; their current verification limits remain recorded in `idea.md`.

## Recommendation

Develop MiniCity3D around repeatable jobs, believable police pursuits, and missions with several possible solutions. These would give the existing vehicles, weapons, physics, weather, and large map more purpose.

This analysis is based on repository inspection of `idea.md`, the proposed `phase3.md`, relevant gameplay code, and official documentation for MTA:SA and other games. Gameplay feel still needs hands-on evaluation.

Phase 3 already proposes courier, vehicle-recovery, and fire-response jobs. Complete that foundation before building the phase 4 extensions below. Keep each milestone playable and verified in the Direct3D 11 `MiniCity3D` target. Leave the OpenGL fallback, `renderer.cpp`, `textures.cpp`, and `MiniCity3DGL` untouched.

## Lessons from MTA:SA

### Modular activities

MTA packages scripts, maps, assets, and dependencies into resources that can be started and stopped. For MiniCity3D, a similar structure could let a race, delivery route, or rescue scenario ship as a small content pack. The existing data catalogs provide a starting point.

Source: [MTA resources](https://wiki.multitheftauto.com/wiki/Resources).

### Accessible content creation

MTA's map editor supports placing objects, vehicles, and checkpoints. A small editor in MiniCity3D could reduce the effort of creating activities: place a start, destinations, props, and rewards, then immediately play the result.

Source: [MTA editor](https://wiki.multitheftauto.com/wiki/Resource:Editor).

### Different experiences in the same city

MTA supports racing, roleplay, stealth, and custom modes through scripting. The design takeaway is that a strong sandbox gains replayability when the same streets support different rules and objectives.

Source: [MTA overview](https://multitheftauto.com/).

MTA is a multiplayer platform, so individual servers determine much of the experience. For MiniCity3D, make consistent controls, clear objectives, quick retries, and enjoyable solo activities part of the foundation. Creator tools can follow without requiring multiplayer first.

## Other games worth studying

The adaptations in this table are proposals for MiniCity3D, not claims about implemented features.

| Reference | Useful feature to study | Feasible adaptation for MiniCity3D |
| --- | --- | --- |
| [HITMAN](https://ioi.dk/hitman) | Freedom of approach. | One harbor objective with street, rooftop, and waterfront routes. |
| [Watch Dogs 2](https://news.ubisoft.com/en-us/article/63LcbEhcTsso6Qvc87ylfs/this-week-at-ubisoft-division-day-showcase-mighty-quest-rogue-palace-available-now) | Manipulating infrastructure and creating distractions. | Operable gates, alarm boxes, lights, and traffic signals that affect missions. |
| [Teardown](https://teardowngame.com/) | Preparing routes and using the environment creatively. | Breakable fences, movable obstacles, and a getaway vehicle positioned before starting a job. |
| [BeamNG.drive](https://www.beamng.com/game/) | Distinct vehicle behavior and focused driving scenarios. | Cargo condition, difficult deliveries, recovery jobs, and time trials using the existing Jolt vehicles. |
| [Wreckfest](https://store.steampowered.com/app/228380/Wreckfest/) | Damage, upgrades, and destructive racing. | Tire, engine, and steering damage, plus an enclosed demolition event. Full soft-body simulation would be a much larger undertaking. |
| [FiveM / OneSync](https://docs.fivem.net/docs/scripting-reference/onesync/) | Synchronizing nearby entities and managing shared world state. | A later small co-op prototype with explicit authority over vehicles, missions, and rewards. |

## Priorities and playable milestones

### 1. Complete the phase 3 district-job foundation

Courier work, vehicle recovery, and fire response reuse existing systems. Give each a different success condition: careful driving, preserving a target car, or controlling spreading fire. Local standing should unlock useful opportunities such as garage services, equipment, or harder contracts.

**First milestone:** complete two different deliveries, reload during one, and receive each reward exactly once. Follow the detailed persistence, validation, and release gates in `phase3.md`.

### 2. Turn wanted stars into a readable pursuit system

The current police code already handles witnesses, delayed reports, officer spawning, and wanted decay. Extend it with `suspicious → pursuit → searching → escaped` states. Show the last-known search area; officers should investigate that location after losing sight. Introduce surrender or fines for minor offenses.

**First milestone:** escape by breaking visibility and leaving the search area; being seen again resumes pursuit.

### 3. Create a mission that rewards experimentation

**Marina Recovery:** recover a marked car from a guarded waterfront yard. Enter through the gate, climb in from an adjacent roof, or approach by boat. Disable the alarm or accept a pursuit. Vehicle condition affects payment.

This combines existing mechanics while giving the player meaningful choices. Add only the interactions needed to make those routes work.

**First milestone:** the same objective can be completed through the street, rooftop, and waterfront approaches, with readable alarm consequences and a condition-based payout.

### 4. Give one neighborhood everyday life

Add pedestrians waiting at crossings, sitting at cafés, visiting shops, and sheltering from rain. Let traffic stop for blocked lanes and yield to emergency vehicles. Begin with a few authored routines near Marina Part.

**First milestone:** the same block has recognizably different activity at morning, evening, and during rain.

### 5. Make vehicle ownership matter

The game already has houses, garages, durability, and differentiated handling. Extend them with visible repair stages, paint choices, storage, and modest handling upgrades. A damaged tire or engine should produce a clear driving consequence and a reason to visit a garage.

**First milestone:** a persistent owned vehicle can suffer a readable component failure, be repaired at a garage, and retain its condition and customization after save/load.

### 6. Build a small activity editor

Once the job format settles, start with checkpoint placement, vehicle selection, time limits, and rewards. Include validation and a “test from here” button.

**First milestone:** create, save, reload, and play a new race without recompiling C++.

## Polish and verification

Reserve a polish pass for movement transitions, camera behavior, contextual prompts, engine and impact audio, and consistent materials. The roadmap explicitly leaves interactive feel and broader performance validation open, so those checks remain release requirements.

Use original or appropriately licensed content. Adapt the design patterns above to the existing project rather than copying another game's assets, missions, or branding.

Keep roadmap status factual. Jolt physics and ragdolls, live skeletal animation, and GPU instancing/LOD must only be described as complete to the extent that their respective implementations are integrated and verified. This proposal does not change their existing status or close any outstanding checks.

## Suggested sequence

Complete district jobs, then develop police search behavior, Marina Recovery, neighborhood routines, and the activity editor. Vehicle-ownership improvements can follow the recovery-job foundation as a separate playable increment.

Small co-op is a separate later milestone, after the solo loop and persistent world state are dependable.
