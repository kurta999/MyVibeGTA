param([switch]$Benchmark,[switch]$Quick,[string]$Capture='')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$runtime=Join-Path $root 'build-msvc-ninja'
$evidence=Join-Path $root 'evidence/grass-20261003'
New-Item -ItemType Directory -Path $evidence -Force | Out-Null
$cases=@(
    @{Name='meadow';Flags=@()},
    @{Name='lawn';Flags=@('--grass-lawn')},
    @{Name='savanna';Flags=@('--grass-savanna')},
    @{Name='desert';Flags=@('--grass-desert')},
    @{Name='snow';Flags=@('--grass-snow')},
    @{Name='coastal';Flags=@('--grass-coastal')},
    @{Name='distance-off';Flags=@('--grass-off')},
    @{Name='distance-max';Flags=@('--grass-max')},
    @{Name='graphics-menu';Flags=@('--grass-max','--grass-lod-max','--graphics-menu')},
    @{Name='lod-default';Flags=@('--grass-max','--grass-lod-preview')},
    @{Name='lod-min';Flags=@('--grass-max','--grass-lod-preview','--grass-lod-min')},
    @{Name='lod-max';Flags=@('--grass-max','--grass-lod-preview','--grass-lod-max')}
)
if($Quick){$cases=@($cases[0])}
if($Benchmark){$cases=@(
    @{Name='benchmark-off';Flags=@('--grass-off','--benchmark')},
    @{Name='benchmark-default';Flags=@('--benchmark')},
    @{Name='benchmark-max';Flags=@('--grass-max','--benchmark')})}
if($Capture){
    $cases=@($cases | Where-Object { $_.Name -eq $Capture })
    if($cases.Count -ne 1){throw "Unknown capture: $Capture"}
}
foreach($case in $cases){
    $log=Join-Path $runtime 'MiniCity3D.log'
    $logLines=if(Test-Path -LiteralPath $log){@(Get-Content -LiteralPath $log).Count}else{0}
    $flags=@('--smoke','--grass-preview','--day','--screenshot','--1080p')+$case.Flags
    $process=Start-Process -FilePath (Join-Path $runtime 'MiniCity3D.exe') -WorkingDirectory $runtime -ArgumentList $flags -WindowStyle Hidden -Wait -PassThru
    if($process.ExitCode -ne 0){throw "$($case.Name) preview failed: $($process.ExitCode)"}
    $latest=Get-ChildItem -LiteralPath (Join-Path $runtime 'screenshots') -Filter '*.png' | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if(-not $latest){throw 'Screenshot missing'}
    Copy-Item -LiteralPath $latest.FullName -Destination (Join-Path $evidence "$($case.Name).png") -Force
    if($Benchmark){
        Get-Content -LiteralPath $log | Select-Object -Skip $logLines | Select-String 'Benchmark|GPU|Grass|grass|CPU rendering|Scene workers' |
            Set-Content -LiteralPath (Join-Path $evidence "$($case.Name).txt")
    }
    Write-Output "$($case.Name): exit 0; $($latest.Name)"
}
