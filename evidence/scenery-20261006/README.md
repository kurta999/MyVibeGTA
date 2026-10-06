# Builder scenery harvesting evidence — 2026-10-06

This pass adds persistent local cuts to existing trees, foliage, bushes, and scanned rocks in the DX11 builder layer. The full `minecraft plan.md` remains incomplete.

## Implementation

- Forty-unit negative cells are keyed by stable generated object IDs. Scene mesh clipping preserves original exterior UVs and material ranges; solid cross-sections supply textured cut caps. Transparent texture cutouts are respected by targeting where the source material enables alpha testing.
- Tree trunk collision retains the original normal-mode cylinder. Builder cuts subtract from that cylinder using clipped circular convex pieces. Foliage and bushes keep their previous non-solid behavior. Nearby scanned rocks gain matching surviving triangle/cap collision only in builder mode.
- F5 restores original scenery and normal collision; re-entry restores the saved cuts. Camera obstruction, reticle rays, line of sight, projectile occlusion/tree damage, grapple collision, and removed climbing support use the active layer.
- Successful axe/shears mining yields the catalog's resource once, consumes durability once through the regular mining loop, and saves edits/resources together. Unknown, duplicate, non-canonical, and geometrically invalid scenery records are rejected before committing a snapshot.

## Source geometry

The existing game-sized tree meshes use sampled triangles. These leave gaps that prevent reliable closed wood cross-sections. Thirty indexed companion meshes preserve connected source wood geometry for cap construction while existing exterior rendering remains intact. `assets/models/baked/nature/mining-solids.json` retains source paths and SHA-256 hashes; existing asset author/license records remain in `assets/models/LICENSES.md` and the tree catalogs.

The 30 companion assets passed hash, binary layout, and runtime count-limit checks and occupy 140.1 MiB. This is asset integrity evidence, not a dense-build memory/performance result. Wood companions are transformed only for edited trees; unchanged targeting traces shared source meshes in model space.

Reproduce missing source buffers with `tools/fetch_mining_sources.ps1`, then run `tools/build_city_models.py --mining-solids`. The fetcher downloads only absent geometry buffers from Poly Haven, compares retained and official glTF content, and verifies buffer MD5 checksums. Existing files are retained. The retained island-tree metadata differed in formatting but matched the official parsed content.

## Automated checks

All **27 CTest suites passed in 91.38 seconds** after the final gameplay changes (`ctest.log`). Subsequent additions strengthened malformed-record and F5 scanned-rock collision checks; the updated focused scenario passed in **4.47 seconds**. No gameplay source changed after the full-suite pass.

`tests/scenery_scenarios.cpp` covers actual held-LMB axe and shears mining, a partial tree surviving the cut, exposed log caps, no geometry inside the removed volume, cleared-cell placement and retained-trunk rejection, surviving Jolt trunk support, camera/projectile rays through the gap, real projectile damage to surviving wood, normal tree damage independence, climbing support, scanned-rock geometry/caps/collision, F5 isolation, generated-bush save/load, retained tool durability, and atomic rejection of unknown/duplicate records. Fixture resources and access geometry are supplied explicitly; this does not verify complete resource progression.

Run the full regressions with `ctest --test-dir build-msvc-ninja --output-on-failure`. Tests and previews write the runtime save and must run sequentially.

## DX11 captures

`tools/verify_scenery.ps1` runs the focused scenario and four deterministic rendering fixtures: `tree.png` (local tree removal and equipped axe), `rock.png` (scanned cliff cuts and pickaxe), `bush.png` (trimmed vegetation and shears), and `normal.png` (original tree after exiting the layer, with builder tool/HUD removed). Grass is disabled in these fixtures to expose the edited surfaces. These are rendering fixtures, not acquisition playthroughs.

All four views were visually inspected. The cut tree retains unsupported branches, as expected for block subtraction; settling/falling of unsupported voxel structures is not implemented.

The root and runtime `MiniCity3D.exe` match SHA-256 `23BA6EF27F42AE847CC3AEA4782C5D5E16E86F701588756306513A1D1BCF5841`. The user's root save retained SHA-256 `5A4C87DB5DC9E23148622694D21056E0B18EC4FE45FE7B294C59826B289FC539`.

## Remaining scope

Hoe/brush actions, tool secondary actions and repair costs, contact-specific animations/cracks/particles/audio, full underground navigation/actor integration, loose-prop/corpse transition recovery, face/collider merging, failure recovery, and dense-build performance remain open. Tool/material balance and interactive progression need wider play checks. Shears currently use the common equipped-tool swing; animated blade clipping feedback is still pending.
