# Realistic assets and DX11 rendering upgrade

## Goal and execution

Upgrade the models and supporting engine features to deliver polished, realistic gameplay in one finished district before expanding across the map.

- Use free assets first, including appropriately licensed account-based downloads.
- Target 1080p/60 FPS on an RTX 3060-class GPU. **Lower FPS on this PC’s Radeon 680M is acceptable** and will be reported accurately.
- Develop only the Direct3D 11 `MiniCity3D` target; preserve the OpenGL fallback.
- Keep existing gameplay, physics, missions, and save compatibility.
- At implementation start, save this plan as `graphics-upgrade-plan.md` and maintain milestone status and verification results there.
- Execute all milestones continuously without routine approval stops. Continue independent work if asset access is blocked; report unavoidable account, permission, or licensing dependencies explicitly.

**Current status (2026-09-28):** implementation is in progress in the Direct3D 11 target. Verified slices cover texture mips/filtering, GPU timing, transparent instance order, normal/roughness and indirect-light buffers, indexed cooked meshes, per-range cutouts, model-instance frustum culling, projected-size LOD selection, sunlight cascades, downsampled bloom, half-resolution SSR, indexed weapon material import, bounded player-headlight and nearby streetlight shadows, High TAA, ordinary-humanoid GPU deformation with prior-pose vectors, pinned Blender source conversion, and authored four-level Marina building LODs. The showcase and the remaining engine features below are incomplete.

## 1. Asset acquisition and import pipeline

Build the showcase around the existing starting street and nearby green space: one detailed car, two pedestrian variants, one animated wolf species, and three detailed building variations.

Starting assets are the previously identified free Classic Sedan, Renderpeople rigged samples, animated Grey Wolf, and the existing Poly Haven apartment/factory source models. Validate actual downloaded contents and licenses before integration. Do not purchase assets.

- Preserve indexed geometry, material sections, tangents, UV sets, vertex colors, skeletons, hierarchy, and attachment transforms.
- Convert FBX through a pinned Blender 4.5 LTS toolchain into the canonical glTF/GLB import path.
- Normalize coordinate conventions and scale once during conversion; preserve natural proportions.
- Import complete metallic/roughness materials, including base color, normal, occlusion, emissive, transparency, clearcoat, texture transforms, and sampler settings.
- Bake unsupported source shader effects into compatible textures where possible; report unsupported required features instead of silently dropping them.
- Introduce versioned cooked assets with backward-compatible readers for existing files.
- Add a catalog mapping gameplay IDs to models, LODs, material sets, animation mappings, and attachment points.
- Record sources, licenses, hashes, conversion settings, and dependency versions. Keep restricted source files private and exclude them from public packages.

High-resolution source geometry must be reduced and baked before gameplay use. Initial LOD0 budgets are 60k triangles per car, 40k per pedestrian, and 30k per animal. Further LODs target approximately 50%, 20%, and 8% of that geometry while preserving silhouettes and material boundaries.

## 2. Mipmaps, filtering, and texture management

The current loader creates only one texture level. Replace this with a complete texture pipeline.

- Generate full mip chains down to 1×1 for imported model textures and tileable world materials.
- Filter color maps in linear light and retain correct sRGB sampling. Treat normal, roughness, metallic, and occlusion maps as linear data.
- Renormalize normal-map mips and apply normal-variance roughness adjustment to reduce distant specular shimmer.
- Preserve alpha-test coverage across foliage, hair, and fur-card mip levels.
- Add atlas padding and edge dilation before mip generation to prevent color bleeding.
- Cook DDS textures using BC7 for color and packed material maps, BC5 for normals, and suitable single-channel compression for masks.
- Support uncompressed fallbacks and generated mipmaps for legacy PNG/JPEG assets.
- Add anisotropic filtering: 16× High, 8× Medium, 4× Low. Preserve per-material wrap/clamp behavior; use clamp sampling for screen-space passes.
- Cache textures by resource identity and color-space interpretation to prevent incompatible reuse.
- Use 2K textures by default, with selective 4K close-up surfaces.
- Implement asynchronous loading, bounded GPU uploads, and distance-based mip residency. Keep coarse mips resident while finer levels load.
- Preserve uncompressed HDR formats for lighting buffers and reflection data where compression would lose required range.

Expose texture quality and filtering controls through persisted graphics settings.

## 3. Lighting, materials, and image quality

Extend the existing renderer through separately verifiable passes.

**Materials and geometry**

- Apply consistent energy-conserving metallic/roughness shading to sunlight and local lights.
- Use correct tangent-space normals, handedness, normal strength, and transformed normals.
- Add clearcoat car paint, Fresnel glass, masked hair/fur cards, cloth sheen, and a restrained skin-scattering approximation.
- Separate opaque, masked, and transparent render queues. Sort transparency back-to-front and use per-material culling.
- Preserve alpha masking in depth and shadow passes; stop applying generic model-wide cutoffs.

**Lighting and shadows**

