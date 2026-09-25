# Mini City 3D

A playable C++ third-person city sandbox with a downtown grid, beach, harbor, pedestrians, vehicles, weapons, and six missions. The DX11 world now spans 16,800 × 16,800 units across two cities, countryside, snowfields, desert, and savanna, connected by roads and a river bridge. `MiniCity3D.exe` uses **Direct3D 11**. The earlier OpenGL renderer remains available as a fallback build.

The DX11 renderer has shaders, depth buffering, day/night lighting, directional shadows, fog, tone mapping, imported building, character, tree, car, and boat meshes, and tileable color and normal maps for building walls, foliage, roads, ground, vehicles, and clothing. Imported models use GPU instance buffers in the color and shadow passes. Streets, ground, and docks use a static GPU buffer; repeated boxes and spheres for props, lamps, and markers use GPU instances; distant vegetation, buildings, and pedestrians use coarse proxy meshes. Nearby characters use runtime joint skinning from the original GLB animations, including hit, death, fire, and reload transitions and a right-hand weapon attachment. Jolt Physics v5.6.0 handles movable props, the player capsule, dynamic vehicle chassis, boat buoyancy, and joint-constrained pedestrian ragdolls, with the visible character mesh following the ragdoll bodies. Selected CC0 source glTF/GLB files and baked runtime meshes are under [`assets/models/`](assets/models/); the PBR materials and licenses are under [`assets/materials/`](assets/materials/). The window defaults to 1600 × 900, with 1280 × 720 and 1920 × 1080 options. The minimap uses a directional player arrow, and the upper-right HUD shows the current weapon, ammo, health, cash, and time. The older top-down experiment remains in [`src/prototype2d.cpp`](src/prototype2d.cpp).

## Build and run

The C++ sources and headers are in `src/`. Initialize the pinned Jolt v5.6.0 submodule after cloning:

```powershell
git submodule update --init
```

With LLVM `clang++`, CMake, and Ninja on Windows:

```powershell
./build.ps1
./MiniCity3D.exe
```

To build the OpenGL fallback, run `./build.ps1 -OpenGL` and launch `MiniCity3DGL.exe`.

Or with a Windows C++ toolchain and CMake:

```powershell
cmake -S . -B build
cmake --build build --config Release
```

The game loads the `assets/` and `data/` folders beside the executable. CMake copies both folders after building. `build.ps1` builds the DX11 executable and Jolt-backed smoke test in `build-jolt-ninja/`, then copies the executable to the project root. Jolt is pinned to v5.6.0 under `third_party/JoltPhysics/`; its MIT license is included in packages. To regenerate baked meshes and skinning clips after editing a source GLB, run `python tools/convert_assets.py` with Python 3. The converter uses only the Python standard library. Converted static GLBs now export per-material `.pbr` ranges with base color, normal, metallic/roughness, occlusion, and emissive maps when those maps exist in the source. The 30 generated urban building atlases and most vegetation assets still contain mainly base color, so their map coverage remains future asset work.

Run `./package.ps1` to build the DX11 game, copy its executable with runtime assets, `data/`, and the Jolt license, smoke-test day, night, causeway, and each distant biome from the copied folder, and create a zip under `dist/`. Source GLBs and source texture archives are excluded from the player package. Use `./package.ps1 -SkipBuild` to package the latest existing build. `-IncludeOpenGL` includes the existing fallback executable without rebuilding it.

GitHub Actions builds and runs both CMake smoke tests on Windows for pull requests and `main` pushes. After a successful `main` build, it publishes the tested zip as a prerelease tagged `build-<run number>`. The workflow packages without graphical smoke runs on the hosted runner; local `./package.ps1` still runs those checks. `workflow_dispatch` builds a downloadable Actions artifact without publishing a release.

## Controls

| Input | Action |
| --- | --- |
| W / A / S / D | Move or drive |
| Shift | Run on foot |
| Space | Jump |
| S / Space while driving | Brake / handbrake for controlled drifts |
| Move mouse | Rotate the camera in every gameplay view without aiming |
| Hold right mouse button | Aim; sniper sight has 2×, 4×, and 8× wheel zoom |
| Left mouse button while aiming or driving | Fire; driving supports pistol and SMG drive-by shots |
| Left mouse button with a melee weapon | Swing at a nearby person |
| Space + left mouse button without aiming | Unarmed jump strike |
| F near a ladder or marked palm | Start climbing; W/S move, F lets go, Space jumps from a tree |
| C | Cycle close/wide first-person, near/far third-person, and overview cameras |
| B / mouse wheel | Open telescope / change telescope zoom (B closes it) |
| 1-9 / Q | Select or cycle unlocked weapons and tools |
| R | Reload the current weapon; restart after death |
| E | Enter or exit nearby vehicle |
| F / Tab | Use the selected nearby action / cycle nearby actions, including looting, shops, and houses |
| Arrow keys / Enter / Esc in shop or house menu | Select, buy or use, close |
| G | Carry or drop a body |
| M | Open or close the map |
| T | Advance time by one hour |
| Esc | Pause and open settings |
| F3 | Toggle frame rate and simulation timing |
| F4 | Debug menu: god mode, fly, full health, and all weapons |
| Space / Ctrl while flying | Rise / descend; Shift increases flight speed |

