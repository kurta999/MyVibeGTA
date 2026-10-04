# Regional mountains and dry basins

The DX11 terrain uses 20 original landforms in `data/terrain.ini` plus broad
biome-specific undulations. Sahara and savanna each have four mountain/ridge
groups and two dry depressions. Snow and countryside also gain hills and valleys.
Roads, cities, hubs, shops and houses have smooth level approaches.

The same 50-unit sampled surface drives Jolt collision, immutable DX11 chunk
meshes, ground placement, camera obstruction and swept projectile hits. Dry
basins can lie below sea level without becoming water. Save loading resolves
ground positions onto the new surface. Terrain chunks share boundary samples
and the Jolt cell diagonal, with no overlapping terrain LOD rings.

Downloaded assets are from Poly Haven and use [CC0](https://polyhaven.com/license):

| Asset | Source | Runtime use |
| --- | --- | --- |
| Namaqualand Boulder 02 | https://polyhaven.com/a/namaqualand_boulder_02 | Textured outcrops on raised terrain |
| Coastal Cliff 02 | https://polyhaven.com/a/coastal_cliff_02 | Sparse textured ridge outcrops |
| Rocky Terrain | https://polyhaven.com/a/rocky_terrain | Tileable base color and DirectX normal map on exposed slopes |

Scanned rocks are decorative; the continuous terrain underneath supplies
collision. The importer reduces geometry by welding position/UV clusters and
removing collapsed triangles, retains texture seams, and produces full/LOD
meshes. It does not remove random triangles. Full source geometry, maps,
download URLs, MD5 and SHA-256 hashes are in `source/terrain/manifest.json`.
Runtime terrain maps and the license legal text are in `assets/materials/terrain/`,
which the package script copies independently of source geometry.

Rebuild with the project's Python art environment (NumPy and Pillow):

```powershell
python tools/fetch_terrain_assets.py --fetch --bake
./build.ps1 -RunTests
```

`--fetch` checks the pinned manifest instead of silently accepting changed
upstream files. `--bake` works offline when the sources are present.

Preview locations:

```powershell
./MiniCity3D.exe --smoke --terrain-savanna --screenshot
./MiniCity3D.exe --smoke --terrain-desert --screenshot
./MiniCity3D.exe --smoke --terrain-basin --screenshot
```
