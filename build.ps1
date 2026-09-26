param([switch]$OpenGL,[string]$OutputPath='',[switch]$RunTests)
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
    $root = $PSScriptRoot
    if (-not (Test-Path -LiteralPath (Join-Path $root 'third_party\JoltPhysics\Build\CMakeLists.txt'))) {
        throw 'Jolt is missing. Run git submodule update --init from the repository, then build again.'
    }
    $vswhere = if (${env:ProgramFiles(x86)}) {
        Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    } else { '' }
    $vs2022 = if ($vswhere -and (Test-Path -LiteralPath $vswhere)) {
        & $vswhere -products '*' -version '[17.0,18.0)' -latest `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    } else { '' }
    $vs2026 = if ($vswhere -and (Test-Path -LiteralPath $vswhere)) {
        & $vswhere -products '*' -version '[18.0,19.0)' -latest `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    } else { '' }
    $vsInstall = if ($vs2022) { $vs2022 } else { $vs2026 }
    $cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
    $cmakeInstall = if ($vs2026) { $vs2026 } else { $vsInstall }
    $cmake = if ($cmakeCommand) { $cmakeCommand.Source } elseif ($cmakeInstall) {
        Join-Path $cmakeInstall 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
    } else { '' }
    if (-not $cmake -or -not (Test-Path -LiteralPath $cmake)) {
        throw 'CMake 3.20+ is required. Install it or add cmake.exe to PATH.'
    }
    if ($vsInstall) {
        $generator = if ($vs2022) { 'Visual Studio 17 2022' }
            else { 'Visual Studio 18 2026' }
        $buildName = if ($vs2022) { 'build-msvc' } else { 'build-msvc-v18' }
        $buildDirectory = Join-Path $root $buildName
        $builtExecutable = Join-Path $buildDirectory 'Release\MiniCity3D.exe'
        & $cmake -S $root -B $buildDirectory -G $generator -A x64 -DBUILD_TESTING=ON
    } else {
        $compiler = Get-Command clang++ -ErrorAction SilentlyContinue
        $ninja = Get-Command ninja -ErrorAction SilentlyContinue
        if (-not $compiler -or -not $ninja) {
            throw 'Install Visual Studio C++ tools, or install both clang++ and Ninja.'
        }
        $buildDirectory = Join-Path $root 'build-jolt-ninja'
        $builtExecutable = Join-Path $buildDirectory 'MiniCity3D.exe'
        & $cmake -S $root -B $buildDirectory -G Ninja `
            "-DCMAKE_CXX_COMPILER=$($compiler.Source.Replace('\','/'))" `
            -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
    }
    if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed ($LASTEXITCODE)." }
    & $cmake --build $buildDirectory --config Release `
        --target MiniCity3D simulation_smoke asset_smoke --parallel 6
    if ($LASTEXITCODE -ne 0) { throw "Build failed ($LASTEXITCODE)." }
    if ($RunTests) {
        $ctest = Join-Path (Split-Path -Parent $cmake) 'ctest.exe'
        if (-not (Test-Path -LiteralPath $ctest)) { throw 'ctest.exe was not found beside cmake.exe.' }
        & $ctest --test-dir $buildDirectory -C Release --output-on-failure
        if ($LASTEXITCODE -ne 0) { throw "Smoke tests failed ($LASTEXITCODE)." }
    }
    $target = if ($OutputPath) {
        if ([System.IO.Path]::IsPathRooted($OutputPath)) {
            [System.IO.Path]::GetFullPath($OutputPath)
        } else {
            [System.IO.Path]::GetFullPath((Join-Path (Get-Location).Path $OutputPath))
        }
    } else { Join-Path $root 'MiniCity3D.exe' }
    if (-not [string]::Equals($builtExecutable,$target,[System.StringComparison]::OrdinalIgnoreCase)) {
        try {
            Copy-Item -LiteralPath $builtExecutable -Destination $target -Force -ErrorAction Stop
        } catch [System.IO.IOException] {
            throw "Build succeeded at $builtExecutable, but $target could not be replaced. Close the running game and rerun build.ps1."
        }
    }
    Write-Host "Built $target (Direct3D 11 + Jolt Physics)"
}
