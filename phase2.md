# Mini City 3D — phase 2 implementation plan

## Scope and ground rules

This is the plan for the requested expansion, not an implementation status report. Ship it as small, runnable milestones in the Direct3D 11 `MiniCity3D` target. Keep the existing OpenGL fallback, `renderer.cpp`, `textures.cpp`, and `MiniCity3DGL` untouched. At the end of every milestone, build the DX11 game, run the relevant deterministic smoke tests, launch it for a short human play check, and update `idea.md`/README status only for verified behavior.

Keep editable gameplay definitions in `./data/`; keep meshes, textures, sounds, icons, and their license records in `./assets/`. Neither a new renderer nor a one-time replacement of the entire existing game is required. Preserve the current six-mission loop throughout migration.

Only import new 3D models that meet a high visual-quality bar in the DX11 game at normal camera distances. Inspect geometry, textures/materials, scale, and LOD before integration; reject low-detail packs even when their license is suitable. Record provenance and license for every accepted asset.

## What exists now

| Area | Current state | Phase 2 work |
| --- | --- | --- |
| Content | `content.cpp` hardcodes buildings, pedestrian/vehicle spawns, pickups, and missions; `assets/weapons.ini` tunes five guns. | Move gameplay definitions and placements into validated `data/` files, with stable IDs. |
| Character | Jolt capsule movement and fall damage exist; pedestrians can have armor. | Swimming, climbing, consistent player armor/damage, contextual interactions, carryable bodies. |
| Combat | Five firearms, visible bullet spheres, pedestrian hits, impacts, ragdolls, and gunshot reactions exist. | Weapon categories and assets, sniper scope, drive-by fire, melee, fire/water, bow pinning, silencers, progression, distinct impact effects. |
| Vehicles | Jolt wheeled constraints and collision damage exist; the HUD displays condition and heavily damaged cars show smoke. | Separate health states, gun/RPG damage, explosions, repair, explicit brakes/handbrake, tuned drift scoring. Verify wheel behavior instead of describing it as absent. |
| Progression | Money is awarded by missions, shown in HUD, and saved. | Loot, theft, drift rewards, shops, house/garage ownership, wanted level, save migration. |
| World | A 2400 × 2200 fixed district, day/night cycle, four baked tree meshes, DX11 instancing/LOD, minimap. | Streamed larger map with several cities, biomes, weather, ≥20 distinct sourced tree models, map/UI changes. |

The existing save is version 1 and stores weapon state by five array indexes. New systems must migrate this save; silently resetting cash, unlocks, or mission completion is unacceptable.

## Working interpretations of unclear requests

These are implementation defaults to review before the affected milestone. They keep the plan concrete without claiming that the ambiguous wording is already settled.

