param([string]$Runtime='build-msvc-ninja')
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskRuntime=Join-Path $taskRoot $Runtime
$taskEvidence=Join-Path $taskRoot 'evidence/scenery-20261006'
New-Item -ItemType Directory -Path $taskEvidence -Force | Out-Null
$taskCtest='C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe'
& $taskCtest --test-dir $taskRuntime -R '^scenery_scenarios$' --output-on-failure
if($LASTEXITCODE -ne 0){throw 'Scenery simulation regression failed'}
foreach($taskView in @('tree','rock','bush','normal')){
    $taskStarted=Get-Date
    $taskProcess=Start-Process -FilePath (Join-Path $taskRuntime 'MiniCity3D.exe') -WorkingDirectory $taskRuntime `
        -ArgumentList @('--smoke','--day','--scenery-preview',"--scenery-$taskView",'--screenshot') -WindowStyle Hidden -Wait -PassThru
    if($taskProcess.ExitCode -ne 0){throw "Scenery $taskView preview failed ($($taskProcess.ExitCode))"}
    $taskImage=Get-ChildItem -LiteralPath (Join-Path $taskRuntime 'screenshots') -Filter '*.png' |
        Where-Object LastWriteTime -ge $taskStarted | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if(-not $taskImage){throw "Scenery $taskView produced no capture"}
    Copy-Item -LiteralPath $taskImage.FullName -Destination (Join-Path $taskEvidence "$taskView.png") -Force
    Write-Output "Scenery $taskView DX11 capture saved"
}
