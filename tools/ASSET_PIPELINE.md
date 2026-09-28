# Asset conversion workflow

## Pin and verify Blender

Run `./tools/bootstrap_blender.ps1`. It downloads the official portable Blender
4.5.14 LTS Windows archive into the ignored `build-tools/` directory, verifies
SHA-256 `b9533d2397ac1984db4466fb23a7a4649391cca93f6e84209f9bcc60d071c8b9`, and
checks the executable version. The upstream build hash is `62c1db4208e8`.
The checksum comes from [Blender's official checksum file](https://download.blender.org/release/Blender4.5/blender-4.5.14.sha256).
The tool is not installed system-wide and is not copied into game packages.

## Convert FBX or an authored Blender scene

```powershell
./tools/convert_source.ps1 -InputPath path/to/model.fbx `
  -OutputPath build-tools/canonical/model.glb `
  -SourceUrl 'https://original-author.example/model' `
  -License 'CC-BY-4.0' -Credit 'Original creator'
```

This launches Blender in background mode with factory settings, embedded source
scripts disabled, and a workspace-local user-resource directory. FBX embedded
axes/units are handled by the importer. Authored scenes with physical units use
one parent scale to normalize metres. Hierarchy and natural proportions remain;
the exporter changes the coordinate basis to glTF Y-up.

The GLB carries indexed primitives, UV/color sets, normals/tangents, skeletons,
local TRS animation channels, skin influences, morphs, hierarchy, custom extras,
and supported glTF material extensions. Animations are sampled at each source
scene frame. The adjacent `.conversion.json` records source/image hashes,
license/credit, Blender version/build, settings, imported hierarchy, output
attributes/extensions, and conversion changes. Polygon identity from FBX is not
retained. Existing glTF/GLB sources should enter the runtime cooker directly so
their source indices remain intact.

Connected procedural shader nodes, unsupported texture coordinates/projections,
missing external images, and unapplied geometry modifiers fail with an explicit
report. Shader-effect baking is not yet implemented. FBX can only carry the
features represented by that source file; conversion cannot recover original
authoring features absent from the supplied FBX.

## Verification

```powershell
python tests/blender_conversion.py
```

Four integration checks create original fixtures in `build-tools/`: FBX
rig/hierarchy/UV/color/animation conversion, authored clearcoat export,
centimetre normalization, and procedural-shader rejection. Run with a Python
installation on PATH, or the bundled Python runtime on this machine. The
PowerShell wrapper also completed a fixture conversion.

## Runtime cooking and distribution

Canonical GLB conversion is a separate milestone from complete runtime cooking.
The existing weapon cooker supports a restricted static glTF subset and writes
M3D2 plus material maps/loss reports. Runtime tangents, multiple UV sets,
local-TRS/quaternion clips, all material extensions, source-geometry LOD baking,
and the complete showcase catalog remain open. Do not route an arbitrary new
GLB through the weapon cooker and treat its output as a complete import.

Validate the actual archive and its terms before conversion. Record its source
URL, license, credit and hash; keep restricted sources outside public packages.
Renderpeople's extractability restriction also applies when evaluating cooked
files. Conversion to M3D/M3S alone does not establish license compliance. Named
asset access and provenance status is recorded in `assets/models/SHOWCASE_SOURCES.md`.
