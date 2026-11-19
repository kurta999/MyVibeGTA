param([switch]$Fix)
# Checks (or with -Fix, applies) .clang-format on the files listed in
# tools/format-files.txt. Run in CI; extend the list as files get formatted.
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$command = Get-Command clang-format -ErrorAction SilentlyContinue
$clangFormat = if ($command) { $command.Source } else { '' }
if (-not $clangFormat) {
    $candidates = @("$env:ProgramFiles\LLVM\bin\clang-format.exe")
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $vs = & $vswhere -latest -products '*' -property installationPath
        if ($vs) { $candidates += Join-Path $vs 'VC\Tools\Llvm\x64\bin\clang-format.exe' }
    }
    $clangFormat = $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}
if (-not $clangFormat) { throw 'clang-format was not found (install LLVM or the VS Clang tools).' }
$files = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'format-files.txt') |
    Where-Object { $_ -and -not $_.StartsWith('#') }
$failed = @()
foreach ($file in $files) {
    $path = Join-Path $root $file
    if ($Fix) { & $clangFormat -i $path; continue }
    & $clangFormat --dry-run --Werror $path 2>$null
    if ($LASTEXITCODE -ne 0) { $failed += $file }
}
if ($failed.Count) {
    Write-Host "Not clang-formatted ($($failed.Count)): $($failed -join ', ')"
    Write-Host 'Run tools/check_format.ps1 -Fix'
    exit 1
}
Write-Host "clang-format: $($files.Count) files OK"
