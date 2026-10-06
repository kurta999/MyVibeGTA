# Builder hoe, brush and repair evidence — 2026-10-06

The DX11 builder layer now includes natural/placed soil preparation, generated plant clearing with hoes, modeled loose deposits harvested by brushes, and data-defined repair costs for all 22 tools.

## Behavior

- All five hoe tiers use RMB to till exposed soil, spending durability only when the surface changes. Natural tilled surfaces retain the original terrain geometry/collision and use a clipped furrow-texture overlay. Placed soil changes material while retaining its cube collision; actual shovel input recovers soil. Hoes also clear suitable generated bushes through ordinary LMB harvesting.
- Brush deposits have original 3D crumb geometry and deterministic world-cell IDs. Only exposed dry-land soil/sand/gravel surfaces qualify; weighted rewards in `data/builder-actions.ini` are coal, gravel, sticks and iron ore. Held RMB or LMB takes two seconds with the current brush tuning. Releasing input, switching tools, opening inventory and F5 cancel progress without a yield or durability charge. A completed deposit yields one item once, or one loose drop if inventory is full.
- Natural tilled cells and consumed deposits use G/R records in the existing coherent builder snapshot. Their visuals and interaction disappear outside builder mode. Consumed deposits do not respawn after save/load or repeated F5 transitions.
- Repairs require a crafting bench within reach and an empty cursor stack. The inventory repair button or R consumes one material configured in `data/builder.ini` and restores up to a quarter of maximum durability. Wood/brush use planks, stone tools accept the stone material group, metal tools use their ingot, and diamond tools use diamond. Full tools do not consume resources.
- Hoes have a short downward motion; brushes use short strokes. The brush mesh has a metal binding and staggered, textured bristle bundles. Exposed-ground targeting outlines its surface. Tool inventory hover cards show tier, remaining durability and preferred materials.

## Verification

All **28 CTest suites passed in 84.28 seconds**, after final gameplay/render changes and the combined on-disk persistence test. See `ctest.log`.

`tests/tool_work_scenarios.cpp` exercises actual reticle targeting and input for all five hoes, generated plant harvesting, placed soil/shovel recovery, brush completion/cancellation and wrong-tool rejection, one-time drops when inventory is full, every repair material/cost, station/shortage/cursor gates, clamping, UI activation, snapshot validation and restart persistence. A combined saved generation retains soil work, a consumed deposit and repaired durability together, followed by repeated F5 checks.

The 67 generated assets passed model SHA-256, binary layout, retained OBJ source, texture and icon presence checks (3,276 triangles total). Assets are original project-authored work, reproduced by `tools/build_builder_assets.py`; the manifest is `assets/models/source/builder/manifest.json`.

Reproduce tests/captures with `tools/verify_tool_work.ps1`. Save-writing tests/previews run sequentially in `build-msvc-ninja`; they do not run against the user's root save.

All four DX11 captures were visually inspected: `hoe.png` shows a prepared soil patch and equipped hoe; `brush.png` shows the bristled model and deposit at 30% progress; `repair.png` shows the inventory repair cost and availability; `normal.png` shows original soil with builder tool/HUD removed. Fixtures explicitly supply tools/materials and prepare surrounding soil; the center hoe action and brush progress use the regular input path. These captures are not an acquisition/progression playthrough. Hover-card behavior was implemented but is not shown in these captures.

The verified root/runtime executable SHA-256 is `DE07913C6D60BFC264AEE63461717C69BDCD7953CE90B0A4A36D801202E90676`. The user's root save retained SHA-256 `5A4C87DB5DC9E23148622694D21056E0B18EC4FE45FE7B294C59826B289FC539`.

## Remaining scope

This pass does not complete the full plan. Complete material/contact animation and sound/particle feedback, shears blade motion, progression play tuning, complete underground navigation/actor recovery, transition/save failure handling, face/collider merging, and dense-world performance/interactive verification remain open.
