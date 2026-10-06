param([string]$Runtime='build-msvc-ninja')
$ErrorActionPreference='Stop'
$builderRoot=Split-Path -Parent $PSScriptRoot
$builderRuntime=Join-Path $builderRoot $Runtime
$builderEvidence=Join-Path $builderRoot 'evidence/builder-20261006'
New-Item -ItemType Directory -Path $builderEvidence -Force | Out-Null
$builderCtest=Get-Command ctest -ErrorAction SilentlyContinue
if($builderCtest){$builderCtestPath=$builderCtest.Source}else{
    $builderCtestPath='C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe'
}
& $builderCtestPath --test-dir $builderRuntime -R '^builder_scenarios$' --output-on-failure
if($LASTEXITCODE -ne 0){throw 'Builder simulation regression failed'}
# Save-writing tests and preview processes intentionally run sequentially.
foreach($builderView in @('world','inventory','loading','tool','third-person')){
    $builderStarted=Get-Date
    $builderArguments=@('--smoke','--day','--builder-preview','--screenshot')
    if($builderView -ne 'world'){$builderArguments+="--builder-$builderView"}
    $builderProcess=Start-Process -FilePath (Join-Path $builderRuntime 'MiniCity3D.exe') `
        -WorkingDirectory $builderRuntime -ArgumentList $builderArguments -WindowStyle Hidden -Wait -PassThru
    if($builderProcess.ExitCode -ne 0){throw "Builder $builderView preview failed ($($builderProcess.ExitCode))"}
    $builderImage=Get-ChildItem -LiteralPath (Join-Path $builderRuntime 'screenshots') -Filter '*.png' |
        Where-Object LastWriteTime -ge $builderStarted | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if(-not $builderImage){throw "Builder $builderView preview produced no new capture"}
    Copy-Item -LiteralPath $builderImage.FullName -Destination (Join-Path $builderEvidence "$builderView.png") -Force
    Write-Output "Builder $builderView DX11 capture saved"
}
