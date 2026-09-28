param(
    [Parameter(Mandatory)][string]$InputPath,
    [Parameter(Mandatory)][string]$OutputPath,
    [Parameter(Mandatory)][string]$SourceUrl,
    [Parameter(Mandatory)][string]$License,
    [Parameter(Mandatory)][string]$Credit
)
$ErrorActionPreference = 'Stop'
$blenderExecutable = & "$PSScriptRoot\bootstrap_blender.ps1"
$sourcePath = (Resolve-Path -LiteralPath $InputPath).Path
$destinationPath = [IO.Path]::GetFullPath($OutputPath)
$previousUserResources = $env:BLENDER_USER_RESOURCES
$env:BLENDER_USER_RESOURCES = Join-Path (Split-Path -Parent $PSScriptRoot) 'build-tools\blender-user'
try {
& $blenderExecutable --background --factory-startup --python-exit-code 1 `
    --python "$PSScriptRoot\blender_convert.py" -- --input $sourcePath `
    --output $destinationPath --source-url $SourceUrl --license $License --credit $Credit
if ($LASTEXITCODE -ne 0) { throw "Canonical conversion failed with exit code $LASTEXITCODE" }
if (-not (Test-Path -LiteralPath $destinationPath)) { throw 'Conversion did not produce its GLB' }
} finally { $env:BLENDER_USER_RESOURCES = $previousUserResources }
