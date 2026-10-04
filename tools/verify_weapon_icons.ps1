param([string[]]$Only)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$runtime=Join-Path $root 'build-msvc-ninja'
$evidence=Join-Path $root 'evidence/icons-vehicles-20261003'
New-Item -ItemType Directory -Path $evidence -Force | Out-Null
$cases=@(
    @{Name='weapon-pickups-day';Args=@('--weapon-icons-preview','--day')},
    @{Name='weapon-pickups-night';Args=@('--weapon-icons-preview','--night')},
    @{Name='vehicle-scale';Args=@('--vehicle-scale-preview','--day')},
    @{Name='car-driver';Args=@('--driver-preview','--modern-car-preview','--day')},
    @{Name='helicopter-flight';Args=@('--helicopter-preview','--day')}
)
foreach($case in $cases){
    if($Only -and $case.Name -notin $Only){continue}
    $args=@('--smoke','--1080p','--screenshot')+$case.Args
    $process=Start-Process -FilePath (Join-Path $runtime 'MiniCity3D.exe') -WorkingDirectory $runtime -ArgumentList $args -WindowStyle Hidden -Wait -PassThru
    if($process.ExitCode -ne 0){throw "$($case.Name): exit $($process.ExitCode)"}
    $latest=Get-ChildItem -LiteralPath (Join-Path $runtime 'screenshots') -Filter '*.png' | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if(-not $latest){throw 'Screenshot missing'}
    Copy-Item -LiteralPath $latest.FullName -Destination (Join-Path $evidence ($case.Name+'.png')) -Force
    Write-Output "$($case.Name): exit 0, $($latest.Name)"
}
