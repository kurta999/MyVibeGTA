param([switch]$Benchmark)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$runtime=Join-Path $root 'build-msvc-ninja'
$evidence=Join-Path $root 'evidence/sky-20261003'
New-Item -ItemType Directory -Path $evidence -Force | Out-Null
$cases=@(
    @{Name='day';Flags=@('--day','--windy')},
    @{Name='zenith';Flags=@('--day','--windy','--sky-zenith')},
    @{Name='overcast';Flags=@('--day','--overcast')},
    @{Name='sunset';Flags=@('--sunset','--windy')},
    @{Name='night';Flags=@('--night','--windy')},
    @{Name='rain';Flags=@('--day','--rain')},
    @{Name='above';Flags=@('--day','--overcast','--sky-above')},
    @{Name='inside';Flags=@('--day','--overcast','--sky-inside')},
    @{Name='debug';Flags=@('--day','--windy','--infinite-ammo-preview')}
)
if($Benchmark){$cases=@(@{Name='benchmark';Flags=@('--day','--windy','--benchmark','--1080p')})}
foreach($case in $cases){
    $flags=@('--smoke','--sky-preview','--screenshot')+$case.Flags
    $process=Start-Process -FilePath (Join-Path $runtime 'MiniCity3D.exe') -WorkingDirectory $runtime -ArgumentList $flags -WindowStyle Hidden -Wait -PassThru
    if($process.ExitCode -ne 0){throw "$($case.Name) preview failed: $($process.ExitCode)"}
    $latest=Get-ChildItem -LiteralPath (Join-Path $runtime 'screenshots') -Filter '*.png' | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if(-not $latest){throw 'Screenshot missing'}
    Copy-Item -LiteralPath $latest.FullName -Destination (Join-Path $evidence "$($case.Name).png") -Force
    Write-Output "$($case.Name): exit 0; $($latest.Name)"
}
