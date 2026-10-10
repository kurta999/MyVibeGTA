# Mini City 3D

A playable Windows city sandbox written in C++17, with **native Direct3D 12**, **AMD FSR2**, **Jolt Physics**, and **XAudio2**. Explore two cities, a beach and harbor, countryside forests, snowfields, desert, savanna, and a Marina Part-inspired waterfront district. Regional terrain includes mountains, rocky ridges, rolling hills, and dry basins with matching Jolt collision; [terrain assets and rebuild instructions](assets/models/TERRAIN.md) document the imported CC0 rocks.

The project is an evolving prototype. Implementation and verification status are tracked in [idea.md](idea.md) and the [graphics upgrade plan](graphics-upgrade-plan.md).

The ongoing codebase refactor and current module boundaries are documented in [Architecture](ARCHITECTURE.md).

For an in-depth explanation of the engine, see the [Technical Guide](TECHNICAL_README.md). It follows models and vertices from asset files through Direct3D 11 rendering, mipmaps, lighting, animation, camera movement, and collision, with graphics concepts explained for experienced programmers new to the subject.

## Contents

- [Features](#features)
- [Screenshots](#screenshots)
- [Build and run](#build-and-run)
- [Controls](#controls)
- [Animals and destructible trees](#animals-and-destructible-trees)
- [Tests and visual previews](#tests-and-visual-previews)
- [Packaging](#packaging)
- [Project layout](#project-layout)
- [Technical Guide](TECHNICAL_README.md)
- [Assets and attribution](#assets-and-attribution)
- [Vehicle expansion controls and configuration](#vehicle-expansion-controls-and-configuration)
- [Explosives, hiding and underwater controls](#explosives-hiding-and-underwater-controls)
- [Current limitations](#current-limitations)

## Features

- **World:** 16,800 x 16,800 world units, connected roads, a river bridge, biome hubs, shops, houses, and a waterfront neighborhood with piers and boats.
- **Rendering:** imported textured meshes, PBR material ranges, compressed DDS materials and mipmaps, HDR lighting, day/night cycles, sun and selected local-light shadows, bloom, SSAO, screen-space reflections with local-probe fallback, filmic tone mapping with bounded automatic exposure, and selectable FXAA/TAA. Supported scenery uses GPU instancing and geometry LOD.
- **Animation:** runtime skeletal clips for humanoids, GPU deformation for ordinary humanoid clips, procedural seated/swimming/climbing poses, and visible Jolt pedestrian ragdolls.
- **Physics:** a capsule player controller, nearby pedestrian controllers, vehicle chassis with suspension, boat buoyancy, movable props, animal bodies, solid tree trunks, and physical tree fragments.
- **Vehicles:** 13 authored vehicle kinds, including cars, sports cars, motorcycles, boats, helicopters, skateboards, bicycles, tractors, combines with rotating cutting augers, tanks with independently aimed turrets and barrels, trucks with detachable physical trailers, and fixed-wing airplanes with thrust/lift/drag/stall and landing gear. Vehicles show dirt and paint scratches and break into colliding imported mesh parts when destroyed. [Vehicle sources and licenses](assets/models/VEHICLE_EXPANSION.md) include the private-use combine and noncommercial trailer.
- **Audio and radio:** continuous recorded engine loops, distinct weapon effects, and ten live SomaFM presets. Mouse-wheel scrolling switches stations while driving; `data/radio.ini` accepts additional direct MP3/AAC stream URLs.
- **Destruction:** RPGs, tank shells and explosive devices cut visible building holes and update collision for the current session. Building blasts eject physical bricks, hollow blocks, concrete chunks and exposed-rebar slabs that tumble and settle.
- **Display:** windowed, borderless and exclusive fullscreen settings, saved resolution, and Alt+Enter switching, with borderless fallback when exclusive mode is unavailable.
- **Wildlife:** 15 animal species, rideable elephants and tigers, and five bird species. Animals wander, play, flee, retaliate, and hunt according to species.
- **Gameplay:** 27 weapons and tools, including C4/remote detonators, frag and smoke grenades, molotovs, flashbangs and timed bombs; six main missions, four regional missions, wanted levels, police, traffic, driver retaliation, civilian conversations and fights, looting, shops, house ownership, and garages. Hideout corners let an unseen stationary player shed wanted stars; rear knife takedowns use witness-facing and visibility checks.
- **Environment:** changing weather, volumetric cumulus clouds, cirrus, stars and moon, rain and snow, fire spread, burning trees and vehicles, swimming with dive/surface controls, ladders, and tree climbing.
- **Grass:** textured 2K foliage with curved blades, wind, and distinct lawn, meadow, savanna, desert, snow, and coastal variants. **Esc → Graphics → Grass distance** controls the radius from **Off** to **800 world units** (default **230**). The separate **Grass LOD** slider extends detailed blades from **40 to 800 units** (default **95**) and pushes the next LOD transition farther out. Its visible extent is capped by Grass distance. Vegetation density selects Off/Medium/High independently; both distance controls are saved automatically. Larger detail radii draw more blade geometry and near grass shadows.
- **Persistence:** saved progression, inventory, ownership, living wildlife damage and position, tree destruction, and graphics/control/audio settings.
- **Debug tools:** F4 provides god mode, flight, health and equipment actions, infinite ammo, cash grants, wanted reset, and ammo grants. F3 shows performance information and F11 captures screenshots.

The latest additions are documented in the [vehicle](#vehicle-expansion-controls-and-configuration) and [explosives](#explosives-hiding-and-underwater-controls) sections. Recorded verification from **2026-10-04** includes all **23 CTest entries passing** and inspected DX11 captures for [explosives and underwater controls](evidence/ordnance-20261004/README.md) and [masonry debris and tank aiming](evidence/masonry-tank-20261004/README.md). These are tested milestones within an unfinished prototype; see [current limitations](#current-limitations).

## Screenshots

Grass source credits and reproducible import steps are in [GRASS.md](assets/models/GRASS.md). Run `./tools/verify_grass.ps1` for 1080p surface/distance/LOD/menu captures, or add `-Benchmark` to compare Off/default/maximum draw distance. Use `-Capture lod-default` or `-Capture lod-max` to compare the detailed grass radius. The `grass_scenarios` test checks 2K PBR assets, transparency, rooted wind weights, surface exclusions, deterministic placement, density, distance disabling, and independently adjustable LOD selection without changing roots. The simulation suite verifies both grass settings survive save/load.

**Waterfront at sunset**

![The player exploring the waterfront at sunset, with buildings, pedestrians, piers, and boats](github_screenshots/beach.png)

**Driving through the city at night**

![A car at a city intersection at night, with headlights and the vehicle damage HUD visible](github_screenshots/driving.png)

**Countryside wildlife**

![The player approaching a group of countryside animals, including an elephant, tiger, and bear](github_screenshots/wildlife.png)

**Animals in the forest**

![The player encountering a cow and pig among forest trees](github_screenshots/animals.png)

**Riding a tiger**

![The player riding a tiger through the countryside at dusk](github_screenshots/animal_riding.png)

**City combat**

![The player aiming a pistol during city combat at sunset, with wanted stars visible](github_screenshots/shooting.png)

**Hit effects**

![An armed pedestrian shooting the player, with a blood hit effect visible](github_screenshots/blood.png)

## Build and run

### Requirements

- Windows with a Direct3D 11-capable graphics device.
- Visual Studio 2022 or 2026 with **Desktop development with C++** and a Windows SDK, or a compatible LLVM/Clang toolchain with Ninja.
- CMake 3.20 or newer. The build script can use the CMake installation bundled with Visual Studio.
- Git and the pinned **Jolt Physics v5.6.0** submodule.

### Quick start

From a checkout of this repository:

```powershell
git submodule update --init --recursive
./build.ps1 -RunTests
./MiniCity3D.exe
```

`build.ps1` configures the compiler environment, builds the Direct3D game and test executables, copies runtime assets and data, runs tests when requested, and places `MiniCity3D.exe` in the repository root. Close a running root executable before rebuilding it.

To build without running tests:

```powershell
./build.ps1
```

To choose a different output executable:

```powershell
./build.ps1 -OutputPath 'build-output/MiniCity3D.exe'
```

Create the destination directory first and keep `assets/` and `data/` beside a relocated executable. The build directory already contains both folders.

### Native DX12 and AMD FSR2

`MiniCity3D` creates a native D3D12 device and flip-model swapchain. Its rendering passes record D3D12 graphics/compute commands with explicit resource barriers, descriptor heaps, cached pipeline state objects, upload/readback buffers, and GPU fences. It does not use D3D11On12 or a D3D11 device. The older DX11 source remains for reference; CPU scene/asset modules retain their historical `dx11_` names. The OpenGL fallback is unchanged.

In **Esc → Graphics**, select **AMD FSR2** and adjust **FSR2 sharpness** from 0–100%. Modes apply immediately and persist in `settings.ini` as `FSR2=0..4` and `FSR2Sharpness=0..100`.

| Mode | Render resolution at 1920 × 1080 | Linear scale |
| --- | --- | --- |
| Off | 1920 × 1080 | Native |
| Quality (default) | 1280 × 720 | 1.5× |
| Balanced | 1129 × 635 | 1.7× |
| Performance | 960 × 540 | 2× |
| Ultra Performance | 640 × 360 | 3× |

FSR2 uses jittered scene color, depth, motion vectors, and a reactive mask. It replaces FXAA/TAA while enabled; HUD and menus are composed afterward at display resolution. Quality/resolution changes and camera cuts reset temporal history. This is temporal upscaling, not frame generation. Native DX12 preserves the existing shadow, HDR, post-processing, GPU skinning, instancing and LOD passes.

CMake fetches AMD's MIT-licensed **FSR2 2.2.1**, pinned at `1680d1edd5c034f88ebbbb793d8b88f8842cf804`, and builds its DX12 backend and shader permutations. The first build needs internet access and takes longer to compile the permutations. FSR2 is linked statically; no FSR DLL needs to ship. Packages include `AMD-FSR2-LICENSE.txt`. Use a current Windows SDK and Visual Studio x64 C++ tools; FSR2 needs Shader Model 6.2 support.

`dx12_fsr2_smoke` verifies all quality modes, resize/history reset, and reconstructed GPU pixels using the real AMD dispatch. `--dx12-debug` enables the D3D12 debug layer when Windows Graphics Tools is installed. Game checks accept `--fsr2=0` through `--fsr2=4`, `--fsr2-cycle`, and `--capture-converged` (with `--smoke --benchmark --benchmark-short`).

DX12 overlaps CPU preparation with GPU execution using two fence-protected recording slots. Upload memory is reused after both GPU work and live buffer references release it; texture descriptor tables are cached, and scene vertices copy directly into upload memory using the CPU worker pool. `--scene-workers=1` selects the serial preparation path; command-list recording remains on the main thread. `--dx12-sync` restores a full GPU wait per frame for diagnostics.

For matched API benchmarks, configure with `-DMINI_CITY_BUILD_DX11_BENCHMARK=ON` and build the optional `MiniCity3D-DX11-benchmark` target. Run both executables from the same build directory with `--smoke --benchmark`, adding `--fsr2=0` to DX12 for native-resolution comparisons. Keep settings identical and disable debug validation and screenshots while measuring. Results and remaining limits are in [DX12 performance evidence](evidence/dx12-performance-20261009/README.md).

### CMake directly

For a Visual Studio 2022 installation:

```powershell
cmake -S . -B build -G 'Visual Studio 17 2022' -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
./build/Release/MiniCity3D.exe
```

CMake copies `assets/` and `data/` beside the game. Baked runtime assets are included; Python and Blender are needed only when rebuilding or converting art.

### Startup feedback

The Direct3D executable immediately opens a loading window with a progress bar, current stage, and elapsed time. It remains responsive while graphics initialize; closing it cancels at the next startup checkpoint. The game appears after its first frame is prepared. `--smoke` runs keep the loading window hidden.

Startup prepares regional meshes and textures in advance to avoid uploads interrupting travel. Independent image decoding, mipmap generation, and graphics shader compilation now use a bounded CPU worker pool. The default uses up to four workers, reserving a logical CPU for the main thread when possible. GPU uploads and renderer cache updates remain on the main thread, and loading workers finish before gameplay starts. Closing the loader cancels pending work and joins the workers before cleanup.

Use `--loader-workers=1` for the serial reference, or `--loader-workers=2` through `--loader-workers=8` to select a worker count. Each stage and total startup time are logged in `MiniCity3D.log`, including decode/mipmap/upload timings and actual peak concurrency. `--validate-loading` additionally checksums the prepared textures and shader bytecode; hashing adds work, so leave it off for performance measurements. Measured results and verification are in [threading evidence](evidence/threading-20260928/README.md).

To repeat matched startup measurements (three interleaved runs per worker count):

```powershell
./tools/measure_threading.ps1
```

`-Route` runs the six-scene 3,600-frame 1080p benchmark, while `-Travel` checks regional jumps. Smoke runs use a fixed random seed. The benchmark reports scene preparation, CPU deformation, grass rebuilds, uploads/culling, draw/HUD submission, and presentation separately.

During gameplay a persistent pool also handles large CPU character/ragdoll deformation loops and grass-cache rebuild rows. Workers read the frame's unchanged world state and write separate preallocated output slots or private rows. The main thread joins them before merging results, uploading, or updating simulation again. Small deformation loops remain inline; ordinary humanoid clip deformation still runs on the GPU. `--scene-workers=1` selects serial scene preparation, and overrides accept 1–8 workers. Jolt physics and gameplay updates remain single threaded.

To compare runtime scene workers while holding loading at four workers:

```powershell
./tools/measure_threading.ps1 -CompareScene -Travel -Workers 1,4
./tools/measure_threading.ps1 -CompareScene -Route -Repeats 1 -Workers 1,4
```

The game embeds an original city icon at 16, 24, 32, 48, 64, 128, and 256 pixels for Explorer, the taskbar, and window captions. The resource is in `assets/app/`; regenerate it with `./tools/build_game_icon.ps1`.

`MiniCity3D` is the actively developed Direct3D target. The existing OpenGL fallback remains available through `./build.ps1 -OpenGL` as `MiniCity3DGL.exe`.

## Controls

These are the default bindings. Movement, sprint, vehicle interaction, sensitivity, and inverted mouse look can be changed under **Esc > Controls**.

| Input | Action |
| --- | --- |
| W / A / S / D | Move, drive, or ride an animal relative to the camera |
| Shift | Run on foot or while riding |
| Hold Alt | Walk slowly |
| Left Ctrl | Toggle crouch |
| Space | Jump; handbrake while driving |
| S while driving | Brake or reverse |
| Mouse | Rotate the camera; aim the tank turret and elevate its barrel independently of the hull |
| Hold right mouse button | Aim |
| Left mouse button | Fire, use melee/tools, throw the selected device, or open timed-bomb placement; dive while swimming |
| Right mouse button while swimming | Rise toward the surface; release both mouse buttons to hold depth |
| Space + left mouse button without aiming | Unarmed punch or jump strike |
| R | Reload; restart after death |
| 1-9 / Q | Select or cycle unlocked weapons and tools |
| E | Enter/exit a vehicle; mount/dismount a nearby tiger or elephant |
| J in a truck | Attach/detach a nearby trailer at the rear hitch |
| X | Detonate all active C4 charges |
| K with the knife selected | Perform a rear takedown when prompted |
| F / Tab | Use the selected nearby action / cycle nearby actions |
| G | Carry or drop a corpse |
| F near a ladder or climbable tree | Start climbing; W/S climb, F releases, Space jumps from a tree |
| H while driving | Toggle car or motorcycle headlights |
| C | Cycle first-person, third-person, and overview cameras |
| B / mouse wheel | Telescope / telescope or sniper zoom |
| Mouse wheel in a powered vehicle | Switch live radio stations or select Off |
| M | Toggle the map |
| T | Advance time by one hour |
| Esc | Pause, settings, Save, and Load |
| F1 | Toggle the full controls overlay |
| F3 | Toggle performance information |
| F4 | Open the debug menu |
| F11 | Save a PNG screenshot |
| Alt+Enter | Toggle windowed/borderless display |

Three helicopters are parked at **(170, 250)** downtown, **(1550, 1690)** near the beach, and **(2070, 1700)** near the harbor. Enter with **E**, use **W/S** for forward/reverse flight, **A/D** for yaw, **Space** to ascend, and hold **Ctrl** to descend. Releasing lift holds altitude after rotor spin-up. Landing uses the same solid-world collision and damage rules as other vehicles; exiting in the air starts a fall.

The **minigun** fires 25 rounds per second with a 300-round magazine and a 3.2-second reload. The **shovel** uses the melee swing animation and does not consume ammo. The **grapple hook** aims at solid buildings or trees within 700 units: hold **LMB** to launch and reel in, use **WASD** to steer, and release **LMB** to detach with momentum. It has a half-second cooldown and eight-second attachment limit, rejects sky/ground and dynamic blockers, and releases at obstructions. It is a traversal grapple, with no object-to-object tether mode. All three are sold in shops and have pickups near the starting intersection; **Q** cycles unlocked tools, and **F4** can equip any catalog weapon. The radar and map use original pictograms for weapons, tools, shops, houses, vehicles, and missions.

Shops and house menus use arrow keys, Enter, and Esc. In debug fly mode, Space/Ctrl move vertically and Shift increases speed.

### Getting started

A car starts near the player. Follow the gold map line to the next mission marker and press **F**. Cyan markers provide weapons, green markers identify shops, purple markers identify houses for sale, and blue markers identify owned houses. Orange markers identify regional missions.

Graphics, controls, and volume are saved in `settings.ini`. Save/Load use `savegame.ini`. These files, `MiniCity3D.log`, and the `screenshots/` directory are located beside the executable. Mission rewards and weapon pickups also trigger saves.

In the DX11 game, autosaves capture a snapshot on the gameplay thread and write it on a dedicated worker. One active write and one pending snapshot bound the queue; newer requests replace an unwritten pending snapshot. The worker writes a complete temporary INI and replaces the destination after flushing it. Manual Save waits for completion, Load waits for pending writes, and normal shutdown drains them. Failed autosaves report a message while preserving the previous save. Sound effects reuse PCM prepared during startup, including four takes for noise-based sounds, and held-weapon graphics are prepared before gameplay. See [pickup hitch measurements](evidence/pickup-hitches-20260928/README.md).

## Animals and destructible trees

Twenty countryside groves contain tigers, elephants, cats, dogs, pigs, cows, capybaras, bears, goats, donkeys, roe deer, deer, weasels, beavers, and mice. Grove centers use X = 3000/4000/5000/6000/7000 and Z = 1300/3900/6400/9000. Each grove has up to eight animals with stable save IDs; distant wildlife sleeps until approached.

- Living animals have Jolt collision bodies. Characters meet their bodies, and vehicle impacts can damage or kill them. Very small animals can be stepped over.
- Press **E** near a living tiger or elephant, then use **WASD** and **Shift**. Movement checks the animal's long, narrow footprint so forest gaps remain usable. Mounting recovers positions from older saves that overlap a trunk.
- Ridden animals can cross roads and stop at water, buildings, trunks, props, vehicles, and other animals. **E** dismounts into clear space. Mounted combat is disabled; loading returns the rider to the ground.
- Tree trunks block characters and vehicles. Low-speed car impacts stop at the trunk; sufficiently hard impacts break the tree into trunk, branch, and foliage pieces that fall, collide, and settle. Breakage depends on closing speed, vehicle mass, tree scale, and tree health.
- Tree fragments are temporary, capped physics objects. The destroyed tree remains a stump, and its destruction is saved.
- Use weapons or explosives to hunt, **F** to loot once, and **G** to carry/drop a corpse. Living wildlife damage and position persist across saves. Dead pedestrians, animals, birds, carried bodies, ragdoll poses, and arrow pins are session-only; old save corpse records are ignored. Money and weapon progression still persist. Carrying prevents firing and reloading.

Animal gait, play, attack, and corpse poses are procedural. The roe-deer model is an adapted fawn; see [animal attribution](assets/models/ANIMALS.md).

To repeat the equipment visuals, run `./tools/verify_equipment.ps1` after building. It writes Direct3D captures to `evidence/equipment-20261003/`. The `equipment_scenarios` CTest checks flight at 30/60 Hz, hover/landing/airborne exits, wall collisions, minigun cadence/reload, shovel damage and poses, grapple cover/reeling/release, and corpse-free legacy save loading.

The Direct3D sky renders at the window's native resolution. It replaces the repeating sky pattern and opaque cloud spheres with an analytic atmosphere, wind-driven volumetric cumulus clouds, thin cirrus, and angular stars/moon. Cloud coverage follows weather, lighting follows time of day, and scene depth clips clouds against geometry when flying into or above the layer. Low/Medium/High graphics quality traces 24/36/48 cloud samples with early opacity termination and three sun-shadow samples. This is an approximation with Rayleigh/Mie-inspired sky coloration, not a complete physical atmosphere or cloud shadow system for terrain. The rendering approach follows [Guerrilla's Horizon cloud presentation](https://www.guerrilla-games.com/read/the-real-time-volumetric-cloudscapes-of-horizon-zero-dawn) and [Epic's volumetric cloud overview](https://dev.epicgames.com/documentation/en-us/unreal-engine/volumetric-clouds?application_version=4.27).

**F4 → Infinite ammo** enables sustained fire without consuming magazines or reserves, including empty weapons. The HUD shows **INFINITE AMMO**; disabling the toggle restores the original inventory. The toggle is session-only and resets when loading/resetting the game. Vehicle destruction and explosive projectiles play a distinct positional blast with a crack, bass impact, and debris tail.

Run `./tools/verify_sky.ps1` for day, zenith, overcast, sunset, night, rain, high-altitude, and F4-menu captures in `evidence/sky-20261003/`, or add `-Benchmark` for a 120-frame 1080p sample. The `sky_smoke` test renders and reads GPU pixels to check periodic noise, coverage, wind, night illumination, altitude transitions, and foreground clipping. The equipment and XAudio2 suites also cover the new ammo mode and cached explosion effect.

## Tests and visual previews

Run the complete build and CTest suite:

```powershell
./build.ps1 -RunTests
# Equivalent wrapper:
./test.ps1
```

The 23 CTest entries cover the main simulation, vehicle expansion, equipment, ordnance, wildlife, birds, drivers, vehicle collisions, pedestrians, traffic, navigation, grass, serial/parallel scene jobs, autosaves and background save jobs, assets, texture mips and loading, shader loading, reflection probe data, audio, GPU sky rendering, and loading-window responsiveness, cancellation, lifecycle, and embedded icon sizes. Collision scenarios exercise animal bodies, riding through narrow gaps, old-position recovery, low/high-impact tree crashes, physical fragments, and collider cleanup. Expansion and ordnance scenarios cover independent tank aiming, rubble settling and expiry, building-hole traversal, trailer coupling, flight, explosive devices, hiding, takedown witnesses, and underwater controls. See the [recorded full-suite result](evidence/masonry-tank-20261004/ctest.txt).

To rerun focused scenarios after building with Ninja:

```powershell
ctest --test-dir build-msvc-ninja -C Release -R 'wildlife_scenarios|vehicle_collision_scenarios' --output-on-failure
ctest --test-dir build-msvc-ninja -C Release -R 'expansion_scenarios|equipment_scenarios|ordnance_scenarios|grass_scenarios' --output-on-failure
```

Use the build directory produced by your generator. To reproduce the latest DX11 captures after building:

```powershell
./tools/verify_vehicle_expansion.ps1
./tools/verify_ordnance.ps1
./tools/verify_masonry_tank.ps1
```

Visual smoke flags stage repeatable scenes and exit after rendering:

```powershell
./MiniCity3D.exe --smoke --day --animals --animal-ride --animal-ride-move --screenshot
./MiniCity3D.exe --smoke --day --animals --animal-ride --animal-ride-move --tiger --screenshot
./MiniCity3D.exe --smoke --day --tree-impact-preview --1080p --screenshot
./MiniCity3D.exe --smoke --day --tree-impact-preview --settled --1080p --screenshot
./MiniCity3D.exe --smoke --night --driver-preview --damaged --screenshot
./MiniCity3D.exe --smoke --day --marina --1080p --screenshot
```

Smoke mode uses staged test state. To record performance, use `--smoke --benchmark --1080p --day`, or `--smoke --benchmark-route --1080p` for the deterministic six-segment route. Timings are written to `MiniCity3D.log`; local measurements do not establish performance on other hardware.

## Packaging

```powershell
./package.ps1
```

This builds, copies cooked assets, data, attribution records, and the Jolt license, runs packaged graphical smoke checks, and creates a ZIP under `dist/`. Raw model sources are excluded.

Use `./package.ps1 -SkipBuild` to package an existing build. `-SkipSmoke` skips graphical checks; `-IncludeOpenGL` includes an existing fallback executable.

The [Windows workflow](.github/workflows/windows-release.yml) builds and tests pull requests and pushes to `main`. It uploads a downloadable package; successful `main` pushes also publish a prerelease tagged `build-<run number>`. Hosted CI skips graphical package smoke runs. A manual workflow run uploads an artifact without publishing a release.

## Project layout

| Path | Purpose |
| --- | --- |
| [`src/`](src/) | C++ simulation, rendering, input, audio, and physics |
| [`tests/`](tests/) | Deterministic scenarios and asset/pipeline checks |
| [`data/`](data/README.md) | Versioned gameplay and world configuration |
| [`assets/models/`](assets/models/) | Original sources, baked meshes, textures, and provenance |
| [`assets/materials/`](assets/materials/) | Material textures and licenses |
| [`assets/lighting/`](assets/lighting/README.md) | Cooked reflection probes and lighting data |
| [`tools/`](tools/) | Asset import, conversion, generation, and cooking scripts |
| [`third_party/JoltPhysics/`](third_party/JoltPhysics/) | Pinned physics submodule |
| [`idea.md`](idea.md) | Implementation roadmap and verified milestones |
| [`graphics-upgrade-plan.md`](graphics-upgrade-plan.md) | Rendering roadmap and open work |
| [`TECHNICAL_README.md`](TECHNICAL_README.md) | Detailed DX11 engine, asset, rendering, camera, and collision guide |
| [`traffic-ai.md`](traffic-ai.md) | Traffic and retaliation behavior |

The older top-down experiment is preserved in `src/prototype2d.cpp`.

## Assets and attribution

Third-party assets have individual licenses; attribution is recorded alongside their sources and baked files.

- [Model licenses](assets/models/LICENSES.md), [nature manifest](assets/models/NATURE_MANIFEST.csv), and [city manifest](assets/models/CITY_MANIFEST.csv).
- [Animals](assets/models/ANIMALS.md) and [birds](assets/models/BIRDS.md): Poly by Google assets via Poly Pizza, under CC BY 3.0, with modification notes.
- [Traffic and weapon licenses](assets/models/TRAFFIC_WEAPONS_LICENSES.md).
- [Vehicle expansion sources and licenses](assets/models/VEHICLE_EXPANSION.md), [recorded audio credits](assets/audio/README.md), and [C4 source manifest](assets/models/source/explosives/manifest.json).
- [Marina Part provenance](assets/models/MARINA_PART.md) and [showcase sources](assets/models/SHOWCASE_SOURCES.md).
- [Material licenses](assets/materials/LICENSES.md) and [effect licenses](assets/effects/LICENSES.md).
- Jolt Physics: MIT license in `third_party/JoltPhysics/LICENSE`, copied into packages.

Most environment and original character/vehicle sources are CC0; consult each record when redistributing assets.

For art changes, see the [asset conversion workflow](tools/ASSET_PIPELINE.md) and [probe lighting workflow](tools/PROBE_LIGHTING.md). `python tools/convert_assets.py` rebuilds the supported runtime assets. Animal imports use `python tools/import_animals.py` with Pillow.

The large `island_tree_03.bin` and `jacaranda_tree.bin` source files are omitted from Git; their baked meshes are included. Restore those source files before recooking them:

```powershell
./tools/fetch_city_sources.ps1 -AssetIds island_tree_03,jacaranda_tree
```

## Vehicle expansion controls and configuration

Use **E** to enter/exit. **W/S** accelerate/brake ground vehicles; the combine's
actual corn-header cutting auger turns while moving. In a truck, stop with its
rear hitch close to the trailer and press **J** to attach/detach. In a tank,
the **mouse** aims the turret/barrel independently of driving, and **LMB** fires
from the moving barrel tip. Trailer coupling and fragments use Jolt rigid bodies.

In an airplane, **W/S** adjust throttle, **Space/Ctrl** pitch, **A/D** bank and
**Q/R** operate the rudder. Gain airspeed before pulling up; excessive angle of
attack reduces lift. Helicopter controls remain unchanged. The skateboard and
bicycle have no engine audio or radio.

Scroll the **mouse wheel** while driving a powered vehicle to choose a station
or **Off**. `data/radio.ini` ships ten SomaFM stations: Groove Salad, Drone Zone,
Indie Pop Rocks!, Groove Salad Classic, Deep Space One, Space Station Soma,
Secret Agent, Lush, Underground 80s and Left Coast 70s. Append `[Station10]`
with unique `Id`, `Name` and a direct `URL`, and increment `[Radio] Count`.
Restart to reload the file. Websites, HLS playlists and authentication pages
are not direct audio streams. Connection errors appear in the HUD; decoding
is asynchronous. Streaming stops on pause/exit and follows master volume.
[Recorded engine sound credits](assets/audio/README.md) accompany the clips.

**Esc → Graphics → Display mode** selects Windowed, Borderless or Fullscreen.
**Alt+Enter** toggles windowed/borderless. Resolution and display mode persist
in `settings.ini` and `savegame.ini`, along with the radio station. Exclusive
fullscreen leaves on focus loss and retries on return; unavailable DXGI
exclusive mode falls back to borderless. The desktop verification confirmed
windowed/borderless and fallback; this Windows session rejected actual exclusive
mode with `DXGI_ERROR_NOT_CURRENTLY_AVAILABLE`.

RPG/tank and explosive-device blasts remove building volume and collision, including projectile,
character, camera and navigation checks. Surviving facade triangles keep their
UVs/materials. Holes and debris reset on load/new game and are omitted from the
save. Cuts are bounded axis-aligned volumes with matching compound collision;
this is not a structural building-collapse simulation. Vehicle debris uses
imported mesh assemblies with box collision approximations and an 18-second
lifetime; the active debris count is bounded.

In a tank, moving the mouse turns the turret independently of the driving
direction and elevates the barrel within its mechanical limits. LMB fires
from the moving barrel tip along its actual orientation. Tank camera aim stays
where it is pointed while driving, without automatic recentering toward the
hull. Joint poses update after Jolt's chassis movement. Unoccupied tanks
keep their last turret direction. `expansion_scenarios` covers aiming, shots,
rubble settling/cleanup and building-hole traversal;
`tools/verify_masonry_tank.ps1` captures daytime DX11 previews.

New spawns (`data/world.ini`, X/Z): skateboard 410/250, bicycle 500/250,
tractor 4200/4500, combine 4400/4500, tank 1500/120, truck 1500/250,
trailer 1360/250, airplane 4500/4900. New vehicles also appear when loading
an older save whose vehicle list predates this expansion.

`tools/verify_vehicle_expansion.ps1` captures the vehicle lineup, combine,
destruction and airborne plane in Direct3D 11. `--smoke --display-preview`
checks real window styles and DXGI mode/fallback transitions without saving.

DX11 weapon pickups and the HUD now share 27 transparent weapon/tool textures, replacing the cyan pickup spheres. [Icon sources and generation prompt](assets/icons/weapons/README.md) are included. Cars, sports cars, and helicopters render at twice their previous dimensions, with matching Jolt chassis, wheels, lamps, seating, camera framing, entry/exit clearance, and oriented projectile hit volumes. Traffic spacing and intersection yielding account for the larger cars. Fixed-wing flight uses force-based arcade dynamics with thrust, lift, drag, stall and landing gear; it is not a calibrated flight simulator.

## Explosives, hiding and underwater controls

The Direct3D 11 game has C4, a remote trigger, frag grenades, smoke grenades, molotov cocktails, police flashbangs, and a timed bomb. Buy them at shops, collect their city pickups, or equip them through F4. C4 uses an online CC0 model by Lucian Pavel; provenance is in `assets/models/source/explosives/manifest.json`.

- LMB throws the selected device. C4 sticks to buildings and follows attached cars, with at most 40 active charges including airborne ones. Equip the remote and click LMB, or press X, to detonate all active C4.
- Frag grenades bounce and explode after three seconds using RPG building-hole and blast damage. Smoke grenades produce sight-blocking smoke for 24 seconds. Molotovs break on impact and ignite nearby surfaces and actors. Flashbangs stun visible nearby pedestrians for five seconds and briefly flash the player's view.
- LMB with the timed bomb opens a paused seconds-entry screen. Enter 1-600 seconds and press Enter to place it, or Escape to cancel without spending ammunition. Its 500-unit blast can destroy whole nearby buildings, vehicles and props, with broad fire and lingering smoke. Nearby bombs show a countdown. Active devices and smoke are session effects and are cleared on new game/load.
- Teal hideout markers appear at 16 randomly chosen reachable building corners. Stand within a marker, away from police sight, to lose one wanted star every 15 seconds. Movement, witnessed crime or a visible officer interrupts this progress.
- With the knife selected, approach behind a pedestrian and press K when the takedown prompt appears. An unwitnessed takedown produces no wanted report; nearby observers must face the victim and have an unobstructed view to report it.
- While swimming, hold LMB to dive and RMB to rise to the surface. Releasing both holds depth. Swimming uses a bounded 80-unit water layer; weapons cannot fire while these controls are active. RPGs explode at their range limit and all ballistic weapons clip their actual travel to their catalog range.
- F4 includes Add $100,000, Reset wanted level, and Add 1,000 ammo to all weapons. Vehicles receive UV-attached dirt patches and fine paint scratches through the DX11 material shader; glass keeps its transparent surface response.

`ctest -R ordnance_scenarios` covers the new gameplay, and `tools/verify_ordnance.ps1` captures fresh DX11 equipment, timer, large-blast and underwater screenshots. Throwable flight and bounces use continuous swept game-level collision rather than separate Jolt rigid bodies. Interactive balance tuning and detailed underwater scenery remain open.

### Physical building rubble

Building blasts in DX11 now eject beveled clay bricks, broken bricks, hollow
concrete blocks, irregular concrete chunks and slabs with exposed steel.
These original masonry meshes have mottled surface colors and Jolt bodies
that tumble, collide and settle. Convex colliders follow the chipped outlines;
blocks use five wall/web colliders so their two cores remain open. Blasts eject
16-32 pieces per changed building. Rubble lasts 45 seconds, shares the existing
160-fragment cap, and clears on new game/load.

## Current limitations

The following work is still missing or needs further verification. The integrated Jolt physics/pedestrian ragdolls, runtime humanoid skeletal animation, and supported GPU instancing/LOD are present; their broader roadmap extensions remain open.

| Area | Missing work or current boundary |
| --- | --- |
| Animal animation | Animal gait, combat, corpse poses and rider seating are procedural. Animal skeletal clips, GPU skinning and animal ragdolls are not implemented. |
| Humanoid animation | Procedural poses and ragdolls retain CPU deformation. General retargeting, local-TRS/quaternion hierarchy support and motion vectors for all animated objects remain open. |
| World streaming | Nearby physics colliders stream around the player, but world definitions and mutable regional state still load globally. Wildlife navigation uses bounded steering. |
| Destruction and saves | Building holes, physical rubble, vehicle fragments, active devices, smoke and corpses are session-only and clear on new game/load. Building cuts subtract bounded axis-aligned volumes; structural collapse is not implemented. |
| Vehicle physics | Handling and fixed-wing flight are arcade-oriented; the motorcycle uses a narrow four-wheel surrogate. Imported vehicle fragments use approximate colliders. |
| Underwater world | Swimming depth is capped at 80 units; detailed seabed scenery is not implemented. |
| Assets and rendering | Complete source material/sampler import, distance-based texture mip residency, broader authored LOD coverage, animal deformation and full animated-object temporal coverage remain open. Material map coverage varies by asset. Reflections use screen-space data and bounded local probes. |
| Display verification | Windowed/borderless and exclusive-mode fallback were verified. Actual exclusive fullscreen and its focus restoration remain unverified on the tested desktop, which rejected exclusive mode. |
| Playtesting and performance | A complete human mission playthrough, dense-forest and combat balance tuning, moving-scene visual checks, clean-machine validation and performance across more Windows hardware remain open. Automated tests and staged captures cover specific scenarios. |

See [idea.md](idea.md) and the [graphics upgrade plan](graphics-upgrade-plan.md) for milestone history and remaining scope. Some earlier entries describe features that later milestones have superseded.
