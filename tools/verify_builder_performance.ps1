param([string]$Runtime='build-msvc-ninja', [string]$Evidence='evidence/builder-performance-20261006', [string]$Executable='MiniCity3D.exe')
$ErrorActionPreference='Stop'
$performanceRoot=Split-Path -Parent $PSScriptRoot
$performanceRuntime=[System.IO.Path]::GetFullPath((Join-Path $performanceRoot $Runtime))
$performanceEvidence=[System.IO.Path]::GetFullPath((Join-Path $performanceRoot $Evidence))
if($performanceRuntime -eq $performanceRoot -or -not $performanceRuntime.StartsWith($performanceRoot+'\',[System.StringComparison]::OrdinalIgnoreCase)){throw 'Use a separate runtime directory inside the repository.'}
if(-not $performanceEvidence.StartsWith($performanceRoot+'\',[System.StringComparison]::OrdinalIgnoreCase)){throw 'Keep evidence inside the repository.'}
if([System.IO.Path]::GetFileName($Executable) -ne $Executable){throw 'Choose an executable filename inside the runtime directory.'}
New-Item -ItemType Directory -Force -Path $performanceEvidence | Out-Null
$performanceSave=Join-Path $performanceRuntime 'savegame.ini'
$performanceHadSave=Test-Path -LiteralPath $performanceSave
$performanceBackup=Join-Path $performanceEvidence 'benchmark-runtime-save-backup.ini'
$performanceRootHash=(Get-FileHash -LiteralPath (Join-Path $performanceRoot 'savegame.ini')).Hash
if($performanceHadSave){Copy-Item -LiteralPath $performanceSave -Destination $performanceBackup -Force}
$performanceProfile=[Environment]::GetEnvironmentVariable('MINICITY_CPU_PROFILE','Process')
try{
    foreach($performanceCase in @('normal-120','builder-probe','builder-120')){
        $performanceArguments=@('--smoke','--day','--1080p')
        if($performanceCase -eq 'normal-120'){$performanceArguments+='--benchmark-normal'}
        else{$performanceArguments+='--benchmark-builder'}
        if($performanceCase -eq 'builder-probe'){
            $performanceArguments+='--benchmark-probe'
            [Environment]::SetEnvironmentVariable('MINICITY_CPU_PROFILE','1','Process')
        }else{[Environment]::SetEnvironmentVariable('MINICITY_CPU_PROFILE',$null,'Process')}
        $performanceProcess=Start-Process -FilePath (Join-Path $performanceRuntime $Executable) -WorkingDirectory $performanceRuntime -ArgumentList $performanceArguments -WindowStyle Hidden -Wait -PassThru
        $performanceLog=Join-Path $performanceEvidence ($performanceCase+'-published.log')
        Copy-Item -LiteralPath (Join-Path $performanceRuntime 'MiniCity3D.log') -Destination $performanceLog -Force
        if($performanceProcess.ExitCode -ne 0){throw "Performance $performanceCase failed ($($performanceProcess.ExitCode))."}
        $performanceTiming=Get-Content -LiteralPath $performanceLog | Select-String -SimpleMatch 'Benchmark 1920x1080:' | Select-Object -Last 1
        if(-not $performanceTiming){throw "Missing 1080p report for $performanceCase."}
        Write-Output $performanceTiming.Line
    }
}finally{
    [Environment]::SetEnvironmentVariable('MINICITY_CPU_PROFILE',$performanceProfile,'Process')
    if($performanceHadSave){Copy-Item -LiteralPath $performanceBackup -Destination $performanceSave -Force;Remove-Item -LiteralPath $performanceBackup}
    elseif(Test-Path -LiteralPath $performanceSave){Remove-Item -LiteralPath $performanceSave}
    if((Get-FileHash -LiteralPath (Join-Path $performanceRoot 'savegame.ini')).Hash -ne $performanceRootHash){throw 'Root save changed during benchmarking.'}
}
