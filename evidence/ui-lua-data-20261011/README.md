# Direct3D 11 menus, Lua missions and file-based world data

Verified on 2026-10-11. The delivered root `MiniCity3D.exe` uses Direct3D 11, statically linked Dear ImGui 1.92.6 and Lua 5.4.8. Its SHA-256 matches the build executable in `executables.json`.

## Requirement audit

| Requirement | Implementation and current evidence |
| --- | --- |
| Rewrite every game menu with ImGui | `src/imgui_menus.cpp` renders Pause, Graphics, Controls, Audio, shops, unowned/owned houses, garages/fast travel, F4 debug, bomb delay, inventory, crafting/repair and chest/processing storage. F1 help also uses ImGui. The DX11 lifecycle owns the official Win32/DX11 backends, the frame renders them before capture, and input prevents duplicate legacy handling. Legacy GDI menus are excluded from this target. |
| Preserve working menu actions | `native-menu-final.txt` and `native-root-final.txt` record real widget input and state assertions: graphics combo selection, checkbox, audio slider, Win32 key rebinding, navigation/resume, purchase, house ownership, garage storage, fast travel, debug cash, timer cancel/arm, inventory pickup/place/split/merge, crafting, chest transfers, save/load. Native review passes in both build and delivered root executables. |
| Move mission decisions to Lua | `data/missions.lua` defines all ten missions and start eligibility, prerequisites, vehicle/target predicates, checkpoints, objectives, next mission, timers, failure and completion. `game.cpp` calls the script for the interaction prompt/F action, ticking and target hits; it retains generic reward/save/audio/presentation effects. General simulation completes all ten missions and checks target shooting, prerequisites, rewards and save migration. |
| Demonstrate editable mission behavior | `mission_script_scenarios` edits the actual script to change a car checkpoint into an on-foot checkpoint, expands start distance and exercises the real prompt/F action, tests timeout, malformed modules/transitions/query returns and the instruction limit. Invalid replacements/transitions retain prior definitions/state. Final corrected fixture passes in `mission-final.txt`. |
| Load terrain and placements from files | `terrain.ini` supplies 20 landforms and all height-shaping parameters; `geography.ini` supplies water/crossing masks, piers, navigation roads and habitat centers. `regional_placements.txt` contains 17,435 explicit records consumed by regional population, DX11 scenery/lighting, and scenery editing. `props.ini` supplies 34 movable-prop candidates. Existing `world.ini`, commerce/traversal/roads/species files continue supplying downtown layout rules, vehicles, pickups and other authored locations. |
| Demonstrate file edits affect the game | `placement_data_scenarios` changes terrain height and actual prop population, loads edited buildings/trees/decorations/pedestrians/vehicles, checks spatial indexes, verifies six actual DX11 ground vertices at the edited position, moves a bridge mask and checks water queries. Invalid numeric/enum/type/geography/terrain replacements are rejected. The final placement regression passes. Shared terrain samples continue driving DX11 rendering and Jolt collision. |
| Deliver a runnable target and preserve progress | `native-root-final.txt` records a passing review of the delivered executable launched from a different working directory. `root-preserved-files.json` records the unchanged original save/settings hashes. The original save hash is `B1597CBDC25AFFDACFC0FC7C3019BD6DC401209137B5F15AE4DF4363314E4AF6`; settings hash is `B297CD8FE66BFB7CDBDD7E00340A0E6D4862D89B21DA378EA3F8308493774B32`. |

Placement totals: 65 buildings, 8,441 trees, 6,574 decorations, 109 pedestrians, 42 vehicles, 438 cached ground surfaces, 824 other ground surfaces, 38 beams, 173 visual props, 55 glows, 45 lights, 613 outcrop candidates and 18 editable plants. Runtime terrain/biome filters still determine which outcrop candidates appear. Terrain algorithms, mesh construction and relative visual details remain generic native code; authored map coordinates and shaping parameters are data.

## Verification

- `ctest-full.txt`: all 46 CTest entries passed in 536.28 seconds before the final help/scrolling/start-query refinements.
- `regression-rechecks.txt`: the final game simulation, placement, builder interaction and builder police suites passed. Its initial malformed-query fixture failed because it assigned a global rather than replacing an inline exported callback; the corrected test replaces the actual module field, and `mission-final.txt` records its passing rerun. Runtime gameplay code did not change to accommodate the fixture.
- `imgui-final.txt`: the final build menu review passed; `review-preserved-files.json` verifies byte-for-byte save/settings restoration.
- `build-delivered.txt`: successful final native build. A final review-fixture correction locates captures beside the executable instead of assuming the current working directory is the screenshot directory. The root review then passed from this evidence directory.
- `menus/`: fourteen retained native captures. Visual inspection covered the menu types, corrected F1 binding names and the Graphics/chest layouts at 1280×720; controls are readable, all nine inventory columns fit, and Close/Back remain accessible. Graphics options and long lists scroll.
- `runtime-data.json`: runtime file hashes and source/build agreement.

Run the game from the root executable, or build with `build.ps1`. Reproduce script/data tests with:

```powershell
ctest --test-dir build-msvc-ninja -R 'mission_script_scenarios|placement_data_scenarios' --output-on-failure
```

Reproduce native widget checks/captures with:

```powershell
.\MiniCity3D.exe --smoke --1080p --imgui-review
```

The review restores prior save/settings files after its Save/Load actions. Runtime contracts are in `data/README.md`; dependency pins/licenses are in `third_party/DEPENDENCIES.md`, and packaging copies both new licenses.

The optional DX12/FSR2 build retains its previous menu backend. This milestone develops and verifies the requested game changes in the Direct3D 11 `MiniCity3D` target. `renderer.cpp`, `textures.cpp` and the OpenGL target block were not edited. The wider animation/physics/LOD/architecture roadmap remains governed by its existing evidence and open items.
