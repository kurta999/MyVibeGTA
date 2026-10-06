param([string]$Runtime='build-msvc-ninja',[switch]$CaptureOnly)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskRuntime=[IO.Path]::GetFullPath((Join-Path $taskRoot $Runtime))
if(-not $taskRuntime.StartsWith($taskRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){
    throw 'Progression verification must use a runtime subdirectory, away from the root player save'
}
$taskEvidence=Join-Path $taskRoot 'evidence/builder-progression-20261006'
New-Item -ItemType Directory -Path $taskEvidence -Force | Out-Null
$taskCtest='C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe'
if(-not $CaptureOnly){
    & $taskCtest --test-dir $taskRuntime -R '^(builder_progression_scenarios|builder_scenarios|tool_work_scenarios|builder_feedback_scenarios)$' --output-on-failure -j 1
    if($LASTEXITCODE -ne 0){throw 'Builder progression regressions failed'}
}
$taskReport=Join-Path $taskRuntime 'builder-progression-report.tsv'
$taskEarnedSave=Join-Path $taskRuntime 'builder-progression-save.ini'
$taskRows=@(Import-Csv -LiteralPath $taskReport -Delimiter "`t")
if($taskRows.Count -ne 22 -or @($taskRows | Where-Object crafted_used_dropped_stored_reloaded -ne 'PASS').Count){
    throw 'Missing complete 22-tool progression report'
}
Copy-Item -LiteralPath $taskReport -Destination (Join-Path $taskEvidence 'catalog.tsv') -Force
Copy-Item -LiteralPath $taskEarnedSave -Destination (Join-Path $taskEvidence 'earned-save.ini') -Force
$taskSave=Join-Path $taskRuntime 'savegame.ini'
$taskBackup=Join-Path $taskRuntime ('builder-review-'+[guid]::NewGuid().ToString()+'.backup.ini')
Copy-Item -LiteralPath $taskSave -Destination $taskBackup
$taskPreviousReview=$env:MINICITY_REVIEW_DIR
try{
    $env:MINICITY_REVIEW_DIR=$taskEvidence
    Copy-Item -LiteralPath $taskEarnedSave -Destination $taskSave -Force
    $taskProcess=Start-Process -FilePath (Join-Path $taskRuntime 'MiniCity3D.exe') -WorkingDirectory $taskRuntime `
        -ArgumentList @('--smoke','--day','--builder-catalog-review') -WindowStyle Hidden -Wait -PassThru
    if($taskProcess.ExitCode -ne 0){throw "Earned tool catalog review failed ($($taskProcess.ExitCode))"}
    $taskViews=@(Import-Csv -LiteralPath (Join-Path $taskRuntime 'builder-catalog-captures.tsv') -Delimiter "`t")
    if($taskViews.Count -ne 67 -or @($taskViews.view | Select-Object -Unique).Count -ne 67){throw 'Expected 67 distinct catalog captures'}
    foreach($taskView in $taskViews){
        if($taskView.file -ne $taskView.view+'.png' -or -not (Test-Path -LiteralPath (Join-Path $taskEvidence $taskView.file))){throw 'Missing direct review capture'}
    }
    Copy-Item -LiteralPath (Join-Path $taskRuntime 'builder-catalog-captures.tsv') -Destination (Join-Path $taskEvidence 'captures.tsv') -Force
    Get-Content -LiteralPath (Join-Path $taskRuntime 'MiniCity3D.log') |
        Select-String -SimpleMatch 'Builder catalog' | Select-Object -Last 69 | ForEach-Object Line |
        Out-File -LiteralPath (Join-Path $taskEvidence 'review.log') -Encoding utf8
    Write-Output 'All 22 earned tools captured in hand/third/drop views; storage capture and catalog retained'
}finally{
    $env:MINICITY_REVIEW_DIR=$taskPreviousReview
    Copy-Item -LiteralPath $taskBackup -Destination $taskSave -Force
    Remove-Item -LiteralPath $taskBackup
}
