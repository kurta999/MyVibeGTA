param([string]$Runtime='build-msvc-ninja', [string]$Evidence='evidence/builder-destinations-20261006')
$ErrorActionPreference='Stop'
$destinationRoot=Split-Path -Parent $PSScriptRoot
$destinationRuntime=[System.IO.Path]::GetFullPath((Join-Path $destinationRoot $Runtime))
$destinationEvidence=[System.IO.Path]::GetFullPath((Join-Path $destinationRoot $Evidence))
if($destinationRuntime -eq $destinationRoot -or -not $destinationRuntime.StartsWith($destinationRoot+'\',[System.StringComparison]::OrdinalIgnoreCase)){
    throw 'Use a separate runtime directory inside the repository.'
}
if(-not $destinationEvidence.StartsWith($destinationRoot+'\',[System.StringComparison]::OrdinalIgnoreCase)){throw 'Keep evidence inside the repository.'}
New-Item -ItemType Directory -Force -Path $destinationEvidence | Out-Null
$destinationSave=Join-Path $destinationRuntime 'savegame.ini'
$destinationHadSave=Test-Path -LiteralPath $destinationSave
$destinationBackup=Join-Path $destinationEvidence 'runtime-save-backup.ini'
$destinationRootHash=(Get-FileHash -LiteralPath (Join-Path $destinationRoot 'savegame.ini')).Hash
if($destinationHadSave){Copy-Item -LiteralPath $destinationSave -Destination $destinationBackup -Force}
try{
    $destinationRecords=@()
    foreach($destinationView in @('wander','flee','cover','normal')){
        $destinationStarted=Get-Date
        $destinationProcess=Start-Process -FilePath (Join-Path $destinationRuntime 'MiniCity3D.exe') -WorkingDirectory $destinationRuntime `
            -ArgumentList @('--smoke','--day','--builder-destination-preview',"--destination-$destinationView",'--screenshot') -WindowStyle Hidden -Wait -PassThru
        if($destinationProcess.ExitCode -ne 0){throw "Destination $destinationView capture failed ($($destinationProcess.ExitCode))."}
        $destinationCapture=Get-ChildItem -LiteralPath (Join-Path $destinationRuntime 'screenshots') -File -Filter '*.png' |
            Where-Object LastWriteTime -ge $destinationStarted | Sort-Object LastWriteTime -Descending | Select-Object -First 1
        if(-not $destinationCapture){throw "Destination $destinationView produced no image."}
        $destinationFile=Join-Path $destinationEvidence ($destinationView+'.png')
        Move-Item -LiteralPath $destinationCapture.FullName -Destination $destinationFile -Force
        $destinationLog=Get-Content -LiteralPath (Join-Path $destinationRuntime 'MiniCity3D.log')
        $destinationLog | Set-Content -LiteralPath (Join-Path $destinationEvidence ($destinationView+'.log'))
        $destinationPose=($destinationLog | Select-String -SimpleMatch 'Destination review:' | Select-Object -Last 1).Line
        if(-not $destinationPose){throw 'Missing native actor/destination report.'}
        $destinationRecords+=[pscustomobject]@{View=$destinationView;Image=$destinationView+'.png';SHA256=(Get-FileHash -LiteralPath $destinationFile).Hash;Position=$destinationPose}
        Write-Output "Destination $destinationView DX11 capture retained."
    }
    $destinationRecords | Export-Csv -LiteralPath (Join-Path $destinationEvidence 'captures.csv') -NoTypeInformation
}finally{
    if($destinationHadSave){Copy-Item -LiteralPath $destinationBackup -Destination $destinationSave -Force;Remove-Item -LiteralPath $destinationBackup}
    elseif(Test-Path -LiteralPath $destinationSave){Remove-Item -LiteralPath $destinationSave}
    if((Get-FileHash -LiteralPath (Join-Path $destinationRoot 'savegame.ini')).Hash -ne $destinationRootHash){throw 'Root save changed during destination review.'}
}
