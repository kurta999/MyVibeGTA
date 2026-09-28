param(
    [string]$Executable=(Join-Path $PSScriptRoot '../build-msvc-ninja/MiniCity3D.exe'),
    [string]$OutputDirectory=(Join-Path $PSScriptRoot '../evidence/threading-20260928'),
    [int]$Repeats=3,
    [int[]]$Workers=@(1,2,4),
    [int]$SceneWorkers=4,
    [switch]$CompareScene,
    [switch]$Route,
    [switch]$Travel,
    [switch]$Validate
)
$ErrorActionPreference='Stop'
$executablePath=(Resolve-Path -LiteralPath $Executable).Path
$runtimeDirectory=Split-Path -Parent $executablePath
$log=Join-Path $runtimeDirectory 'MiniCity3D.log'
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$rows=@()
for($run=1;$run -le $Repeats;$run++){
    # Reverse alternate runs to reduce ordering and cache bias. Never run two
    # performance samples concurrently; that would distort both measurements.
    $order=@($Workers)
    if($run%2 -eq 0){[array]::Reverse($order)}
    foreach($worker in $order){
        if($worker -lt 1 -or $worker -gt 8){throw 'Workers must be 1 through 8'}
        $before=if(Test-Path -LiteralPath $log){(Get-Content -LiteralPath $log).Count}else{0}
        $loaderCount=if($CompareScene){4}else{$worker}
        $sceneCount=if($CompareScene){$worker}else{$SceneWorkers}
        $arguments="--smoke --day --1080p --loader-workers=$loaderCount --scene-workers=$sceneCount"
        if($Route){$arguments+=' --benchmark --benchmark-route --high-shadows --high-taa'}
        if($Travel){$arguments+=' --benchmark --benchmark-travel --high-shadows'}
        if($Validate){$arguments+=' --validate-loading'}
        $timer=[Diagnostics.Stopwatch]::StartNew()
        $process=Start-Process -FilePath $executablePath -WorkingDirectory $runtimeDirectory `
            -ArgumentList $arguments -WindowStyle Hidden -PassThru
        $process.WaitForExit()
        if($process.ExitCode -ne 0){throw "Game failed with exit code $($process.ExitCode)"}
        $lines=@(Get-Content -LiteralPath $log | Select-Object -Skip $before)
        $kind=if($Route){'route'}elseif($Travel){'travel'}else{'startup'}
        $lines | Set-Content -LiteralPath (Join-Path $OutputDirectory "$kind-workers-$worker-run-$run.txt")
        $ready=@($lines | Select-String 'Startup ready in ([0-9.]+) s')
        if($ready.Count -ne 1){throw 'Missing startup-ready measurement'}
        $readySeconds=[double]::Parse($ready[0].Matches[0].Groups[1].Value,[Globalization.CultureInfo]::InvariantCulture)
        $row=[pscustomobject]@{Mode=$kind;Workers=$loaderCount;SceneWorkers=$sceneCount;Run=$run;
            ReadySeconds=$readySeconds.ToString('F3',[Globalization.CultureInfo]::InvariantCulture);
            ProcessSeconds=$timer.Elapsed.TotalSeconds.ToString('F6',[Globalization.CultureInfo]::InvariantCulture)}
        $rows+=$row
        Write-Output $row
        $lines | Select-String 'CPU scene workers:|Shader loading:|Shader checksum:|Texture loading:|Texture checksum:|Benchmark.*avg|CPU rendering:|CPU scene work:|GPU timing:' | ForEach-Object {$_.Line}
    }
}
$rows | Export-Csv -LiteralPath (Join-Path $OutputDirectory "$kind-summary.csv") -NoTypeInformation
