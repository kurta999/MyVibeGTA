param([string]$Executable=(Join-Path $PSScriptRoot '../build-msvc-ninja/MyVibeGTA.exe'),
    [string]$OutputDirectory=(Join-Path $PSScriptRoot '../evidence/textures-20260928'))
$ErrorActionPreference='Stop'
$executablePath=(Resolve-Path -LiteralPath $Executable).Path
$runtimeDirectory=Split-Path -Parent $executablePath
$outputDirectory=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
$log=Join-Path $runtimeDirectory 'MyVibeGTA.log'
$before=if(Test-Path -LiteralPath $log){(Get-Content -LiteralPath $log).Count}else{0}
$arguments='--smoke --benchmark-route --1080p --day --high-shadows --high-taa'
$otherBefore=@(Get-Process -Name MyVibeGTA -ErrorAction SilentlyContinue)
if($otherBefore.Count){throw 'Close other MyVibeGTA instances before the isolated benchmark'}
$process=Start-Process -FilePath $executablePath -WorkingDirectory $runtimeDirectory `
    -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru
if($process.ExitCode -ne 0){throw "Benchmark failed: $($process.ExitCode)"}
$lines=@(Get-Content -LiteralPath $log | Select-Object -Skip $before)
$lines | Set-Content -LiteralPath (Join-Path $outputDirectory 'benchmark-route.txt')
if(@($lines | Select-String 'Benchmark route v1 1920x1080:').Count -ne 1){throw 'Missing measured route result'}
@{ExecutableSha256=(Get-FileHash -LiteralPath $executablePath -Algorithm SHA256).Hash;
    Arguments=$arguments;ExitCode=$process.ExitCode;OtherGameInstancesBefore=$otherBefore.Count;
    OtherGameInstancesAfter=@(Get-Process -Name MyVibeGTA -ErrorAction SilentlyContinue).Count} | ConvertTo-Json |
    Set-Content -LiteralPath (Join-Path $outputDirectory 'benchmark.json')
$lines | Select-String 'GPU adapter|Benchmark|GPU timings|Loaded image textures'
