# Mini City 3D: major upgrade roadmap

## Goal

Build one polished, playable third-person city district with downtown streets, a beach, a harbor, traffic, pedestrians, combat, vehicles, and missions. Keep the project in C++ and make each implementation pass playable.

## Technical direction

- Windows C++ application with a Direct3D 11 rendering backend. Replace the current fixed-function OpenGL renderer in stages while keeping a runnable build.
- Jolt Physics for collision, character movement, rigid props, vehicles, boats, and ragdolls. Run simulation at a fixed 60 Hz and interpolate rendering.
- glTF/GLB asset pipeline for UV-mapped meshes, materials, skeletons, and animation clips. Use original or appropriately licensed assets.
- XAudio2 for overlapping positional sound effects and ambient audio.
- In-game UI for HUD, map, pause menu, and graphics/audio/control settings.
- Pin dependency versions and make the Windows build reproducible through CMake.

## Milestones

### 1. Foundation

- Separate application, input, simulation, rendering, physics, audio, AI, UI, and content code.
- Add a fixed simulation tick, frame interpolation, logging, and a debug overlay with frame rate and physics timing.
- Keep the existing world, vehicles, weapons, missions, and map usable while systems are replaced.

**Done when:** the game launches reliably and a complete mission remains playable.

### 2. Rendering and assets

- Move to a shader-based renderer with mesh buffers, UV maps, normal maps, directional sunlight, shadows, fog, tone mapping, streetlights, and quality settings.
- Load glTF assets. Build modular facades with doors, balconies, storefronts, windows, roofs, and different silhouettes.
- Replace box characters and vehicles with detailed meshes and coherent materials. Add LOD and instancing for repeated props and vegetation.
- Add a beach with sand, water, palms, umbrellas, grass, street trees, signs, benches, fences, and harbor props.

**Done when:** the same street reads clearly at midday and at night, with stable frame rate.

### 3. Character, camera, and weapons

- Use a rigged player and several pedestrian variants.
- Blend idle, walk, run, aim, fire, reload, hit, fall, and vehicle-entry animations.
- Hold right mouse button to aim over the shoulder. Mouse yaw and pitch control a 3D camera; left mouse button fires. Use camera collision so walls do not obscure the player.
- Trace from the reticle to an aim point, then fire from the visible weapon muzzle toward that point. Add recoil and muzzle effects.
- Put weapon stats in data: magazine, reserve ammo, reload, fire rate, recoil, spread, range, damage, and falloff.

**Done when:** the reticle and visible muzzle agree at near and far targets, including targets above or below the player.

### 4. Physics and vehicles

- Use a grounded capsule character controller with gravity, jumping, slopes, steps, and fall damage.
- Simulate movable props and collision impulses.
- Give a sedan, sports car, motorcycle, and boat separate mass, steering, traction, suspension or buoyancy, acceleration, braking, and damage settings.
- Add vehicle body damage and effects that can alter handling.

**Done when:** each vehicle feels distinct and can be driven around the district without unstable collisions.

### 5. Damage, pedestrian AI, and reactions

- Add head, torso, and limb hit zones; health, armor, knockback, and hit reactions.
- Transition defeated characters into ragdolls. Severe impacts can detach stylized low-poly parts that become temporary physics objects.
- Add capped blood particles and decals, with an effects setting.
- Give pedestrians behavior states: wander, investigate, flee, take cover, defend, and attack. Armed pedestrians use the same aiming and damage rules as the player.

**Done when:** shots cause readable reactions, and different pedestrians respond differently to an attack.

### 6. Activities and progression

- Refine the existing checkpoint, beach collection, harbor, and target missions with clear start, objective, failure, retry, and reward states.
- Add a few stronger missions using vehicles, combat, and the beach rather than many shallow markers.
- Save money, unlocked weapons, completed missions, and settings.
- Improve the map with objective routing and useful landmarks.

**Done when:** a new player can finish a coherent mission sequence without developer guidance.

### 7. Audio, menu, and packaging

