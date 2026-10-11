# Gameplay data

The DX11 game loads these versioned runtime files from the `data/` directory beside the executable. Art and licenses remain in `assets/`. A malformed or missing required file stops startup with a file/section/key error in `MiniCity3D.log`.

| File | Contents |
| --- | --- |
| `weapons.ini` | Ordered catalog for 27 firearms, tools, melee weapons, bow, and traversal grapple, including dual-wield eligibility. The first five IDs remain fixed for version 1 save migration. |
| `vehicles.ini` | Handling, Jolt chassis/wheel tuning, surface and wet tire grip, durability, repair, and drift thresholds for each vehicle kind. |
| `surfaces.ini` | Grass, sand, asphalt, metal, wood, concrete, water, and snow fire spread and burn rules. |
| `police.ini` | Witness/report timing, response locations, and weapon/armor tiers for wanted levels 1–4. |
| `commerce.ini` | Stable-ID shops and houses across cities and biome hubs, item prices, vehicle stock, garage capacity, and fast travel locations. Fourteen houses are available; the player can own ten. |
| `traversal.ini` | Stable-ID downtown ladders, generated tall-building ladder thresholds, and climb anchors for selected trees. |
| `regions.ini` | The 16,800 × 16,800 world, seven regions, second-city layout, and four population hubs; counts are derived from explicit placement IDs. |
| `terrain.ini` | Landforms plus editable amplitudes, frequencies, masks and smoothing distances sampled into shared DX11/Jolt terrain. |
| `roads.ini` | Ten connected regional road segments through the bridge, countryside, snowfields, desert, and savanna. |
| `trees.ini` | Twenty-nine Kenney tree meshes assigned to countryside, snow, savanna, and desert biomes with render sizes and species climb heights. |
| `biome_props.ini` | Six Kenney cactus and rock meshes for desert decoration. |
| `weather.ini` | Timed clear, wind, overcast, rain, and snow states with cloud, visibility, wind, and precipitation tuning. |
| `world.ini` | Building/tree layout rules, a deterministic generation seed, pedestrians, traffic, stable-ID vehicle placements, and stable-ID weapon pickups. |
| `missions.lua` | Ten missions and all start, checkpoint, target, timeout, failure and completion decisions. IDs retain save compatibility. |
| `regional_placements.txt` | Explicit regional buildings, trees, decorations, pedestrians, vehicles, ground surfaces, scenery, street props and lights. |
| `geography.ini` | River, sea, bridge, causeway, bay, piers, promenade, downtown navigation roads and wildlife/flock habitat centers. |
| `props.ini` | Movable barrel/crate positions, health and placement filtering. |

INI schemas use `[Schema] Version=1` except `regions.ini` (3) and `terrain.ini` (2). Lua modules return `version=1`; the placement text header is `MINICITY_REGIONAL_PLACEMENTS 1`. IDs use lowercase letters, digits, hyphens, and underscores. `world.ini` and the Lua definitions are validated before replacing the live world. Lua, placement, prop and terrain parsers validate replacements before publishing them; their malformed-input tests verify that the previous catalog survives rejection. Save version 2 stores weapon inventory and armed-opponent kills, mission, owned house and parked vehicle, tree, pedestrian, armor, and repair-kit state by stable IDs; version 1 index-based saves are imported on load and rewritten as version 3 on the next save. Corpse records are neither written nor restored; dead pedestrians, animals, and birds respawn fresh on load.

The DX11 renderer loads the regional and species data, while some scenery rules remain procedural. `asset_smoke` checks all referenced tree and desert prop models against baked assets and the nature manifest. Regional tree records include their species, render dimensions and climb height. The 53.45× world uses local terrain tiles and cell indexes for nearby tree and desert decoration rendering and fire lookups; distant wandering pedestrians sleep and return to their regional homes after respawning. Jolt building colliders and pedestrian capsules stream near the player; full mutable region-state streaming remains open. Surface material rules cover regional grass, desert sand, snow, roads, and river water using a sparse active fire grid.

