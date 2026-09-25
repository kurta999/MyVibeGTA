$ErrorActionPreference = 'Stop'
& ./build.ps1
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$ctest = (Get-Command ctest -ErrorAction SilentlyContinue).Source
if (-not $ctest) {
    $ctest = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe'
}
$buildDirectory = if (Get-Command clang++ -ErrorAction SilentlyContinue) { 'build-jolt-ninja' } else { 'build-msvc' }
& $ctest --test-dir $buildDirectory -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