1. **“50x bigger” means at least 50 times the current playable map area**, not 50 times each axis. The current rectangular bounds are 5.28 million square world units, so the new traversable region should exceed 264 million square units, with enough connected land/roads to make the increase useful. Measure playable area as well as bounding-box area. Build through streamed regions instead of allocating a 50x world at startup.
2. **“FPS/RTS” means camera views**, not a real-time-strategy command system. `C` cycles narrow/close first person, wide/far first person, near third person, far third person, and a high overview view. “Closer/further FPS” changes field of view rather than moving the camera outside the player's head. The overview still controls the same player. Sniper scope and telescope are separate zoom modes. First person uses a genuine eye position and collision/occlusion handling.
3. **“Double handed” means dual wielding** a weapon after 100 kills of armed opponents with that weapon. It applies only to weapons that can sensibly be held one in each hand. Total fire rate doubles, ammo use doubles, each hand has a muzzle and animation, and the bonus does not stack. Sniper, RPG, bow, melee weapons, and tools retain their own progression behavior unless a later decision expands the rule.
4. **Wanted stars escalate police response**, rather than turning every civilian into a gunman. Armed civilians may defend themselves under the existing AI rules. Four stars is the cap; response count, equipment, tactics, and pursuit range increase at each star.
5. **Interaction keys need one consistent rule.** `E` enters/exits vehicles; `F` uses the nearest clearly prompted context action (mission, loot, theft, shop, ladder, house, or fast travel). `G` grabs/drops a body. When several `F` actions overlap, show a small selection prompt rather than silently choosing the wrong one. Theft requires being behind a living pedestrian; loot requires a dead body.
6. **Drive-by input:** driving keeps W/S throttle and A/D steering. Mouse movement can orbit the vehicle camera. Holding left mouse draws/aims a compatible equipped gun and fires at its normal cadence; release stops firing. The camera, reticle, passenger-side firing arc, and muzzle trace must agree. The boat/bike behavior is defined and tested separately.
7. **“Jump + LMB with no aiming” means an unarmed aerial strike** when Space and left mouse are used without right-mouse aim; ordinary unarmed LMB is a ground punch. This must not accidentally fire an equipped gun.
8. **Arrows pin on a valid wall hit.** A shot that strikes a character and then a suitable static wall can pin the defeated ragdoll for a limited time. Living characters take damage/knockback; no permanent constraint is created on moving vehicles, foliage, or doors.
9. **“Fire spreads a little on metal” means weak, short-lived flame on combustible residue or nearby objects**, not self-propagating solid steel. Grass/dry vegetation spread fastest; wet surfaces, water, rain, and suppression reduce spread. This gives each surface a material and moisture value with predictable behavior.
10. **Telescope is an observation tool**, selectable like equipment, with smooth zoom and no firing. Sniper wheel zoom works only while scoped; the ordinary wheel can change camera distance or weapon selection after the input map is finalized.

## Architecture decisions before feature work

- Create a versioned `data/` schema with explicit IDs for weapons, vehicles, items, shops, houses, surface types, biomes, tree species, police tiers, missions, region templates, and hand-placed instances. Move `assets/weapons.ini` to `data/weapons.*` through a deliberate migration; do not keep two sources of truth. Keep art paths relative to `assets/` and reject missing references in tests.
- Separate immutable definitions from saveable runtime instances. Every placed car, shop, house, ladder, pickup, tree, and pedestrian spawn gets a stable ID. Save owned/destroyed/looted state by ID rather than vector position. Define deterministic seeds for generated region content.
- Use a shared event/damage pipeline: source, target, hit point/normal, damage type, weapon ID, instigator, witness context, and surface. Player, pedestrians, cars, trees, props, and fire can respond consistently. Armor absorbs eligible damage before health; record special rules for fall, drowning, fire, explosions, and collision.
- Use one interaction resolver with distance, view angle, line of sight, and action priority. The HUD displays the exact action and key. Interaction and purchases are single transactions so holding a key cannot duplicate rewards.
- Use one projectile/effect description for bullets, rockets, arrows, flame, water jets, decals, and trails. Keep simulation hits authoritative; graphics are attached to actual hit positions and have a bounded lifetime and count.
- Keep settings, saves, and packaged resource data distinct. Add a versioned save migration from version 1 and a recovery path for missing/invalid optional content. A packaged game must run offline.

## Milestones

### 1. Data and save foundation

Move existing weapon stats, placed vehicles, pickups, and mission definitions from C++/`assets/weapons.ini` into `data/`; define schemas for later shop/house locations, item prices, surface rules, and region metadata there as well. The loader validates required fields, ranges, unique IDs, art references, and cross references, then reports exact file/entry errors. Preserve existing gameplay values during migration. Replace fixed five-weapon arrays with ID-based inventory while keeping the five current weapons working. Update CMake/build/package scripts to copy `data/` beside the executable. Implement a version 2 save that imports version 1 money, mission flags, health, position, ammo, and unlocks.

**Done when:** edits to `data/` change placements/stats without recompilation; bad data fails clearly; the existing six missions and a version 1 save still work in the packaged DX11 game. Automated tests load every definition and test migration/round-trip saves.

