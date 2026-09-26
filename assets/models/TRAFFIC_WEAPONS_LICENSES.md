# Textured traffic and weapon models

All models below are published under Creative Commons Zero (CC0 1.0). The
selected original OBJ and GLTF/GLB files are in `source/`; the DX11 game reads
the converted M3D meshes and texture maps in `baked/`. Rebuild the conversions
with `tools/import_traffic_weapons.py` using Python 3.

| Asset in game | Original author | Source | License |
| --- | --- | --- | --- |
| Five traffic car bodies, models 1–5 | GGBotNet | https://opengameart.org/content/psx-style-cars | CC0 1.0 |
| Pistol | loafbrr_1 | https://opengameart.org/content/pistol-5 | CC0 1.0 |
| AK rifle | loafbrr_1 | https://opengameart.org/content/ak | CC0 1.0 |
| Lightning pump-action rifle | LonesomeDucky | https://opengameart.org/content/lightning-pump-action-rifle | CC0 1.0 |

The car pack contains UV-mapped 128×128 paint and lamp atlases. The weapon
meshes include embedded base-color and normal maps. The in-game lamp flash
uses separate emissive panels placed near the cars' textured lamp areas.
