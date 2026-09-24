# Gameplay data

The DX11 game loads these versioned INI files from the `data/` directory beside the executable. Art and licenses remain in `assets/`. A malformed or missing required file stops startup with a file/section/key error in `MiniCity3D.log`.

| File | Contents |
| --- | --- |
| `weapons.ini` | Ordered catalog for 17 firearms, tools, melee weapons, and bow, including dual-wield eligibility. The first five IDs remain fixed for version 1 save migration. |
| `vehicles.ini` | Handling, Jolt chassis/wheel tuning, surface and wet tire grip, durability, repair, and drift thresholds for each vehicle kind. |
| `surfaces.ini` | Grass, sand, asphalt, metal, wood, concrete, water, and snow fire spread and burn rules. |
| `police.ini` | Witness/report timing, response locations, and weapon/armor tiers for wanted levels 1–4. |
| `commerce.ini` | Stable-ID shops and houses across cities and biome hubs, item prices, vehicle stock, garage capacity, and fast travel locations. Fourteen houses are available; the player can own ten. |
| `traversal.ini` | Stable-ID downtown ladders, generated tall-building ladder thresholds, and climb anchors for selected trees. |
| `regions.ini` | The 16,800 × 16,800 world, six biome regions, second-city layout, and four population hubs with data-defined pedestrian counts and vehicle spawn cycles. |
| `roads.ini` | Ten connected regional road segments through the bridge, countryside, snowfields, desert, and savanna. |
| `trees.ini` | Twenty-nine Kenney tree meshes assigned to countryside, snow, savanna, and desert biomes with render sizes and species climb heights. |
| `biome_props.ini` | Six Kenney cactus and rock meshes for desert decoration. |
| `weather.ini` | Timed clear, wind, overcast, rain, and snow states with cloud, visibility, wind, and precipitation tuning. |
| `world.ini` | Building/tree layout rules, a deterministic generation seed, pedestrians, traffic, stable-ID vehicle placements, and stable-ID weapon pickups. |
| `missions.ini` | Six ordered missions, stable IDs, starts, goals, timers, and rewards. Their IDs stay fixed for save compatibility. |

Each file has `[Schema] Version=1` except `regions.ini`, which is version 2. IDs use lowercase letters, digits, hyphens, and underscores. `world.ini` and `missions.ini` are validated together before replacing the live world. Save version 2 stores weapon inventory and armed-opponent kills, mission, owned house and parked vehicle, tree, pedestrian, armor, and repair-kit state by stable IDs; version 1 index-based saves are imported on load and rewritten as version 2 on the next save.

The DX11 renderer loads the regional and species data, while some scenery rules remain procedural. `asset_smoke` checks all referenced tree and desert prop models against baked assets and the nature manifest. Eligible regional trees get deterministic climb anchors at the frequency set in `regions.ini`. The 53.45× world uses local terrain tiles and cell indexes for nearby tree and desert decoration rendering and fire lookups; distant wandering pedestrians sleep and return to their regional homes after respawning. Jolt building colliders and pedestrian capsules stream near the player; full mutable region-state streaming remains open. Surface material rules cover regional grass, desert sand, snow, roads, and river water using a sparse active fire grid.