- Replace single-effect audio with overlapping and positional sounds: footsteps by surface, gunfire, reloads, impacts, engine RPM, skids, water, traffic, and surf.
- Escape opens a pause menu: Resume, Graphics, Audio, Controls, Save, and Exit.
- Add resolution, shadow quality, vegetation density, mouse sensitivity, volume, and key binding controls. Persist settings and restore the cursor while paused.
- Package the executable with all required assets and licenses.

**Done when:** settings survive a restart and the packaged game completes its mission loop on a clean Windows machine.

## First playable checkpoint

Prioritize one finished street and beach approach: improved camera and aiming, visible weapon and animation, one reactive pedestrian, one car, one motorcycle, one boat, one complete mission, and an Escape menu. Expand the district once this slice feels good.

### Implementation status

- **Implemented in the current OpenGL build:** 60 Hz simulation tick, basic interpolation, over-the-shoulder 3D aiming with camera collision, visible low-poly weapons, character texture atlas, weapon magazines/reloads/recoil/range, differentiated vehicle handling and condition, simple jumping and damage, hit zones and physical body-part debris, armed pedestrian retaliation, pushable and breakable props, overlapping synthesized effects and surf/engine ambience, Escape settings and Save/Load, procedural grass and street trees, more detailed building silhouettes, five missions including a motorcycle route, map routing, and an F3 performance overlay.
- **New DX11 milestone:** `MiniCity3D.exe` now uses Direct3D 11 shaders, a swap chain, depth buffer, fog, day/night directional and local streetlight lighting, normal-mapped tileable materials, and a GDI-backed HUD texture. CC0 Kenney commercial buildings and nature models, Quaternius character meshes with baked idle/aim/walk/run frames, and Quaternius sedan, sports car, and boat meshes are integrated. Building heights and tree sizes have been increased; 1600 × 900 is the default resolution, with 1920 × 1080 available. The map has a directional player arrow, and the upper-right HUD shows weapon, ammo, health, cash, and time. `MiniCity3DGL.exe` is the fallback build. Source links and license texts are in `assets/models/LICENSES.md` and `assets/materials/LICENSES.md`.
- **Current roadmap pass:** input, pedestrian AI, and content population have separate modules. Startup/error logging, frame and movement/prop physics timing, draw-call and active-AI overlay metrics, a CMake smoke-test target, reticle-to-world aiming, a DX11 directional shadow pass with persisted quality settings, and tone mapping have been added. Weapon tuning loads from `assets/weapons.ini`. XAudio2 mixes overlapping synthesized effects with distance and stereo placement. Pedestrians investigate, flee, seek cover, defend, and attack, with armor, knockback, and weapon-stat projectiles. Missions unlock in sequence, route to the next start marker, and culminate in a car, beach shooting, and boat mission. `package.ps1` smoke-tests both renderers in a zip with assets and licenses. DX11 imported models now use static GPU mesh buffers and per-frame instance data in both the color and shadow passes. Vegetation switches to coarse proxy meshes beyond 360 world units. Procedural streets, markers, props, and effects still use CPU-side batches.
- **Still planned:** live glTF animation blending and attachments, Jolt rigid-body physics, full skeleton/ragdoll simulation, deeper pedestrian tactics, more complete LOD coverage and tuning, and more cinematic mission presentation. Current character and nature meshes retain a stylized low-poly look even with added surface materials.

## Quality checks

- Maintain a runnable build after every milestone.
- Test mouse aiming at different elevations and near walls.
- Test weapon ammo, reloads, hits, mission state, and save/reload deterministically.
- Test character, car, motorcycle, and boat collisions at fixed frame rates.
- Record frame time, physics time, draw calls, and active AI count; tune for smooth 1080p play on a mainstream Windows PC.

## References

- [Direct3D 11 programming guide](https://learn.microsoft.com/en-us/windows/win32/direct3d11/dx-graphics-overviews)
- [Jolt Physics samples](https://github.com/jrouwe/JoltPhysics/blob/master/Docs/Samples.md)
- [glTF 2.0 specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)
- [XAudio2 introduction](https://learn.microsoft.com/en-us/windows/win32/xaudio2/xaudio2-introduction)
- [Dear ImGui backends](https://github.com/ocornut/imgui/blob/master/docs/BACKENDS.md)
