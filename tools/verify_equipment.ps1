$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$runtime=Join-Path $root 'build-msvc-ninja'
$evidence=Join-Path $root 'evidence/equipment-20261003'
New-Item -ItemType Directory -Path $evidence -Force | Out-Null
foreach($preview in @('helicopter','minigun','shovel','grapple')) {
    $process=Start-Process -FilePath (Join-Path $runtime 'MiniCity3D.exe') -WorkingDirectory $runtime -ArgumentList @('--smoke','--day',"--$preview-preview",'--screenshot') -WindowStyle Hidden -Wait -PassThru
    if($process.ExitCode -ne 0){throw "$preview preview failed: $($process.ExitCode)"}
    $latest=Get-ChildItem -LiteralPath (Join-Path $runtime 'screenshots') -Filter '*.png' | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if(-not $latest){throw 'Screenshot missing'}
    Copy-Item -LiteralPath $latest.FullName -Destination (Join-Path $evidence "$preview.png") -Force
    Write-Output "$preview preview: exit 0; $($latest.Name)"
}
