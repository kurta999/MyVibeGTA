param([string]$Runtime='build-msvc-ninja', [string]$Evidence='evidence/editor-sizes-20261009')
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskRuntime=[IO.Path]::GetFullPath((Join-Path $taskRoot $Runtime))
if(-not $taskRuntime.StartsWith($taskRoot+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Use a separate repository runtime.'}
$taskEvidence=Join-Path $taskRoot $Evidence
New-Item -ItemType Directory -Force -Path $taskEvidence | Out-Null
$taskSave=Join-Path $taskRuntime 'savegame.ini'
$taskHadSave=Test-Path -LiteralPath $taskSave
$taskBackup=Join-Path $taskEvidence 'runtime-save-backup.ini'
if($taskHadSave){Copy-Item -LiteralPath $taskSave -Destination $taskBackup -Force}
try{
    foreach($taskView in @('tank-surface','truck','trailer','builder-stack','builder-stack-third','builder-stack-respawn','saved-builder')){
        $taskStarted=Get-Date
        $taskFlag=if($taskView -like 'builder-stack*'){'builder-stack'}else{$taskView}
        $taskArguments=@('--smoke','--day',"--$taskFlag-preview",'--screenshot')
        if($taskView -eq 'saved-builder'){Copy-Item -LiteralPath (Join-Path $taskRoot 'savegame.ini') -Destination $taskSave -Force}
        if($taskView -eq 'builder-stack-third'){$taskArguments+='--stack-third-person'}
        if($taskView -eq 'builder-stack-respawn'){$taskArguments+='--stack-respawn'}
        $taskProcess=Start-Process -FilePath (Join-Path $taskRuntime 'MiniCity3D.exe') -WorkingDirectory $taskRuntime -ArgumentList $taskArguments -WindowStyle Hidden -Wait -PassThru
        if($taskProcess.ExitCode -ne 0){throw "$taskView failed: $($taskProcess.ExitCode)"}
        $taskCapture=Get-ChildItem -LiteralPath (Join-Path $taskRuntime 'screenshots') -File -Filter '*.png' |
            Where-Object LastWriteTime -ge $taskStarted | Sort-Object LastWriteTime -Descending | Select-Object -First 1
        if(-not $taskCapture){throw "$taskView produced no capture"}
        Copy-Item -LiteralPath $taskCapture.FullName -Destination (Join-Path $taskEvidence "$taskView.png") -Force
        Copy-Item -LiteralPath (Join-Path $taskRuntime 'MiniCity3D.log') -Destination (Join-Path $taskEvidence "$taskView.log") -Force
        Write-Output "$taskView DX11 capture passed"
    }
}finally{
    if($taskHadSave){Copy-Item -LiteralPath $taskBackup -Destination $taskSave -Force;Remove-Item -LiteralPath $taskBackup}
    elseif(Test-Path -LiteralPath $taskSave){Remove-Item -LiteralPath $taskSave}
}
