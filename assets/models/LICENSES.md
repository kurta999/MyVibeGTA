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

`tools/convert_assets.py` bakes GLB geometry and animation samples into `.m3d` files. It extracts the Kenney building colormap beside the baked meshes. The generated texture atlases in `assets/` are original project assets.
