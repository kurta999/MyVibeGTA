# Imported 3D assets

The original city models listed below are CC0. The newer animal and bird models are CC BY 3.0; their attribution and license are listed in `ANIMALS.md`, `BIRDS.md`, and `HELICOPTER.md`.

| Models | Creator and source | License |
| --- | --- | --- |
| `source/buildings/building-*.glb`, shared colormap | Kenney, [City Kit Commercial 2.1](https://kenney.nl/assets/city-kit-commercial) | CC0; original text in `source/buildings/License.txt` |
| `source/nature/*.glb` | Kenney, [Nature Kit 2.1](https://kenney.nl/assets/nature-kit) | CC0; original text in `source/nature/License.txt` |
| `source/characters/casual-man.glb` | Quaternius, [Casual Character](https://poly.pizza/m/kZ3DmIoGip) | CC0 as stated on model page |
| `source/characters/hoodie-man.glb` | Quaternius, [Hoodie Character](https://poly.pizza/m/gKLBoRsyKe) | CC0 as stated on model page |
| `source/characters/casual-woman.glb` | Quaternius, [Woman Casual](https://poly.pizza/m/jpKRgGDxhk) | CC0 as stated on model page |
| `source/characters/beach-man.glb` | Quaternius, [Beach Character](https://poly.pizza/m/DojKLcO34E) | CC0 as stated on model page |
| `source/vehicles/sedan.glb` | Quaternius, [Car](https://poly.pizza/m/unqqkULtRU) | CC0 as stated on model page |
| `source/vehicles/sports-car.glb` | Quaternius, [Sports Car](https://poly.pizza/m/1mkmFkAz5v) | CC0 as stated on model page |
| `source/vehicles/motorboat.glb` | Quaternius, [Boat](https://poly.pizza/m/5UEl54KsuC) | CC0 as stated on model page |
| `source/ggbot-cars/Car 01` through `Car 05` | GGBotNet, [PSX Style Cars](https://opengameart.org/content/psx-style-cars) | CC0 1.0 |
| `source/pistol/Pistol.gltf`, `source/ak/AK.gltf` | loafbrr_1, [Pistol](https://opengameart.org/content/pistol-5) and [AK](https://opengameart.org/content/ak) | CC0 1.0 |
| `source/lightning/lightning.glb` | LonesomeDucky, [Lightning Pump Action Rifle](https://opengameart.org/content/lightning-pump-action-rifle) | CC0 1.0 |
| `source/polyhaven/tree_small_02`, `fir_sapling`, `quiver_tree_01`, `quiver_tree_02`, `island_tree_01`, `island_tree_02`, `island_tree_03`, `jacaranda_tree`, `pine_sapling_small` | [Poly Haven](https://polyhaven.com/models) tree assets | CC0 |
| `source/polyhaven/shrub_*`, `wild_rooibos_bush`, `fern_02`, `nettle_plant`, `periwinkle_plant`, `weed_plant_02`, `crystalline_iceplant` | [Poly Haven](https://polyhaven.com/models) forest plants | CC0 |
| `source/polyhaven/modular_urban_apartments_facade`, `modular_factory_facade` | [Poly Haven](https://polyhaven.com/models) facade textures | CC0 |
| `source/3dassets/*.glb` | [3D Assets](https://3dassets.dev/) date palm, acacia, baobab, cherry, olive, fig, buttress, canopy, persimmon, timberline, and palm sapling models | CC0 1.0 per asset API |

`tools/convert_assets.py` bakes GLB geometry and animation samples into `.m3d` files. It extracts the Kenney building colormap beside the baked meshes. The generated texture atlases in `assets/` are original project assets.

The [nature manifest](NATURE_MANIFEST.csv) lists each runtime nature model, its source, and its license. Thirty tree variants now use 20 distinct CC0 source trees with individual geometry and color atlases; 36 undergrowth variants combine 11 Poly Haven plant sources with individual geometry and texture atlases. The six desert cacti and rocks still use Kenney Nature Kit. The [city manifest](CITY_MANIFEST.csv) lists 30 original modular building meshes, each with a separate atlas made from Poly Haven facade textures. These are variants generated from shared source assets, not 96 independently authored downloads. The official CC0 1.0 legal text is included at `source/CC0-1.0.txt`. `tools/fetch_city_sources.ps1` checks source file hashes; `tools/build_city_models.py` reproduces the baked geometry and atlases.
# Forest wildlife

Regional mountain outcrops use Poly Haven's CC0 Namaqualand Boulder 02 and Coastal Cliff 02 scanned meshes. Exposed terrain uses the CC0 Rocky Terrain material. [TERRAIN.md](TERRAIN.md) records source pages, modifications, download hashes and the offline rebuild procedure.

High-resolution Direct3D grass uses Poly Haven's CC0 Grass Medium 01 model and shared 2K PBR textures. [GRASS.md](GRASS.md) records credits, download verification, and the six surface adaptations.

The fifteen textured wildlife meshes use **CC BY 3.0** assets by **Poly by Google**, obtained from Poly Pizza. See [ANIMALS.md](ANIMALS.md) for each source link, attribution, and modification notes, including the fawn-derived roe-deer stand-in. Source GLBs, checksums, and the complete license are in `source/animals/`. These assets are not CC0.

Five bird models also use **CC BY 3.0** assets by **Poly by Google** via Poly Pizza. [BIRDS.md](BIRDS.md) records the original models and procedural flight-wing modifications; source GLBs, textures, checksums, and the full license ship in `source/birds/`.

### C4 and throwable equipment

The C4 mesh and its original UV texture are Lucian Pavel's **Low poly Sticky-Bomb**, downloaded from https://opengameart.org/content/low-poly-sticky-bomb under **CC0 1.0** (https://creativecommons.org/publicdomain/zero/1.0/). The original archive and .blend are in `source/explosives/`; `source/explosives/manifest.json` records the download URL and SHA-256. `tools/import_explosives.py` normalizes and cooks its 201 triangles into `baked/weapons/c4.m3d` and renders the HUD icon. The grenade, smoke grenade, flashbang, molotov, remote trigger, and timed-bomb meshes and icons are original procedural models generated by that script.
