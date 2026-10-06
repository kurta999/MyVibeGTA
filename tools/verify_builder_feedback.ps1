param(
    [string]$Runtime='build-msvc-ninja',
    [ValidateSet('stone','wood','soil','shears-open','shears-close','shears-cut','shears-third','brush','normal')]
    [string[]]$Views=@('stone','wood','soil','shears-open','shears-close','shears-cut','shears-third','brush','normal')
)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskRuntime=Join-Path $taskRoot $Runtime
$taskEvidence=Join-Path $taskRoot 'evidence/builder-feedback-20261006'
New-Item -ItemType Directory -Path $taskEvidence -Force | Out-Null
$taskCtest='C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe'
& $taskCtest --test-dir $taskRuntime -R '^(builder_feedback_scenarios|tool_work_scenarios|scenery_scenarios)$' --output-on-failure
if($LASTEXITCODE -ne 0){throw 'Builder feedback regressions failed'}
& (Join-Path $taskRuntime 'audio_smoke.exe') --builder-evidence (Join-Path $taskEvidence 'audio')
if($LASTEXITCODE -ne 0){throw 'Builder audio evidence failed'}
foreach($taskView in $Views){
    $taskStarted=Get-Date
    $taskArgs=@('--smoke','--day','--builder-feedback-preview',"--feedback-$taskView",'--screenshot')
    if($taskView -eq 'shears-third'){$taskArgs+='--feedback-third'}
    $taskProcess=Start-Process -FilePath (Join-Path $taskRuntime 'MiniCity3D.exe') -WorkingDirectory $taskRuntime `
        -ArgumentList $taskArgs -WindowStyle Hidden -Wait -PassThru
    if($taskProcess.ExitCode -ne 0){throw "Feedback $taskView preview failed ($($taskProcess.ExitCode))"}
    $taskImage=Get-ChildItem -LiteralPath (Join-Path $taskRuntime 'screenshots') -Filter '*.png' |
        Where-Object LastWriteTime -ge $taskStarted | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if(-not $taskImage){throw "Feedback $taskView produced no capture"}
    Copy-Item -LiteralPath $taskImage.FullName -Destination (Join-Path $taskEvidence "$taskView.png") -Force
    Write-Output "Builder feedback $taskView DX11 capture saved"
}