The player has 400 maximum health. Health kits restore 200 health, and mission rewards restore 80. Ballistic headshots instantly defeat pedestrians; torso, arm, and leg hits have different damage. Driven cars cause speed-dependent hit reactions, knockdowns, or ragdolls without pedestrian capsules pushing the vehicle. The DX11 player mesh has procedural swim, climb, and jump poses built on the imported rig.

Weapons and tools are picked up at cyan markers. Their stats are in `data/weapons.ini`; vehicle tuning, surfaces, police response, shops, houses, the world layout, pickups, and missions are in the other versioned files under `data/`. Green map markers show shops, purple markers show houses for sale, and blue markers show owned houses. Shops sell all 17 current weapons and tools, supplies, wanted-level reduction, and existing world vehicles while stock lasts. The six melee items attack at their own reach, damage, and speed; the bow fires visible arrows and can tether a killed person to a nearby wall for up to 15 seconds, including after a nearby save/load. Eligible pistols and SMGs unlock dual wield after 100 armed-opponent kills with that weapon, firing two shots per trigger and using two rounds. Owned houses have finite garage slots and offer fast travel to other owned houses when out of combat and missions. Purchases, parked cars, and house ownership are saved. Invalid required gameplay data stops startup with a logged error. A red car starts near the player; the sports car, motorcycle, and boats have different handling. Vehicles take damage from bullets, rockets, fire, and collisions; a nearby smoking vehicle can be repaired once with the starter repair kit by pressing F. Space applies the handbrake while driving, and a qualifying controlled drift pays once per segment. Players can swim into the harbor or leave a boat into the water. The flamethrower ignites surfaces, the extinguisher and water cannon suppress fire, and water cannon hits knock pedestrians down. Grass fire spreads, metal fire spreads very little, and trees can burn down. Pointing a gun at a nearby person can make them flee; witnessed crimes raise a wanted level of up to four stars and bring increasingly armed police. A silenced kill with no surviving witness does not raise it. The DX11 night sky shows a moon and nearby stars. Crates and barrels can be pushed and shot. Missions unlock in sequence; the gold map line points to the next start marker. The final mission combines a car checkpoint, a beach shooting target, and a boat trip. Graphics quality, draw distance, LOD distance, window size, vegetation, effects, DX11 shadows, mouse sensitivity, key bindings, and volume can be changed in the Escape menu and are saved in `settings.ini` next to the executable. Save and Load in that menu use `savegame.ini`; version 1 and 2 saves preserve their health percentage and migrate to version 3 on the next save. Mission rewards and weapon pickups also trigger a save. XAudio2 mixes overlapping sounds and places pedestrian shots, impacts, and traffic in stereo according to their distance and direction.

Run `./test.ps1` to compile and execute the simulation and asset smoke tests. They check all six missions in sequence, aiming geometry, pedestrian reactions and hostile bullets, thin-wall projectile collision and RPG impact placement, tree trunk hits and blast ignition, weapon data loading, reloads, prop damage, car and motorcycle travel, jump ascent and landing, nearby pedestrian capsule movement, skeletal asset data, save/load, and settings persistence. Bullet and RPG hits now stop at the first static wall contact and show a brief 3D impact flash; dynamic targets and tree trunks still use distance-based collision samples. `F3` shows frame and simulation timing, draw calls, active AI, Jolt building and pedestrian counts, ragdolls, fire, shots, and props. The application writes startup and error events to `MiniCity3D.log` beside the executable. For a repeatable local performance sample, run `MiniCity3D.exe --smoke --benchmark --1080p --day` or replace `--day` with `--night --ragdoll`; the average and p95 frame times over 120 frames are written to the log.

The expanded world measures 53.45 times the original 2,400 × 2,200 map rectangle. Its 30 textured tree variants and six cactus/rock props are assigned by biome in `data/trees.ini` and `data/biome_props.ini`; 36 textured bush variants form non-colliding undergrowth in 20 countryside groves. Forest trees vary from saplings to occasional 5× and 10× landmarks. Thirty textured urban building variants replace the old city kit blocks. Per-model provenance and license records are in `assets/models/NATURE_MANIFEST.csv` and `assets/models/CITY_MANIFEST.csv`. A harbor causeway and 16 data-defined roads link both cities to snow, desert, and savanna. Four biome hubs have pedestrians, parked vehicles, shops, and small solid shop and house structures. Fourteen houses are for sale across the map, with a ten-house ownership limit and fast travel between owned houses. Jolt tire grip responds to asphalt, grass, sand, snow, and precipitation using values in `data/vehicles.ini`. The terrain mesh follows the player locally, nearby trees are queried from a cell index, and distant wandering pedestrians sleep until approached. Regional trees and props are generated deterministically at startup, while the second city has pedestrians and vehicles. The full map and local minimap show the enlarged world.

