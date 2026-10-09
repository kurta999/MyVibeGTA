param([string]$Runtime='build-msvc-ninja',[switch]$Before)
$ErrorActionPreference='Stop'
$treeRoot=Split-Path -Parent $PSScriptRoot
$treeRuntime=Join-Path $treeRoot $Runtime
$treeEvidence=Join-Path $treeRoot 'evidence/trees-20261007'
$treeAssets=Join-Path $treeRuntime 'assets/models/baked/nature'
$treeStage=if($Before){'before'}else{'after'}
New-Item -ItemType Directory -Path $treeEvidence -Force | Out-Null
function Save-TreeLog([string]$Destination){
    $treeLines=@(Get-Content -LiteralPath (Join-Path $treeRuntime 'MiniCity3D.log'))
    $treeFirst=0
    for($treeLine=0;$treeLine -lt $treeLines.Count;$treeLine++){
        if($treeLines[$treeLine].Contains('Application started')){$treeFirst=$treeLine}
    }
    $treeLines | Select-Object -Skip $treeFirst | Set-Content -LiteralPath $Destination
}
if($Before){
    $treeOriginal=Join-Path $treeRoot 'build-tools/trees-before'
    if(-not(Test-Path -LiteralPath $treeOriginal)){throw 'Original tree snapshot is missing'}
    Get-ChildItem -LiteralPath $treeOriginal -File | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $treeAssets -Force
    }
}
try{
    foreach($treeModel in @('tree_detailed','tree_default','tree_oak','tree_thin','tree_pineDefaultA')){
        foreach($treeDistance in @('near','distant')){
            $treeStarted=Get-Date
            $treeArgs=@('--smoke','--day','--tree-preview',"--tree-model=$treeModel",'--screenshot')
            if($treeDistance -eq 'distant'){$treeArgs+='--tree-distant'}
            $treeProcess=Start-Process -FilePath (Join-Path $treeRuntime 'MiniCity3D.exe') -WorkingDirectory $treeRuntime `
                -ArgumentList $treeArgs -WindowStyle Hidden -Wait -PassThru
            if($treeProcess.ExitCode -ne 0){throw "Tree $treeModel $treeDistance failed ($($treeProcess.ExitCode))"}
            $treeImage=Get-ChildItem -LiteralPath (Join-Path $treeRuntime 'screenshots') -Filter '*.png' |
                Where-Object LastWriteTime -ge $treeStarted | Sort-Object LastWriteTime -Descending | Select-Object -First 1
            if(-not $treeImage){throw "Tree $treeModel $treeDistance produced no capture"}
            Copy-Item -LiteralPath $treeImage.FullName -Destination (Join-Path $treeEvidence "$treeStage-$treeModel-$treeDistance.png") -Force
            Save-TreeLog (Join-Path $treeEvidence "$treeStage-$treeModel-$treeDistance.txt")
            Write-Output "$treeStage $treeModel $treeDistance captured"
        }
    }
    $treeStarted=Get-Date
    $treeProcess=Start-Process -FilePath (Join-Path $treeRuntime 'MiniCity3D.exe') -WorkingDirectory $treeRuntime `
        -ArgumentList @('--smoke','--day','--woods','--1080p','--benchmark','--screenshot') -WindowStyle Hidden -Wait -PassThru
    if($treeProcess.ExitCode -ne 0){throw 'Woods benchmark failed'}
    Save-TreeLog (Join-Path $treeEvidence "$treeStage-woods-benchmark.txt")
    $treeImage=Get-ChildItem -LiteralPath (Join-Path $treeRuntime 'screenshots') -Filter '*.png' |
        Where-Object LastWriteTime -ge $treeStarted | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if(-not $treeImage){throw 'Woods benchmark produced no capture'}
    Copy-Item -LiteralPath $treeImage.FullName -Destination (Join-Path $treeEvidence "$treeStage-woods.png") -Force
}finally{
    if($Before){
        Get-ChildItem -LiteralPath $treeOriginal -File | ForEach-Object {
            Copy-Item -LiteralPath (Join-Path $treeRoot "assets/models/baked/nature/$($_.Name)") -Destination $treeAssets -Force
        }
    }
}
