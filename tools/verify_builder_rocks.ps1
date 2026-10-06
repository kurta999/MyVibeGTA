param([string]$Runtime='build-msvc-ninja', [string]$Evidence='evidence/builder-materials-20261006')
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskRuntime=[System.IO.Path]::GetFullPath((Join-Path $taskRoot $Runtime))
$taskEvidence=[System.IO.Path]::GetFullPath((Join-Path $taskRoot $Evidence))
if($taskRuntime -eq $taskRoot -or -not $taskRuntime.StartsWith($taskRoot+'\',[System.StringComparison]::OrdinalIgnoreCase)){
    throw 'Use a separate runtime directory inside the repository.'
}
if(-not $taskEvidence.StartsWith($taskRoot+'\',[System.StringComparison]::OrdinalIgnoreCase)){throw 'Keep review evidence inside the repository.'}
New-Item -ItemType Directory -Force -Path $taskEvidence | Out-Null
$taskRootSave=Join-Path $taskRoot 'savegame.ini'
$taskRootHash=(Get-FileHash -LiteralPath $taskRootSave).Hash
$taskSave=Join-Path $taskRuntime 'savegame.ini'
$taskHadSave=Test-Path -LiteralPath $taskSave
$taskBackup=Join-Path $taskEvidence 'runtime-save-backup.ini'
if($taskHadSave){Copy-Item -LiteralPath $taskSave -Destination $taskBackup -Force}
$taskPreviousReview=$env:MINICITY_REVIEW_DIR
try{
    $env:MINICITY_REVIEW_DIR=$taskEvidence
    $taskProcess=Start-Process -FilePath (Join-Path $taskRuntime 'MiniCity3D.exe') -WorkingDirectory $taskRuntime `
        -ArgumentList @('--smoke','--day','--builder-rock-review') -WindowStyle Hidden -Wait -PassThru
    if($taskProcess.ExitCode -ne 0){throw "Native rock review failed ($($taskProcess.ExitCode))."}
    $taskRows=@(Import-Csv -LiteralPath (Join-Path $taskRuntime 'builder-rock-report.tsv') -Delimiter "`t")
    $taskViews=@(Import-Csv -LiteralPath (Join-Path $taskRuntime 'builder-rock-captures.tsv') -Delimiter "`t")
    if($taskRows.Count -ne 20 -or @($taskRows | ForEach-Object item | Select-Object -Unique).Count -ne 20 -or
       $taskViews.Count -ne 23 -or @($taskViews.view | Select-Object -Unique).Count -ne 23){throw 'Missing complete rock review.'}
    if(@($taskRows.frames | Select-Object -Unique).Count -ne 20){throw 'Mining durations do not distinguish all 20 strengths.'}
    $taskImages=@()
    foreach($taskView in $taskViews){
        $taskImage=Join-Path $taskEvidence $taskView.file
        if($taskView.file -ne $taskView.view+'.png' -or -not (Test-Path -LiteralPath $taskImage)){throw 'Missing named rock capture.'}
        $taskImages+=[pscustomobject]@{View=$taskView.view;Image=$taskView.file;SHA256=(Get-FileHash -LiteralPath $taskImage).Hash}
    }
    $taskImages | Export-Csv -LiteralPath (Join-Path $taskEvidence 'captures.csv') -NoTypeInformation
    Copy-Item -LiteralPath (Join-Path $taskRuntime 'builder-rock-report.tsv') -Destination (Join-Path $taskEvidence 'gameplay.tsv') -Force
    Copy-Item -LiteralPath (Join-Path $taskRuntime 'builder-rock-captures.tsv') -Destination (Join-Path $taskEvidence 'captures.tsv') -Force
    Copy-Item -LiteralPath $taskSave -Destination (Join-Path $taskEvidence 'gallery-save.ini') -Force
    Get-Content -LiteralPath (Join-Path $taskRuntime 'MiniCity3D.log') | Select-String -SimpleMatch 'Rock review:' |
        Select-Object -Last 21 | ForEach-Object Line | Set-Content -LiteralPath (Join-Path $taskEvidence 'native-review.log')
    Write-Output 'All 20 rocks placed, rendered and mined with distinct durations and single yields/durability; save/F5 gallery reconstructed.'
}finally{
    $env:MINICITY_REVIEW_DIR=$taskPreviousReview
    if($taskHadSave){Copy-Item -LiteralPath $taskBackup -Destination $taskSave -Force;Remove-Item -LiteralPath $taskBackup}
    elseif(Test-Path -LiteralPath $taskSave){Remove-Item -LiteralPath $taskSave}
    if((Get-FileHash -LiteralPath $taskRootSave).Hash -ne $taskRootHash){throw 'Root save changed during rock verification.'}
}
