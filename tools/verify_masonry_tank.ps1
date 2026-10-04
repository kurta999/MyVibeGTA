$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$runtime=Join-Path $root 'build-msvc-ninja'
$evidence=Join-Path $root 'evidence/masonry-tank-20261004'
New-Item -ItemType Directory -Force -Path $evidence | Out-Null
foreach($name in @('masonry','tank-aim','tank-aim-side')){
    $started=Get-Date
    $arguments=@('--smoke','--day',"--$name-preview",'--screenshot')
    if($name -eq 'tank-aim-side'){$arguments=@('--smoke','--day','--tank-aim-preview','--tank-side-view','--screenshot')}
    $process=Start-Process -FilePath (Join-Path $runtime 'MiniCity3D.exe') -WorkingDirectory $runtime `
        -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru
    if($process.ExitCode -ne 0){throw "$name preview failed: $($process.ExitCode)"}
    $latest=Get-ChildItem (Join-Path $runtime 'screenshots') -Filter '*.png' |
        Where-Object LastWriteTime -ge $started | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if(-not $latest){throw "$name preview produced no new screenshot"}
    Copy-Item -LiteralPath $latest.FullName -Destination (Join-Path $evidence "$name.png") -Force
    Write-Output "$name DX11 preview passed"
}