### 2. Shared health, interaction, and economy

Add visible player armor, a single damage calculation, and consistent armor-first handling for bullet/impact damage. Keep fall damage from the Jolt landing speed, tune thresholds, and test high/low falls and water landings. Give pedestrians persistent death/loot states and randomized data-driven cash, with armed pedestrians tending to carry more. `F` loots once. Add behind-the-back pickpocketing with success/recognition odds based on distance, facing, awareness, and skill; recognition triggers a specific reaction/possible attack. `G` carries/drops a dead body with movement/weapon limits and Jolt-safe attachment. Add cash debit/credit transactions and save them.

**Done when:** armor drains before health where specified; a high fall can injure the player; the same corpse cannot pay twice; successful and detected theft behave differently; carrying survives movement and drops cleanly; saving/loading cannot duplicate cash or corpses.

### 3. Cameras, aiming, and weapon presentation

Add the `C` camera cycle, narrow/wide first-person FOV presets, adjustable near/far third-person distance, freely rotatable car camera, and proper first-person occlusion. Add sniper aim transition into a scope overlay with crosshair and a camera FOV derived from selectable magnification steps (for example 2×, 4×, 8×); mouse wheel changes magnification only while scoped. A telescope uses the same safe zoom/collision path without weapon fire. Implement drive-by aiming/fire for eligible weapons with camera-ray-to-muzzle correction. Add a small icon for each selected weapon in the upper-right HUD. Keep on-foot right-mouse aim and reload behavior intact.

**Done when:** camera modes do not clip through nearby walls, scoped center matches the actual impact point at near/far range, wheel zoom is bounded, car orbit remains controllable while steering, and a drive-by shot visibly starts at the correct muzzle and hits where aimed.

### 4. Vehicle handling, damage, explosions, and repair

Give each vehicle definition mass, torque curve, gear/acceleration response, brake and handbrake forces, steering curve, tire grip, suspension, collision thresholds, max health, smoke threshold, and explosion rules. Use the existing Jolt wheeled constraint path as the base and remove contradictory duplicate tuning where verified. Apply damage from bullets, rockets, fire, and collisions to one vehicle health value. Implement the initial data-defined RPG projectile here so the vehicle explosion loop is playable; milestone 5 completes its presentation and inventory integration. States are intact → smoking/repairable → disabled/burning → exploded; car health reaching zero triggers an explosion, and an RPG direct hit can cause an immediate explosion. An explosion damages nearby people, cars, and props and leaves a non-drivable wreck. A repair tool works only before explosion, costs a defined resource or money, and cannot restore a wreck. Add measurable drift detection using speed, slip angle, duration, control/recovery, and cooldown; pay once per qualifying drift segment and reject spin-in-place farming.

**Done when:** sedan/sports car/bike have distinct acceleration and braking, handbrake permits controllable drift, collision and gunfire both reduce condition, repair works only for a smoking non-exploded car, and a direct RPG destroys a test car. Drift rewards are reproducible and non-farmable in deterministic tests.

### 5. Expanded combat and projectile feedback

Add data-driven weapon categories and per-weapon damage/range/rate/reload/ammo/impulse/noise/price/allowed camera/drive-by stats. Include RPG, bow, flamethrower, fire extinguisher, water cannon, silenced firearms, katana, knife, machete, adult novelty melee item, rolling pin, and bat. Give every weapon a distinct pickup/HUD icon, held model, animation or pose, sound, and hit behavior. Implement basic ignition, wetness, and water-impulse targets for the three fire/water tools here; milestone 6 adds sustained material-aware spread and tree destruction. Track armed-opponent kills by weapon ID in the save; at 100 qualifying kills unlock dual wield for eligible weapons, with two visible hands/muzzles, doubled aggregate fire rate, and doubled ammo consumption. Add ground punch and aerial strike. Make gun bullets, rockets, and arrows readable in flight at gameplay speed (tracers/smoke or arrow mesh where necessary), with surface-specific bullet impact flashes, sparks, dust, and persistent short decals. Use continuous collision/swept tests so fast rockets and bullets do not tunnel. Arrow pinning uses the ragdoll/static geometry conditions above.

