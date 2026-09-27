# Textured traffic and weapon models

All models below are published under Creative Commons Zero (CC0 1.0). The
selected weapon GLTF/GLB files are in `source/`; the DX11 game reads converted
M3D meshes and texture maps in `baked/`. Rebuild the available weapon sources
with `python tools/import_traffic_weapons.py --weapons-only`. This now emits
indexed M3D2 meshes. The GGBot car OBJ sources are absent from this checkout;
their existing cooked M3D1 meshes remain supported. The full importer also
supports those sources when they are present.

| Asset in game | Original author | Source | License |
| --- | --- | --- | --- |
| Five traffic car bodies, models 1–5 | GGBotNet | https://opengameart.org/content/psx-style-cars | CC0 1.0 |
| Pistol | loafbrr_1 | https://opengameart.org/content/pistol-5 | CC0 1.0 |
| AK rifle | loafbrr_1 | https://opengameart.org/content/ak | CC0 1.0 |
| Lightning pump-action rifle | LonesomeDucky | https://opengameart.org/content/lightning-pump-action-rifle | CC0 1.0 |

The car pack contains UV-mapped 128×128 paint and lamp atlases. The weapon
meshes include embedded base-color and normal maps. The in-game lamp flash
uses separate emissive panels placed near the cars' textured lamp areas.

## Indexed weapon provenance

The source files below contain the embedded images used by the importer. SHA-256
hashes identify the inputs and the M3D2 geometry produced by
`python tools/import_traffic_weapons.py --weapons-only`.

| Weapon | Source SHA-256 | Cooked M3D2 SHA-256 |
| --- | --- | --- |
| `pistol/Pistol.gltf` | `BA9BE3F44C320EB9809FACFE6259E406E3CE18B601124C4FBB8AC43F96384F4E` | `CEF1ED818CB3D83CAF0F191AB0EAE4AA06BBB346EDC43E6DF803161429967E96` |
| `ak/AK.gltf` | `B16018A7011DBF3FF4E4566AE5A47EE94509FA0F87FDA98E2E6481C200C4E17C` | `D8FD2755F539E7AD676ECB42A86D70ED135A94988255CFB42338F28189FCCF50` |
| `lightning/lightning.glb` | `3F84F2B0D011EBFB142DE7F7D9CFA7D57A59451A815B834B4F33603256C8F911` | `0A84063EA443FAC1438D1D1D578FB41DB1926D4FCE4200EEE2BCDB09EBE464B0` |
