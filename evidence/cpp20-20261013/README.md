# C++20 and default DX12 build

Verified on 2026-10-13 with MSVC 19.51.36260.0, Release, Tracy disabled.

Direct3D game, benchmark, and test targets share the `MiniCityCpp20` language
requirement. Fresh CMake configurations and `build.ps1` default to DX12/FSR2.
`build.ps1 -DX11` delivers `MiniCity3D-DX11.exe` with Dear ImGui menus. Existing
CMake caches retain their backend choice until explicitly changed.

Cleanup removes duplicated worker-option parsing and runtime copies, uses
C++20 container membership/removal operations, handles `char8_t` paths in UTF-8
logging, and expands compressed backend/worker/save code into readable
statements. The root `.clang-format` replaces the identical renderer-only
configuration. Existing GPU owners and API policies remain in place.

## Verification

| Check | Result |
| --- | --- |
| `./build.ps1 -RunTests` | DX12 build; 46/46 tests passed in 647.61 seconds |
| `./build.ps1 -DX11` | C++20 DX11 game and test targets built |
| DX11 `imgui_menu_scenarios` | Passed in 34.49 seconds |
| Native DX12, FSR2 Quality, four recording workers | All full-game runs passed; zero debug-layer errors |
| GPU skin readbacks and compressed texture mip checks | Passed in all three runs |
| Script syntax, conflicting backend switches, formatting, diff whitespace | Passed |

See [ctest.txt](ctest.txt), [dx11-menu-test.txt](dx11-menu-test.txt),
[build-settings.txt](build-settings.txt), and [runs.json](runs.json). Reproduce
the full-game checks with:

```powershell
./tools/verify_rendering_refactor.ps1 -Evidence evidence/cpp20-20261013 `
    -SelectedConfigurations @('dx12', 'dx12-fsr2', 'dx12-workers') `
    -AdditionalArguments @('--validate-loading')
```

All three 1080p captures were visually inspected. The native capture matches
the retained 2026-10-09 fixture's scene and HUD, including its existing weapon
showcase placement. These checks establish regression coverage, not a graphics
quality or performance improvement.

The delivered root DX12 binary matches the tested SHA-256:
`025029A9F666F4CDC164D354D9E84850314FBA9F0C36F8123C2D3CEE5E212A41`.
The root save was preserved. OpenGL renderer/texture sources and its CMake target
block are unchanged and retain C++17.

DX12 retains its existing menus; Dear ImGui remains specific to DX11. The wider
architecture and gameplay roadmaps remain in progress.
