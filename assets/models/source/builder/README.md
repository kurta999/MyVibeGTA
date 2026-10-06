# Original builder assets

All geometry, UV textures, and item renders in this folder and `assets/models/baked/builder/` are original project-authored work generated reproducibly by `tools/build_builder_assets.py`. No Minecraft artwork or model files are included.

The generator uses only the Python standard library. It retains OBJ sources, exports the existing M3D1 runtime mesh format, generates deterministic 256-pixel RGBA textures, and renders 96-pixel transparent inventory icons from the same mesh/UV data. `manifest.json` records source paths, triangle counts, and SHA-256 mesh hashes.

There are 20 rock block materials, 20 tiered pickaxes/axes/shovels/hoes, shears, a brush, and additional building/crafting items. Tool heads use authored beveled contours and separate textured handles. Wood, stone, iron, gold, and diamond appearances share family geometry where appropriate and have distinct textures and catalog tuning.

Run the generator to reproduce the asset set and `data/builder.ini` / `data/builder-recipes.ini`. Gameplay controls and material research sources are recorded in `minecraft plan.md`; those references inform behavior and are not asset sources.
