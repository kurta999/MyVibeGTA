param([switch]$SkipBuild,
    [string]$Direct3DExecutable='MiniCity3D.exe',
    [string]$OpenGLExecutable='MiniCity3DGL.exe')
$ErrorActionPreference = 'Stop'

if (-not $SkipBuild) {
    & "$PSScriptRoot\build.ps1" -OutputPath $Direct3DExecutable
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & "$PSScriptRoot\build.ps1" -OpenGL -OutputPath $OpenGLExecutable
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$outputDirectory = Join-Path $PSScriptRoot 'dist'
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
$packageName = 'MiniCity3D-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' +
    ([guid]::NewGuid().ToString('N').Substring(0,4))
$packageDirectory = Join-Path $outputDirectory $packageName
New-Item -ItemType Directory -Path $packageDirectory | Out-Null

Copy-Item -LiteralPath (Join-Path $PSScriptRoot $Direct3DExecutable) `
    -Destination (Join-Path $packageDirectory 'MiniCity3D.exe')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot $OpenGLExecutable) `
    -Destination (Join-Path $packageDirectory 'MiniCity3DGL.exe')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'README.md') -Destination $packageDirectory
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'assets') -Destination $packageDirectory -Recurse
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'third_party\JoltPhysics\LICENSE') `
    -Destination (Join-Path $packageDirectory 'JoltPhysics-LICENSE.txt')

foreach ($fileName in @('MiniCity3D.exe','MiniCity3DGL.exe')) {
    $gameProcess = Start-Process -FilePath (Join-Path $packageDirectory $fileName) `
        -ArgumentList '--smoke --day' -WorkingDirectory $packageDirectory `
        -PassThru -Wait -WindowStyle Hidden
    if ($gameProcess.ExitCode -ne 0) {
        throw "$fileName failed its packaged smoke run with exit code $($gameProcess.ExitCode)"
    }
}

$packageLog = Join-Path $packageDirectory 'MiniCity3D.log'
if (Test-Path -LiteralPath $packageLog) { Remove-Item -LiteralPath $packageLog }
$archivePath = "$packageDirectory.zip"
Compress-Archive -LiteralPath $packageDirectory -DestinationPath $archivePath -CompressionLevel Optimal
Write-Host "Packaged and smoke-tested $archivePath"