- Add diffuse irradiance, prefiltered specular environment maps, and a BRDF lookup texture.
- Bake local reflection and diffuse-light probes for day, dusk, and night in the showcase; blend with the time/weather system.
- Replace the single sunlight shadow with three stabilized cascades on High, filtered edges, slope-aware bias, and smooth cascade transitions.
- Add shadowed player headlights and a capped nearby streetlight shadow budget.
- Keep emissive surfaces visually distinct from actual light sources.

**Reflections and post-processing**

- Add depth, normal/roughness, and motion-vector resources.
- Replace flat-normal reflections with half-resolution SSR using surface normals, roughness, thickness checks, edge fading, and reflection-probe fallback.
- Upgrade ambient occlusion with depth/normal sampling and edge-preserving filtering; apply it to indirect lighting.
- Add a downsampled bloom chain, filmic tone mapping, bounded exposure adaptation, and consistent output color conversion.
- Implement TAA with animated-object motion vectors, history rejection, and controlled sharpening. Reset history after camera cuts, teleports, and resizing.
- Retain FXAA for Low and render the HUD after scene post-processing.
- Add material, normal, roughness, mip-level, shadow-cascade, and motion-vector debug views.

The target is convincing rasterized gameplay. Hardware ray tracing, Lumen/Nanite equivalents, strand hair, and a full material editor are outside this implementation.

## 4. Animation, scene integration, and performance

- Move ordinary pedestrian and animal deformation to GPU skinning across color, depth, shadow, and motion-vector passes.
- Store skeleton hierarchy and local translation/rotation/scale animation; blend rotations with quaternions.
- Preserve previous poses for motion vectors. Support larger skeletons through explicit joint limits and validated palette allocation.
- Retarget existing humanoid gameplay actions offline. Preserve aiming, weapon attachment, climbing, melee, seated occupants, and Jolt ragdoll transitions.
- Map wolf behavior to skeletal idle, locomotion, attack, hit, and death clips. Create missing required clips before marking integration complete.
- Animate wheels and steering from vehicle state; align seats, door access points, and occupants with the new car.
- Assemble detailed building geometry with recessed windows, storefronts, roof details, and complete PBR materials.
- Add frustum culling, projected-size LOD selection with hysteresis, instancing, appropriate shadow LODs, and reduced distant animation update rates.
- Preload the showcase working set and eliminate repeated per-frame allocations and unnecessary geometry uploads.
- Add GPU timestamp queries, adapter identification, per-pass timing, draw/triangle counts, and texture residency metrics.
- Handle resize/minimize, render-target recreation, failed resource creation, and device-loss reporting without stale-resource crashes.

High targets the discrete GPU. Medium reduces reflections and shadows. Low disables SSR/local-light shadows and reduces texture, animation, and geometry budgets. Lower local FPS does not trigger removal of the requested High-quality features.

## 5. Verification and delivery

Each milestone must leave a runnable build.

- **Asset validation:** indices, material ranges, texture channels, mip chains, alpha coverage, tangents, skeleton weights, clips, attachments, bounds, LODs, and legacy loading.
- **Rendering validation:** daylight, dusk, night, rain, close-up materials, glass, fur edges, shadow transitions, mip transitions, and temporal stability.
- **Gameplay regression:** walking, combat, aiming, ragdolls, climbing, driving, entry/exit, visible occupants, animal combat/carrying, missions, and save/load.
- **Performance:** benchmark a deterministic 60-second route after warm-up with VSync disabled. Target average below 14 ms, p95 at or below 16.7 ms, and p99 below 25 ms on RTX 3060-class hardware. Report local measurements separately; do not claim untested hardware results.
- **Packaging:** run existing CTest suites and packaged graphical smoke checks, verify runtime dependencies and attribution, and exclude restricted source files.
- **Visual evidence:** provide matching before/after screenshots and inspect moving scenes for shimmer, ghosting, transparency errors, and animation defects.
- **Final delivery:** saved plan with factual completion status, reproducible import/build commands, runnable package, screenshots, performance report, and explicit remaining limitations.

Update `idea.md` only for integrated and verified results. Complete the showcase and engine work before considering map-wide asset replacement.
## Implementation log

- 2026-09-27: Plan saved at implementation start. Existing gameplay/save compatibility and the OpenGL fallback remain in scope and unchanged.

### Verified DX11 texture and telemetry slice