Grapple design references: the official [Halo equipment guide](https://support.halowaypoint.com/hc/en-us/articles/26608497528596-Equipment-in-Halo-Infinite) describes pull and cooldown mechanics, while the [Halo map design discussion](https://www.halowaypoint.com/news/echoes-within-maps-modes-preview) discusses momentum-based swings. [Just Cause developers](https://blog.playstation.com/2016/10/19/just-cause-a-decade-of-destruction/) describe the grapple as a central traversal tool. This implementation uses a collision-resolved capsule with a capped radial reel velocity, tangential steering, visible launch/cable, and release momentum; it does not duplicate their internal physics or object-to-object tethers.

## Lua mission contract

`missions.lua` returns a module with `version`, `missions`, and the functions `start_candidate(context)`, `start(context)`, `update(context, dt, event)`, `objective(context)`, `is_target(context)` and `next(context)`. `start_candidate` chooses the mission offered by the interaction prompt and F action; both it and `next` return a one-based integer mission index, or zero when none is available. `is_target` returns a boolean and `objective` returns a nonempty string of at most 512 bytes. Definition records contain stable `id`, `name`, legacy `kind`, `start={x,z}`, `seconds`, integer `reward`, and `goals={{x,z}, ...}`. Additional fields belong to the script; the shipped script uses prerequisites, goal vehicle requirements and objective text. Retain the first six mission IDs and order for old index-based saves.

The host passes a copied snapshot of active mission, step, remaining time, health, player position, occupied vehicle category and completed missions. Lua indices are one-based; active `0` means idle. Update events are `tick` and `target_hit`. A transition returns `active`, `step`, `time`, and an event: `none`, `start`, `locked`, `checkpoint`, `hit`, `fail`, or `complete`. Notifications also require `message`, `banner` and `detail`; `"-"` suppresses the banner. The host validates the whole transition before applying it, and owns reward payment, audio, saving and presentation. Each protected invocation has a 100,000-instruction limit. Only base, math, table and string libraries are opened; file/OS/package access and dynamic loading are absent.

## Placement text contract

Records are whitespace-separated, with unique lowercase IDs. Colours use values in `[0,1]`; angles are radians. Lines beginning with `#` are comments. Records load in order. Invalid types, duplicate IDs, missing/trailing fields, non-finite numbers and out-of-range values reject the entire replacement.

```text
building id x z width depth height r g b
tree id x z palm variant scale model width height climbHeight
decoration id x z model width height depth
pedestrian id x z speed style armed r g b
vehicle id x z angle kind trafficRoute r g b
base-ground id x0 z0 x1 z1 y r g b atlasTile material cullRange
ground id x0 z0 x1 z1 y r g b atlasTile material cullRange
beam id ax ay az bx by bz width height r g b cullRange
model id asset px py pz width height depth yaw r g b cullRange
plant id asset px py pz width height depth yaw r g b cullRange
prop id asset px py pz yaw scale cullRange
glow id px py pz width height depth yaw r g b cullRange
light id px py pz radius r g b brightness streetShadow
outcrop id editId x z asset width height yaw
```

`base-ground` supplies cached downtown surfaces; other visual records enter the regional scene. Asset paths refer to baked model names such as `modern/bench`. `glow` and `light` activate at night. `plant` records preserve editable high-density beach bushes; `outcrop` records anchor scanned rocks to the current terrain and retain their old edit keys. Model/prop records are visual scenery; movable/crushable props use `props.ini`, and buildings/trees/decorations use their dedicated typed records for collision and gameplay. `vehicle.kind` is the existing 0-12 enum, and `trafficRoute=-1` means no regional route.

The shipped placement snapshot preserves the previous generated layout and IDs. Change both visual surfaces/railings in the placement file and walkability masks in `geography.ini` when moving a bridge or waterfront. Pier meshes, water queries, grass exclusions, Jolt crossing floors and map overlays consume the shared geography. Downtown navigation roads and generated buildings are separate authored layers (`geography.ini` and `world.ini`); keep their layouts aligned.

Restart the game after editing required runtime files. Use a fresh world when reviewing authored placements; loading existing progress restores saved actor/world state. No C++ rebuild is needed for script or data edits.

For verification, run `ctest --test-dir build-msvc-ninja -R "mission_script_scenarios|placement_data_scenarios" --output-on-failure`. The tests edit the actual runtime script, terrain, crossing masks, placements and props and verify their effects without rebuilding the game. `MiniCity3D.exe --smoke --1080p --imgui-review` captures menus and exercises native widgets; it restores the executable's previous saves/settings after exercising those actions.
