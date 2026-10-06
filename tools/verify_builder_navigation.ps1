param([string]$Runtime='build-msvc-ninja', [switch]$CaptureOnly, [string]$Evidence='evidence/builder-navigation-20261006')
$ErrorActionPreference='Stop'
$navigationRoot=Split-Path -Parent $PSScriptRoot
$navigationRuntime=[System.IO.Path]::GetFullPath((Join-Path $navigationRoot $Runtime))
if($navigationRuntime -eq [System.IO.Path]::GetFullPath($navigationRoot) -or
   -not $navigationRuntime.StartsWith($navigationRoot+'\',[System.StringComparison]::OrdinalIgnoreCase)){
    throw 'Use a separate runtime directory inside the repository.'
}
$navigationEvidence=Join-Path $navigationRoot $Evidence
New-Item -ItemType Directory -Force -Path $navigationEvidence | Out-Null
$navigationRootSave=Join-Path $navigationRoot 'savegame.ini'
$navigationRootHash=(Get-FileHash -LiteralPath $navigationRootSave).Hash
$navigationSave=Join-Path $navigationRuntime 'savegame.ini'
$navigationHadSave=Test-Path -LiteralPath $navigationSave
$navigationBackup=Join-Path $navigationEvidence 'runtime-save-backup.ini'
if($navigationHadSave){Copy-Item -LiteralPath $navigationSave -Destination $navigationBackup -Force}
try{
    if(-not $CaptureOnly){
        $navigationCtest='C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe'
        & $navigationCtest --test-dir $navigationRuntime -R '^builder_navigation_(scenarios|restart_save|restart_load)$' --output-on-failure -j 1
        if($LASTEXITCODE -ne 0){throw 'Builder navigation scenarios failed.'}
    }
    $navigationRecords=@()
    foreach($navigationView in @('pit','tunnel','normal','streaming')){
        $navigationStarted=Get-Date
        $navigationArgs=@('--smoke','--day','--builder-navigation-preview','--screenshot')
        if($navigationView -ne 'pit'){$navigationArgs+="--navigation-$navigationView"}
        $navigationProcess=Start-Process -FilePath (Join-Path $navigationRuntime 'MiniCity3D.exe') -WorkingDirectory $navigationRuntime -ArgumentList $navigationArgs -WindowStyle Hidden -Wait -PassThru
        if($navigationProcess.ExitCode -ne 0){throw "Navigation $navigationView capture failed ($($navigationProcess.ExitCode))."}
        $navigationCapture=Get-ChildItem -LiteralPath (Join-Path $navigationRuntime 'screenshots') -File -Filter '*.png' |
            Where-Object LastWriteTime -ge $navigationStarted | Sort-Object LastWriteTime -Descending | Select-Object -First 1
        if(-not $navigationCapture){throw "Navigation $navigationView produced no image."}
        $navigationDestination=Join-Path $navigationEvidence "$navigationView.png"
        Move-Item -LiteralPath $navigationCapture.FullName -Destination $navigationDestination -Force
        $navigationLog=Get-Content -LiteralPath (Join-Path $navigationRuntime 'MiniCity3D.log')
        $navigationLog | Set-Content -LiteralPath (Join-Path $navigationEvidence "$navigationView.log")
        $navigationRecords+=[pscustomobject]@{View=$navigationView;Image="$navigationView.png";SHA256=(Get-FileHash -LiteralPath $navigationDestination).Hash;Position=($navigationLog | Select-String 'Navigation review:' | Select-Object -Last 1).Line}
        Write-Output "Navigation $navigationView DX11 capture retained."
    }
    $navigationRecords | Export-Csv -LiteralPath (Join-Path $navigationEvidence 'captures.csv') -NoTypeInformation
}finally{
    if($navigationHadSave){Copy-Item -LiteralPath $navigationBackup -Destination $navigationSave -Force;Remove-Item -LiteralPath $navigationBackup}
    elseif(Test-Path -LiteralPath $navigationSave){Remove-Item -LiteralPath $navigationSave}
    if((Get-FileHash -LiteralPath $navigationRootSave).Hash -ne $navigationRootHash){throw 'Root save changed during navigation verification.'}
}