- PNG/JPEG model and tileable world textures now load complete immutable mip chains through 1×1. Color mips filter in linear light and use sRGB sampling. Normal and data maps stay linear. Normal mips renormalize direction and store directional coherence in alpha; the shader uses it to broaden distant highlights. Masked textures dilate transparent edge colors and adjust mip alpha to retain cutout coverage as closely as each mip resolution allows.
- Texture cache keys include interpretation (sRGB color, masked color, linear data, or normal). World materials use wrap sampling; model textures and screen passes use clamp sampling. Saved Graphics options select Low/Medium/High texture detail (minimum mip 2/1/0) and 4×/8×/16× anisotropic filtering.
- The debug overlay reports submitted triangles and nonblocking DX11 timestamp results for shadow, scene, and post/HUD work. Startup logs the adapter. Benchmark runs disable VSync and log GPU timings and submitted triangles. Failed Present calls log the device-removal reason and stop submitting frames to invalid resources.
- `MiniCity3D` builds with MSVC. All 10 CTest suites pass, including the new texture mip test. Hidden-window day, night/ragdoll, and Graphics menu DX11 smoke runs exit 0. Day and menu screenshots were visually inspected.
- Local 1920×1080, 120-frame benchmark on AMD Radeon 680M: 32.42 ms average, 36.54 ms p95, 44.98 ms p99, 47.62 ms maximum; 119 nonblocking GPU samples averaged 5.92 ms shadow, 12.16 ms scene, 1.84 ms post/HUD. This is a short local sample, not the required deterministic 60-second RTX 3060 validation.
- The copied DX11 package passed day, night/ragdoll, causeway, east, snowfield, desert-hub, savanna, swim, climb, fall, and debug-menu smoke checks. Artifact: `dist/MiniCity3D-gpu-upgrade-20260927.zip`. The package contains the pre-existing licensed assets and their attribution; no new restricted source asset was added.

### DX11 transparency and surface-buffer slice

- Transparent model instances, including vehicle glass and effects, are now ordered from far to near using their transformed mesh centers. Opaque instances retain material/mesh batching; transparent instances render separately so batching cannot reorder overlapping alpha surfaces. The asset smoke test checks both camera directions.
- The scene shader now applies the cutout threshold only to meshes marked `alphaTest`; ordinary textured meshes are opaque. Existing foliage still uses its cutout shadow shader.
- An HDR normal/roughness render target is written by the opaque pass and preserved through the transparent pass. SSR now uses the surface normal and roughness, bounded ray thickness, and screen-edge fading. AO samples neighboring normals to reduce bleeding across surface boundaries. `--normal-view` and `--roughness-view` show the buffer in smoke captures. This is still a single-resolution SSR/AO implementation without probe lighting, temporal rejection, or indirect-only AO application.
- `MiniCity3D` builds with MSVC. All 10 CTest suites pass. DX11 day and night/ragdoll smoke renders exit 0; their captures and the normal debug capture were visually inspected.

### Deterministic performance route

- `--smoke --benchmark-route --1080p` now warms up for 120 fixed ticks and measures 3,600 fixed 60 Hz updates and renders across six repeatable 10-second scene segments. VSync is disabled, and the log records average, p95, p99, maximum, simulation/render/physics splits, draw and triangle counts, GPU pass samples, and adapter identity. The route ran to completion with exit code 0.
- Initial local Radeon 680M baseline at 1920×1080: 41.58 ms average, 218.29 ms p95, 275.00 ms p99, 298.81 ms maximum; 1.22 ms simulation, 40.36 ms rendering on average. GPU samples averaged 2.98 ms shadow, 26.86 ms scene, 3.15 ms post/HUD; 197 draw calls and 5.38 million submitted triangles per frame. This precedes model culling; the current-code result is below. RTX 3060-class hardware has not been measured. The six locations include expensive regional content; per-segment profiling and optimization remain open.

### Indexed cooked mesh slice

- The DX11 loader accepts legacy triangle-list M3D1 and indexed M3D2, validates index bounds and material ranges, and creates immutable index buffers for the latter. Color and shadow instance draws use indexed calls when available. Existing legacy meshes remain readable, including vehicle glass splitting.
- The available CC0 pistol, AK, and lightning weapon sources were recooked to M3D2 by `tools/import_traffic_weapons.py --weapons-only`; their triangle order and `.pbr` material ranges remain stable. The three baked files now hold 2,895/7,611, 2,948/7,446, and 2,332/9,138 unique/expanded vertices respectively. `tools/index_m3d.py` can upgrade a legacy triangle-list file without changing triangle or sidecar order.
- MSVC builds pass. All 10 CTest suites pass, including indexed weapon range/index checks alongside a legacy M3D1 vehicle check. Pistol, rifle, and shotgun DX11 smoke captures exited 0 and were visually inspected. This validates the format and current weapon rendering, not the complete glTF geometry/material/skeleton import requirements.

### Model culling and cutout metadata slice

- Camera-visible and shadow-visible model instances now use separate ranges in one dynamic instance buffer. Camera and shadow frustum tests use conservative transformed bounds; `--no-frustum-cull` retains a matched measurement path. Transparent instances remain ordered separately. Static world geometry is not culled by this pass.
- Material sidecars can specify `OPAQUE` or `MASK` with an alpha cutoff per range. The DX11 color and shadow shaders use that cutoff; legacy mesh-wide foliage cutouts remain supported. The weapon importer emits the metadata and rejects unsupported `BLEND` materials rather than silently dropping them. The asset fixture checks a masked M3D2 range, a rejected blend mode, and an out-of-range index.
- Matched 120-frame 1080p woods samples on Radeon 680M: culling on averaged 29.73 ms and 11.18 million submitted triangles/frame; `--no-frustum-cull` averaged 44.57 ms and 20.89 million triangles/frame. These are short local samples, not a target-hardware result.
- The full six-segment, 3,600-frame 1080p route with culling averaged 24.09 ms, p95 35.44 ms, p99 43.08 ms, maximum 56.60 ms; GPU samples averaged 1.97 ms shadow, 11.20 ms scene, 2.69 ms post/HUD, with 115 draws and 2.02 million submitted triangles/frame. This supersedes the 41.58 ms local baseline above for the current code. It still misses the stated frame-time targets on this Radeon 680M, and RTX 3060-class hardware remains unmeasured.
- `./build.ps1 -RunTests` now initializes the installed MSVC environment from a normal PowerShell session and passes all 10 CTest suites. `./package.ps1 -SkipBuild -PackageName MiniCity3D-graphics-milestone-20260927` produced a smoke-tested 673-entry zip at `dist/MiniCity3D-graphics-milestone-20260927.zip`. The copied package completed its day, night/ragdoll, and regional smoke runs. The zip contains baked weapon M3D2 assets and attribution and has no `assets/models/source/` entries. Clean-machine validation remains open.

