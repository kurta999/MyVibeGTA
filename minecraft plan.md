# Minecraft-style building layer plan

Status: implementation in progress. The full requirements below remain the target; partial implementation does not mean the plan is complete.

## Requested tool expansion

- Include **at least 22 usable tools with real textured 3D models**: pickaxes, axes, shovels, and hoes in wood, stone, iron, gold, and diamond variants, plus shears and a brush.
- Equip the selected model in the player's hand, animate its use, and provide matching hotbar/inventory icons, dropped-item models, durability, crafting, and chest storage.
- Use axes on existing trees and wooden buildings, shovels on existing ground, and pickaxes on existing rocks, ores, and masonry. Harvest matching resources and save the removed sections together with inventory changes.
- F5 exit restores the normal world's appearance and physics; re-entry restores the saved destruction, placed items, and builder equipment. Show loading progress while switching layers.

The detailed catalog, model shapes, tool actions, research references, and review checks are specified in [Textured 3D tools and additional items](#textured-3d-tools-and-additional-items). This expansion belongs to the plan for review; this amendment does not change implementation status.

## Implementation progress (2026-10-06)

- F5 switches persisted normal/builder building and tree states with a staged loading HUD, input cancellation, nearby scenery-collision rebuilding, and safe actor positioning. Builder blocks, reticle/projectile obstruction, and Jolt collision are deactivated on exit and restored on entry.
- A 36-slot inventory, nine-slot hotbar, stack splitting/transfers, cursor-stack persistence, modeled chests and contents, dropped resources, durability storage, and coherent atomic save records are integrated.
- The catalog contains 67 original textured model assets and matching mesh-rendered icons, including all 20 rocks, coal/iron/gold/diamond ore, 20 tiered tools, shears, a brush, tilled soil, and loose surface deposits. Three additional shears components provide independently moving halves and a shared pivot. Reproducible OBJ sources and hashes for every model, texture, icon, and source are retained. Data-defined recipes provide wood-to-tool crafting, bench requirements, and furnace processing of iron/gold. The brush has a separate metal binding and staggered textured bristle bundles.
- Placed blocks can be mined; existing building sections, tree trunks/foliage, bushes, and scanned rocks can be cut and harvested in local cells. Builder block blast resistance, persistent blast damage, and one-time chest/content drops are integrated. Nearby torches use the existing capped local-light selection.
- Material suitability, ore harvest tier, hand speed, harvest drops, and stone crafting membership are catalog-defined. Held right-click placement repeats with a cooldown, inventory/mode changes cancel it, and Shift/Control provide builder crouch/sprint. Target HUD hints explain tool and tier requirements. C switches between first-person building and a third-person inspection view.
- Equipped models use the animated character hand in third person and a progress-driven swing in first person. Sticks, metal ingots, and diamonds now have slender, beveled, and faceted geometry respectively, with proportional dropped-item rendering.
- Surface pits and roofed tunnels now subtract local cells from the original ground, including on slopes. DX11 top geometry is clipped and textured interior surfaces are added; matching Jolt meshes replace disabled heightfield patches. Placed blocks fit inside excavations. Existing vegetation stays at its original root height, and surface grass disappears where that ground cell is removed.
- Deterministic terrain formations expose all 20 rocks and coal/iron/gold/diamond ore. Actual held-LMB tests harvest every material, enforce diamond's tier gate, consume tool durability once per successful cut, and retain resources and cuts across save/load. The all-material fixture prepares access shafts; a separate new empty-inventory progression scenario mines every shaft layer through live input and actual Jolt falls, obtains/processes materials and crafts all 22 tools. Travel between sites is scripted, so continuous exploratory play and balance remain unverified.
- Excavation scenarios verify capsule falls, one-block jump exits, a roofed tunnel across a patch boundary, ceiling/wall collision, mountain digging, camera/projectile/grapple queries, underground pedestrian targeting and hits, ragdoll spawn height, and vehicle falls. F5 restores the normal ground and safely resurfaces the player and nearby live pedestrians/vehicles; re-entry restores the excavations and collision.
- Automated builder scenarios cover model/catalog presence, stack conservation, F5 collision isolation, standing on blocks, actor safety, save/load, building/tree state separation, chest transfers/content persistence, blast drops, all 22 tool recipes, processing-station gates, live tool mining/tier/durability/cancellation, held placement, animated hand attachment, contact feedback, and loose-actor recovery. All 31 CTest suites passed after the outgoing-save checkpoint integration (176.34 seconds). DX11 world, inventory, loading, first-person tool, and third-person tool captures are reproduced by `tools/verify_builder.ps1`; pit, tunnel, normal-mode restoration, and slope captures by `tools/verify_excavation.ps1`; tree, rock, bush, and restored-normal scenery captures by `tools/verify_scenery.ps1`; tilled soil, brush progress, repair inventory, and restored normal soil by `tools/verify_tool_work.ps1`; cracks, shears movement, foliage clippings, brush dust, and normal-mode cleanup by `tools/verify_builder_feedback.ps1`; crate/corpse recovery and restored pit support by `tools/verify_builder_recovery.ps1`; outgoing-save loading, failure, retry and focus-loss views by `tools/verify_builder_transition.ps1`.
- Stable scenery IDs now retain block-sized cuts in tree, bush, and scanned-rock models. Original exterior UVs survive subtraction, connected source-derived wood meshes provide cut caps, surviving trunk proxies are rebuilt, and scanned-rock collision is confined to builder mode. Tests exercise axe/shears input, one-time resources/durability, placement in cleared cells, camera/projectile queries, removed climbing support, atomic record validation, generated-bush save/load, and F5 restoration. Thirty connected wood companion assets have a reproducible source pipeline and SHA-256 manifest. Verification notes are in `evidence/scenery-20261006/`.
- Hoes clear suitable generated plants with LMB and till exposed natural or placed soil with RMB. Tilled natural surfaces retain the original ground collision; placed tilled cubes retain their block collision and yield soil when shoveled. Brushes use held RMB or LMB on modeled, deterministically assigned loose deposits, with progress, cancellation, one-time resources, and one durability charge. Soil/deposit records persist together with inventory. All five hoe variants, live plant clearing, brush cancellation/full-inventory drops, and combined on-disk persistence/F5 isolation are tested.
- All 22 tools have data-defined repair materials and amounts. A nearby crafting bench is required; the inventory repair button or R consumes one configured material and restores up to one quarter of maximum durability. Tests cover every tool's cost, missing materials/station, cursor gating, full-durability rejection, clamping, UI activation, and persistence. Distinct short hoe and brush motions, ground-surface outlines, and inventory tooltips are integrated. Four new DX11 views were visually inspected; evidence is in `evidence/tool-work-20261006/`.
- Family-specific mining motions, moving shears blades around a shared pivot, progressive surface-projected cracks, typed material chips/foliage clippings, and rising brush dust are integrated. Eight material contact cues use original positional synthesized audio. Live-input tests check contact timing, tool poses, crack projection against actual tree/ground surfaces, protected-ore rejection, single durability/drop transactions, particle limits/expiry, and transient F5 cleanup. All 70 generated meshes passed model/texture/icon/OBJ hashes and binary-layout checks. Nine DX11 feedback views were visually inspected, including the corrected shovel grip and visible brush dust; audio exports were checked for distinct, nonsilent, unclipped samples and live muted XAudio2 playback. Evidence is in `evidence/builder-feedback-20261006/`; subjective sound quality and complete progression/appearance review remain unverified.
- F5 now checks nearby loose props and ragdolls against actual incoming Jolt collision. Overlapping props recover, unaffected momentum and submerged water-bed positions remain unchanged, and bodies wake to fall when support disappears. Ragdolls move as rigid groups with their rotations, internal joints, skin origins and death/loot/timer state preserved; obsolete external wall pins release. Settled corpse poses use shape casts onto incoming ground or shared prop/vehicle support. Legacy corpse animations find a ground origin rather than an unsupported saved roof altitude. Tests cover repeated pit recovery, reappearing blocks/trees, wide restored buildings, unchanged crate support and valid/removed wall pins through the game-update transition path. Three DX11 views were visually inspected; evidence is in `evidence/builder-recovery-20261006/`. Exceptional unresolved escape searches still need the planned transactional failure handling.
- F5 now waits for confirmation that its exact outgoing snapshot was flushed and atomically replaced before loading the other layer. The loading HUD continues rendering while the save worker runs. Failed or superseded checkpoints keep the current mode, scenery, collision, inventory and actors intact and display an F5 retry message; failed first entry also discards its provisional baseline. Real Windows file-lock tests cover save creation/replacement failures, repeated failures, successful retries, chest tool durability, dropped items, terrain/building/tree edits, unchanged previous save bytes and on-disk reload. Worker tests verify nonblocking receipts and superseded snapshots without an unbounded queue. The frame loop also completes an already-started transition through a focus-loss pause without advancing the game clock, then permits Escape to resume; four final DX11 views were visually inspected. Reproduction and evidence are in `tools/verify_builder_transition.ps1` and `evidence/builder-transition-20261006/`. Collision/actor reconstruction failures and process interruption still require broader transactional recovery verification.
- All 22 tools now have individually verified empty-inventory acquisition, UI crafting, suitable existing-world action, resource/surface-state result, drop/pickup, chest storage, F5 isolation and on-disk reload loops. The final earned chest, 203 terrain cuts and 16 edited scenery objects persist together. This exposed and fixed brushes blocking chest/bench/furnace interaction. Builder inspection now uses a tested right-shoulder camera, with target hints beside the tool. All 32 CTest suites passed in 283.26 seconds after the gameplay/camera changes; the final HUD-only adjustment was verified in DX11. Sixty-six held/third-person/dropped tool views were visually inspected via labeled crops, plus a native storage capture. Per-item asset hashes, recipe ingredients/stations and durability evidence are in `evidence/builder-progression-20261006/`; reproduction uses `tools/verify_builder_progression.ps1` and `tools/build_builder_review_sheets.py`.
- Builder pedestrians now use support-aware multi-floor routes validated against the real Jolt capsule, including eight-point footprint support, slopes and headroom. Controlled moving-actor tests cover roofed and stacked passages, placed-block detours and removal, pit avoidance, F5 normal-route restoration/re-entry, insufficient ceiling clearance, a 40.1-unit slope ascent and a 12-walker crowd. Cached routes/shared floor queries leave all twelve walkers progressing within an 18,000-query frame limit; the observed steady crowd frame uses 888 queries. Height-aware AI checks reject sight, talk, melee and gunfire through retained roofs while permitting underground interactions; police reports retain the underground target height. All 33 CTest suites passed in 280.94 seconds, including the new navigation suite. Three DX11 pit/tunnel/normal views were visually inspected, and the root executable matches the verified runtime build with the root save unchanged. Evidence and reproduction are in `evidence/builder-navigation-20261006/` and `tools/verify_builder_navigation.ps1`. Far actor streaming, cover/wander and other height consumers, navigation after a full save/restart, and dense-world navigation performance remain open.
- Builder pedestrian capsules now retain actual height and falling velocity through streaming release/recreation. Returning capsules are rebuilt after incoming collision is ready, before movement/render; distant F5 changes repair buried poses, and replaced actors do not inherit old indexed poses. Builder AI uses Jolt movement within its 1,200-unit actor budget; tree/rock collision now reaches 1,400 units and cached building/block collision 1,900 units to cover diagonal focus-cell drift. Tests cover real far AI movement, in-progress falls, immediate restoration, tree/scanned-rock collision at the actor boundary and building/block collision after cell drift. Vehicle impacts, NPC car selection/doors/boarding/exits, and player entry/exit respect underground height and walls. Two separate processes save/load 42 terrain cuts and a granite obstacle, then walk saved tunnel/pit detours and verify F5 route restoration. All 35 CTest suites passed in 296.63 seconds. Four DX11 pit/tunnel/normal/streamed-actor views were visually inspected; the verified executable is copied to the root and the root save is unchanged. Evidence is in `evidence/builder-streaming-20261006/`; reproduction uses `tools/verify_builder_navigation.ps1 -Evidence evidence/builder-streaming-20261006`. This does not claim NPC positions or AI state persist across restart.
- The complete 20-rock texture/icon sheet was inspected, with model/texture/icon/OBJ hashes checked against the manifest. Several materials reuse wavy stripes, checker patterns, or diagonal bands instead of their specified grain, pores, pebbles, or crystals. Distinct durability values and file hashes do not complete the appearance requirement. `tools/build_builder_rock_review.py` reproduces the sheet and per-rock catalog/hash report in `evidence/builder-streaming-20261006/`; differentiated textures and native material review remain required.
- The rock appearance follow-up replaces the repeated patterns with original deterministic grain, pores, pebbles, crystals, cleavage seams, veins and aligned flakes. All 20 cubes now use a full texture tile per face, with cube geometry and gameplay strengths unchanged. All 70 asset hashes/layouts pass, all 20 rock recipes reproduce current pixels, and the other 50 asset entries remain unchanged. Twenty native DX11 block/icon views were visually inspected. Actual held-LMB mining yields each material once, charges durability once and has 20 distinct durations matching catalog resistance within one update. A gallery on the original map saves all 20 identities; normal mode hides its rendering and builder collision, and F5 restores them. The three complete gallery/normal/restored views were visually inspected. Evidence and reproduction are in `evidence/builder-materials-20261006/`, `tools/verify_builder_rocks.ps1` and `tools/build_builder_rock_review.py --native`. The earlier texture audit is superseded by this verified appearance review; the native cards use supplied items and do not claim an exploratory acquisition playthrough.
- Builder wandering, fleeing and cover destinations now use actual supported Jolt floors and reachable routes instead of flat world footprints. Actual AI walks beneath an original building, flees around a pit and reaches placed underground block cover. Defenders retain cover after sight memory expires, investigate on timer expiry, can retain nearby existing shelter and respond when cover is removed; ordinary gunshot responses receive a hold interval on arrival. Same-frame terrain edits synchronize collision and invalidate cached floors before target selection. Selection/movement share four route plans and 18,000 queries per frame; twelve actual wandering actors move within that budget (observed crowd peak 4,790 queries). Tests verify F5 normal planner restoration and builder re-entry. All 36 CTest suites passed in 272.10 seconds, and four native DX11 views were visually inspected. The matching verified runtime/root executables preserve the root save. Evidence and reproduction are in `evidence/builder-destinations-20261006/` and `tools/verify_builder_destinations.ps1`. These controlled scenarios do not establish continuous exploration or dense-world performance; the remaining height audit records concrete wildlife, pickup and combat queries still needing integration.
- Builder wildlife now shares actual runtime floor height across species-sized Jolt movement, rendering, vehicle-contact bodies, attacks, projectile hits, reticle obstruction, placement and mounted/corpse interactions. Retained roofs separate surface animals from underground players and prey. All 15 species fall and settle on an excavated floor; a tiger/rider moves underground and dismounts onto supported ground, while insufficient rider headroom rejects mounting. Placed support, block removal, an underground obstacle detour, corpse carry/drop and F5 resurface/re-entry falls are verified. Original weapon pickup markers and collection share builder support height, with vertical overlap and 3D obstruction checks preventing collection through roofs or blocks. All 37 CTest suites passed in 281.19 seconds, including the existing normal wildlife regression; five final native DX11 views were visually inspected. The matching verified root/runtime executables preserve the root save. Evidence and reproduction are in `evidence/builder-wildlife-20261006/` and `tools/verify_builder_wildlife.ps1`. These controlled fixtures do not prove dense wildlife performance, complex routing or persistence of animal AI/falling state across restart; existing approximate hit/reticle silhouettes also need broader review.
- Pedestrian loot/carry now use actual live-ragdoll or captured-pose height, vertical overlap and 3D obstruction; a falling corpse remains reachable on its current floor rather than its old death-capsule height. Builder releases validate a lying-body volume against real static/dynamic Jolt collision and create a fallen ragdoll at the player's height. Tests cover roof separation, single loot, excavated/placed support, blocked-release retry, captured pickup/drop and carried F5 transitions, with corpse cash/health/loot/timer state retained. Pickpocket/talk, regular player melee and vehicle repair share actual actor reach checks. Armed retreat/approach/strafe destinations use supported routes; edits invalidate endpoints and an elevated threat cannot overwrite the selected floor. Builder interaction/drop hints appear above the hotbar; carrying hides equipped models and cancels mining/placement without changing inventory. All 38 CTest suites passed in 284.57 seconds; five final DX11 views were visually inspected, and matching verified root/runtime executables preserve the root save. Evidence and reproduction are in `evidence/builder-interactions-20261006/` and `tools/verify_builder_interactions.ps1`. The initial run's transition/timing failures and an obsolete cover assertion are retained with the rechecks; deadlines were unchanged. These controlled cases do not prove dense combat fairness/performance, all height consumers or corpse/AI persistence across restart.
- **Still open:** continuous exploratory progression and harvest-tier/balance tuning; separate stealth/witness combat checks, vehicle road pursuit, static-world interactions and broader actor/height-query integration; complex wildlife routing/targeting and behavior beyond streaming budgets; face/collider merging, exceptional collision/actor reconstruction rollback and interrupted-save recovery, and dense-build performance/interactive validation. Full-detail wood companions and repeated scenery cuts still need dense-world memory/performance checks; subjective audio review remains open. The whole plan remains incomplete until these are integrated and verified.

Evidence and verification notes are retained in `evidence/builder-20261006/`, `evidence/excavation-20261006/`, `evidence/scenery-20261006/`, `evidence/tool-work-20261006/`, `evidence/builder-feedback-20261006/`, `evidence/builder-recovery-20261006/`, `evidence/builder-transition-20261006/`, and `evidence/builder-materials-20261006/`. The earlier nine builder/excavation views, four scenery views, four tool-work views, nine feedback views, three recovery views, and four transition views were visually inspected. The material follow-up passed all 35 CTest suites in 264.53 seconds and reviewed 23 native views (20 through labeled material/icon crops, three as complete gallery views). The root `MiniCity3D.exe` matches the verified runtime build, and the user's root save was preserved.

## Current checkpoint: builder performance follow-up (2026-10-07)

The earlier wildlife HUD stall remains fixed. The DX11 follow-up caches exact static navigation floors across frames with live-collision invalidation, keeps route and moving-footprint histories separate, reuses clearance/support checks within searches, filters compatible wander floors before pathfinding, and staggers failed or budget-deferred wander retries. Pickup collection checks now reject distant or occupied-player candidates before builder visibility work. Full route searches, nearby obstruction/height checks, live Jolt movement and world-wide pickup respawn timers remain active.

Matched 120-frame generated-city 1080p samples at the user's identical graphics settings on the Radeon 680M improve builder simulation from 34.65 to 10.86 ms and total frame time from 73.89 to 40.17 ms (about 13.5 to 24.9 FPS). p95 improves from 106.40 to 55.01 ms. The final nine affected suites pass, including pits/tunnels, moving crowds, police, interactions, ordinary navigation and pickup/autosave checks. The initial full 39-suite run passed 38; an obsolete navigation fixture lacked a legal concealed police spawn and was corrected and rechecked. A tighter search cutoff was rejected by the pit regression and removed. The verified root executable is published, with the latest root save preserved and the runtime save restored. Evidence and exact reproduction are in `evidence/builder-performance-20261007/` and `tools/verify_builder_performance.ps1`.

Helper-level builder stealth/witness reach, supported police dispatch and armed route-budget retry scenarios are integrated and tested. Actual builder F/G/K keyboard dispatch is still consumed by the builder key handler and needs integration; direct-helper previews do not verify these keys. Dense construction, repeated excavation, continuous travel and wider hardware performance, the broader remaining requirements above and the full plan remain incomplete. This checkpoint covers the requested performance correction.

## Goal and agreed F5 behavior

Add a persistent Minecraft-style building layer on the existing map. Players can mine the existing world, collect resources, store them, and place textured blocks.

The requested item expansion includes real textured 3D pickaxes, axes, shovels, and hoes in wood, stone, iron, gold, and diamond variants, plus shears and a brush: at least 22 usable tools. Each needs a recognizable model with volume, textured handles and working parts, rather than an inventory icon alone. Equip them from the hotbar, show their models and harvesting animations in the player's hand, and use them to destroy suitable sections of the existing world. Axes harvest existing trees and wood; shovels excavate ground materials; pickaxes mine rocks, ores, and masonry. Tools and collected materials belong in the inventory and chests, with matching icons and persistent durability. The detailed tool catalog, model requirements, and acceptance checks below are required parts of this plan.

F5 switches between two world states:

- **Normal mode:** builder-placed blocks disappear, builder-mined buildings, trees, rocks, and terrain return, and normal-world collision is restored. The normal world retains its own damage and saved state.
- **Building mode:** saved builder blocks, destroyed scenery, excavations, chests, and their corresponding collision reappear.
- Builder inventory, tools, dropped resources, and chest contents persist between visits and are available in building mode.

Builder changes must not leak into normal-world rendering, collision, camera obstruction, projectile hits, grapple anchors, or navigation. Damage produced by existing gameplay systems while building mode is active belongs to the builder layer. Mode switching is not a rollback of the entire gameplay simulation: it swaps the mode-specific editable world state and reconciles actors with the incoming geometry.

Implement and verify rendering features in the Direct3D 11 `MiniCity3D` target. Leave `renderer.cpp`, `textures.cpp`, and `MiniCity3DGL` unchanged. Keep the existing Jolt physics engine, simulation timing, vehicle handling, ragdolls, and weather.

## Existing foundation and constraints

- The game already has Jolt physics, destructible trees, and building holes with updated collision.
- Tree destruction already persists in saves. Building damage needs explicit persistence for each mode; the current builder implementation now saves these independent edit records.
- Terrain uses a heightfield. Below-surface excavation requires local voxel geometry, replacement collision, and updates to consumers that currently assume one ground height.
- Some scanned rocks are decorative and need mining geometry and collision.
- The current bounded building subtraction needs adaptation for repeated block-sized cuts and fragmentation growth.

## Mode switching and loading screen

F5 toggles while on foot. Building mode selects a first-person building camera; exiting restores the prior normal-game camera.

A few seconds of loading is acceptable. Pause simulation and show a loading screen with progress reflecting completed work:

1. Capture the outgoing layer's changes and queue a coherent save snapshot.
2. Load the incoming world state and its persistent edits.
3. Rebuild affected rendering geometry, physics collision, and navigation.
4. Validate the player and nearby vehicles against the incoming geometry.
5. Move overlapping or unsupported actors to safe positions when necessary, restore input and camera state, and resume.

Keep input disabled during transition and clear held mining, placement, and firing inputs. If loading fails, retain or restore a coherent outgoing state and report the failure.

Define the normal and builder baseline relationship before implementation: each mode must retain its own saved scenery state without later changes in one mode silently overwriting the other. Stable generated world definitions and mode-specific edit records should provide deterministic reconstruction.

## Controls and placement

| Control | Building-mode behavior |
|---|---|
| Hold left mouse | Mine the targeted block or world section |
| Right mouse / hold right mouse | Place against the targeted face; repeat placement while held, or open a chest |
| Mouse wheel / 1–9 | Select a hotbar slot |
| E | Open or close inventory |
| Q | Drop one selected item |
| Shift + right mouse | Place against an interactive object |
| Hold Shift / hold Control | Crouch / sprint within building mode |
| C | Switch between first-person building and third-person inspection |
| Escape | Close inventory, then use the existing pause menu |
| F5 | Enter or exit building mode |

These bindings apply within building mode. Normal-game bindings retain their existing behavior.

Use a shared cube grid, highlighted target face, and transparent placement preview. Reject occupied cells, placement through walls, and blocks overlapping actors. Start with approximately five block lengths of reach and tune against the game's scale. Block size must be chosen relative to the existing player capsule and buildings.

Use the familiar interaction pattern documented in [Minecraft's official controls guide](https://www.minecraft.net/en-us/article/minecraft-controls).

## Inventory, storage, and HUD

- Provide 27 inventory storage slots and nine hotbar slots, initially allowing stacks of 64 building items.
- Support stack movement, stack splitting, single-item placement, and Shift-click transfers.
- Opening inventory releases the cursor and pauses this single-player game.
- Placeable chests initially provide 27 persistent slots. Breaking a chest drops its contents exactly once.
- Save inventory, hotbar selection, tool durability, chest contents, and dropped resources.
- Add the textured 3D tool catalog described below; assign tool suitability, harvest tier, speed, and durability through data. Tools occupy individual inventory slots and do not stack like building blocks.

The building HUD shows textured item icons, stack quantities, selected-slot highlight, held-item name, tool durability, target material, mining progress, cracking feedback, pickup notifications, and inventory-full feedback.

## Textured 3D tools and additional items

Provide actual modeled tools for destroying and harvesting the world, with distinct silhouettes, UV-mapped textures, and visible use animations. A HUD icon or flat sprite alone does not satisfy the tool-model requirement.

Prioritize the axe, shovel, and pickaxe in the first tool review: show their textured 3D models equipped in the hand, use them on an existing tree, ground section, and rock respectively, and display the collected resources in the inventory. Then extend the same complete interaction to all required variants, hoes, shears, and the brush. Include the tool models, their matching hotbar icons, and stored tools in the review so appearance and functionality can be assessed together.

Tool-driven destruction must operate on the original map: an axe removes targeted wooden sections, a shovel removes targeted ground cells, and a pickaxe removes targeted rock or masonry sections. Each successful action updates the visible cut, builder-layer collision, material drops, and tool durability together. Preserve the existing physics for surviving scenery and falling debris; leaving with F5 hides these cuts and restores normal-world physics, while re-entering restores the saved cuts and their collision.

The requested expanded item set is a required part of this plan: **at least 22 textured 3D harvesting tools** (five variants each of pickaxe, axe, shovel, and hoe, plus shears and a brush), alongside textured resource and storage/utility models. An axe must visibly chop the existing world and a shovel must visibly dig it; every tool family needs its own working action rather than only appearing in an item menu.

Treat the expanded tools as usable player equipment throughout the harvesting loop: obtain or craft the item, select it from the hotbar, see its textured model in the player's hand, destroy a suitable part of the existing world, collect the resulting material, and store both the material and tool. The HUD must identify the equipped tool, show its remaining durability, and explain whether it can harvest the current target. A generic cube with a tool picture on it does not count as a real tool model.

For review, retain a per-item catalog checklist covering all 22 tools: model and texture paths, matching icon, recipe, suitable world targets, expected drops, speed, harvest tier, durability, and save behavior. Include representative views of each family equipped, dropped, and in storage, plus a demonstration of its actual world-destruction or harvesting action. Newly modeled items remain pending until their gameplay behavior is verified.

### Requested equipment expansion and review order

Prioritize the requested axe and shovel alongside the pickaxe, then review the remaining Minecraft-inspired harvesting equipment:

1. **Axe:** show a real textured handle and blade in the player's hand, chop a block-sized section from an existing tree or wooden building, and collect the matching wood item.
2. **Shovel:** show a real textured handle and scoop, excavate existing ground, and collect its soil, sand, gravel, or snow item. The removed ground must change collision while builder mode is active.
3. **Pickaxe:** show a real textured handle and mining head, remove existing rock or masonry sections, and collect the appropriate material with the required harvest tier.
4. **Expanded equipment:** complete all five material variants of those three families and hoes, plus modeled shears and a brush, for the required minimum of 22 tools. Verify each item's defined action and material feedback.

Present these tools in the Minecraft-style inventory and hotbar with matching model-derived icons, names, material tiers, preferred targets, and durability. Tools and harvested resources must be obtainable, transferable to chests, and saved together with the world cuts. Review each loop with F5 exit/re-entry: the original world and physics return on exit, and the saved destruction, placed items, and builder equipment return on entry, including after restart.

| Tool | Main purpose | Visible action and material feedback |
|---|---|---|
| Pickaxe | Mine rocks, ores, brick, and concrete; suitable tool tiers are required for valuable ore drops | Repeated pick strikes, stone chips, cracks, and rock/metal impact sounds |
| Axe | Chop existing trunks, logs, planks, and wooden construction sections | Chopping swing, wood chips, and wood impact sounds |
| Shovel | Excavate soil, sand, gravel, and snow | Digging/scooping motion and material-colored particles |
| Hoe | Clear suitable vegetation and prepare the top soil cell | Short downward swing and a saved tilled-soil surface state; a complete farming system is separate future work |
| Shears | Harvest leaf blocks, bushes, and appropriate plant items without treating them as stone | Opening/closing blades, foliage clipping particles, and cutting sounds |
| Brush | Clear loose surface material and recover designated small resource deposits | Brushing motion and dust feedback; archaeology and loot puzzles are separate future work |

For pickaxes, axes, shovels, and hoes, provide **wood, stone, iron, gold, and diamond variants**: at least 20 tiered tool items, plus shears and a brush. Each variant must have its own textured material appearance, model/material reference, inventory icon, and independent tuning. Geometry can share a family base mesh where appropriate, but heads, handles, bindings, and material details must remain recognizable. Additional reinforced-alloy/endgame tiers can be introduced later if supported by the resource catalog.

Required tool and item deliverables:

The minimum equipment catalog for this request is explicit below. Each listed variant is a separate usable inventory item with a textured 3D model, matching icon, recipe, and saved durability.

| Family | Required variants | Count | Existing-world harvesting target |
|---|---|---:|---|
| Pickaxe | Wood, stone, iron, gold, diamond | 5 | Rock, ore, and masonry sections |
| Axe | Wood, stone, iron, gold, diamond | 5 | Tree trunks and wooden construction sections |
| Shovel | Wood, stone, iron, gold, diamond | 5 | Soil, sand, gravel, and snow sections |
| Hoe | Wood, stone, iron, gold, diamond | 5 | Suitable vegetation and exposed soil |
| Shears | Metal shears with moving blades | 1 | Leaves, bushes, and designated plants |
| Brush | Bound bristle brush | 1 | Designated loose surface deposits |
| **Total** | | **22** | |

For every tool, the same world-target classification must drive the HUD hint, mining speed, harvest eligibility, contact feedback, and resulting drop. Apply it consistently to original scenery and player-placed materials. Minecraft documents separate matching-tool and minimum-harvest-tier tags; use that separation as a design reference for this game's item data. [Official Minecraft tool-tag reference](https://feedback.minecraft.net/hc/en-us/articles/360060771772-Minecraft-Java-Edition-Snapshot-21w19a).

- **Five pickaxes:** wooden, stone, iron, gold, and diamond, for harvesting the existing rock deposits and construction materials as well as placed blocks.
- **Five axes:** wooden, stone, iron, gold, and diamond, for chopping existing trees and wooden world sections.
- **Five shovels:** wooden, stone, iron, gold, and diamond, for digging the existing ground and loose materials.
- **Five hoes, shears, and a brush:** modeled and textured, with the vegetation, soil, and deposit actions specified above.
- **Matching resource and utility models:** logs, planks, sticks, rock blocks, ores, processed metals, diamonds, chests, crafting benches, furnaces, and torches. Collected and dropped items must visually match their inventory icons and placed versions where applicable.
- **Harvested vegetation and ground items:** leaf blocks, designated bush/plant items, soil, sand, gravel, and snow need recognizable textures and matching dropped, inventory, and placeable models. Preserve the source material identity when harvesting existing scenery instead of converting every tree or ground section into one generic item.

Selecting a tool in the hotbar must visibly equip its actual 3D model. Holding the mining input must animate that tool against the targeted world section and apply the appropriate harvesting rule. Asset availability alone is insufficient: each required tool needs a verified gameplay use, pickup/storage behavior, and saved durability where applicable.

### Required model shapes and textures

Each family needs recognizable modeled parts, visible from the player's hand and when dropped in the world:

| Family | Required 3D shape | Texture details |
|---|---|---|
| Pickaxe | Long handle and a crosswise pointed mining head | Wood grain, head material, and attachment/binding detail |
| Axe | Handle and a broad chopping blade with a visible cutting edge | Grain on the handle and stone, metal, or diamond detail on the head |
| Shovel | Handle and a shaped digging blade with depth | Blade material, handle grain, and a distinct blade edge |
| Hoe | Handle and an angled soil-working head | Head material and readable separation between head and handle |
| Shears | Two blades, a pivot, and finger handles | Metal surfaces and visible cutting edges; blades open and close during use |
| Brush | Grip, binding, and a bristle cluster with volume | Handle grain, binding detail, and contrasting bristle texture |

Wood, stone, iron, gold, and diamond variants must be distinguishable in the hand, inventory, and dropped-item view. Keep the material appearance consistent across tool heads, collected resources, and matching blocks. Render inventory/hotbar icons from the textured models. Inventory tooltips show the tool family, material tier, preferred targets, and remaining durability.

Review the expanded items through complete gameplay loops: axe → existing tree → logs → chest; shovel → existing ground → matching resource → placed block; pickaxe → existing rock/ore → resource → crafted tool. Record the resulting world cuts and resource changes together. Exiting with F5 hides those cuts and their changed physics; re-entering restores them, including after a restart.

### Tool interaction and review checklist

The expanded item set must work on the existing world as well as player-placed blocks. Define each item's model, texture, icon, preferred targets, mining speed, harvest tier, durability, recipe, and resulting drops in the item catalog.

- **Axe demonstration:** equip a textured axe, chop a section of an existing tree, collect its logs, and store them in the inventory or a chest.
- **Shovel demonstration:** equip a textured shovel, dig a section of existing soil, sand, gravel, or snow, collect the matching resource, and place it elsewhere.
- **Pickaxe demonstration:** equip a textured pickaxe, mine existing rock or masonry, and verify that protected ore drops require the appropriate tool tier.
- **Other tool demonstrations:** show shears harvesting designated foliage, a hoe preparing soil, and a brush recovering a designated surface deposit, each with its own modeled action.
- **Appearance review:** inspect every tool family in the first-person hand, on the third-person character, as a dropped world item, and in the inventory/hotbar. Textures and silhouettes must make both the tool family and material tier recognizable.
- **Persistence review:** save after harvesting and after storing tools; restart and check resources, chest contents, tool durability, and world edits. F5 must hide these builder edits and their physics, then restore them on re-entry.
- **Catalog review:** enumerate all 22 required tools and their textures/models, recipes, targets, and drops. Demonstrate each family on suitable existing scenery and check that variants differ in the intended speed, harvest tier, and durability. Missing actions remain unfinished even when their model assets exist.
- **Per-item completion:** verify all 22 tools individually, including every wood/stone/iron/gold/diamond variant. Equip the textured 3D model, use it on a suitable existing-world target, confirm the expected cut and resource yield, then drop, retrieve, store, save, and reload the tool with its remaining durability. Check that its model and texture match the hotbar/inventory icon. Model-file presence or a demonstration of only one variant does not complete the expanded tool requirement.

Left-click with an equipped tool applies its harvesting action to the highlighted target. Right-click with a placeable item places it against that face; right-click with a tool performs only its defined secondary action, if any. Selecting an axe or shovel must not place a tool-shaped block or trigger normal-game weapon firing.

Define the hoe's soil preparation on right-click and the brush's deposit-clearing action on held right-click, with visible progress and cancellation when the button is released. These actions save their surface/deposit state in the builder layer and follow the same F5 isolation rule as mined blocks.

Model and texture requirements:

- Use original or appropriately licensed authored 3D assets with real heads, handles, blades, and bindings. Retain source files, licenses, hashes, import settings, and reproducible mesh processing.
- Import through the existing baked-model pipeline. Supply base-color textures and normal/material detail where the DX11 path supports it, with mipmaps and coherent scale.
- Render the equipped model in the first-person building view. Attach the same tool asset to the character's hand for third-person/debug views, using the existing skeletal attachment system.
- Render dropped tools as textured world models with bounded pickup collision; inventory and hotbar icons should be rendered from those same assets so they match.
- Synchronize swing/contact feedback with mining progress. Releasing the mouse, changing targets, opening inventory, or switching modes cancels mining cleanly without free damage or duplicated yields.
- Give each family a readable use animation: chopping for axes, striking for pickaxes, digging for shovels, soil-working for hoes, opening/closing blades for shears, and brushing strokes for brushes. Contact particles and sound should match the targeted material, and successful harvesting must leave a visible saved cut in that world section.
- Reuse the existing shovel asset and animation when their visual quality and attachment are suitable; add builder-specific harvesting behavior without changing its normal-game behavior.

Tool behavior and progression:

- Tune preferred material, minimum harvest tier, mining speed, durability, and repair/crafting costs independently. Wrong tools are slower and may fail to yield protected ores; show the reason in the target HUD.
- Wood provides an entry tier, stone a stronger early tier, iron a general-purpose advanced tier, and diamond a durable high tier. Gold can be fast but fragile; tier ordering must not force every statistic to increase together.
- Save each tool instance's type, material tier, and remaining durability. Display durability in inventory and the hotbar, and remove or mark a broken tool consistently.
- Allow an initial wood-gathering loop by hand, then data-defined recipes for planks, sticks, and wooden tools. Add a textured 3D crafting bench for advanced tool recipes and a simple defined route from metal ore to usable crafting material. Exact processing recipes and balance are implementation decisions.
- Include iron/gold ore and their processed crafting items alongside diamond resources so modeled tiers are obtainable through gameplay rather than only a debug menu.
- Add textured 3D torches as placeable utility items and give the existing chest plan a real textured model. Torches use the existing local-light system with a capped nearby-light budget; no fluid or farming engine replacement is implied by the tool expansion.
- Keep all builder tool acquisition, use, recipes, placed utility items, and resources within the persistent builder layer. Returning to normal mode hides builder items and their added collision/light effects.

Official Minecraft references for the interaction roles and tool variants:

- [Minecraft: Taking Inventory — Axe](https://www.minecraft.net/en-us/article/taking-inventory--axe)
- [Minecraft: Taking Inventory — Shovel](https://www.minecraft.net/en-us/article/taking-inventory-shovel)
- [Minecraft: Taking Inventory — Shears](https://www.minecraft.net/en-us/article/taking-inventory--shears)
- [Minecraft: Taking Inventory — Hoe](https://www.minecraft.net/en-us/article/taking-inventory--hoe)
- [Minecraft: Taking Inventory — Brush](https://www.minecraft.net/en-us/article/brush)
- [Microsoft Minecraft item catalog](https://learn.microsoft.com/en-us/minecraft/creator/commands/enums/item?view=minecraft-bedrock-stable)

The proposed tool statistics and world-harvesting rules are this game's design; they are not a claim to reproduce Minecraft's exact balance.

## Material catalog and research

Provide at least 20 distinct rock types. Each receives a recognizable texture, mining resistance, blast resistance, preferred tool, and resource distribution. Distinguish grain, layering, pores, and veins rather than merely recoloring one texture.

The following HP values are proposed **gameplay durability**, not measured geological strength. Tune them through play checks.

| Rock | Proposed block HP | Texture character |
|---|---:|---|
| Chalk | 40 | Pale, powdery |
| Mudstone | 50 | Fine brown layers |
| Shale | 60 | Dark, thin layers |
| Tuff | 70 | Ashy fragments |
| Pumice | 80 | Light, porous |
| Sandstone | 90 | Sandy grains |
| Limestone | 100 | Cream, mottled |
| Travertine | 110 | Banded, pitted |
| Dolostone | 120 | Buff crystalline grain |
| Conglomerate | 130 | Embedded rounded pebbles |
| Slate | 140 | Dark cleavage lines |
| Marble | 150 | Light stone with veins |
| Schist | 165 | Sparkling, layered |
| Gneiss | 180 | Alternating mineral bands |
| Andesite | 195 | Fine gray speckles |
| Granite | 210 | Coarse multicolor crystals |
| Diorite | 225 | Black-and-white crystals |
| Gabbro | 240 | Dark coarse crystals |
| Basalt | 255 | Dark, fine volcanic grain |
| Quartzite | 280 | Dense, sparkling grain |

Additional materials include diamond ore and diamond items, iron/gold ore and processed crafting materials, wood logs, planks, sticks, leaves, soil, sand, gravel, snow, brick, concrete, and glass. Diamond is a mineral and is additional to the 20-rock requirement.

Mohs hardness measures resistance to scratching. Rock strength also depends on composition, fractures, and weathering. Diamond's Mohs rating of 10 must not automatically make a diamond block indestructible. Keep mining resistance separate from impact and blast resistance.

Research sources:

- [USGS mineral hardness and diamond](https://pubs.usgs.gov/gip/gemstones/mineral.html)
- [USGS: rocks and minerals](https://www.usgs.gov/faqs/what-difference-between-a-rock-and-a-mineral)
- [BGS rock classification](https://www.bgs.ac.uk/technologies/bgs-rock-classification-scheme/)
- [BGS rock strength and weathering](https://www.bgs.ac.uk/datasets/bgs-civils-strength/)

Use suitable CC0 texture assets from [Poly Haven](https://polyhaven.com/license) and [ambientCG](https://docs.ambientcg.com/license/), supplemented with original materials where needed. Exact asset selection remains implementation work. Retain source links, licenses, hashes, and reproducible processing in the asset manifest.

## Harvesting existing scenery

Create editable cells around scenery when it is mined, preserving untouched scenery's current appearance.

- **Buildings:** remove a block-sized section, reveal an interior surface, update collision, and drop the assigned construction material. Use facade geometry to guide conversion so empty space does not become harvestable stone.
- **Trees and plants:** use axes to harvest trunk sections into logs and shears to harvest suitable foliage; hoes clear designated vegetation. Integrate with existing tree destruction and falling fragments, and add explicit bush harvest state where currently decorative.
- **Boulders and cliffs:** assign rock materials, add mining collision where needed, and remove local sections.
- **Terrain:** introduce local voxel patches for surface and below-surface digging. Replace corresponding rendering and collision patches so an invisible heightfield cannot obstruct an excavation.

Assign materials deterministically by object and region. Buildings primarily yield masonry and wood; rocks and terrain provide geology-inspired deposits and rarer ores.

Every removed section has one resource record. Mining, explosions, and existing destruction consume that same record to prevent repeated harvesting and duplicate drops. Resource changes and world edits must be captured coherently in saves.

## Physics and rendering integration

- Placed blocks become static Jolt scenery in building mode. Existing loose debris remains dynamic and capped.
- Update cameras, projectiles, grapple anchors, pedestrians, and vehicles to recognize builder additions and removals only while that layer is active.
- Reconcile ground queries, load positioning, and movement assumptions with mined terrain and placed surfaces.
- Render and collide nearby chunks; rebuild only affected chunks.
- Merge compatible faces and colliders and use bounded update work to keep dense builds practical.
- Unload or deactivate builder geometry and collision on exit. Restore the normal world's geometry and collision before resuming simulation.

## Persistence

Save mode-specific world state and builder resources:

- Placed blocks and materials.
- Removed terrain, rocks, trees, and building sections.
- Building and other scenery damage, including damage caused through existing explosions.
- Builder inventory, hotbar selection, tools, dropped resources, and chest contents.
- Individual tool durability, tier, crafting/processing state, tilled soil, and placed utility items such as torches and crafting benches.
- Enough baseline/version information to reconstruct both modes deterministically.

Store edits against stable world IDs and chunk coordinates rather than saving a complete copy of the map. Use versioned snapshots with atomic replacement. If chunk data and player data use separate files, publish a single generation manifest only after all files for that generation are complete.

Load world edits before collision reconstruction and actor positioning. Import existing save versions 1–3 with empty builder state and an empty builder inventory. Switching out of builder mode must preserve its saved edits rather than discard them.

## Runnable milestones and acceptance checks

| Milestone | Required acceptance check |
|---|---|
| A. F5 layer foundation | Toggle modes with a loading screen; place and mine a textured block; walk on it; save and reload it. Exit and walk through its former location, then re-enter and collide with it again. |
| B. Inventory, storage, and starter tools | Verify hotbar, stack transfers, real textured 3D wood/stone pickaxes, axes, and shovels, matching icons, held/dropped models, tools, pickups, full-inventory behavior, modeled chest contents, mode isolation, and persistence across restart. |
| C. Existing-world harvesting | Mine trees, boulders, and building sections with the appropriate modeled tools and synchronized animations; obtain resources exactly once; save damage; verify originals return in normal mode and edits return in builder mode. |
| D. Full material, tool, and utility catalog | Verify all 20 rocks have distinct textures and resistance; all 20 tiered tools plus shears and a brush have textured models and functioning harvesting rules; crafting, durability, iron/gold/diamond resources, hoes, torches, and crafting benches are obtainable, usable, and persistent. |
| E. Terrain excavation | Dig a pit and tunnel, enter them, save and reload them, and verify camera, projectile, and Jolt collision. Verify normal terrain returns on exit and excavations return on entry. |
| F. Integration and performance | Run appropriate existing regressions, inspect DX11 captures, test repeated transitions and interrupted saves, and benchmark dense construction and repeated excavation. |

The first playable loop is: **F5 → mine → collect → place → walk on the block → F5 and verify it disappears physically → F5 and verify it returns → save and restart with builder edits intact.**

Final verification must also cover switching while standing on builder blocks or inside excavations, actor overlap with restored scenery, normal-world damage independence, no duplicate resource yields, and no builder collision leaking into normal mode. Verify tool/material suitability, harvest-tier failures, tool breakage and durability across restart, crafting resource conservation, model/texture loading and licenses, first-person hand placement, mining cancellation, and removal/restoration of builder tool and torch visuals during F5 transitions.

Roadmap status must remain factual. Mark each milestone complete only when its implementation is integrated and its acceptance checks are verified.

## Editor sizing and respawn update (2026-10-09)

The shared editor grid now measures 20 units for all placed blocks, placement previews and reticles, terrain excavation, scenery cuts, surface work, navigation and collision. First-person eye height is 40 units and the third-person editor camera is raised to 72. A live reticle/use scenario stacks two blocks and verifies their combined 40-unit collision height. Builder save format 2 imports format 1: saved blocks shrink while retaining their world-space corner positions and contents, and old removed volumes subdivide to preserve their extent. R after death preserves editor mode, inventory and edits. All 39 CTest suites pass (466.37 seconds), including the full earned-resource progression through 22 tools; narrower mining shafts use a 2x2-cell opening for capsule clearance, normal crafting for replacement tools, and Q for excess overburden. Evidence and native DX11 preview reproduction are in `evidence/editor-sizes-20261009/` and `tools/verify_editor_sizes.ps1`. The open roadmap work listed above remains open.
