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
    $compiler = Get-Command clang++ -ErrorAction SilentlyContinue
    if ($compiler) {
        $buildDirectory = 'build-jolt-ninja'
        $builtExecutable = Join-Path $buildDirectory 'MiniCity3D.exe'
        & $cmake -S . -B $buildDirectory -G Ninja "-DCMAKE_CXX_COMPILER=$($compiler.Source.Replace('\','/'))" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        & $cmake --build $buildDirectory --target MiniCity3D simulation_smoke asset_smoke -j 6
    } else {
        $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
        if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Clang or Visual Studio C++ tools are required.' }
        $vs2022 = & $vswhere -products '*' -version '[17.0,18.0)' -latest `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        $vs2026 = & $vswhere -products '*' -version '[18.0,19.0)' -latest `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        $generator = if ($vs2022) { 'Visual Studio 17 2022' }
            elseif ($vs2026) { 'Visual Studio 18 2026' }
            else { throw 'Clang or Visual Studio C++ tools are required.' }
        $buildDirectory = 'build-msvc'
        $builtExecutable = Join-Path $buildDirectory 'Release\MiniCity3D.exe'
        & $cmake -S . -B $buildDirectory -G $generator -A x64 -DBUILD_TESTING=ON
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        & $cmake --build $buildDirectory --config Release --target MiniCity3D simulation_smoke asset_smoke --parallel 6
    }
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    try {
        Copy-Item -LiteralPath $builtExecutable -Destination $target -Force -ErrorAction Stop
        Write-Host "Built $target (Direct3D 11 + Jolt Physics)"
    } catch [System.IO.IOException] {
        Write-Warning "Built $builtExecutable; $target is in use and was not replaced."
    }
}
