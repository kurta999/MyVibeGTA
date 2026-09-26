# Mini City 3D — phase 3 increment: district jobs

**Status:** proposed plan. None of the work below is claimed as implemented.

## Goal

Turn the existing large world into a place with a useful repeatable play loop. A player should be able to take a short job in downtown or Marina Part, use the current vehicles or fire tools to finish it, earn cash and local standing, then take a different job without resetting the game. Keep every step runnable in the Direct3D 11 `MiniCity3D` target.

This increment builds on the existing ten authored missions (six main and four regional), data catalogs, Jolt vehicles, fire simulation, shops, map, and version 3 saves. The authored missions and current progression remain available while district jobs are added. Before calling the increment complete, resolve the current open human playthrough and performance checks against a packaged DX11 build.

## Ideas adapted from other games

| Game and source | Useful design pattern | Mini City adaptation |
| --- | --- | --- |
| [GTA Online Taxi Work and Odd Jobs](https://www.rockstargames.com/newswire/article/o39kk138351118) | Short, repeatable work makes driving and local road knowledge valuable. | Timed courier runs across the existing road network, with a safe-delivery bonus. |
| [Cyberpunk 2077 fixer gigs](https://www.cyberpunk.net/en/news/41435/patch-1-5-next-generation-update-list-of-changes) | Local jobs unlock in small sets as standing grows. | Each district tracks its own standing and opens a harder job variant after successful work. |
| [Watch Dogs: Legion borough activities](https://news.ubisoft.com/en-us/article/5RIKcWb9HBn65e9qRmnVkE/watch-dogs-legions-clint-hocking-on-how-delaying-the-game-let-innovation-flourish) | Several activity types give a neighborhood a recognizable identity. | Downtown emphasizes deliveries and vehicle recovery; Marina Part adds fire response near the promenade and road verge. |

Use these as structural references. Job names, text, locations, art, and tuning should be original to this project.

## Playable slice

1. A map icon leads to a clearly marked job board in downtown or Marina Part. The `F` interaction shows the available job, reward, time limit, and risk before acceptance.
2. The player accepts one job. A route and objective card explain the next action, and the map shows only its current objective. Starting a main mission while a job is active, or accepting a job during a main mission, gives a clear prompt.
3. The player completes or fails the job. The result card explains the payout, any bonus, and district standing change. Jobs rotate among valid authored locations after a short cooldown.
4. Saving and loading during a job restores its timer, objective, target IDs, and temporary scene state. Repeating an interaction or reloading at the finish cannot pay twice.

The first complete slice is a downtown courier run: pick up a parcel, drive through one route point, deliver it, receive the reward, save, reload, and take a second run with a different destination.

## Milestones

### 1. Contract spine and first courier run

- Add a validated `data/contracts.ini` catalog with stable job, board, route, pickup, and destination IDs. Validate world bounds, region references, reachable road endpoints, unique IDs, positive timers, and reward limits at startup. The first authored board and two destinations are in downtown.
- Add a small contract state machine: `available → accepted → pickup → transit → delivery → success/failure → cooldown`. Keep the active contract separate from `activeMission` and `missionDone`. Use the fixed simulation tick for timers and proximity checks.
- Reuse the current interaction resolver, mission cards, routing line, and map/HUD drawing. Show an exact `F` prompt for pickup and delivery; pressing `F` once is one transaction.
- Record the active job ID, run seed, chosen endpoint IDs, step, remaining time, and payout flag in a version 4 save. Import versions 1–3 with no job active and zero district standing. Save to the existing temporary-file path before replacing the live save.
- Add deterministic simulation tests for acceptance, wrong-location interaction, timeout, success, repeat acceptance, save/load mid-run, and reward exactly once. Finish with a short interactive DX11 play check.

**Done when:** two consecutive courier runs can end at different authored destinations; a mid-run reload resumes at the same step and time; cash increases once per successful run; all existing missions still complete in the simulation test.

### 2. Vehicle recovery job

- Author one downtown and one Marina Part recovery route using existing car spawns or a contract-owned spawned car with a stable run ID. The HUD identifies the exact target car and garage drop-off.
- Require the target vehicle to reach the drop-off under player control. Pay a condition bonus based on remaining vehicle health. A destroyed target fails the job; entering another car does not advance it. If a variant is marked as stolen, use the existing witness and wanted rules and disclose that risk on the offer card.
- Make spawn, abandon, timeout, and load cleanup idempotent. Restoring a saved run must not create a duplicate vehicle or overwrite a normally owned garage car.
- Test target selection, wrong vehicle, vehicle destruction, delivery, and reload while driving. Check interaction and garage use manually in the DX11 game.

**Done when:** the marked car can be recovered from both districts, the condition bonus is predictable, and abandoning or reloading never creates extra cars or rewards.

### 3. Marina fire response and district standing

- Author two small fire response sites on valid Marina Part ground near accessible roads, away from critical mission starts. Ignite a bounded set of contract-owned fire cells and make the objective extinguish them with the existing water or extinguisher tools. Track the contract's cells by run ID; naturally spreading fire remains governed by the normal fire system.
- Add district standing for downtown and Marina Part: 0–3 levels, earned from completed jobs and stored by stable district ID. Level 2 unlocks a longer courier variant; level 3 unlocks a harder recovery or fire variant. Show standing and the next unlock at each board and on the full map.
- Set reward and standing values in data. Credit them through one completion transaction and store a completed run ID so a saved near-finish run cannot replay its payout. Failed and abandoned jobs give no standing.
- Test rain and water suppression, fire spread outside the original cells, success at zero contract fires, failure on timeout, unlock thresholds, and save/load at each threshold. Inspect day, night, and rain views in DX11 for legible fire and objectives.

**Done when:** all three job types are playable, standing unlocks variants at the stated thresholds, and a fire job can finish without leaving contract-created fire active after abandonment or load.

### 4. Balance and release gate

- Play the loop from a new save for at least 30 minutes: complete one of each job, deliberately fail one, save and reload during one, then complete an existing main mission. Note confusing prompts, route dead ends, overgenerous payouts, and police interactions; fix the issues found.
- Run `./test.ps1`, then `./package.ps1` and play from the copied package. Check the existing day/night, biome, traversal, and debug smoke runs as well as the new job tests. Update `README.md`, `data/README.md`, and `idea.md` only with verified behavior.
- Compare paired 1080p DX11 samples in downtown, Marina Part, and six-region travel against the pre-increment baseline using the existing `--smoke --benchmark` and `--smoke --benchmark-travel` modes. Investigate any repeatable p95 increase above 10% or new visible hitch; record hardware, settings, and sample results.
- Perform a clean Windows machine play check of the packaged game, including a complete six-mission main sequence and at least one district job. Record any failure separately if a clean machine is unavailable; do not mark that check complete on the strength of smoke tests.

**Done when:** the packaged DX11 game passes automated checks, a human can understand and finish the new loop, existing story progression still works, and measured performance and clean-machine status are recorded factually.

## Implementation map

| Area | Expected change |
| --- | --- |
| `data/contracts.ini`, `data/README.md` | Authored boards, locations, variants, rewards, validation rules. |
| `src/contracts.h/.cpp`, `src/content.cpp`, `src/regions.cpp` | Contract state, stable IDs, catalog loading, location checks. |
| `src/game.cpp`, `src/input.cpp`, `src/police.cpp`, `src/fire.cpp` | Interaction, progression, and existing system hooks where needed. |
| `src/dx11_hud.cpp`, `src/dx11_scene.cpp` | Offer, objective, result, standing, and map markers. |
| `src/savegame.cpp` | Version 4 migration and active-run persistence. |
| `CMakeLists.txt`, `tests/simulation_smoke.cpp` | Build integration and deterministic end-to-end checks. |

Keep the OpenGL fallback, `renderer.cpp`, `textures.cpp`, and `MiniCity3DGL` untouched. Prefer reusing the present map, HUD, physics, AI, and fire systems over adding a new UI framework or world representation for this increment.

## Success measure

A new player can discover a board, understand the offer, complete three different short jobs, see meaningful local progress, save and resume, and then return to the existing missions. The city has a reason to revisit it beyond one-shot markers, with no duplicate payouts, broken saves, blocked roads, or DX11 performance regression.