### Sunlight cascade and inspection slice

- DX11 High now renders three sunlight depth-array slices. Each light matrix is snapped to shadow texels and encloses its camera-frustum segment; the shader uses four comparison samples, a normal-slope bias, and blended transitions at 250 and 650 world units scaled by draw distance. Medium retains one 1024-pixel shadow map and Low disables it. Shadow-casting model instances are culled separately for each cascade.
- `--smoke --shadow-cascade-view`, `--material-view`, and `--mip-view` expose cascade selection, unlit base color, and sampled base-texture mip level. The mip view colors textured surfaces blue at finer levels and red at coarser levels; geometry without a base texture is dark. `--medium-shadows` selects the single-map path for smoke and benchmark comparisons.
- The DX11 target builds with MSVC. Day High, day Medium, night/ragdoll, cascade, material, and mip smoke runs exit 0; their screenshots were inspected. All 10 CTest suites pass. On the local Radeon 680M, a 120-frame 1080p High sample before the final frustum-fit adjustment averaged 40.74 ms with 12.00 ms GPU shadow time and 3.07 million submitted triangles/frame. The matched Medium sample averaged 32.16 ms with 3.77 ms GPU shadow time and 1.86 million triangles/frame. These measurements expose the cost of three shadow passes on this integrated GPU; target discrete-GPU validation is still open.
- The full 3,600-frame 1080p route after the cascade-frustum fit averaged 24.83 ms, p95 39.40 ms, p99 52.56 ms, and maximum 87.22 ms on the Radeon 680M. GPU samples averaged 3.55 ms shadow, 10.87 ms scene, and 2.69 ms post/HUD, with 161 draws and 2.60 million submitted triangles/frame. The route still misses the frame-time target on this integrated GPU; it is not an RTX 3060-class result.
- `./build.ps1 -RunTests` passes after the final debug-view change. `./package.ps1 -SkipBuild -PackageName MiniCity3D-graphics-cascades-20260927` copied the matching current DX11 binary and passed day, night/ragdoll, and nine regional/traversal/menu graphical smoke runs. The 259,619,179-byte archive has 673 entries and no `assets/models/source/` entries. Clean-machine validation remains open.

### Downsampled bloom slice

- The DX11 post pass now extracts highlights into three half/quarter/eighth-resolution floating-point targets and composites their filtered contributions before tone mapping. The chain uses a threshold matched to this renderer's HDR lighting range; Low skips the bloom draws. Target recreation releases and recreates all three levels on resize. `--bloom-view` shows the combined highlight buffer for inspection.
- Day, night, and bloom debug smoke runs exit 0, and their captures were inspected. The bloom debug capture isolates nearby emissive surfaces and lit windows; the regular night capture keeps the glow restrained and leaves HUD text sharp. All 10 CTest suites pass. A local 120-frame 1080p sample after adding the chain averaged 41.46 ms, with 3.41 ms GPU post/HUD time; it is a short integrated-GPU measurement and not a target-hardware result.
- The full 3,600-frame 1080p route with the bloom chain averaged 24.37 ms, p95 36.66 ms, p99 42.54 ms, and maximum 59.19 ms. GPU samples averaged 3.16 ms shadow, 10.26 ms scene, and 2.53 ms post/HUD. This local run remains over the stated frame-time targets; changes of this size between route runs may reflect measurement variability.

### Half-resolution SSR slice

- SSR ray tracing now runs into a half-resolution floating-point target, using the scene depth and normal/roughness buffer. The full-resolution post pass reconstructs the reflection delta from four half-resolution samples and rejects samples across depth discontinuities. Ray misses retain the existing sky fallback; a reflection-probe fallback and temporal history remain open. Low graphics quality skips the SSR pass.
- DX11 causeway and rain captures exit 0 and were inspected alongside a causeway/rain capture from the prior package. The pre-existing dark rain-puddle shapes are visible in both versions. With identical saved settings on the local Radeon 680M, 120-frame 1080p causeway samples measured 33.28 ms average and 2.69 ms GPU post/HUD before the half-resolution pass, versus 33.57 ms and 2.79 ms after bloom and half-resolution SSR. This is a visual/structural milestone, not a measured local speedup.
- The full 3,600-frame route with half-resolution SSR averaged 24.89 ms, p95 36.25 ms, p99 42.68 ms, and maximum 69.12 ms on the Radeon 680M; GPU shadow/scene/post samples averaged 2.94/10.15/2.87 ms. It remains above the target frame time on this integrated GPU. All 10 CTest suites pass.

