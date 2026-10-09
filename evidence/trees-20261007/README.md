# DX11 tree geometry correction

The old tree bake randomly selected individual wood triangles and arbitrary
two-triangle foliage pairs. Poly Haven leaves can contain many quads, so this
left both bark and leaves in fragments. Nearby and distant trees were affected.

`tools/build_city_models.py --trees` now retains complete connected leaf
surfaces, uses a nested distant subset, and scales complete leaves to compensate
for the reduced sample density. Solid geometry uses Blender edge collapse.
Coincident seam positions are welded while UVs remain per face corner; duplicate
front/back faces are removed and winding is repaired before decimation. If
thousands of disconnected twigs prevent collapse from meeting its target, the
largest complete branch surfaces are retained within the budget. Atlas UV endpoints of 1.0 stay
at the texture edge. Raw glTF UVs keep their top-left origin, matching DX11
image uploads; the previous vertical flip sampled transparent/background areas
inside modeled leaves. All 30 tree IDs and their distant meshes were rebuilt.
Sources, textures, catalog IDs and licensing records are retained.
The coordinate convention is defined in the
[Khronos glTF specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#images).

DX11 distant tree bounds now use the near tree's placement bounds, avoiding
trunk resizing or movement when the selected detail level changes. The root
OpenGL source files and target were not changed.

## Reproduction

Use Python with NumPy/Pillow and Blender 4.5 LTS. Set `TREE_BLENDER` to the
Blender executable if it is not at the repository's existing
`build-tools/blender-4.5.14-windows-x64/blender.exe` location. Connected wood
outputs are cached in ignored `build-tools/tree-wood-cache/`, keyed by source
arrays, target counts and worker/helper source bytes.

```
python tools/build_city_models.py --trees
python tests/tree_geometry_test.py
python tools/audit_tree_geometry.py
./build.ps1
ctest --test-dir build-msvc-ninja -R "^(asset_smoke|scenery_scenarios|scene_jobs_scenarios)$" --output-on-failure
./tools/verify_trees.ps1
```

The audit's optional historical comparison reads the original local snapshot
at `build-tools/trees-before/`. `verify_trees.ps1 -Before` uses that snapshot and
restores current assets in a `finally` block. Rebuild assets before refreshing
the runtime; avoid baking while the before script restores its files.

## Evidence

- `bake-trees.txt`: final per-model triangle counts and Blender output.
- `geometry-tests.txt`: whole multi-quad leaves, nested subsets, shared corners,
  density compensation, complete branch selection and closed wood with UV
  seam/duplicate face fixtures. A real source/cooked leaf mask check catches
  flipped UVs: sampled source triangle centers cover 84.1% opaque texels in
  the correct orientation versus 36.2% when flipped.
- `geometry-audit.json`: source/buffer and baked hashes, finite vertex/UV checks,
  near/distant triangle counts and exposed wood edge measurements for 30 trees.
  Boundary measurements include intentional open branch ends and are not a
  claim that every source mesh is watertight.
- `ctest.txt`: asset, scenery/mining and serial/parallel scene checks.
- Native views for five representative species at 180 and 420 units, with
  matching original views. Captures disable grass to expose tree geometry.
- Matching 120-frame 1080p woodland samples include normal grass and scenery.

These samples verify the affected tree rendering and scoped gameplay regressions.
They do not complete the wider graphics/art or world-streaming roadmap.

## Final verification

Four Python geometry/mask checks and all three selected CTest suites passed.
CTest completed in 14.76 seconds. The complete near/distant review sheet and
both woodland scenes were visually inspected. `comparison.png` shows the
original and repaired jacaranda at the same camera position.

The mining fixture now explicitly targets the supporting trunk cell. Full
wood geometry exposes peripheral roots absent from the old sampled mesh;
cutting a side root correctly leaves climbing support intact. The fixture
still mines through held input, checks feedback/caps/yield/durability and
verifies that removing the supporting cell prevents climbing.

| 120-frame 1080p woodland sample, Radeon 680M | Original | Fixed |
| --- | ---: | ---: |
| Average frame | 32.15 ms | 35.08 ms |
| p95 | 34.56 ms | 36.78 ms |
| GPU shadows | 6.37 ms | 7.55 ms |
| GPU scene | 15.40 ms | 18.22 ms |
| Submitted triangles/frame | 5,020,489 | 6,570,721 |
| Working set | 864 MiB | 896 MiB |

The additional complete geometry has a cost of 2.93 ms in this local sample.
This is one seeded scene and device, rather than a broad travel/hardware result.
The root and runtime DX11 executable hashes match. The root save hash remains
`187346E5FA230973BD495D58E6FDDC9C26E7A8873134EC7831C845263B83570C`.
