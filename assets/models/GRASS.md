# Textured grass (Direct3D 11)

Source: [Grass Medium 01](https://polyhaven.com/a/grass_medium_01) by **Rico Cilliers** (modeling) and **Rob Tuytel** (photography), Poly Haven, **CC0 1.0**. The license text is included at `source/CC0-1.0.txt`.

`source/polyhaven/grass_medium_01/downloads.json` records download URLs and the publisher's MD5 checksums. `tools/fetch_grass.ps1` verifies them. `tools/build_grass.py` imports six original tuft meshes, preserves source UVs and curved blades, converts handedness, and cooks indexed M3D2 geometry. It combines the 16-bit alpha mask with the original 2048×2048 green and dry color maps. A frost variant derives from the dry map. The original DirectX normal and packed ambient-occlusion/roughness/metalness maps remain at 2K.

Lawn, meadow, savanna, desert, snow, and coastal variants use different tuft geometry, height, width, coverage, and color maps. These are six adaptations of one source asset, not six independently sourced species. The two inexpensive LODs use crossed, segmented cards sampling the source atlas's clump cutouts; their material maps share the full-resolution textures. Nearby blades cast alpha-tested shadows. Distant clumps do not cast shadows. Vertex alpha stores rooted wind bending weight; texture alpha supplies coverage.

`baked/nature/GRASS_MANIFEST.json` records geometry counts and texture resolution. Grass uses the existing instanced DX11 draw path with deterministic world-space roots, density selection, three spacing rings, distance fading, and road/water/building/promenade exclusions. It does not alter physics or saved world state.

The saved Graphics **Grass LOD** setting controls detailed blades from 40 to 800 world units (95 by default), independently of the overall Grass distance. It applies across all spacing rings without moving roots; the middle-to-far LOD transition expands with it. Increasing detail distance increases geometry and shadow work. The overall draw radius still limits visible grass.
