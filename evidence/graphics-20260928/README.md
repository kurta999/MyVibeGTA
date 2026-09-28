# DX11 graphics milestone captures (2026-09-28)

All images are 1920×1080 captures from `MiniCity3D` on the local AMD Radeon
680M. Each named High/Medium pair uses the same smoke preview setup and camera.

| Images | Reproduction flags | Inspection point |
| --- | --- | --- |
| `headlight-high.png`, `headlight-medium.png` | `--smoke --night --driver-preview --headlight-preview --1080p --screenshot`, with `--high-shadows` or `--medium-shadows` | The blocker casts a beam shadow on High. |
| `streetlight-high.png`, `streetlight-medium.png` | `--smoke --night --streetlight-preview --1080p --screenshot`, with `--high-shadows` or `--medium-shadows` | The nearby lamp casts a ground shadow on High. |
| `taa-high-day.png`, `taa-fxaa-day.png` | `--smoke --day --1080p --high-taa --temporal-preview --screenshot`, with `--no-taa` added for the reference | Frame 60 of the same moving-camera path. |
| `fxaa-low-day.png` | `--smoke --day --1080p --low-aa --screenshot` | The Low anti-aliasing setting runs a lighter FXAA pass. |
| `camera-motion-view.png` | The TAA day command plus `--motion-view` | Static depth pixels show camera motion; moving meshes and sky are black because they have no usable object vectors/history. |
| `taa-high-night.png` | `--smoke --night --ragdoll --1080p --high-taa --temporal-preview --screenshot` | Night and animated-character rendering after 60 frames. |

The temporal screenshots establish a runnable resolve and camera-vector debug
view. A single captured frame cannot establish absence of ghosting or shimmer
through a longer moving sequence. Object and skeletal motion vectors remain
open beyond the ordinary-humanoid follow-up described below in `graphics-upgrade-plan.md`.

## Ordinary humanoid GPU deformation and pose vectors

| Capture | Arguments | Check |
| --- | --- | --- |
| `gpu-skin-day.png` | `--smoke --validate-gpu-skinning --screenshot --1080p` | Ordinary humanoids use GPU-deformed vertices in scene and shadow draws. |
| `cpu-skin-day.png` | `--smoke --cpu-skinning --screenshot --1080p` | Matching CPU deformation reference. |
| `skin-motion-view.png` | `--smoke --skin-motion-preview --high-taa --high-shadows --motion-view --validate-gpu-skinning --screenshot --1080p` | At frame 60 with fixed camera yaw, humanoid silhouettes contain pose motion. Vehicles remain black. |
| `skin-taa-day.png` | `--smoke --skin-motion-preview --high-taa --high-shadows --screenshot --1080p` | High TAA with ordinary humanoid pose vectors. |
| `cpu-skin-taa-day.png` | `--smoke --skin-motion-preview --high-taa --high-shadows --cpu-skinning --screenshot --1080p` | CPU pose reference; these poses opt out of history. |

Readback validation at frames 1, 30, and 60 compared both current vertices and
previous positions/validity against CPU deformation. All three passed; maximum
error was 0.00006104 world units. There were nine current poses at each sample,
zero valid prior poses on frame 1, and nine on frames 30 and 60. Seated and
pitched-aim routing is covered by the driver/pedestrian tests. These captures
do not verify animal skinning, procedural CPU pose vectors, ragdoll vectors,
vehicle vectors, or the absence of artifacts throughout a longer sequence.

## Authored Marina LODs

`marina-lod0.png` through `marina-lod3.png` use `--smoke --day --marina
--1080p --screenshot --lod-level-N`. `marina-lod-view.png` replaces the forced
level with `--lod-view`: green/yellow/orange/red mean full/LOD1/LOD2/LOD3.
All five runs exit 0 and their captures were inspected. Catalog and geometry
validation plus hysteresis boundary tests pass in the asset suite. The coarse
forced view exposes merged balcony guard panels intended for distant use.
These stills do not verify moving threshold transitions or LODs for the absent
sedan, pedestrians, or wolf.