**Done when:** every listed weapon can be acquired, equipped, used, and saved; per-weapon stats come from `data/`; bullets/rockets/arrows and impacts are visible; melee cannot shoot; suppressors reduce audible witness radius; the 100th qualifying armed-opponent kill unlocks eligible dual wield with twice the total shot cadence and ammunition cost; arrow wall pinning releases safely when the body despawns.

### 6. Surfaces, fire, vegetation damage, and water

Assign semantic material types to terrain, roads, buildings, props, cars, and trees, independent of visual PBR texture. Use the material table for bullet impacts, friction, footsteps, flammability, ignition temperature, burn rate, and wetness. Simulate fire on a bounded spatial grid or active-cell graph at fixed ticks, with wind-biased spread, faster spread on dry grass, slower spread on wood, and minimal spread on metal. Trees ignite, lose health slowly, show staged burning, then fall/collapse or become charred stumps. Flamethrower/rockets/lightning or scripted fire can ignite; extinguisher, water cannon, rain, and water surfaces suppress and add wetness. Water cannon applies a bounded Jolt impulse/knockdown to characters and extinguishes fire; test it on players and pedestrians. Limit active fires, effects, and smoke for frame time and save only durable world changes.

**Done when:** a grass fire spreads beyond its start, wet grass resists it, bare metal does not sustain a large wildfire, a burning tree is eventually destroyed, and both suppression tools put out fire. Water cannon causes a clear fall reaction without unstable ragdoll physics.

### 7. Crime, witnesses, and police

Add crime events for assault, killing, theft, car theft, arson, and attacks on police. Witness checks use line of sight, distance, sound/noise, and a short report delay; a silenced kill with no surviving witness produces no wanted level. A visible gun threat makes unarmed pedestrians immediately flee before firing. Wanted level rises and decays from 0 to 4 stars with clearly defined rules and map/HUD display. Police spawn from valid routes/locations, investigate, chase, take cover, and fire with tier-specific data-defined weapons and accuracy; each star raises threat. Avoid endless spawning in view, invisible evidence, or automatic reporting by a dead lone witness. Police vehicles are optional only after foot response works.

**Done when:** an observed killing raises wanted level and brings police; an unwitnessed silent kill does not; pointing a gun triggers panic; each star changes enemy loadout/pressure; wanted level can be lowered at a shop; load/save preserves expected wanted state.

### 8. Swimming, climbing, and vertical traversal

Add a water-volume state to the player controller: entry/exit, floating, surface swim, optional dive, stamina/air, drowning, shore re-entry, and boat exit safety. Keep the existing boat system. Place ladders on selected tall building exteriors with valid top/bottom dismount points and collision checks; climbing locks to the ladder and permits safe exit at the roof. Add climbable trees with authored trunk/branch anchors for selected species rather than claiming all trees are climbable. Make fall damage account for ladder jumps, tree falls, and water landings.

**Done when:** a player can swim from shore and climb back out, survive a reasonable water landing, climb a marked tall building to its roof, climb a marked tree, and cannot pass through walls or get trapped at exits.

### 9. Shops, houses, garages, and fast travel

Mark shops and owned houses on the minimap/full map. Shops list stock and price from `data/`: all obtainable weapons, ammunition, health, armor, wanted-level reduction, repair supplies, and vehicles. Buying is atomic and respects cash and ownership. Allow up to ten houses; each has a purchase price, safe spawn/fast-travel point, and finite garage slots. Save the identity, condition, upgrades, and location of parked owned vehicles. Fast travel works between owned houses when out of combat/mission and not driving/carrying a body; place the player and streamed world safely. Define a recovery rule for destroyed stored cars that does not duplicate them.

