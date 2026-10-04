# DX11 regional terrain verification — 2026-10-04

Savanna and Sahara now contain mountains, rocky ridges, rolling ground and deep
dry basins. Snow and countryside also have relief. Twenty authored landforms
and biome undulations share one 50-unit sampled surface across DX11 rendering,
Jolt collision, actor placement, camera obstruction and projectile queries.
Roads, settlements and service locations have smooth level approaches.

## Verification

- `./build.ps1 -RunTests`: all 24 CTest suites passed in 103.18 seconds.
- After the final rendering/material-path adjustments, the DX11 build passed
  and `ordnance_scenarios`, `scene_jobs_scenarios`, `grass_scenarios`,
  `terrain_scenarios` and `asset_smoke` all passed in 7.11 seconds.
- Terrain regression covers road/hub clearance, positive and negative terrain,
  capsule contact, walking up/down slopes, vehicle suspension contacts, ground
  save/load, swept surface hits and render/collision height agreement.
- Source downloads match their pinned SHA-256 hashes. Imported rocks have
  valid full/LOD geometry and textures. Packaging includes runtime terrain maps
  and CC0 license text independently of retained source geometry.
- The three final previews below exited successfully and were visually inspected.

| Capture | Command arguments | Result |
| --- | --- | --- |
| [savanna.png](savanna.png) | `--smoke --terrain-savanna --screenshot` | Rocky mountain skyline, grounded vegetation and level road |
| [desert.png](desert.png) | `--smoke --terrain-desert --screenshot` | Sandy depression, rolling terrain and exposed rocky elevation |
| [basin.png](basin.png) | `--smoke --terrain-basin --screenshot` | Below-sea-level dry basin with rising sides |

Run these commands against `build-msvc-ninja/MiniCity3D.exe` with that directory
as the working directory. Compact application logs accompany each capture.

## Short performance sample

`--smoke --terrain-savanna --benchmark --1080p --day` on the AMD Radeon 680M:
120 frames, 16.72 ms average, 19.40 ms p95, 21.69 ms p99; 110 draws and
483,240 submitted triangles/frame. GPU timing averaged 0.44 ms shadows,
7.39 ms scene and 7.40 ms combined post/HUD. Raw output is in `benchmark.txt`.
This is one short stationary sample with other application activity possible;
it does not establish performance across travel routes or other hardware.

## Assets and remaining scope

Poly Haven's CC0 Namaqualand Boulder 02, Coastal Cliff 02 and Rocky Terrain
provide scanned outcrops and exposed-slope materials. Sources, hashes, licenses
and rebuild instructions are in `assets/models/TERRAIN.md` and the source manifest.
The scanned outcrops are decorative; continuous terrain supplies ground collision.
Wider interactive traversal/art tuning and route performance remain open.
