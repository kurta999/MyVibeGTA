# Builder tool contact feedback — 2026-10-06

This pass integrates material contact feedback and tool-family motions in the Direct3D 11 builder layer. It does not complete the full Minecraft plan.

## Behavior

- Held mining uses family-specific stroke periods and poses for pickaxes, axes, shovels, hoes, shears, and brushes. Fast cuts retain a short follow-through. A contact before completion produces cosmetic feedback without awarding a resource or spending durability; successful harvesting retains the existing single resource/durability transaction.
- Progressive cracks follow the targeted surviving surface. Block/building endpoints are constrained to their occupied section, natural-ground endpoints follow the original surface, and scenery endpoints are projected back onto the actual textured/alpha-tested triangles. Foliage and brush deposits use clipping/dust feedback instead of stone cracks.
- Contact produces material-textured chips, elongated wood fragments, flat foliage clippings, or small rising brush dust clouds. There are at most 192 cosmetic particles. They expire, have no physics bodies, and are cleared by inventory/mode transitions and state restoration. Builder dust has its own soft radial-opacity mesh; ordinary smoke retains its existing material.
- Stone, wood, soil, sand/gravel, snow, foliage, metal/ore, and brushing have eight distinct original synthesized contact sounds, routed through the existing positional XAudio2 cache and voice limits.
- Shears have two independently moving textured blade/handle halves around a shared pivot, including real finger holes. The held model opens and closes in first and third person; dropped/inventory shears retain the complete model. The shovel shaft now ends inside its blade, and its first-person grip places the full blade in view.

## Verification

All **29 CTest suites passed in 94.63 seconds** after the final gameplay/render changes; see `ctest.log`. The audio smoke test was then strengthened to explicitly play variants 6 and 7 as well as the other six material variants and boundary values, rebuilt, and passed with WAV export; see `audio.log`.

`tests/builder_feedback_scenarios.cpp` uses live mining input to check contact timing, progressive cracks, typed material feedback, tool pose changes, cancellation, protected-ore rejection, durability/drop conservation, ground projection, shears pivots/opening in both cameras, the complete dropped model, particle limits/expiry, and F5/restoration cleanup.

`tests/scenery_scenarios.cpp` retraces crack endpoints against an actual tree surface before cutting it. `tests/tool_work_scenarios.cpp` covers contact from every hoe variant and actual RMB brushing; it checks that brush particles become rendered transparent instances above the ground and ordinary smoke remains unchanged.

All 70 generated builder meshes passed binary-layout and SHA-256 checks for their models, textures, icons, and retained OBJ sources: **67 catalog items plus three shears components, 4,268 triangles total**. The additional procedural dust mesh is created by `src/dx11_assets.cpp`. Asset reproduction uses `tools/build_builder_assets.py` and `assets/models/source/builder/manifest.json`.

The eight WAV files in `audio/` come from the runtime synthesizer. Export checks verify nonzero RMS, unclipped sample peaks, and distinct synthesis fingerprints. Live XAudio2 checks exercise cached positional playback, voice overlap/stealing, and reopening with volume muted. These checks do not establish subjective sound quality; listening and play tuning remain review work.

## Visual evidence and reproduction

All nine DX11 images were visually inspected:

| Capture | Observed behavior |
|---|---|
| `stone.png` | Equipped pickaxe, textured granite, growing cracks and contact chips at 48% mining progress |
| `wood.png` | Axe chopping pose, textured log and cracks at 60% progress |
| `soil.png` | Entire shovel blade in view, handle ending within blade, soil cracks at 68% progress |
| `shears-open.png` | Open blades, shared pivot, textured finger handles with holes |
| `shears-close.png` | Blades closing at 60% foliage-cutting progress |
| `shears-cut.png` | Removed leaf block, visible foliage clippings, one collected leaf item and one durability charge |
| `shears-third.png` | Modeled shears attached to the animated character hand |
| `brush.png` | Equipped bristled brush, small dust clouds above a modeled deposit at 20% progress |
| `normal.png` | Builder block, tool, cracks, particles and hotbar hidden after F5 exit |

Reproduce the focused regressions, WAV export and all captures with `tools/verify_builder_feedback.ps1`. Use `-Views soil` or `-Views brush` for a selected capture. Fixtures supply tools/materials and use ordinary input for mining/brushing; they are not an acquisition/progression playthrough. Save-writing tests and previews run sequentially in `build-msvc-ninja`, away from the user's root save.

The verified executable SHA-256 is `66DF348C729F235F07DBD679E3C679D17E157A6F131E9057350758F404ED3EA7`. The user's root save retained SHA-256 `5A4C87DB5DC9E23148622694D21056E0B18EC4FE45FE7B294C59826B289FC539`.

## Remaining scope

The full plan still requires progression/play tuning, a complete tool/item appearance and gameplay-loop review, complete underground actor/navigation integration and loose-prop/corpse transition recovery, transition/save failure handling, face/collider merging, bounded dense-world work and memory checks, and interactive performance verification. Passing contact-feedback tests is not evidence that those requirements are complete.