**Done when:** map icons lead to usable shops/houses, cash is never overdrawn, the eleventh house cannot be bought, a parked owned vehicle survives save/load and fast travel, and purchases cannot be duplicated by repeated input.

### 10. Large streamed world, biomes, weather, and assets

Replace the single fixed city rectangle with a region graph and streamed terrain/props/collision/AI. Target more than 50× current playable area, with at least two distinct large cities joined by a drivable bridge, countryside, snowy region, savanna, and Sahara-like desert. Define coherent roads and waterways, biome transitions, pedestrian/traffic population rules, missions/shops/houses, and long-range map navigation. Keep a persistent main-thread-safe spatial index and stable content IDs; stream visible/collision areas around the player, and use distant impostors/LOD, instancing, and view-distance settings to control memory and draw cost. Save region changes and restore them on revisit. Add weather states (clear, overcast, rain, snow, wind), moving cloud cover, precipitation, visibility, and wind-driven tree sway; biome/weather values also affect fire and traction. Add stars visible near the moon at night, with reproducible placement and no city-light overload.

Source a broad set of appropriately licensed models and sounds for cities, shops, props, weapons, vehicles, and biomes. For trees, integrate **at least 20 visually distinct licensed 3D tree models**, not merely recolors, across temperate, conifer, tropical, savanna, and desert groups. Record each model's source URL, author, exact license, download date, and any required attribution in an asset manifest; run it through the existing conversion/LOD pipeline and include source/derived files and license text in packages. Candidate primary catalogs to evaluate are [Kenney Nature Kit](https://kenney.nl/assets/nature-kit) (CC0), [Quaternius Ultimate Nature Pack](https://quaternius.com/packs/ultimatenature.html) (CC0), [Kenney City Kit Suburban](https://kenney.nl/assets/city-kit-suburban) (CC0), and [Kenney Car Kit](https://kenney.nl/assets/car-kit) (CC0). Their pages establish pack size/license, not that they already contain 20 suitable distinct trees; verify contents before counting them.

**Done when:** measured traversable area meets the 50× target; both cities and every biome are reachable without teleporting; a bridge and region boundaries work with cars, AI, saves, and physics; weather visibly affects clouds, trees, fire, and grip; 20 tree models are actually loaded and rendered; draw/LOD settings change distance as advertised; the packaged build runs offline with all assets and license records.

### 11. Integration and release checks

Complete a clean-machine human playthrough of the existing missions plus representative new actions: drive-by, sniper scope, car repair/explosion, fire suppression, theft/loot, police escalation, swimming, ladder/tree climbing, shop purchase, house/garage save and fast travel, and cross-biome travel. Update control hints and settings pages, especially for `C`, wheel, brakes, `F`, `G`, and camera modes. Profile p95 frame time, simulation/physics time, draw calls, streamed memory, and loading hitches at 1080p in dense city, bridge, forest fire, and long view distance. Fix material regressions before calling a milestone complete. Package `data/` and `assets/`, test from the copied folder, and update `idea.md` with factual verified status.

## Test and performance gates shared by all milestones

- Run `./test.ps1`, the relevant new simulation/data tests, a DX11 launch, and package smoke tests after changing resource paths or save formats. Use fixed 60 Hz simulation tests for money transactions, damage, fire spread, wanted events, drift rewards, and state persistence.
- Test actual camera/reticle/muzzle alignment at different heights and against near walls. Test rocket/arrow collision at high speed and across frame-rate changes.
- Cap active police, ragdolls, fires, water particles, bullets, decals, tree debris, and streamed regions; expose counts in F3. Check frame time and memory in worst-case scenes, not only an empty street.
- Keep renderer material IDs separate from surface gameplay types, with a mapping validated at load time. A visual material change must not accidentally change flammability or traction.
- For any save migration or content schema change, load a version 1 fixture and a current fixture, then verify money, mission progress, inventory, house/vehicle ownership, and world changes survive a second save/load.
