param([string]$Runtime='build-msvc-ninja')
$ErrorActionPreference='Stop'
$excavationRoot=Split-Path -Parent $PSScriptRoot
$excavationRuntime=Join-Path $excavationRoot $Runtime
$excavationEvidence=Join-Path $excavationRoot 'evidence/excavation-20261006'
New-Item -ItemType Directory -Path $excavationEvidence -Force | Out-Null
$excavationCtest='C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe'
& $excavationCtest --test-dir $excavationRuntime -R '^excavation_scenarios$' --output-on-failure
if($LASTEXITCODE -ne 0){throw 'Excavation simulation regression failed'}
# Fixtures write runtime saves; keep them sequential with save-writing tests.
foreach($excavationView in @('pit','tunnel','normal','mountain')){
    $excavationStarted=Get-Date
    $excavationArguments=@('--smoke','--day','--excavation-preview','--screenshot')
    if($excavationView -ne 'pit'){$excavationArguments+="--excavation-$excavationView"}
    $excavationProcess=Start-Process -FilePath (Join-Path $excavationRuntime 'MiniCity3D.exe') `
        -WorkingDirectory $excavationRuntime -ArgumentList $excavationArguments -WindowStyle Hidden -Wait -PassThru
    if($excavationProcess.ExitCode -ne 0){throw "Excavation $excavationView preview failed ($($excavationProcess.ExitCode))"}
    $excavationImage=Get-ChildItem -LiteralPath (Join-Path $excavationRuntime 'screenshots') -Filter '*.png' |
        Where-Object LastWriteTime -ge $excavationStarted | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if(-not $excavationImage){throw "Excavation $excavationView produced no capture"}
    Copy-Item -LiteralPath $excavationImage.FullName -Destination (Join-Path $excavationEvidence "$excavationView.png") -Force
    Write-Output "Excavation $excavationView DX11 capture saved"
}
