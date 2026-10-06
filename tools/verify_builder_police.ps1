param([string]$Runtime='build-msvc-ninja', [string]$Evidence='evidence/builder-police-20261006')
$ErrorActionPreference='Stop'
$policeRoot=Split-Path -Parent $PSScriptRoot
$policeRuntime=[System.IO.Path]::GetFullPath((Join-Path $policeRoot $Runtime))
$policeEvidence=[System.IO.Path]::GetFullPath((Join-Path $policeRoot $Evidence))
if($policeRuntime -eq $policeRoot -or -not $policeRuntime.StartsWith($policeRoot+'\',[System.StringComparison]::OrdinalIgnoreCase)){
    throw 'Use a separate runtime directory inside the repository.'
}
if(-not $policeEvidence.StartsWith($policeRoot+'\',[System.StringComparison]::OrdinalIgnoreCase)){throw 'Keep evidence inside the repository.'}
New-Item -ItemType Directory -Force -Path $policeEvidence | Out-Null
$policeSave=Join-Path $policeRuntime 'savegame.ini'
$policeHadSave=Test-Path -LiteralPath $policeSave
$policeBackup=Join-Path $policeEvidence 'runtime-save-backup.ini'
$policeRootHash=(Get-FileHash -LiteralPath (Join-Path $policeRoot 'savegame.ini')).Hash
if($policeHadSave){Copy-Item -LiteralPath $policeSave -Destination $policeBackup -Force}
try{
    $policeRecords=@()
    foreach($policeView in @('stealth','witness','dispatch','blocked','normal')){
        $policeStarted=Get-Date
        $policeProcess=Start-Process -FilePath (Join-Path $policeRuntime 'MiniCity3D.exe') -WorkingDirectory $policeRuntime `
            -ArgumentList @('--smoke','--day','--builder-police-preview',"--police-$policeView",'--screenshot') -WindowStyle Hidden -Wait -PassThru
        if($policeProcess.ExitCode -ne 0){throw "Police $policeView capture failed ($($policeProcess.ExitCode))."}
        $policeCapture=Get-ChildItem -LiteralPath (Join-Path $policeRuntime 'screenshots') -File -Filter '*.png' |
            Where-Object LastWriteTime -ge $policeStarted | Sort-Object LastWriteTime -Descending | Select-Object -First 1
        if(-not $policeCapture){throw "Police $policeView produced no image."}
        $policeFile=Join-Path $policeEvidence ($policeView+'.png')
        Move-Item -LiteralPath $policeCapture.FullName -Destination $policeFile -Force
        $policeLog=Get-Content -LiteralPath (Join-Path $policeRuntime 'MiniCity3D.log')
        $policeLog | Set-Content -LiteralPath (Join-Path $policeEvidence ($policeView+'.log'))
        $policePose=($policeLog | Select-String -SimpleMatch 'Police review:' | Select-Object -Last 1).Line
        if(-not $policePose){throw 'Missing native actor/police report.'}
        $policeRecords+=[pscustomobject]@{View=$policeView;Image=$policeView+'.png';SHA256=(Get-FileHash -LiteralPath $policeFile).Hash;Position=$policePose}
        Write-Output "Police $policeView DX11 capture retained."
    }
    $policeRecords | Export-Csv -LiteralPath (Join-Path $policeEvidence 'captures.csv') -NoTypeInformation
}finally{
    if($policeHadSave){Copy-Item -LiteralPath $policeBackup -Destination $policeSave -Force;Remove-Item -LiteralPath $policeBackup}
    elseif(Test-Path -LiteralPath $policeSave){Remove-Item -LiteralPath $policeSave}
    if((Get-FileHash -LiteralPath (Join-Path $policeRoot 'savegame.ini')).Hash -ne $policeRootHash){throw 'Root save changed during police review.'}
}

