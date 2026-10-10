# Shared rendering resource ownership checkpoint

This is a verified, intermediate step in the full codebase rewrite. Both DX11
and DX12 use the same resource owners for immutable mesh revisions, image
textures, and growing dynamic vertex/instance streams. Native allocation and
readback remain in small backend policies. The profiler shares the same release
deleter. OpenGL is excluded.

Mesh replacement publishes only after both buffers succeed. Texture uploads
publish only a successfully created and, when requested, verified view. Dynamic
growth commits buffer/capacity together, fixes the previous failed-allocation
capacity bug, bounds the native byte width, and clamps reserve growth to the
configured element limit. Factory failure/exception paths release assigned
outputs. Surface aliases and mesh/PBR lookups borrow the library's views.

## Verification

- Release builds of `MiniCity3D` and `MiniCity3D-DX11-benchmark` pass (`build.txt`).
- Eight targeted CTest suites pass in 14.00 seconds (`tests.txt`). The new
  `resource_cache_scenarios` checks both counted and void `Release` contracts,
  partial failures, revision replacement/retry, interpretation/sharing,
  padded mip readback, growth limits/retry, reset, and destruction.
- Four matched baseline/candidate game configurations pass: DX11, native DX12,
  FSR2 Quality, and DX12 with four command-recording workers. Each executable
  validates 42 DDS uploads across all their mips and three GPU skin poses.
  All DX12 runs report zero validation errors (`verification.json`). Prepared
  texture checksums and texture payload totals match their baseline.
- Decoded 1920x1080 RGB comparisons: FSR2 is exact; the other three pairs differ
  at five pixels, x=973, y=318..325, with maximum channel differences of 14 for
  DX11 and 13 for DX12 (`pixel-comparisons.json`). A repeated native DX12 pair is
  exact. Its baseline is unchanged, while the same candidate executable varies
  at those five pixels between runs (`repeat-comparisons.json`). This bounded
  variation also appeared in the prior checkpoint; its cause remains unproven.
  The existing foreground helicopter obstructs part of this fixture in both
  versions. This checkpoint makes no performance or broad visual-quality claim.
- The delivered root executable matches the verified DX12 build. The root save
  SHA256 is unchanged (`delivery.json`).

The baseline is the preceding renderer-session checkpoint, preserved before
resource extraction, rather than the repository's old renderer sources. Exact
executable hashes and arguments are retained in `runs.json`; its DX12 baseline
matches the prior checkpoint's delivery hash. `repeat-native/` contains the
additional pair, logs, and hashes. Runtime assets, settings and save files were
isolated under ignored `build-codex/rendering-refactor-runtime`.

Reproduce the game runs with `tools/verify_rendering_refactor.ps1`, specifying
this evidence directory, matching baseline executables via `-BaselineDirectory`,
and `-AdditionalArguments @('--validate-loading')`. Compare the captures with
`tools/compare_refactor_captures.ps1`.

Remaining work includes resource owners for targets, skinning and probes; named
render passes; application and simulation boundaries; native DX12 device/context
ownership; and the wider naming migration. See `ARCHITECTURE.md`.
