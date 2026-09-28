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

Each cooked weapon has a `.import.json` report with the source hash and the
attributes, animation, hierarchy, excluded source nodes, and sampler substitutions made by this
static-only importer. The Lightning source contains one skin and two clips
(`Fire` and `Pump`); the cooked in-game mesh is static and does not preserve
those clips. Its extra color set and source tangents are also omitted. The
first color set is applied to vertex tint. Runtime model sampling currently
clamps UVs and draws materials double sided, so source sampler and culling
settings are not represented per material. These reports describe the current
weapon conversion and do not validate the planned showcase import pipeline.
The AK report lists the Bullet, BulletBox, and BulletFired nodes intentionally
excluded from its held-weapon mesh.

## Indexed weapon provenance

The source files below contain the embedded images used by the importer. SHA-256
hashes identify the inputs and the M3D2 geometry produced by
`python tools/import_traffic_weapons.py --weapons-only`.

| Weapon | Source SHA-256 | Cooked M3D2 SHA-256 |
| --- | --- | --- |
| `pistol/Pistol.gltf` | `BA9BE3F44C320EB9809FACFE6259E406E3CE18B601124C4FBB8AC43F96384F4E` | `5FB4C614ECFBCCD709B6AE11C343A12267524211DB839C86A6AC9E6040FE018A` |
| `ak/AK.gltf` | `B16018A7011DBF3FF4E4566AE5A47EE94509FA0F87FDA98E2E6481C200C4E17C` | `04A3842A85A9AC31B22A93F83AAC805F8022C8B0EF949608B9CE390A8B71E8B0` |
| `lightning/lightning.glb` | `3F84F2B0D011EBFB142DE7F7D9CFA7D57A59451A815B834B4F33603256C8F911` | `0A84063EA443FAC1438D1D1D578FB41DB1926D4FCE4200EEE2BCDB09EBE464B0` |
