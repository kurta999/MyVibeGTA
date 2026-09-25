param([switch]$OpenGL,[string]$OutputPath='')
$ErrorActionPreference = 'Stop'
$common = @('main.cpp','input.cpp','logging.cpp','game.cpp','ai.cpp','content.cpp','audio.cpp','ui.cpp','physics.cpp','weapons.cpp',
    'savegame.cpp','camera.cpp','props.cpp')
if ($OpenGL) {
    $sources = @($common | ForEach-Object { "src/$_" }) + @('src/renderer.cpp','src/textures.cpp')
    $target = if ($OutputPath) { $OutputPath } else { 'MiniCity3DGL.exe' }
    clang++ -std=c++17 -O2 @sources -o $target '-Wl,/SUBSYSTEM:WINDOWS' `
        -luser32 -lgdi32 -lopengl32 -lglu32 -lgdiplus -lole32 -lxaudio2
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    Write-Host "Built $target (OpenGL fallback)"
} else {
    $target = if ($OutputPath) { $OutputPath } else { 'MiniCity3D.exe' }
    $cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source
    if (-not $cmake) {
        $cmake = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
    }
    if (-not (Test-Path -LiteralPath $cmake)) { throw 'CMake 3.20+ is required for the pinned Jolt Physics build.' }
    $compiler = (Get-Command clang++ -ErrorAction Stop).Source.Replace('\','/')
    & $cmake -S . -B build-jolt-ninja -G Ninja "-DCMAKE_CXX_COMPILER=$compiler" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $cmake --build build-jolt-ninja --target MiniCity3D simulation_smoke asset_smoke -j 6
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    try {
        Copy-Item -LiteralPath 'build-jolt-ninja\MiniCity3D.exe' -Destination $target -Force -ErrorAction Stop
        Write-Host "Built $target (Direct3D 11 + Jolt Physics)"
    } catch [System.IO.IOException] {
        Write-Warning "Built build-jolt-ninja\MiniCity3D.exe; $target is in use and was not replaced."
    }
}
