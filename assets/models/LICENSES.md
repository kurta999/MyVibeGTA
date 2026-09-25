# Imported 3D assets

All models in `source/` and geometry baked from them in `baked/` are CC0.

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
| `source/polyhaven/tree_small_02`, `fir_sapling`, `quiver_tree_01`, `quiver_tree_02`, `island_tree_01`, `island_tree_02`, `island_tree_03`, `jacaranda_tree`, `pine_sapling_small` | [Poly Haven](https://polyhaven.com/models) tree assets | CC0 |
| `source/polyhaven/shrub_*`, `wild_rooibos_bush`, `fern_02`, `nettle_plant`, `periwinkle_plant`, `weed_plant_02`, `crystalline_iceplant` | [Poly Haven](https://polyhaven.com/models) forest plants | CC0 |
| `source/polyhaven/modular_urban_apartments_facade`, `modular_factory_facade` | [Poly Haven](https://polyhaven.com/models) facade textures | CC0 |
| `source/3dassets/*.glb` | [3D Assets](https://3dassets.dev/) date palm, acacia, baobab, cherry, olive, fig, buttress, canopy, persimmon, timberline, and palm sapling models | CC0 1.0 per asset API |

`tools/convert_assets.py` bakes GLB geometry and animation samples into `.m3d` files. It extracts the Kenney building colormap beside the baked meshes. The generated texture atlases in `assets/` are original project assets.

The [nature manifest](NATURE_MANIFEST.csv) lists each runtime nature model, its source, and its license. Thirty tree variants now use 20 distinct CC0 source trees with individual geometry and color atlases; 36 undergrowth variants combine 11 Poly Haven plant sources with individual geometry and texture atlases. The six desert cacti and rocks still use Kenney Nature Kit. The [city manifest](CITY_MANIFEST.csv) lists 30 original modular building meshes, each with a separate atlas made from Poly Haven facade textures. These are variants generated from shared source assets, not 96 independently authored downloads. The official CC0 1.0 legal text is included at `source/CC0-1.0.txt`. `tools/fetch_city_sources.ps1` checks source file hashes; `tools/build_city_models.py` reproduces the baked geometry and atlases.
