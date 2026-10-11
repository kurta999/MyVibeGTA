# ImGui build delivery repair

Verified on 2026-10-11. The existing `build-msvc-ninja` cache had `MINI_CITY_USE_DX12=ON`; its `MiniCity3D.exe` was the optional DX12 executable with the previous menu backend. The default root executable was a separate DX11 build, so the two launch paths behaved differently.

`build.ps1` now uses a separate `-dx12` build directory for each supported compiler/generator and defaults DX12 delivery to `MiniCity3D-DX12.exe`. Default DX11 builds retain `build-msvc-ninja/MiniCity3D.exe` and root `MiniCity3D.exe`. Explicit `-OutputPath` remains supported. Packaging uses the requested executable instead of selecting the newest root/build binary by timestamp.

Verification:

- `build.ps1` completed a Release DX11 rebuild and delivered the root executable. The default cache now has `MINI_CITY_USE_DX12=OFF` and `MINI_CITY_TRACY=OFF`.
- `ctest.txt`: the actual `imgui_menu_scenarios` test passed in 44.12 seconds.
- `build-menu-review.txt` and `root-menu-review.txt`: real widget actions and the complete native menu review passed in both executables. The root review ran from `build-imgui-review`, exercising launch from another working directory.
- `before.json` and `after.json`: root/build executable SHA-256 hashes match; save/settings files and the existing optional DX12 executable were unchanged by the reviews.
- PowerShell syntax parsing passed for both changed scripts; `git diff --check` passed. The new optional DX12 directory was not rebuilt in this repair.

Native captures remain under `build-msvc-ninja/imgui-review-27280` and `build-imgui-review/imgui-review-24140`. Visual inspection confirmed the ImGui Graphics page at 1280 x 720. OpenGL sources and target remain unchanged.