### Indirect-only AO slice

- The opaque DX11 pass now writes its ambient diffuse/specular contribution to a third floating-point target. The existing depth/normal AO filter subtracts occlusion from that indirect contribution after reflection reconstruction, leaving direct sunlight, local lights, and emissive lighting outside the AO multiplier. `--indirect-view` displays the buffer for inspection. Transparent surfaces continue to use the opaque depth/surface/indirect data behind them.
- The DX11 day and indirect-buffer smoke captures exit 0 and were inspected. A short 120-frame 1080p Radeon 680M sample averaged 33.54 ms with 3.00 ms GPU post/HUD time; the shadow cost varied substantially between short runs, so this sample is not evidence of a speedup.
- All 10 CTest suites pass; the night/ragdoll capture exited 0 and was inspected. The full 3,600-frame 1080p route averaged 24.51 ms, p95 36.17 ms, p99 41.62 ms, and maximum 53.99 ms, with 3.07/10.35/2.91 ms GPU shadow/scene/post timings on the Radeon 680M. This remains above the stated local frame-time target, and target-hardware validation is open.
- `./build.ps1 -RunTests` passes on the final renderer. `./package.ps1 -SkipBuild -PackageName MiniCity3D-graphics-post-20260927` produced a 259,615,776-byte smoke-tested archive with 673 entries and no raw model-source entries. The packaged executable hash matches the built root executable; day, night/ragdoll, and nine regional/traversal/menu checks pass. Clean-machine validation remains open.

### Projected-size LOD slice

- Existing nature, apartment, and marina detail/proxy pairs now select by projected pixel size from the camera pose, with a distance ceiling to bound geometry cost. A per-object decision cache applies 12% pixel and 10% distance hysteresis, so a small camera motion at a threshold does not flip meshes every frame. The cache is bounded; current LOD models remain simple proxies, not the multi-step 50/20/8% source-geometry LODs specified for new showcase assets.
- The DX11 woods smoke capture exits 0 and was visually compared with the previous packaged build. A focused asset test checks first selection and both sides of the hysteresis band. Matched 120-frame 1080p woods runs on the Radeon 680M measured 34.63 ms average, 300 draws, and 14.92 million submitted triangles/frame before this change; after it, 36.14 ms, 282 draws, and 15.52 million triangles/frame. This visual-detail choice costs about 1.5 ms in that local scene, and moving-camera visual tuning remains open.
- LOD selection and render culling now share the renderer's interpolated camera pose, avoiding a second collision-aware camera calculation per frame. The 3,600-frame route with projected-size LOD before that camera-call cleanup averaged 24.94 ms, p95 38.06 ms, p99 47.72 ms, and maximum 84.35 ms; after the cleanup it averaged 25.46 ms, p95 41.02 ms, p99 54.79 ms, and maximum 90.17 ms. GPU shadow/scene/post averages in the final run were 3.76/10.67/3.01 ms, with 2.53 million submitted triangles/frame. The route misses the local frame-time target, and the camera cleanup did not yield a measurable improvement in these runs.
- `./build.ps1 -RunTests` passes all 10 suites. `./package.ps1 -SkipBuild -PackageName MiniCity3D-graphics-lod-20260927` passed the copied-folder day, night/ragdoll, regional, traversal, and debug-menu smoke checks. The 259,616,683-byte archive has 673 entries and no raw `assets/models/source/` entries, and its executable matches the built DX11 executable by SHA-256. Clean-machine and moving-camera visual validation remain open.

### Indexed weapon import and material-map follow-up

- The existing glTF weapon cooker now keys imported vertices by source node, primitive, and accessor index. Repeated indices within a primitive remain shared, while byte-identical vertices in different material primitives stay distinct. External image URIs resolve relative to the source glTF, and unsupported texture UV sets/transforms or material extensions fail explicitly.
- The three existing CC0 weapon sources were recooked without changing their triangle or unique-vertex counts. Their metallic/roughness textures now ship beside the M3D2 files and bind through the existing linear-data PBR path. The cooker also carries available occlusion and emissive maps and base-color factors. A focused Python fixture checks primitive boundaries and external image lookup; the DX11 asset test checks the cooked pistol's roughness map. This is a verified slice of static weapon import, not complete showcase glTF/PBR import.
- The static weapon cooker now writes one `.import.json` report per weapon with its source hash, encountered vertex attributes, omitted skin/clips, deliberately excluded nodes, and hierarchy/sampler/culling substitutions. It applies the first vertex color set; a nonwhite extra color set fails instead of being silently lost. Unsupported primitive modes and multiple glTF buffers also fail explicitly. The current Lightning source has one skin and `Fire`/`Pump` clips that remain absent from its cooked runtime mesh; its report and `TRAFFIC_WEAPONS_LICENSES.md` make that limitation explicit. The focused importer suite now has five passing tests, and the cooked geometry hashes in the license record match the current files.
- A day weapon-preview DX11 smoke frame exited 0 and was visually inspected. All ten CTest suites passed. `MiniCity3D-graphics-import-20260928.zip` was smoke-tested from its copied folder and contains the new maps but no raw model-source directory. The root executable was in use during the build; these checks ran from the separately built `build-msvc-ninja/MiniCity3D.exe`.

