param([string]$Runtime='build-msvc-ninja', [string]$Evidence='evidence/builder-interactions-20261006')
$ErrorActionPreference='Stop'
$interactionRoot=Split-Path -Parent $PSScriptRoot
$interactionRuntime=[System.IO.Path]::GetFullPath((Join-Path $interactionRoot $Runtime))
$interactionEvidence=[System.IO.Path]::GetFullPath((Join-Path $interactionRoot $Evidence))
if($interactionRuntime -eq $interactionRoot -or -not $interactionRuntime.StartsWith($interactionRoot+'\',[System.StringComparison]::OrdinalIgnoreCase)){
    throw 'Use a separate runtime directory inside the repository.'
}
if(-not $interactionEvidence.StartsWith($interactionRoot+'\',[System.StringComparison]::OrdinalIgnoreCase)){throw 'Keep evidence inside the repository.'}
New-Item -ItemType Directory -Force -Path $interactionEvidence | Out-Null
$interactionSave=Join-Path $interactionRuntime 'savegame.ini'
$interactionHadSave=Test-Path -LiteralPath $interactionSave
$interactionBackup=Join-Path $interactionEvidence 'runtime-save-backup.ini'
$interactionRootHash=(Get-FileHash -LiteralPath (Join-Path $interactionRoot 'savegame.ini')).Hash
if($interactionHadSave){Copy-Item -LiteralPath $interactionSave -Destination $interactionBackup -Force}
try{
    $interactionRecords=@()
    foreach($interactionView in @('corpse','carried','dropped','combat','normal')){
        $interactionStarted=Get-Date
        $interactionProcess=Start-Process -FilePath (Join-Path $interactionRuntime 'MiniCity3D.exe') -WorkingDirectory $interactionRuntime `
            -ArgumentList @('--smoke','--day','--builder-interaction-preview',"--interaction-$interactionView",'--screenshot') -WindowStyle Hidden -Wait -PassThru
        if($interactionProcess.ExitCode -ne 0){throw "Interaction $interactionView capture failed ($($interactionProcess.ExitCode))."}
        $interactionCapture=Get-ChildItem -LiteralPath (Join-Path $interactionRuntime 'screenshots') -File -Filter '*.png' |
            Where-Object LastWriteTime -ge $interactionStarted | Sort-Object LastWriteTime -Descending | Select-Object -First 1
        if(-not $interactionCapture){throw "Interaction $interactionView produced no image."}
        $interactionFile=Join-Path $interactionEvidence ($interactionView+'.png')
        Move-Item -LiteralPath $interactionCapture.FullName -Destination $interactionFile -Force
        $interactionLog=Get-Content -LiteralPath (Join-Path $interactionRuntime 'MiniCity3D.log')
        $interactionLog | Set-Content -LiteralPath (Join-Path $interactionEvidence ($interactionView+'.log'))
        $interactionPose=($interactionLog | Select-String -SimpleMatch 'Interaction review:' | Select-Object -Last 1).Line
        if(-not $interactionPose){throw 'Missing native actor/interaction report.'}
        $interactionRecords+=[pscustomobject]@{View=$interactionView;Image=$interactionView+'.png';SHA256=(Get-FileHash -LiteralPath $interactionFile).Hash;Position=$interactionPose}
        Write-Output "Interaction $interactionView DX11 capture retained."
    }
    $interactionRecords | Export-Csv -LiteralPath (Join-Path $interactionEvidence 'captures.csv') -NoTypeInformation
}finally{
    if($interactionHadSave){Copy-Item -LiteralPath $interactionBackup -Destination $interactionSave -Force;Remove-Item -LiteralPath $interactionBackup}
    elseif(Test-Path -LiteralPath $interactionSave){Remove-Item -LiteralPath $interactionSave}
    if((Get-FileHash -LiteralPath (Join-Path $interactionRoot 'savegame.ini')).Hash -ne $interactionRootHash){throw 'Root save changed during interaction review.'}
}
