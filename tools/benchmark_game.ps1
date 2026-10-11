param(
    [string[]]$Executable=@((Join-Path $PSScriptRoot '../build-msvc-ninja/MiniCity3D.exe')),
    [string]$OutputDirectory=(Join-Path $PSScriptRoot ('../evidence/benchmarks-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))),
    [ValidateSet('city','forest','travel','route')][string[]]$Scenarios=@('city','forest','travel'),
    [ValidateRange(1,36000)][int]$Frames=240,
    [ValidateRange(0,3600)][int]$Warmup=60,
    [ValidateRange(1,20)][int]$Repeats=1,
    [ValidateRange(1,8)][int]$SceneWorkers=4,
    [ValidateRange(1,8)][int]$RecordWorkers=1,
    [ValidateRange(30,7200)][int]$TimeoutSeconds=600,
    [string]$CaptureExecutable='',
    [switch]$Validate
)
$ErrorActionPreference='Stop'
$output=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $output -Force | Out-Null
$capturePath=if($CaptureExecutable){(Resolve-Path -LiteralPath $CaptureExecutable).Path}else{''}
$executables=@($Executable | ForEach-Object {(Resolve-Path -LiteralPath $_).Path})
$rows=@()
$culture=[Globalization.CultureInfo]::InvariantCulture
$mutex=[Threading.Mutex]::new($false,'Local\MiniCity3D-Benchmark')
$locked=$false
try {
    $locked=$mutex.WaitOne(0)
    if(-not $locked){throw 'Another benchmark_game.ps1 run is active.'}
    if(@(Get-Process -Name 'MiniCity3D*' -ErrorAction SilentlyContinue).Count){throw 'Close other MiniCity3D instances before benchmarking.'}
    for($repeat=1;$repeat -le $Repeats;$repeat++){
        # Alternate backend order to reduce first-run and thermal bias.
        $order=@($executables);if($repeat%2 -eq 0){[array]::Reverse($order)}
        foreach($exe in $order){
            $source=Split-Path -Parent $exe
            $hash=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
            foreach($scenario in $Scenarios){
                $id=([IO.Path]::GetFileNameWithoutExtension($exe))+'-'+$scenario+'-'+$repeat+'-'+[Guid]::NewGuid().ToString('N').Substring(0,8)
                $run=Join-Path $output $id
                New-Item -ItemType Directory -Path $run | Out-Null
                $runtime=Join-Path $PSScriptRoot ('../build-benchmarks/'+$id)
                New-Item -ItemType Directory -Path $runtime -Force | Out-Null
                $runtime=[IO.Path]::GetFullPath($runtime)
                $staged=Join-Path $runtime ([IO.Path]::GetFileName($exe))
                Copy-Item -LiteralPath $exe -Destination $staged
                $pdb=[IO.Path]::ChangeExtension($exe,'.pdb')
                if(Test-Path -LiteralPath $pdb){Copy-Item -LiteralPath $pdb -Destination $runtime}
                foreach($folder in @('assets','data')){
                    $target=(Resolve-Path -LiteralPath (Join-Path $source $folder)).Path
                    New-Item -ItemType Junction -Path (Join-Path $runtime $folder) -Target $target | Out-Null
                }
                $arguments="--smoke --benchmark --benchmark-csv --benchmark-frames=$Frames --benchmark-warmup=$Warmup --1080p --day --high-shadows --high-taa --fsr2=0 --loader-workers=4 --scene-workers=$SceneWorkers --record-workers=$RecordWorkers"
                if($scenario -eq 'forest'){$arguments+=' --woods'}
                if($scenario -eq 'travel'){$arguments+=' --benchmark-travel'}
                if($scenario -eq 'route'){$arguments+=' --benchmark-route'}
                if($Validate){$arguments+=' --validate-loading --dx12-debug'}
                $capture=$null;$game=$null
                try {
                    if($capturePath){
                        $arguments+=' --tracy-wait=15'
                        $capture=Start-Process -FilePath $capturePath -WorkingDirectory $run -WindowStyle Hidden -PassThru `
                            -ArgumentList @('-a','127.0.0.1','-o',('"'+(Join-Path $run 'capture.tracy')+'"')) `
                            -RedirectStandardOutput (Join-Path $run 'capture.stdout.txt') -RedirectStandardError (Join-Path $run 'capture.stderr.txt')
                    }
                    Write-Host "Benchmarking $id ($Frames frames, $Warmup warmup ticks)"
                    $timer=[Diagnostics.Stopwatch]::StartNew()
                    $game=Start-Process -FilePath $staged -WorkingDirectory $runtime -ArgumentList $arguments -WindowStyle Hidden -PassThru
                    if(-not $game.WaitForExit($TimeoutSeconds*1000)){throw "Game benchmark timed out after $TimeoutSeconds seconds."}
                    $game.Refresh()
                    if($game.ExitCode -ne 0){throw "Game benchmark failed ($($game.ExitCode)); see $run/MiniCity3D.log"}
                    if($capture){
                        if(-not $capture.WaitForExit(60000)){throw 'Tracy capture did not finish after game shutdown.'}
                        $capture.Refresh()
                        if($capture.ExitCode -ne 0 -or -not (Test-Path -LiteralPath (Join-Path $run 'capture.tracy'))){throw 'Tracy capture failed; inspect capture stdout/stderr.'}
                    }
                    $log=Get-Content -LiteralPath (Join-Path $runtime 'MiniCity3D.log')
                    $result=@($log | Select-String 'Benchmark.*: avg ([0-9.]+) ms .*p95 ([0-9.]+) ms, p99 ([0-9.]+) ms, max ([0-9.]+) ms')
                    if($result.Count -ne 1){throw 'Missing benchmark summary.'}
                    if(-not @($log | Select-String 'Smoke render completed').Count){throw 'Game did not complete smoke shutdown.'}
                    if(@($log | Select-String 'DX12 validation errors: [1-9]|validation.*FAIL').Count){throw 'Graphics validation failed; inspect game.log.txt.'}
                    $samples=@(Import-Csv -LiteralPath (Join-Path $runtime 'benchmark-frames.csv'))
                    if($samples.Count -ne $Frames){throw "Expected $Frames CSV samples, found $($samples.Count)."}
                    foreach($sample in $samples){
                        foreach($field in @('frame_ms','simulation_ms','render_ms')){
                            $value=[double]::Parse($sample.$field,$culture)
                            if([double]::IsNaN($value) -or [double]::IsInfinity($value) -or $value -lt 0){throw "Invalid $field sample."}
                        }
                    }
                    $match=$result[0].Matches[0]
                    $backend=if(@($log | Select-String 'Native Direct3D 12 adapter').Count){'DX12'}else{'DX11'}
                    $metadata=[ordered]@{Executable=$exe;SHA256=$hash;Backend=$backend;Scenario=$scenario;Repeat=$repeat;
                        Arguments=$arguments;Frames=$Frames;Warmup=$Warmup;ExitCode=$game.ExitCode;
                        RuntimeDirectory=$runtime;
                        ProcessSeconds=$timer.Elapsed.TotalSeconds;TracyCapture=[bool]$capturePath;
                        TracyEnabled=[bool]@($log | Select-String 'Tracy v0.13.1 enabled').Count;
                        GpuCsv='Latest completed nonblocking timings; -1 means unavailable. Samples may repeat.'}
                    $metadata | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $run 'run.json')
                    $rows+=[pscustomobject]@{Backend=$backend;Scenario=$scenario;Repeat=$repeat;Frames=$Frames;
                        MeanMs=$match.Groups[1].Value;P95Ms=$match.Groups[2].Value;P99Ms=$match.Groups[3].Value;MaxMs=$match.Groups[4].Value;
                        Directory=$run;SHA256=$hash;TracyCapture=[bool]$capturePath}
                    $rows | Export-Csv -LiteralPath (Join-Path $output 'summary.csv') -NoTypeInformation
                    $log | Select-String 'Benchmark.*avg|GPU timing:|CPU rendering:|Tracy.*enabled' | ForEach-Object {$_.Line}
                } finally {
                    # Only terminate processes launched for this individual run.
                    foreach($process in @($game,$capture)){if($process){$process.Refresh();if(-not $process.HasExited){$process.Kill();$process.WaitForExit()};$process.Dispose()}}
                    foreach($file in @('MiniCity3D.log','benchmark-frames.csv')){
                        $saved=Join-Path $runtime $file
                        if(Test-Path -LiteralPath $saved){
                            $destination=if($file -eq 'MiniCity3D.log'){'game.log.txt'}else{$file}
                            Copy-Item -LiteralPath $saved -Destination (Join-Path $run $destination)
                        }
                    }
                }
            }
        }
    }
} finally {
    if($locked){$mutex.ReleaseMutex()};$mutex.Dispose()
}
Write-Host "Logs, frame CSVs, metadata and summary: $output"