### Bounded local-light shadow slice

- At High shadow quality, a 1024² perspective depth map follows the occupied car or motorcycle's forward lamp direction. The two front lamps of a car share the map; only those selected local lights sample it. A separate 512² map covers the nearest selected streetlight's downward beam at night. Both passes draw nearby opaque shadow casters, preserve masked cutouts, and cull models outside their light frusta before instance upload. The two maps can render together. Medium and Low skip these local shadow passes. Other vehicles' headlights, more distant streetlights, and shop/flame lights still illuminate without shadows.
- A night driver preview places a blocker in the beams. Matched High and Medium 1080p captures exit 0 and visibly show the blocker shadow only on High. Before frustum culling, the 120-frame local Radeon 680M High sample averaged 28.08 ms and submitted 5.93 million triangles/frame. After culling, High averaged 24.61 ms with 3.07 million triangles/frame and 0.45 ms GPU shadow time. The matched Medium run averaged 24.39 ms with 2.97 million triangles/frame. These are short local samples; they do not establish RTX 3060 performance or moving-light temporal stability.
- A separate night streetlight preview shows its blocker's ground shadow only on High; the changed pixels are confined to the blocker/ground area in matched 1080p captures. A city car preview also rendered both shadow maps together without a runtime error. The 120-frame streetlight scene averaged 31.20 ms on High with 0.87 ms GPU shadow time versus 30.37 ms on Medium on the Radeon 680M. Moving-light stability checks remain open for this feature.
- The final DX11 build passes all ten CTest suites. A 3,600-frame, six-segment High 1080p route with 120 warm-up ticks completed on the Radeon 680M: 24.67 ms average, 36.02 ms p95, 43.05 ms p99, and 62.08 ms maximum; average GPU shadow/scene/post times were 3.31/9.92/2.94 ms. This does not meet the discrete-GPU target by itself, and no RTX 3060 measurement is available.
- `./package.ps1 -SkipBuild -PackageName MiniCity3D-graphics-local-shadows-20260928` passed its copied-folder graphical smoke checks. The 263,415,273-byte archive has 677 entries, includes all four newly cooked metallic/roughness maps, excludes `assets/models/source/`, and contains the exact SHA-256-matching DX11 executable used for the tests. Clean-machine validation remains open.

### Camera-motion TAA slice

- The DX11 High anti-aliasing preset now jitters the projection over eight subpixel positions and writes a full-resolution camera motion-vector buffer from depth and the previous view-projection matrix. A temporal resolve reprojects the prior frame, rejects mismatched depth, clamps history to the current 3×3 color neighborhood, and reduces its weight with motion and color disagreement. HUD rendering remains after the resolve. Medium uses the existing FXAA strength; Low uses a lighter FXAA blend.
- History resets on resize, large camera translation, or abrupt direction changes. Animated mesh categories (characters, vehicles, animals, birds, weapons, and effects), CPU-skinned character geometry, and water skip history blending until previous object/pose transforms are available. `--smoke --high-taa --temporal-preview --motion-view --screenshot --1080p` provides a moving-camera vector inspection frame; `--no-taa` provides a comparison. This is camera-motion TAA, not completion of animated-object motion vectors, GPU skinning, or all moving-scene stability checks.
- The DX11 day and night/ragdoll 60-frame moving-camera previews exited 0, and their 1080p captures were visually inspected. All ten CTest suites pass, including a loader check that moving asset categories opt out of history blending. Matched 120-frame Radeon 680M day samples averaged 34.48 ms with High TAA versus 34.15 ms with `--no-taa`; GPU post/HUD time was 4.37 versus 3.10 ms. These short runs show an added post cost, and neither establishes target-hardware performance.
- A full 3,600-frame High shadows plus High TAA route at 1920×1080, after 120 warm-up ticks, exited 0 on the Radeon 680M: 25.96 ms average, 39.57 ms p95, 50.02 ms p99, and 82.08 ms maximum. GPU shadow/scene/post averages were 3.65/10.09/4.55 ms. This remains above the target frame times on the local integrated GPU; an RTX 3060-class result and extended ghosting/shimmer review are still required.
- Matched headlight, streetlight, and temporal reference captures, plus the camera-vector debug frame, are saved with reproduction flags in `evidence/graphics-20260928/README.md`. The moving-mesh silhouettes are black in the vector view because they intentionally lack history reuse. These static captures do not replace frame-sequence inspection for shimmer or ghosting.

