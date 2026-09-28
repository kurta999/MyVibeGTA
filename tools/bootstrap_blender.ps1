param()
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$version = '4.5.14'
$expectedHash = 'B9533D2397AC1984DB4466FB23A7A4649391CCA93F6E84209F9BCC60D071C8B9'
$workspace = Split-Path -Parent $PSScriptRoot
$toolDirectory = Join-Path $workspace 'build-tools'
$archiveName = "blender-$version-windows-x64.zip"
$archivePath = Join-Path $toolDirectory $archiveName
$blenderDirectory = Join-Path $toolDirectory "blender-$version-windows-x64"
$blenderExecutable = Join-Path $blenderDirectory 'blender.exe'
New-Item -ItemType Directory -Force -Path $toolDirectory | Out-Null

# Pin the official archive and its SHA-256, as published in blender-4.5.14.sha256.
# Nothing is installed system-wide or included in the game package.
if (-not (Test-Path -LiteralPath $archivePath)) {
    Invoke-WebRequest -Uri "https://download.blender.org/release/Blender4.5/$archiveName" `
        -OutFile $archivePath -TimeoutSec 600
}
$actualHash = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash
if ($actualHash -ne $expectedHash) {
    throw "Blender archive SHA-256 mismatch: expected $expectedHash, got $actualHash."
}
if (-not (Test-Path -LiteralPath $blenderExecutable)) {
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [IO.Compression.ZipFile]::ExtractToDirectory($archivePath, $toolDirectory, $true)
}
$versionOutput = & $blenderExecutable --version
if ($LASTEXITCODE -ne 0 -or $versionOutput[0] -ne "Blender $version LTS") {
    throw 'The extracted Blender executable did not report the pinned version.'
}
Write-Output $blenderExecutable
