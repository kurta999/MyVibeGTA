# DX11 district probe lighting

## Reproduce

```powershell
./build.ps1 -OutputPath build-msvc-ninja/MiniCity3D.exe
# Run from build-msvc-ninja, using the DX11 executable:
./MiniCity3D.exe --smoke --bake-probes
# From repository root, with NumPy 2.3.5:
python tools/cook_probes.py --captures build-msvc-ninja/probe-captures
./build.ps1 -OutputPath build-msvc-ninja/MiniCity3D.exe -RunTests
python tests/probe_cooking.py
```

The capture run produces 36 original static scene faces in its executable folder.
Two positions cover the starting green space and nearby street: (100,42,100)
and (300,42,350), with influence radii 750 and 650 world units. States use hours
12, 18.5 and 22. Cube face order is +X/-X/+Y/-Y/+Z/-Z. Captures use square 90°
views, fixed graphics/texture/filtering/draw/LOD settings, clear weather, and a
fixed world time. Characters, vehicles, animals, effects, their lights, markers,
and the visible sun are excluded; static lamps, shop lights and windows remain.
Probe sampling is disabled during the bake to prevent feedback. Pixels store
linear RGBA16F scene radiance before post-processing, with the original analytic
sky gradient filled at depth misses. No HUD, exposure, tone map or gamma is baked.

The cooker writes `assets/lighting/showcase.mcpb` and a report with capture
hashes, positions, hours, settings, dependency version and output hash. It
integrates nine solid-angle-weighted SH coefficients for diffuse radiance,
prefilters GGX specular over all eight mips using 512 Hammersley samples per
texel, and integrates a 128² two-channel split-sum BRDF lookup using 1024 samples.
Lighting stays uncompressed RGBA16F; the BRDF lookup stays RG32F. Three original
analytic sky cubes provide a global fallback outside local influence volumes.
No external HDRI or restricted model is included.

Runtime interpolation smoothly blends day/dusk/night, including a mirrored dawn
transition. Local influence weights blend with each other and the global sky per
pixel. Weather cloud cover attenuates the baked lighting. The scene shader uses
SH diffuse and prefiltered specular with Fresnel/metallic response; AO acts on
that indirect contribution. SSR hits replace only the probe specular lobe;
misses retain the probe result. An opaque reflection-response target prevents
adding another environment term in the post pass. Transparent glass receives
probe lighting but uses opaque depth and surface data for screen-space effects.

`--probe-view` shows indirect probe light, `--probe-weight-view` shows
(global, first local, second local) weights, and `--no-probes` selects the legacy
ambient reference. The loader validates version, dimensions, complete mips,
finite SH/texel/BRDF values and file length; invalid/missing resources log a
message and retain legacy ambient lighting. Resources survive target resize and
are released at shutdown.

## Limits

This is a single static radiance capture using the previous ambient/direct
lighting model, not a multi-bounce path-traced bake. Influence spheres do not
provide interior visibility volumes or box parallax correction. Weather blending
is an approximation. Moving lights and objects are shaded at runtime and are
excluded from the bake. Recapture after static geometry/material/light changes,
including the eventual named showcase integration. New showcase asset access,
full glTF materials, and broader moving-scene validation remain separate work.

## References

- [Microsoft cube face order](https://learn.microsoft.com/en-us/previous-versions/ms859061(v=msdn.10))
- [Microsoft TextureCubeArray](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/texturecubearray)
- [Filament image-based lighting math](https://github.com/google/filament/blob/main/docs/Filament.md.html)

The implementation uses conventional SH cosine convolution and GGX split-sum
integration. It is original project code; no third-party shader code was copied.
