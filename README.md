# Mini City 3D

A small playable C++ third-person city sandbox with a downtown grid, beach, harbor, pedestrians, vehicles, weapons, and six missions. `MiniCity3D.exe` now uses **Direct3D 11**. The earlier OpenGL renderer remains available as a fallback build.

The DX11 renderer has shaders, depth buffering, day/night lighting, directional shadows, fog, tone mapping, imported building, character, tree, car, and boat meshes, and tileable color and normal maps for building walls, foliage, roads, ground, vehicles, and clothing. Imported models use GPU instance buffers in the color and shadow passes; distant vegetation uses coarse proxy meshes. Selected CC0 source GLBs and baked runtime meshes are under [`assets/models/`](assets/models/); the PBR materials and licenses are under [`assets/materials/`](assets/materials/). The window defaults to 1600 × 900, with 1280 × 720 and 1920 × 1080 options. The minimap uses a directional player arrow, and the upper-right HUD shows the current weapon, ammo, health, cash, and time. The older top-down experiment remains in `prototype2d.cpp`.

## Build and run

With LLVM `clang++` on Windows:

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

The game loads the `assets/` folder beside the executable. CMake copies that folder after building. The CMake Visual Studio generator normally puts the executable in `build/Release/`. To regenerate baked models after editing a source GLB, run `python tools/convert_assets.py` with Python 3. The converter uses only the Python standard library.

Run `./package.ps1` to build both renderers, copy the executables with the complete `assets/` directory and license files, smoke-test the copies, and create a zip under `dist/`. Use `./package.ps1 -SkipBuild` to package the latest existing builds.

## Controls

| Input | Action |
| --- | --- |
| W / A / S / D | Move or drive |
| Shift | Run on foot |
| Space | Jump |
| Hold right mouse button and move mouse | Aim over the shoulder, turn and pitch camera |
| Left mouse button while aiming | Fire |
| 1-5 / Q | Select or cycle unlocked weapons |
| R | Reload the current weapon; restart after death |
| E | Enter or exit nearby vehicle |
| F | Start a nearby mission |
| M | Open or close the map |
| T | Advance time by one hour |
| Esc | Pause and open settings |
| F3 | Toggle frame rate and simulation timing |

Weapons are picked up at cyan markers. Magazine sizes, reserve pickups, reload times, fire rates, spread, range, damage, falloff, and recoil are defined in `assets/weapons.ini`; safe built-in values are used if that file is missing. A red car starts near the player; the sports car, motorcycle, and boats have different handling. Boats can be left near the shore. Crates and barrels can be pushed and shot. Missions unlock in sequence; the gold map line points to the next start marker. The final mission combines a car checkpoint, a beach shooting target, and a boat trip. Graphics quality, window size, vegetation, effects, DX11 shadows, mouse sensitivity, key bindings, and volume can be changed in the Escape menu and are saved in `settings.ini` next to the executable. Save and Load in that menu use `savegame.ini`; mission rewards and weapon pickups also trigger a save. XAudio2 mixes overlapping sounds and places pedestrian shots, impacts, and traffic in stereo according to their distance and direction.

Run `./test.ps1` to compile and execute the simulation smoke test. It checks mission completion and unlocks, the combined final mission, aiming geometry, pedestrian reactions and hostile bullets, weapon data loading, reloads, prop damage, save/load, and settings persistence. If CMake is installed, `ctest --test-dir build -C Release --output-on-failure` runs the same test. `F3` shows frame and simulation timing, movement/prop physics timing, draw calls in DX11, and active pedestrians. The application writes startup and error events to `MiniCity3D.log` beside the executable.

## Current limits

This is a prototype, not a finished GTA-style game. It has simple collision and vehicle physics, basic pedestrian defense, and synthesized sound effects. Character walk and run poses are baked from the imported rig animations; real-time skeletal blending and Jolt rigid-body physics remain in the roadmap. Shadows are available only in the DX11 build.

