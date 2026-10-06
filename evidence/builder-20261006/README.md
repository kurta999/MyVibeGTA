# Builder implementation evidence — 2026-10-06

This is an incomplete implementation of `minecraft plan.md`. The remaining requirements in that document still apply.

The subsequent excavation pass and its 26-suite verification are documented in `../excavation-20261006/README.md`. The test log and 64-asset results below describe the earlier tool pass; these five captures were refreshed after the excavation integration, which adds the 65th asset (coal ore).

## Verified behavior

`builder_scenarios` runs the real builder catalog, generated world definitions, DX11 mesh loader, save path, and Jolt character/collision code. It verifies:

- F5 removes/restores placed block collision, independently swaps building/tree damage, safely reconciles the capsule, and retains edits across save/load.
- Inventory stack conservation, split/cursor persistence, chest transfers and saved contents, and one-time explosion drops.
- All 64 textured model assets load; all 22 tool recipes conserve resources and obey their bench/furnace requirements.
- Live mining input distinguishes preferred tools, protects ore tiers, yields diamonds once, consumes durability on successful harvesting, removes broken tools, and preserves durability across restoration.
- Axes, shovels, and shears harvest placed wood, sand, and leaves respectively. These checks do not establish granular harvesting of existing trees, terrain, or decorative bushes.
- Mouse release, hotbar changes, and inventory opening cancel mining. Held placement obeys its cooldown, rejects player overlap without consuming resources, and cancels on chest opening and F5 transitions.
- The equipped tool asset is rendered once in first person, follows a moving skeletal hand in third person, and disappears in inventory and normal mode. Ingot/stick/gem mesh checks distinguish their authored shapes from a generic cube.

The final full 25-test CTest suite passed in **77.50 seconds**, including the extended tool/wood/sand/leaf scenarios and final equipped-tool pose. Its complete output is retained in `ctest.log`. The builder scenario took **2.72 seconds** in that run.

## DX11 captures

`tools/verify_builder.ps1` runs the focused scenario, then separate hidden DX11 preview processes sequentially. The previews use explicit smoke-test fixture inventory and placed blocks; they are rendering evidence, not proof that all resources are obtainable in ordinary gameplay.

- `world.png`: textured block layer, placement preview, target/tool guidance, and hotbar.
- `inventory.png`: inventory, stack counts, modeled icons, and crafting/station requirements.
- `loading.png`: F5 transition screen; the capture shows the initial stage, while scenario ticks exercise all stages.
- `tool.png`: equipped textured axe in first person.
- `third-person.png`: equipped axe attached to the animated character hand.

All five captures were visually inspected. The third-person fixture moves a nearby helicopter away to keep its rotor from obscuring the hand and tool.

Reproduce with a Release build of `MiniCity3D`, `simulation_smoke`, and `asset_smoke`, then run `tools/verify_builder.ps1`. Run the full regressions with `ctest --test-dir build-msvc-ninja --output-on-failure`.

## Remaining scope

Below-surface excavation and tunnels need replacement terrain geometry and collision. Existing trees/foliage need granular conversion, scanned rocks need local subtraction, and regional resources must become obtainable. Hoe/brush and existing-world shears actions, contact-specific effects/audio, dense-build meshing/performance, and transition/save failure recovery remain open. Inventory/menu captures do not prove these requirements complete.

OpenGL rendering files and target were not changed. The existing root `savegame.ini` was preserved with SHA-256 `5A4C87DB5DC9E23148622694D21056E0B18EC4FE45FE7B294C59826B289FC539`.
