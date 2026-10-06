# Builder excavation evidence — 2026-10-06

This pass implements persistent terrain pits and roofed tunnels within the existing DX11 builder layer. The full `minecraft plan.md` is still incomplete.

## Verified implementation

- Forty-unit cells subtract volume from the original piecewise-linear terrain. Rendering clips the original top triangles and adds textured floors, walls, and ceilings. Corresponding heightfield triangles are disabled and replaced with Jolt collision meshes, including at patch boundaries and on slopes.
- F5 removes the excavation geometry and collision, restores normal ground, and resurfaces the player and nearby live pedestrians/vehicles. Re-entry restores the saved cuts. Underground placed blocks participate in collision.
- Untouched tree, decoration, outcrop, and tree-fragment anchors use the generated baseline. Grass rooted in a removed surface cell is suppressed instead of moving to the pit floor.
- Deterministic formations contain all twenty rocks and coal, iron, gold, and diamond ore. A new coal-ore asset brings the builder catalog to 65 original textured models.
- Pedestrian rendering, targeting, projectiles, weapon muzzle height, contact checks, and ragdoll spawning use the character's actual physics height. A surface pedestrian does not obstruct the player in a tunnel beneath it.

## Automated evidence

All **26 CTest suites passed in 88.08 seconds** after the final source changes. Full output is retained in `ctest.log`; `excavation_scenarios` took 3.46 seconds in that run.

`tests/excavation_scenarios.cpp` verifies:

- Actual shovel mining input, soil pickup, successful-use durability, and no repeated yield for the same cut.
- Capsule falls through both the original city floor and heightfield, grounding on the pit floor, and a jump out of a one-block pit. The increased jump impulse applies only in builder mode.
- Walking through a retained-roof tunnel across a 200-unit patch boundary, ceiling collision, and placing/mining a block inside that tunnel.
- Camera, swept projectile, terrain-ray, and grapple wall/ceiling/floor queries; no original pavement triangles covering the pit.
- Atomic rejection of malformed excavation records; save/load; underground positioning; normal-mode restoration and builder re-entry.
- Mountain excavation with Jolt support on the new floor and baseline restoration on exit.
- A surface pedestrian above a tunnel, underground pedestrian targeting and actual projectile damage, underground ragdoll spawn height, and live pedestrian recovery on exit.
- Held-LMB harvesting of all 20 rocks and all four ore types; the diamond tier gate; exact resource increments; durability conservation; and persisted resources/cuts. Test access shafts use the low-level cut helper, while target deposits are mined through normal builder input. This is not evidence of a complete exploratory crafting/progression playthrough.
- A full Jolt car falling into a sufficiently wide pit, grounding through wheel contacts, recovery to normal ground on F5 exit, and falling into the restored pit on re-entry.

## DX11 captures

`tools/verify_excavation.ps1` runs the focused scenario and then four hidden runtime previews sequentially. These previews construct deterministic excavation fixtures and explicitly supply tools; they verify rendering rather than resource acquisition.

- `pit.png`: a soil opening with textured geological walls/floor, target guidance, shovel, and hotbar.
- `tunnel.png`: a retained textured ceiling and surrounding walls/floor, with a modeled diamond pickaxe equipped underground.
- `normal.png`: the same edited district restored to its normal surface after F5, with builder HUD/tool visuals removed and the prior third-person camera restored.
- `mountain.png`: edited cells intersecting a slope, viewed from an elevated fixture camera. Original scanned decorations retain their baseline positions; their local subtraction is still pending.

All four captures were visually inspected. The existing five builder captures were refreshed through `tools/verify_builder.ps1` after this pass and visually inspected again, for nine current DX11 views in total.

Reproduce with Release builds of `MiniCity3D` and `simulation_smoke`, then run `tools/verify_excavation.ps1`. Run all regressions with `ctest --test-dir build-msvc-ninja --output-on-failure`. Save-writing tests and previews must run sequentially, using the runtime directory rather than the root save.

## Remaining work

Granular trees/foliage/bushes and scanned-rock subtraction, hoe/shears/brush world actions, repair costs, contact-specific animations/cracks/particles/audio, full underground navigation and actor integration, loose-prop/corpse transition recovery, face/collider merging, interrupted-save/transition failure recovery, and dense-build performance remain open. Wider interactive progression and traversal are unverified.

OpenGL rendering files and target were unchanged. The user's root `savegame.ini` retains SHA-256 `5A4C87DB5DC9E23148622694D21056E0B18EC4FE45FE7B294C59826B289FC539`.

The root `MiniCity3D.exe` was updated from the verified runtime build. Both executable copies have SHA-256 `C9A47DAB7EC3099DF7D8947079FE5EBB0C56FA75F689625FF880234B54822481`.
