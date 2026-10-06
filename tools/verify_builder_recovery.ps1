param([string]$Runtime='build-msvc-ninja',[switch]$CaptureOnly)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskRuntime=Join-Path $taskRoot $Runtime
$taskEvidence=Join-Path $taskRoot 'evidence/builder-recovery-20261006'
New-Item -ItemType Directory -Path $taskEvidence -Force | Out-Null
$taskCtest='C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe'
if(-not $CaptureOnly){
    & $taskCtest --test-dir $taskRuntime -R '^(builder_recovery_scenarios|builder_scenarios|excavation_scenarios|scenery_scenarios|ped_scenarios|ordnance_scenarios)$' --output-on-failure -j 1
    if($LASTEXITCODE -ne 0){throw 'Builder recovery regressions failed'}
}
foreach($taskView in @('pit','normal','restored')){
    $taskStarted=Get-Date
    $taskArgs=@('--smoke','--day','--builder-recovery-preview',"--recovery-$taskView",'--screenshot')
    $taskProcess=Start-Process -FilePath (Join-Path $taskRuntime 'MiniCity3D.exe') -WorkingDirectory $taskRuntime `
        -ArgumentList $taskArgs -WindowStyle Hidden -Wait -PassThru
    if($taskProcess.ExitCode -ne 0){throw "Recovery $taskView preview failed ($($taskProcess.ExitCode))"}
    $taskImage=Get-ChildItem -LiteralPath (Join-Path $taskRuntime 'screenshots') -Filter '*.png' |
        Where-Object LastWriteTime -ge $taskStarted | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if(-not $taskImage){throw "Recovery $taskView produced no capture"}
    Copy-Item -LiteralPath $taskImage.FullName -Destination (Join-Path $taskEvidence "$taskView.png") -Force
    Get-Content -LiteralPath (Join-Path $taskRuntime 'MiniCity3D.log') -Tail 20 |
        Select-String -SimpleMatch 'Builder recovery preview:' | ForEach-Object Line |
        Out-File -LiteralPath (Join-Path $taskEvidence "$taskView.log") -Encoding utf8
    Write-Output "Builder recovery $taskView DX11 capture saved"
}
