$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskRuntime=Join-Path $taskRoot 'build-msvc-ninja'
$taskEvidence=Join-Path $taskRoot 'evidence/vehicles-20261004'
New-Item -ItemType Directory -Path $taskEvidence -Force | Out-Null
foreach($taskPreview in @('expansion','combine','destruction','airplane')) {
    $taskProcess=Start-Process -FilePath (Join-Path $taskRuntime 'MiniCity3D.exe') -WorkingDirectory $taskRuntime -ArgumentList @('--smoke','--day',"--$taskPreview-preview",'--screenshot','--1080p') -WindowStyle Hidden -Wait -PassThru
    if($taskProcess.ExitCode -ne 0){throw "$taskPreview failed: $($taskProcess.ExitCode)"}
    $taskLatest=Get-ChildItem -LiteralPath (Join-Path $taskRuntime 'screenshots') -Filter '*.png' | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if(-not $taskLatest){throw 'Screenshot missing'}
    Copy-Item -LiteralPath $taskLatest.FullName -Destination (Join-Path $taskEvidence "$taskPreview.png") -Force
    Write-Output "$taskPreview preview: exit 0; $($taskLatest.Name)"
}