- `MiniCity3D-graphics-upgrade-20260928.zip` passed all copied-folder smoke checks. The 263,412,472-byte archive contains 680 entries, the four new metallic/roughness maps and three import reports, no raw model-source entries, and an executable matching the tested build by SHA-256. This package predates the subsequent GPU skinning milestone.

### Ordinary humanoid GPU skinning and pose-vector slice

- Ordinary detailed humanoid clip deformation now runs in a DX11 compute shader with immutable bind vertices and a validated 256-joint limit. The resulting vertex buffer is reused across color/depth, sunlight cascade, player-headlight, and streetlight shadow draws. Skin resource or validation failures select CPU deformation. `--cpu-skinning` exposes the reference path. Procedural seated, swimming, climbing, talking, pitched aiming, and Jolt ragdoll geometry retain their existing CPU paths; skeletal animals remain open.
- Prior palettes and world transforms are tracked by actor identity. A second GPU buffer carries previous world positions and validity. An opaque motion target provides humanoid pose vectors to the High temporal resolve, including camera motion. New/reappearing actors, source changes, and translations over 65 world units reject prior poses. Resize and camera-cut history resets remain in place. Vehicles, animals, birds, effects, water, and procedural CPU geometry still skip history reuse.
- `--smoke --skin-motion-preview --high-taa --high-shadows --motion-view --validate-gpu-skinning --screenshot --1080p` runs 60 frames with fixed camera yaw. Current-vertex and prior-position readback checks passed at frames 1, 30, and 60: nine poses at each sample, nine valid prior poses at the later samples, and maximum error 0.00006104 world units. CPU/GPU day captures and fixed-camera motion/resolved captures were visually inspected. The pedestrian test checks collected palettes against established CPU geometry and pitched-aim fallback; the driver test checks seated fallback. All ten CTest suites pass.
- Before prior-pose vectors were added, matched 120-frame High samples measured 42.18 ms average with GPU skinning versus 36.37 ms with CPU skinning on the Radeon 680M. This short sample did not show a speedup. With pose vectors integrated, the 3,600-frame 1080p High shadows plus High TAA route after 120 warm-up ticks exited 0: 25.41 ms average, 40.51 ms p95, 53.46 ms p99, and 80.14 ms maximum; GPU shadow/scene/post averages were 3.92/10.54/4.59 ms. The current GPU timestamp intervals do not separately include the earlier skin compute dispatch. RTX 3060 measurements and extended motion-sequence inspection remain open.
- Reproduction flags and the current/CPU/prior-pose reference captures are recorded in `evidence/graphics-20260928/README.md`. This is an integrated ordinary-humanoid slice; local-TRS/quaternion clips, GPU animal deformation, procedural/ragdoll/object motion vectors, and all showcase integration requirements remain open.

### Pinned source conversion slice

- `tools/bootstrap_blender.ps1` installs the official portable Blender 4.5.14 LTS archive only under ignored `build-tools/`, verifies its pinned SHA-256, and checks the executable version/build. `tools/convert_source.ps1` converts FBX or authored `.blend` files to canonical GLB with source scripts disabled and workspace-local resources. Indexed primitives, hierarchy, tangent/UV/color sets, skin influences, local TRS clips, and supported material extensions survive conversion; physical units normalize through one root transform.
- Adjacent conversion reports record source/image hashes, provenance, Blender settings/version, hierarchy, attributes/extensions, and changes. Connected unsupported procedural shaders, unsupported coordinate/projection nodes, missing images, and unapplied geometry modifiers fail explicitly. Existing glTF sources are not resampled through Blender. Four integration fixtures pass: FBX rig/animation/hierarchy/attributes, clearcoat export, centimetre normalization, and unsupported-shader rejection. The PowerShell wrapper also completed a fixture conversion.
- Reproduction and limitations are in `tools/ASSET_PIPELINE.md`. This verifies source conversion, not complete runtime cooking of the named showcase assets or shader-effect baking. No absent or restricted model was integrated.

### Authored Marina building LOD slice

- Four original Marina building variants now have full/LOD1/LOD2/LOD3 chains and versioned `MCLOD1` catalogs. Their indexed authored reductions retain bounds, window rhythms, roof/parapets, UV/normal seams, and progressively simpler balcony geometry. Achieved LOD1/LOD2/LOD3 ratios are Wave 49.3/20.5/8.6%, Terrace 49.0/20.7/9.6%, Courtyard 50.2/18.8/12.0%, and Bayfront 50.5/20.1/9.3%. These are original native buildings, not the absent named showcase assets.
- Catalog loading validates paths, mesh presence, strictly decreasing triangle counts, descending projected-size thresholds, and compatible bounds. Invalid chains log a diagnostic and retain the legacy fallback. Automatic selection uses 500/240/80 projected pixels scaled by the LOD setting, with 12% hysteresis at each boundary. Asset tests cover the actual chains and both sides of every hysteresis band, plus invalid catalog rejection.
- All ten CTest suites pass. Fixed-camera `--smoke --day --marina --1080p --screenshot --lod-level-0` through `--lod-level-3` and `--lod-view` runs exit 0; all five captures were inspected. The automatic view selects full nearby facades and reduced background geometry. Forced coarse captures reveal the intentional merged balcony guards. Captures and reproduction flags are in `evidence/graphics-20260928/README.md`; moving threshold transitions and named asset LOD authoring remain open. Regenerate with `python tools/build_marina_assets.py --lods-only`.
- `MiniCity3D-graphics-skin-lod-20260928.zip` passed all copied-folder graphical smoke checks. The 263,703,492-byte archive has 698 entries, twelve new LOD meshes, four catalogs, the source audit and three weapon import reports, no raw model-source entries, and an executable matching the tested DX11 build by SHA-256. It predates probe lighting.

