# Builder rock materials — 2026-10-06

The earlier 20-rock audit found repeated stripe/checker patterns that did not match the planned grain, pores, pebbles and crystals. The original deterministic recipes in `tools/builder_rock_materials.py` replace those materials. Rock cubes now map a complete 0..1 texture tile onto each face instead of stretching the head portion of a tool atlas. The unit cube, normals and gameplay catalog are preserved.

The materials are stylized project-authored art. Geological references inform their morphology; they are not copied images, exact mineral proportions or measured rock strengths. HP 40–280 remains gameplay tuning, separate from blast resistance and tool harvest rules.

## Verified scope

- All 35 CTest suites passed in 264.53 seconds, including progression, scenery, excavation, navigation, separate-process restart, save failures and asset loading. `ctest.log` retains the complete results. The verified runtime/root executable hashes match; the protected root save remains unchanged.
- `asset-validation.log`: all 70 model/texture/icon/OBJ hashes, binary layouts, finite vertices, unit normals and image formats pass. All 20 rock recipes reproduce their current texture pixels; rock cubes retain their six faces and full-face UVs.
- `before-manifest.json` and `changed-assets.csv`: exactly 20 rock entries changed. All four asset hashes for the other 50 entries remain unchanged. `before-config-hashes.json` records unchanged item/recipe data and the protected root save.
- `gameplay.tsv` and `native-review.log`: actual native placement and held-LMB mining of every rock, using the iron pickaxe. Each block yields one resource and charges one durability point (250 → 249). All 20 mining durations differ: 20–141 updates at 120 Hz, within 1.1 updates of the catalog HP/speed calculation.
- `captures.csv`: hashes of 23 native DX11 captures. `rocks.native.review.png` contains labeled crops of all 20 actual world blocks and their hotbar icons, visually reviewed for differentiated morphology. The complete native PNGs remain authoritative for scene context. `rocks.review.png` separately shows the diffuse tiles and model-rendered icons.
- `gallery.png`, `normal.png`, `restored.png`: all 20 materials placed in a legally checked 5×4 area of the original generated map. The fixture retains original scenery for save/load and F5 reconstruction. Normal mode has zero builder colliders; re-entry restores active colliders and every saved cell/material identity. The three full views were visually reviewed. `gallery-save.ini` retains the 20 saved blocks.
- The close material cards deliberately clear scenery for a controlled view and use supplied inventory. They demonstrate rendering, placement and mining; existing-world acquisition and progression are covered separately by the progression/scenery suites. This review does not claim a continuous exploration playthrough.

The initial gallery fixture cleared original buildings, which returned after save/load and occluded the gallery. The final fixture searches valid placement cells on the original world instead. The retained final captures show the same complete gallery before and after restoration.

## Material art recipes

| Material | Distinguishing authored pattern |
|---|---|
| Chalk | Soft pale matrix, cloudy patches and tiny sparse pinholes |
| Mudstone | Fine brown matrix and broad uneven bedding seams |
| Shale | Dark thin laminae with varying spacing and warped boundaries |
| Tuff | Ashy matrix and mixed-size angular fragments |
| Pumice | Dense rounded and elongated dark pores |
| Sandstone | Small warm rounded/cemented sand-grain cells |
| Limestone | Cream mottling and small shell-like inclusions |
| Travertine | Buff bands interrupted by elongated pits |
| Dolostone | Small buff crystalline facets and sparse vugs |
| Conglomerate | Large rounded multicolored pebbles in dark cement |
| Slate | Fine blue-gray matrix and sharp diagonal cleavage seams |
| Marble | Pale matrix and branching gray veins |
| Schist | Dense aligned elongated light/dark mica-like flakes |
| Gneiss | Broad irregular light/dark bands and fine grain detail |
| Andesite | Fine gray matrix with sparse elongated pale crystals |
| Granite | Coarse interlocking pink, pale and dark mineral cells |
| Diorite | Medium interlocking black-and-white crystals |
| Gabbro | Coarse mostly dark green-black crystals with pale grains |
| Basalt | Dark fine groundmass with tiny sparse specks/pores |
| Quartzite | Dense pale sugary grains and bright facets |

BGS describes sediment layers, sand/pebbles, interlocking granite minerals and crystalline marble; this supports using different grain structures rather than color changes alone. [BGS rocks and minerals](https://www.bgs.ac.uk/discovering-geology/rocks-and-minerals/). USGS explains aligned minerals and foliation in schist/gneiss; this informs the flake and band recipes. [USGS metamorphic rocks](https://www.usgs.gov/faqs/what-are-metamorphic-rocks?items_per_page=6&page=1). Coarse intrusive versus fine extrusive igneous grains inform the granite/diorite/gabbro versus basalt/andesite recipes. [USGS igneous rocks](https://www.usgs.gov/faqs/what-are-igneous-rocks). Additional references are retained per material in the source manifest.

## Reproduction

Use the configured Python/Pillow runtime as `$taskPython` and Visual Studio's bundled CMake directory as `$taskCMakeBin`. Run from the repository root. Execute runtime tests and save-writing native reviews sequentially; never run them in the root directory.

```powershell
& $taskPython -B tools/build_builder_assets.py
& $taskPython -B tests/builder_material_assets.py
# Build MiniCity3D in the configured amd64 Visual Studio developer shell.
& "$taskCMakeBin/cmake.exe" --build build-msvc-ninja --target MiniCity3D simulation_smoke --parallel 3
.\tools\verify_builder_rocks.ps1
& $taskPython -B tools/build_builder_rock_review.py evidence/builder-materials-20261006 --native
& "$taskCMakeBin/ctest.exe" --test-dir build-msvc-ninja --output-on-failure -j 1
```

`verify_builder_rocks.ps1` restores the runtime save and checks that the root save remains unchanged. The native review moves named screenshots directly to evidence, avoiding a second full copy. `build.log`, `generation.log`, `ctest.log` and `verification.json` retain build/test identity and final results.

This completes the differentiated rock texture and native material review follow-up. Continuous exploration/balance, remaining actor and height-query consumers, transactional reconstruction recovery, interrupted-process save recovery, geometry/collider merging, dense-world performance and subjective audio review remain open in the full Minecraft plan.
