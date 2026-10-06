param([string]$Runtime='build-msvc-ninja', [string]$Evidence='evidence/builder-wildlife-20261006')
$ErrorActionPreference='Stop'
$wildlifeRoot=Split-Path -Parent $PSScriptRoot
$wildlifeRuntime=[System.IO.Path]::GetFullPath((Join-Path $wildlifeRoot $Runtime))
$wildlifeEvidence=[System.IO.Path]::GetFullPath((Join-Path $wildlifeRoot $Evidence))
if($wildlifeRuntime -eq $wildlifeRoot -or -not $wildlifeRuntime.StartsWith($wildlifeRoot+'\',[System.StringComparison]::OrdinalIgnoreCase)){
    throw 'Use a separate runtime directory inside the repository.'
}
if(-not $wildlifeEvidence.StartsWith($wildlifeRoot+'\',[System.StringComparison]::OrdinalIgnoreCase)){throw 'Keep evidence inside the repository.'}
New-Item -ItemType Directory -Force -Path $wildlifeEvidence | Out-Null
$wildlifeSave=Join-Path $wildlifeRuntime 'savegame.ini'
$wildlifeHadSave=Test-Path -LiteralPath $wildlifeSave
$wildlifeBackup=Join-Path $wildlifeEvidence 'runtime-save-backup.ini'
$wildlifeRootHash=(Get-FileHash -LiteralPath (Join-Path $wildlifeRoot 'savegame.ini')).Hash
if($wildlifeHadSave){Copy-Item -LiteralPath $wildlifeSave -Destination $wildlifeBackup -Force}
try{
    $wildlifeRecords=@()
    foreach($wildlifeView in @('tunnel','pit','block','mounted','normal')){
        $wildlifeStarted=Get-Date
        $wildlifeProcess=Start-Process -FilePath (Join-Path $wildlifeRuntime 'MiniCity3D.exe') -WorkingDirectory $wildlifeRuntime `
            -ArgumentList @('--smoke','--day','--builder-wildlife-preview',"--wildlife-$wildlifeView",'--screenshot') -WindowStyle Hidden -Wait -PassThru
        if($wildlifeProcess.ExitCode -ne 0){throw "Wildlife $wildlifeView capture failed ($($wildlifeProcess.ExitCode))."}
        $wildlifeCapture=Get-ChildItem -LiteralPath (Join-Path $wildlifeRuntime 'screenshots') -File -Filter '*.png' |
            Where-Object LastWriteTime -ge $wildlifeStarted | Sort-Object LastWriteTime -Descending | Select-Object -First 1
        if(-not $wildlifeCapture){throw "Wildlife $wildlifeView produced no image."}
        $wildlifeFile=Join-Path $wildlifeEvidence ($wildlifeView+'.png')
        Move-Item -LiteralPath $wildlifeCapture.FullName -Destination $wildlifeFile -Force
        $wildlifeLog=Get-Content -LiteralPath (Join-Path $wildlifeRuntime 'MiniCity3D.log')
        $wildlifeLog | Set-Content -LiteralPath (Join-Path $wildlifeEvidence ($wildlifeView+'.log'))
        $wildlifePose=($wildlifeLog | Select-String -SimpleMatch 'Wildlife review:' | Select-Object -Last 1).Line
        if(-not $wildlifePose){throw 'Missing native actor/wildlife report.'}
        $wildlifeRecords+=[pscustomobject]@{View=$wildlifeView;Image=$wildlifeView+'.png';SHA256=(Get-FileHash -LiteralPath $wildlifeFile).Hash;Position=$wildlifePose}
        Write-Output "Wildlife $wildlifeView DX11 capture retained."
    }
    $wildlifeRecords | Export-Csv -LiteralPath (Join-Path $wildlifeEvidence 'captures.csv') -NoTypeInformation
}finally{
    if($wildlifeHadSave){Copy-Item -LiteralPath $wildlifeBackup -Destination $wildlifeSave -Force;Remove-Item -LiteralPath $wildlifeBackup}
    elseif(Test-Path -LiteralPath $wildlifeSave){Remove-Item -LiteralPath $wildlifeSave}
    if((Get-FileHash -LiteralPath (Join-Path $wildlifeRoot 'savegame.ini')).Hash -ne $wildlifeRootHash){throw 'Root save changed during wildlife review.'}
}