### Reproduction

On a Windows machine with MSVC and CMake, run `./build.ps1 -RunTests` from the repository root. The new texture test is included in that build. Run `./package.ps1` to rebuild and produce a smoke-tested package. For a short local performance sample, run `MiniCity3D.exe --smoke --benchmark --1080p` from a build or package directory and inspect `MiniCity3D.log`. Run `MiniCity3D.exe --smoke --benchmark-route --1080p` for the 60-second simulated route after warm-up. For shadow previews, run `MiniCity3D.exe --smoke --night --driver-preview --headlight-preview --high-shadows --screenshot --1080p` or `MiniCity3D.exe --smoke --night --streetlight-preview --high-shadows --screenshot --1080p`; replace `--high-shadows` with `--medium-shadows` for unshadowed references. For temporal inspection, run `MiniCity3D.exe --smoke --high-taa --temporal-preview --motion-view --screenshot --1080p`, then omit `--motion-view` for the resolved frame and add `--no-taa` for an FXAA reference. The three indexed weapon M3D2 assets and material maps can be reproduced with `python tools/import_traffic_weapons.py --weapons-only` after obtaining their attributed source files; run `python tests/import_traffic_weapons.py` for the focused importer checks.

### Outstanding milestones and dependencies

- **Assets/import:** the named Classic Sedan, Renderpeople rigged samples, and animated Grey Wolf are absent from the repository, while the pinned Blender 4.5.14 LTS portable toolchain is now available in `build-tools/` through `tools/bootstrap_blender.ps1`. The existing Poly Haven facade sources are present. Source content and licenses for any new downloads must be checked before integration. M3D2 preserves source indices and core texture maps for the three current weapons, while source-index-preserving import for the showcase, complete runtime glTF PBR including texture transforms and transparency, shader baking, named-asset LOD budgets, and the showcase catalog remain open. Pinned Blender source conversion is verified above; the Marina `MCLOD1` catalogs cover only mesh LOD mapping.
- **Source/license audit:** Exact public source links and metadata are recorded in `assets/models/SHOWCASE_SOURCES.md`. The [Grey Wolf by rhcreations](https://sketchfab.com/3d-models/grey-wolf-rigged-and-animated-56de4df672654ed599777d2980bf0f53) listing and API confirm CC BY 4.0, 22,498 triangles, and one animation; its anonymous download endpoint returns HTTP 401 and the available browser has no signed-in session. The [Realistic Classic Sedan](https://sketchfab.com/3d-models/realistic-classic-sedan-unity-unreal-c0f6b70538fc4864816239c764cf6dc7) and [Chevrolet Caprice Classic Sedan](https://sketchfab.com/3d-models/chevrolet-caprice-classic-sedan-96200ce146014b858e2b284c123bbe42) metadata both report downloadable CC BY 4.0 models, but the intended sedan is still ambiguous and neither archive has been inspected. [Renderpeople terms](https://renderpeople.com/general-terms-and-conditions/) prohibit easily extractable model distribution; merely omitting source FBX/glTF files does not establish compliance for the current cooked M3D/M3S files. No restricted model was integrated or shipped.
- **Texture pipeline:** DDS/BC compression, atlas tile padding, asynchronous uploads, bounded upload scheduling, distance-based mip residency, selective 2K/4K budgeting, and full glTF sampler/texture-transform support remain open. Current texture quality changes the sampled minimum mip; all generated mips are resident.
- **Rendering/animation:** shadows for more than one streetlight and for non-player local lights, probe lighting and SSR probe fallback, complete material models, motion vectors beyond ordinary GPU-skinned humanoids, exposure adaptation and temporal stability validation, GPU deformation for animals and procedural poses, skeletal animal clips, multi-step showcase LODs, and animation-rate reduction are open. Sunlight cascades, bounded player-headlight/nearest-streetlight shadows, the normal/roughness and indirect-light buffers, half-resolution SSR, indirect-only AO, downsampled bloom, camera-motion TAA with a motion debug view, Low/Medium FXAA, material cutout metadata, model-instance frustum culling, and projected-size selection for existing LOD pairs above are integrated slices, not completion of the broader rendering and animation requirements. Existing character animation and scene instancing are retained; neither is claimed as completion of these new requirements.
- **Verification/delivery:** the 60-second local route and packaged smoke checks have run; per-segment profiling, RTX 3060 measurements, full gameplay playthrough, moving-scene visual checks, before/after matched captures, and clean-machine validation remain open.
