param([string]$Runtime='build-msvc-ninja',[string]$Evidence='evidence/dx12-optimization-20261009',[switch]$Benchmark,
    [int]$Repeats=3,[switch]$CandidateOnly,[string]$Label='skin-state',
    [string]$Baseline='MiniCity3D-before-optimization.exe',[string[]]$ExtraArgs=@(),[switch]$NativeOnly,
    [switch]$SkyOnly)
$ErrorActionPreference='Stop'
if($SkyOnly -and $ExtraArgs -notcontains '--sky-preview'){throw 'SkyOnly requires the actor-free --sky-preview fixture.'}
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskRuntime=[IO.Path]::GetFullPath((Join-Path $taskRoot $Runtime))
if(-not $taskRuntime.StartsWith($taskRoot+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Use a separate repository runtime.'}
$taskEvidence=Join-Path $taskRoot $Evidence
New-Item -ItemType Directory -Force -Path $taskEvidence | Out-Null
$taskOriginal=@{}
foreach($name in @('settings.ini','savegame.ini')){
    $path=Join-Path $taskRuntime $name
    $taskOriginal[$name]=if(Test-Path -LiteralPath $path){[IO.File]::ReadAllBytes($path)}else{$null}
}
try{
    Copy-Item -LiteralPath (Join-Path $taskRoot 'evidence/dx12-performance-20261009/settings.ini') -Destination (Join-Path $taskRuntime 'settings.ini') -Force
    $taskSave=Join-Path $taskRuntime 'savegame.ini'
    if(Test-Path -LiteralPath $taskSave){Remove-Item -LiteralPath $taskSave}
    $runs=if($Benchmark){
        foreach($index in 1..$Repeats){
            if(-not $CandidateOnly){@{Name="baseline-native-$index";Exe=$Baseline;Args=@('--smoke','--benchmark','--fsr2=0')}}
            @{Name="$Label-native-$index";Exe='MiniCity3D.exe';Args=@('--smoke','--benchmark','--fsr2=0')}
        }
    }else{
        @{Name='native-validation';Exe='MiniCity3D.exe';Args=@('--smoke','--benchmark','--fsr2=0','--dx12-debug','--validate-gpu-skinning','--screenshot')}
        if(-not $NativeOnly){@{Name='quality-validation';Exe='MiniCity3D.exe';Args=@('--smoke','--benchmark','--fsr2=1','--dx12-debug','--validate-gpu-skinning','--screenshot')}}
    }
    foreach($run in $runs){
        if($run.Exe -eq 'MiniCity3D.exe'){$run.Args += $ExtraArgs}
        $started=Get-Date
        $process=Start-Process -FilePath (Join-Path $taskRuntime $run.Exe) -WorkingDirectory $taskRuntime -ArgumentList $run.Args -WindowStyle Hidden -Wait -PassThru
        $log=Join-Path $taskEvidence ($run.Name+'.txt')
        $text=[IO.File]::ReadAllText((Join-Path $taskRuntime 'MiniCity3D.log'))
        $start=$text.LastIndexOf('Application started')
        if($start -lt 0){throw 'Missing application log'}
        $start=$text.LastIndexOf("`n",$start)+1
        [IO.File]::WriteAllText($log,$text.Substring($start))
        if($process.ExitCode -ne 0){throw "$($run.Name) failed with exit $($process.ExitCode)"}
        if(-not $Benchmark){
            $text=[IO.File]::ReadAllText($log)
            if($text -notmatch 'DX12 validation errors: 0' -or $text -match 'GPU skin validation: FAIL|using CPU deformation' -or (-not $SkyOnly -and ([regex]::Matches($text,'GPU skin validation: PASS')).Count -ne 3)){throw "$($run.Name) validation failed"}
            $capture=Get-ChildItem -LiteralPath (Join-Path $taskRuntime 'screenshots') -Filter '*.png' | Where-Object LastWriteTime -ge $started | Sort-Object LastWriteTime -Descending | Select-Object -First 1
            if(-not $capture){throw 'Missing screenshot'}
            Copy-Item -LiteralPath $capture.FullName -Destination (Join-Path $taskEvidence ($run.Name+'.png')) -Force
        }
        Get-Content -LiteralPath $log | Select-String 'Benchmark 1920|GPU timing:|CPU scene work:|CPU rendering:|GPU skin validation:|DX12 validation errors:|DX12 GPU deformation:|DX12 worker recording:|DX12 GPU post stages:|DX12 geometry:'
    }
}finally{
    foreach($name in $taskOriginal.Keys){
        $path=Join-Path $taskRuntime $name
        if($null -ne $taskOriginal[$name]){[IO.File]::WriteAllBytes($path,$taskOriginal[$name])}
        elseif(Test-Path -LiteralPath $path){Remove-Item -LiteralPath $path}
    }
}