Grass terrain in the city, countryside, and savanna now has deterministic, low-poly tufts within 140 world units of the player. The patch recenters every 20 units, avoids roads, water, buildings, and non-grass biomes, and follows the Vegetation setting. DX11 fire, muzzle, impact, smoke, and explosion visuals use one effect handler with authored procedural textures and a blended depth-tested pass; explosions include a flash, expanding ring, flame burst, and smoke. Ballistic projectiles use a small 3D bullet mesh and travel roughly four times faster; rockets, arrows, and spray tools retain their own speeds. Run `--smoke --day --forest-fire` or `--smoke --day --effects-preview` to inspect the new effects. The source texture generator is `tools/make_effect_textures.ps1`.

## DX11 visual quality

The DX11 scene now renders to a floating-point color target and runs a final pass with spatial edge smoothing, depth-based ambient occlusion, restrained bloom, exposure, tone mapping, and color grading by biome and time of day. The sky gradient and cloud tint shift through sunrise, daylight, sunset, and night. Converted static GLBs retain separate glTF material ranges and their base-color, normal, metallic/roughness, occlusion, and emissive textures when available; `tools/convert_assets.py` writes adjacent `.pbr` metadata. Procedural road, ground, foliage, and vehicle surfaces use material roughness and metalness defaults. The HUD is composited after post-processing to keep text sharp.

Rain darkens roads, lowers their roughness, and creates bounded irregular puddles on downtown and regional roads. Puddles use a short screen-space reflection trace with a sky fallback; snow covers upward-facing nearby surfaces. At night, selected building windows, street lamps, shop signs, and vehicle head and tail lights glow, while the nearest light sources affect surrounding geometry. These are screen-space and local approximations: reflections cannot include off-screen objects, ambient occlusion is depth based, and generated urban and nature atlases do not yet include complete PBR map sets. For repeatable captures use `--smoke --sunrise`, `--smoke --night`, `--smoke --day --rain`, or `--smoke --day --snowfield --snow`, optionally with `--1080p` and `--benchmark`.

## Marina Part district

The expanded world now includes a Marina Part-inspired Danube neighborhood with 15 apartment blocks, curved balcony and terraced building variants, a paved promenade, planted strips, benches, lights, local streets, Foka Bay, three piers, and marina boats. The full map and minimap show the district and bay. `data/regions.ini` and `data/roads.ini` define the district and its streets. `python tools/build_marina_assets.py` regenerates four original facade meshes, their LODs, and 2048 × 2048 color and normal atlases; see [asset provenance](assets/models/MARINA_PART.md).

On the DX11 High and Medium graphics settings, hull and domain shaders tessellate nearby building facades, Marina pavement, and untextured tree meshes in the color and shadow passes; detailed imported foliage skips tessellation. Tessellation decreases with distance and is disabled on Low. The small geometric displacement is visual; Jolt collision continues to use the base geometry. Use `MiniCity3D.exe --smoke --marina --day --1080p` to render a repeatable close view and add `--benchmark` for a 120-frame performance sample.

## Current limits

The expanded map streams nearby Jolt building colliders and pedestrian capsules. World definitions and mutable region state still load globally; wilderness traffic and missions are limited.

This is a prototype, not a finished GTA-style game. Timed weather changes clouds, rain or snow particles, fog visibility, tree sway, and fire spread; rain wets and suppresses active fire. Four authored downtown ladders and additional tall-building ladders lead to Jolt-supported roofs, and four marked palms and suitable regional trees have climb anchors; small species remain decorative. Cars and motorcycles use Jolt wheel suspension and drivetrain controls; the motorcycle uses a narrow four-wheel physics surrogate, and boats keep buoyancy controls. Fall and vehicle-entry action clips are adapted from the available source animations. Repeated effects and scenery primitives are GPU instanced; deforming characters and ragdolls still use dynamic CPU vertices. Nearby pedestrians use Jolt capsules, while distant pedestrians use simpler game movement. Character and nature art remain stylized and need further polish. A human mission playthrough on a clean Windows machine remains unverified. At 1080p with far draw distance and near LOD on the development machine, 120-frame hidden-window samples measured east city 14.78 ms average / 16.92 ms p95, bridge 6.94 / 8.23 ms, and countryside fire 9.29 / 11.43 ms. Broader hardware and interactive gameplay still need checking.

