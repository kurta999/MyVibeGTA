param([string]$Runtime='build-msvc-ninja', [string]$Evidence='evidence/helicopter-bike-20261007')
$ErrorActionPreference='Stop'
$vehicleRoot=Split-Path -Parent $PSScriptRoot
$vehicleRuntime=[System.IO.Path]::GetFullPath((Join-Path $vehicleRoot $Runtime))
if($vehicleRuntime -eq [System.IO.Path]::GetFullPath($vehicleRoot) -or
   -not $vehicleRuntime.StartsWith($vehicleRoot+'\',[System.StringComparison]::OrdinalIgnoreCase)){
    throw 'Use a separate runtime directory inside the repository.'
}
$vehicleEvidence=Join-Path $vehicleRoot $Evidence
New-Item -ItemType Directory -Force -Path $vehicleEvidence | Out-Null
$vehicleRootSave=Join-Path $vehicleRoot 'savegame.ini'
$vehicleRootHash=(Get-FileHash -LiteralPath $vehicleRootSave).Hash
$vehicleSave=Join-Path $vehicleRuntime 'savegame.ini'
$vehicleHadSave=Test-Path -LiteralPath $vehicleSave
$vehicleBackup=Join-Path $vehicleEvidence 'runtime-save-backup.ini'
if($vehicleHadSave){Copy-Item -LiteralPath $vehicleSave -Destination $vehicleBackup -Force}
try{
    $vehicleHash=(Get-FileHash -LiteralPath (Join-Path $vehicleRuntime 'MiniCity3D.exe')).Hash
    $vehicleRecords=@()
    foreach($vehicleView in @('helicopter','airplane','combine','bike-seat','bicycle-seat','helicopter-night')){
        $vehicleStarted=Get-Date
        $vehicleKind=if($vehicleView -eq 'helicopter-night'){'helicopter'}else{$vehicleView}
        $vehicleTime=if($vehicleView -eq 'helicopter-night'){'--night'}else{'--day'}
        $vehicleArgs=@('--smoke',$vehicleTime,"--$vehicleKind-preview",'--screenshot')
        if($vehicleView -like '*seat'){$vehicleArgs+='--seat-close-up'}
        $vehicleProcess=Start-Process -FilePath (Join-Path $vehicleRuntime 'MiniCity3D.exe') -WorkingDirectory $vehicleRuntime -ArgumentList $vehicleArgs -WindowStyle Hidden -PassThru -Wait
        if($vehicleProcess.ExitCode -ne 0){throw "$vehicleView capture failed ($($vehicleProcess.ExitCode))."}
        $vehicleCapture=Get-ChildItem -LiteralPath (Join-Path $vehicleRuntime 'screenshots') -File -Filter '*.png' |
            Where-Object LastWriteTime -ge $vehicleStarted | Sort-Object LastWriteTime -Descending | Select-Object -First 1
        if(-not $vehicleCapture){throw "$vehicleView produced no image."}
        $vehicleImage=Join-Path $vehicleEvidence "$vehicleView-final.png"
        Copy-Item -LiteralPath $vehicleCapture.FullName -Destination $vehicleImage -Force
        Copy-Item -LiteralPath (Join-Path $vehicleRuntime 'MiniCity3D.log') -Destination (Join-Path $vehicleEvidence "$vehicleView-capture.txt") -Force
        $vehicleRecords+=[pscustomobject]@{View=$vehicleView;Image="$vehicleView-final.png";ImageSHA256=(Get-FileHash -LiteralPath $vehicleImage).Hash;ExecutableSHA256=$vehicleHash;Arguments=($vehicleArgs -join ' ')}
        Write-Output "$vehicleView final DX11 capture saved."
    }
    $vehicleRecords | Export-Csv -LiteralPath (Join-Path $vehicleEvidence 'captures.csv') -NoTypeInformation
}finally{
    if($vehicleHadSave){Copy-Item -LiteralPath $vehicleBackup -Destination $vehicleSave -Force;Remove-Item -LiteralPath $vehicleBackup}
    elseif(Test-Path -LiteralPath $vehicleSave){Remove-Item -LiteralPath $vehicleSave}
    if((Get-FileHash -LiteralPath $vehicleRootSave).Hash -ne $vehicleRootHash){throw 'Root save changed during vehicle verification.'}
}
