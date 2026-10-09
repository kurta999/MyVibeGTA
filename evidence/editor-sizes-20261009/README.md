# Editor grid and vehicle surfaces

The Direct3D 11 `MiniCity3D` build uses a single 20-unit editor grid for placed blocks, targeting, placement previews, terrain excavation, scenery cuts, surface work, navigation and Jolt collision. Blocks previously measured 40 units. The editor's first-person eye height is 40 units, and its third-person camera is raised to 72 units above the player's feet.

Builder save format 2 records the smaller grid. Format 1 remains readable: placed blocks retain their world-space corner positions and inventory contents while their dimensions shrink; old excavation and scenery volumes expand into smaller cells to retain the previously removed area. Surface work maps onto the smaller grid too.

R after dying retains builder mode, placed blocks, inventory and world edits, while restoring the player at a clear spawn with full health. Repeated R and the inventory repair shortcut are covered by the transition scenarios.

Tank and truck rigid parts bind original generated albedo, normal and ORM maps, with camouflage, panels, seams, scratches, tire and track details. `tools/texture_vehicle_surfaces.py --verify --vehicle tank` and the corresponding truck command verify deterministic maps and unchanged source geometry for 23 tank and 10 truck parts. The trailer is doubled in rendering, collision, wheels, hitch constraints, part effects and explosion debris.

Run `tools/verify_editor_sizes.ps1` after `build.ps1` to reproduce the native DX11 captures. It backs up the separate runtime's save and restores it afterwards. The saved-world view loads a copy of the root save; the root save remains untouched.

Captures: `tank-surface.png`, `truck.png`, `trailer.png`, `builder-stack.png`, `builder-stack-third.png`, `builder-stack-respawn.png`, and `saved-builder.png`. The stack previews place the second block through the live reticle/use path. The respawn preview invokes the real R input handler. `ctest.txt` records the complete scenario and asset suite.

All 39 CTest suites passed in 466.37 seconds. The earned-resource progression test crafted, used, dropped, stored and reloaded all 22 tools. Four tree geometry tests and both tank/truck map verification commands also passed.

All seven final native DX11 captures completed successfully and were visually inspected. The actual root save loaded two migrated blocks on the 20-unit grid. Its SHA-256 remained `154bc6868c85beae20e6de471a6357d1a2800c6e0615cd0fafc3c3f865d505dc`. The published root and verified runtime executables share SHA-256 `29f9920c428f2e58884ba12bedec2c5e3fca9005bc7dd23acc8c897cd95789a9`.

The OpenGL fallback is unchanged.
