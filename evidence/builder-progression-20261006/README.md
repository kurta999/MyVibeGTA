# Builder acquisition, tools and storage — 2026-10-06

This milestone verifies the required 22 textured tools through acquisition, use, drop/pickup, storage and persistence. The full Minecraft plan remains in progress.

## Implemented fixes

- A selected brush now permits ordinary right-click interaction with chests, crafting benches and furnaces. Previously its early return prevented those interactions. Brushing still uses the defined held-input deposit action.
- C's builder inspection camera now looks over the right shoulder, making the equipped tool visible beside the character. Its collision checks remain active. Normal-mode camera behavior is unchanged and tested after F5 exit.
- Third-person target and harvest hints appear beside the character rather than over the hand/tool model. First-person hints retain their existing position.
- The catalog capture fixture retrieves the actually earned tools from their saved chest, reviews them on clear original terrain, drops and picks them up with durability checks, and returns every tool to storage. It does not remove scenery or seed tools. Captures move directly into this folder to avoid maintaining a second complete image set in the runtime directory.

## Gameplay evidence

`tests/builder_progression_scenarios.cpp` starts with an empty inventory and no builder edits. Generated trees, buildings, terrain and stable scenery IDs remain intact; dynamic crowd, traffic and loose actors are cleared for this controlled fixture. Travel between resource sites is teleported by the fixture, so this is not a continuous walking/exploration playthrough.

The scenario hand-harvests generated logs, uses actual crafting-menu clicks to make starter tools, and places a bench/chest/furnace through right-click ground-face targeting. It mines every access-shaft layer through held mining input and uses actual Jolt capsule falls to settle onto the newly exposed floor. No inventory grants or prepared excavation helpers supply the progression resources.

It obtains stone, coal, iron, gold and diamond resources, processes metals at the furnace, and crafts all 22 tools. Ingredient/result counts are checked on each recipe. Pickaxes mine actual underground basalt, axes cut generated trunks, shovels dig natural soil, hoes till natural soil, shears harvest generated foliage, and the brush clears a natural resource deposit. Mining duration follows the configured material/tool rate; successful actions consume one durability charge and produce the expected resource or saved surface state.

Every tool is checked as a textured first-person and animated third-person model, then dropped, picked up, transferred into a chest, taken through F5 exit/re-entry, and saved/reloaded from disk. Durability and unique tool instances persist. The final earned save contains all 22 tools in one chest, 203 terrain cuts and 16 edited scenery objects. Other suites retain coverage of hoe plant clearing, tool-tier rejection, repair, breakage, cancellation and material feedback.

All **32 CTest suites passed in 283.26 seconds**, including this progression suite in 89.21 seconds after the brush/camera changes. See `ctest.log` and the extracted current-run `progression.log`. The subsequent HUD-only placement adjustment was built and verified through the final DX11 captures.

## Catalog and visual review

- `catalog.tsv` records all 22 tools' model, texture and icon paths, recipe ID, speed, tier, maximum/remaining durability, actual target, resulting resource and action duration.
- `catalog-checklist.md` adds exact recipe ingredients and station requirements. Its generator checks each model, texture, icon and retained OBJ source against the current SHA-256 asset manifest.
- `earned-save.ini` retains the coherent progression save used for rendering.
- `captures.tsv` and `review.log` identify the final run's **67 native 1600×900 DX11 images**: one storage view plus each tool in hand, on the character and dropped in the world.
- All 66 tool views were visually inspected through the five labeled `*.review.png` crop sheets; the storage view was inspected at native resolution. The review confirms complete recognizable silhouettes, distinct tier appearances, textured handles/heads, shovel grips/blades, shears handles/blades and brush bristles. The shoulder view exposes the held heads, and dropped views show their full family silhouettes.

The sheet generator preserves the native images and creates only review crops. Earlier obstructed captures were replaced by the final clearing views. Runtime screenshot duplicates were removed only after proving that identical files remained in the historical evidence folders.

## Reproduction

1. Build the DX11 `MiniCity3D` and `simulation_smoke` targets.
2. Run `tools/verify_builder_progression.ps1` for focused regressions and the earned-save captures. `-CaptureOnly` reuses a completed progression report/save.
3. With Python and Pillow available, run `python tools/build_builder_review_sheets.py evidence/builder-progression-20261006` for the asset/recipe checklist and five review sheets.

Save-writing tests and GUI captures run sequentially in `build-msvc-ninja`. The verifier backs up/restores that directory's temporary default save. It refuses the repository root as its runtime destination.

The root executable matches the final DX11 runtime executable: SHA-256 `E0E80AF77C78C3AE16AC69D16BF13997DC2073A176319B896731512C17FCD184`. The user's root save remained SHA-256 `5A4C87DB5DC9E23148622694D21056E0B18EC4FE45FE7B294C59826B289FC539`. These values and milestone counts are retained in `verification.json`.

## Remaining full-plan work

Continuous exploration and balance tuning, the full rock-material appearance review, complete underground navigation and remaining actor/height-query integration, exceptional collision/actor reconstruction rollback and interrupted-process recovery, face/collider merging, bounded dense-world memory/performance checks and subjective audio review remain open. This controlled tool loop does not establish those requirements or complete the full plan.
