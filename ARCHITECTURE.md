# Architecture and refactoring status

The refactoring goal covers the application, simulation, assets, and both Direct3D
backends. It is **in progress**. The first checkpoint establishes renderer
ownership and removes verified duplication; it does not make the whole codebase
SOLID or finish the naming migration.

## Conventions

- Classes and functions use `UpperCaseNames`; data members use `m_`.
- Prefer small concrete components. Introduce an interface or template policy
  only for an actual replaceable dependency or shared algorithm.
- Give resources one clear owner. Borrow dependencies explicitly; do not hide
  device, context, or game-state access behind a service locator.
- Keep GPU memory layouts explicit and preserve shader/C++ layout compatibility.
- Separate behavior changes from structural changes and verify each runnable
  checkpoint against the existing behavior.
- Leave the OpenGL fallback untouched, including its renderer, textures, and
  executable target.

## Current renderer structure

`MiniCity3D` uses DX12. `MiniCity3D-DX11-benchmark` selects the retained DX11
implementation at build time. This is the existing build arrangement, despite
older roadmap text naming DX11 as the main target.

| Location | Responsibility |
| --- | --- |
| `src/renderer_dx11.cpp`, `src/renderer_dx12.cpp` | Application-facing entry points and ownership of one renderer session |
| `src/rendering/dx11/renderer.h`, `dx12/renderer.h` | Backend session state and operations; noncopyable owners |
| `lifecycle.cpp` | Device/session initialization, fullscreen transitions, shutdown, probe-bake orchestration |
| `frame.cpp` | Frame ordering, render passes, presentation, and frame statistics |
| `targets.cpp` | Render targets, shadows, sampling, and pipeline states |
| `geometry.cpp` | Visibility, instance batches, static scene upload, and draws |
| `skinning.cpp` | GPU pose upload/deformation, previous-pose history, CPU fallback |
| `lighting.cpp`, `probes.cpp` | Frame lighting constants and reflection probes |
| `capture.cpp` | Backend-specific framebuffer readback |
| `src/rendering/shaders/` | Shared HLSL and the distinct DX11/DX12 post and skin shaders |
| `src/rendering/frame_types.h` | Shared instance, scene, and probe buffer layouts |
| `src/rendering/loading.*` | Shared shader compilation, texture preparation, and loading helpers |
| `src/rendering/screenshot_writer.*` | PNG naming and encoding, independent of GPU readback |
| `src/rendering/gpu_profiler.h` | Resource-owning, nonblocking timestamp ring |
| `dx11/timing_api.h`, `dx12/timing_api.h` | Small policies adapting the timestamp ring to each API |
| `src/rendering/resource_owner.h` | Common single-reference GPU owner and factory cleanup |
| `src/rendering/mesh_cache.h` | Immutable mesh buffers, committed per mesh revision |
| `src/rendering/texture_library.h` | Shared image ownership, material lookup, upload and mip verification |
| `src/rendering/growable_buffer.h` | Dynamic stream ownership and transactional capacity growth |
| `dx11/resource_api.h`, `dx12/resource_api.h` | Allocation and readback operations specific to each API |

The application entry points currently retain their old names so the renderer
work can be verified independently of the wider application migration. The new
backend methods and components follow the requested naming convention.

### Ownership and lifetime

The application facade constructs a candidate renderer and publishes it only
after initialization succeeds. A failed candidate is destroyed immediately.
Shutdown destroys the session, which releases backend resources in the established
GPU-safe order. Session copying is disabled.

The profiler owns its queries with `unique_ptr` and a release deleter. Device and
context references are borrowed per operation. Eight slots retain pending GPU
results; saturation skips profiling rather than blocking rendering or reusing
queries that are still pending. DX11 uses four timestamp stages; DX12 adds skin
and post-processing stages. Both execute the same ownership and polling logic.

The screenshot writer owns its filename sequence. The backend maps the GPU
readback resource, passes pixels and pitch to the writer, then unmaps it.

The texture library owns one view per file and interpretation. Surface slots,
mesh materials, and PBR channels borrow those views; aliasing a surface does not
retain another reference. The view retains the GPU image. Upload temporaries
are scoped owners, and failed uploads publish no cache entry or statistics.
Compressed-image validation compares every row of every GPU mip, allowing GPU
row padding. The existing `--validate-loading` option enables this check.

The mesh cache builds both vertex and index buffers before publishing a mesh
revision. A failed replacement releases its temporaries and retains the prior
allocation for a retry, but lookup rejects that stale revision for drawing.
Growable buffers likewise publish their resource and capacity together: failed
growth preserves the previous allocation and capacity. Vertex and instance
streams share this algorithm, with explicit stride, growth reserve, and limits.
All four owners use the same release deleter for COM and native DX12 resources.
They borrow the device/context and are destroyed before those dependencies.

### Shared code boundaries

Eight identical shader blocks have a single source. Backend-specific post and
skin shaders remain separate because they implement different behavior. Shared
CPU buffer types keep the two renderer layouts synchronized. API-specific
allocation, barriers, command recording, and FSR2 integration remain in DX12.

## Remaining work

The backend sessions still have too many responsibilities and fields. Splitting
their implementation into files is an intermediate step, not the final design.

1. Extract coherent resource owners for render targets, skinning, and probe
   state; decompose frame execution into named passes. Texture/mesh caches and
   dynamic vertex streams have been extracted.
2. Separate the Windows application lifecycle and smoke/benchmark fixtures from
   the long `WinMain` function.
3. Establish explicit simulation state and service boundaries around the current
   shared game globals, physics, gameplay systems, and persistence.
4. Simplify the native DX12 device/context implementation and clarify its resource,
   queue, descriptor, and command-recording ownership.
5. Complete the class/function/member naming migration across production code and
   its tests, while preserving the excluded OpenGL implementation.
6. Verify both renderers, simulation scenarios, save/load, failure cleanup, and
   representative gameplay before declaring the full rewrite complete.

## Verification

Build both backends with `MINI_CITY_BUILD_DX11_BENCHMARK=ON`. The normal test build
also includes `gpu_profiler_scenarios`, which checks partial creation failures,
ring saturation, deferred/partial results, invalid frequency, reuse, reset, and
destruction for both timestamp layouts.
`resource_cache_scenarios` covers both release contracts, partial mesh uploads,
revision replacement and retry, texture interpretation/sharing, padded mip
readback and failure cleanup, factory exceptions, capacity growth failures,
allocation limits, reset, and destruction.

`tools/verify_rendering_refactor.ps1` runs isolated full-game DX11, native DX12,
FSR2, and worker-recording fixtures, preserving the user's root save/settings.
With rebuilt original executables supplied through `-BaselineDirectory`, it also
captures matching reference runs. `tools/compare_refactor_captures.ps1` compares the
decoded RGB pixels. Evidence belongs in `evidence/rendering-refactor-20261009/`.
The subsequent resource ownership checkpoint uses
`evidence/resource-ownership-20261009/`, with `--validate-loading` supplied through
`-AdditionalArguments` and the preceding checkpoint as its baseline.
