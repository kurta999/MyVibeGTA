param([switch]$SkipBuild,[switch]$SkipSmoke,[switch]$IncludeOpenGL,
    [string]$PackageName='',
    [string]$Direct3DExecutable='MiniCity3D.exe',
    [string]$OpenGLExecutable='MiniCity3DGL.exe')
$ErrorActionPreference = 'Stop'

if (-not $SkipBuild) {
    & "$PSScriptRoot\build.ps1" -OutputPath $Direct3DExecutable
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$outputDirectory = Join-Path $PSScriptRoot 'dist'
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
$packageName = if ($PackageName) { $PackageName } else {
    'MiniCity3D-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' +
        ([guid]::NewGuid().ToString('N').Substring(0,4))
}
if ($packageName -notmatch '^[A-Za-z0-9][A-Za-z0-9._-]*$') {
    throw 'PackageName must contain only letters, digits, dots, underscores, and hyphens.'
}
$packageDirectory = Join-Path $outputDirectory $packageName
New-Item -ItemType Directory -Path $packageDirectory | Out-Null

$builtDirect3D = Join-Path $PSScriptRoot 'build-jolt-ninja\MiniCity3D.exe'
$direct3DSource = if ($Direct3DExecutable -eq 'MiniCity3D.exe' -and
    (Test-Path -LiteralPath $builtDirect3D)) {
    $builtDirect3D
} else { Join-Path $PSScriptRoot $Direct3DExecutable }
Copy-Item -LiteralPath $direct3DSource `
    -Destination (Join-Path $packageDirectory 'MiniCity3D.exe')
$executables = @('MiniCity3D.exe')
if ($IncludeOpenGL) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $OpenGLExecutable) `
        -Destination (Join-Path $packageDirectory 'MiniCity3DGL.exe')
    $executables += 'MiniCity3DGL.exe'
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'README.md') -Destination $packageDirectory
$assetSource = Join-Path $PSScriptRoot 'assets'
$assetDestination = Join-Path $packageDirectory 'assets'
New-Item -ItemType Directory -Path $assetDestination | Out-Null
foreach ($fileName in @('character_atlas.png', 'texture_atlas.png')) {
    Copy-Item -LiteralPath (Join-Path $assetSource $fileName) -Destination $assetDestination
}
foreach ($folderName in @('effects', 'materials')) {
    Copy-Item -LiteralPath (Join-Path $assetSource $folderName) -Destination $assetDestination -Recurse
}
$modelSource = Join-Path $assetSource 'models'
$modelDestination = Join-Path $assetDestination 'models'
New-Item -ItemType Directory -Path $modelDestination | Out-Null
Copy-Item -LiteralPath (Join-Path $modelSource 'baked') -Destination $modelDestination -Recurse
foreach ($fileName in @('LICENSES.md', 'CITY_MANIFEST.csv', 'NATURE_MANIFEST.csv', 'MARINA_PART.md')) {
    Copy-Item -LiteralPath (Join-Path $modelSource $fileName) -Destination $modelDestination
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'data') -Destination $packageDirectory -Recurse
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'third_party\JoltPhysics\LICENSE') `
    -Destination (Join-Path $packageDirectory 'JoltPhysics-LICENSE.txt')

if (-not $SkipSmoke) {
    foreach ($fileName in $executables) {
    $gameProcess = Start-Process -FilePath (Join-Path $packageDirectory $fileName) `
        -ArgumentList '--smoke --day' -WorkingDirectory $packageDirectory `
        -PassThru -Wait -WindowStyle Hidden
    if ($gameProcess.ExitCode -ne 0) {
        throw "$fileName failed its packaged smoke run with exit code $($gameProcess.ExitCode)"
    }
    }
    $nightRun = Start-Process -FilePath (Join-Path $packageDirectory 'MiniCity3D.exe') `
    -ArgumentList '--smoke --night --ragdoll' -WorkingDirectory $packageDirectory `
    -PassThru -Wait -WindowStyle Hidden
    if ($nightRun.ExitCode -ne 0) {
    throw "MiniCity3D.exe failed its packaged night/ragdoll smoke run with exit code $($nightRun.ExitCode)"
    }
    foreach ($regionFlag in @('--causeway', '--east', '--snowfield', '--desert-hub', '--savanna',
        '--swim', '--climb', '--fall', '--debug-menu')) {
    $regionRun = Start-Process -FilePath (Join-Path $packageDirectory 'MiniCity3D.exe') `
        -ArgumentList "--smoke --day $regionFlag" -WorkingDirectory $packageDirectory `
        -PassThru -Wait -WindowStyle Hidden
    if ($regionRun.ExitCode -ne 0) {
        throw "MiniCity3D.exe failed packaged $regionFlag smoke run with exit code $($regionRun.ExitCode)"
    }
    }
}

$packageLog = Join-Path $packageDirectory 'MiniCity3D.log'
if (Test-Path -LiteralPath $packageLog) { Remove-Item -LiteralPath $packageLog }
$archivePath = "$packageDirectory.zip"
Compress-Archive -LiteralPath $packageDirectory -DestinationPath $archivePath -CompressionLevel Optimal
if ($SkipSmoke) { Write-Host "Packaged $archivePath" }
else { Write-Host "Packaged and smoke-tested $archivePath" }
