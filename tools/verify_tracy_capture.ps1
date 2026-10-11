param(
    [Parameter(Mandatory=$true)][string]$Trace,
    [string]$ToolsDirectory=(Join-Path $PSScriptRoot '../build-tools/tracy-0.13.1'),
    [switch]$RequireRecordingWorkers
)
$ErrorActionPreference='Stop'
$tracePath=(Resolve-Path -LiteralPath $Trace).Path
$directory=Split-Path -Parent $tracePath
$export=(Resolve-Path -LiteralPath (Join-Path $ToolsDirectory 'tracy-csvexport.exe')).Path
$update=(Resolve-Path -LiteralPath (Join-Path $ToolsDirectory 'tracy-update.exe')).Path
& $export $tracePath > (Join-Path $directory 'cpu-zones.csv')
if($LASTEXITCODE -ne 0){throw 'CPU trace export failed.'}
& $export -u $tracePath > (Join-Path $directory 'cpu-events.csv')
if($LASTEXITCODE -ne 0){throw 'Thread event export failed.'}
& $export -g $tracePath > (Join-Path $directory 'gpu-zones.csv')
if($LASTEXITCODE -ne 0){throw 'GPU trace export failed.'}
$cpu=@(Import-Csv -LiteralPath (Join-Path $directory 'cpu-zones.csv'))
foreach($name in @('Simulation tick','Pedestrian AI','Jolt physics step','Forest vegetation and LOD','Texture decode and mip preparation','CPU worker job')){
    if(-not @($cpu | Where-Object {$_.name -eq $name -and [int]$_.counts -gt 0}).Count){throw "Missing CPU zone: $name"}
}
if($RequireRecordingWorkers -and -not @($cpu | Where-Object {$_.name -eq 'DX12 worker command recording' -and [int]$_.counts -gt 0}).Count){throw 'Missing DX12 recording worker zones.'}
$threads=@(Import-Csv -LiteralPath (Join-Path $directory 'cpu-events.csv') | Select-Object -ExpandProperty thread -Unique)
if($threads.Count -lt 2){throw 'No instrumented worker threads found.'}
$gpu=@(Import-Csv -LiteralPath (Join-Path $directory 'gpu-zones.csv'))
foreach($name in @('Shadows','Scene and forest','GPU skin deformation')){
    if(-not @($gpu | Where-Object {$_.name -eq $name -and [long]$_.'GPU execution time' -gt 0}).Count){throw "Missing timed native GPU zone: $name"}
}
if(@($gpu | Where-Object {[long]$_.'GPU execution time' -lt 0}).Count){throw 'Trace contains unresolved GPU timings.'}
# The upstream exporter does not expose allocation counts. Verify the memory
# section by rewriting twice with identical compression, stripping only memory.
$temporary=Join-Path $PSScriptRoot ('../build-tools/tracy-verification/'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $temporary -Force | Out-Null
$full=Join-Path $temporary 'full.tracy';$stripped=Join-Path $temporary 'without-memory.tracy'
& $update -z 3 $tracePath $full > (Join-Path $directory 'memory-check.txt')
if($LASTEXITCODE -ne 0){throw 'Trace rewrite failed.'}
& $update -z 3 -s M $tracePath $stripped >> (Join-Path $directory 'memory-check.txt')
if($LASTEXITCODE -ne 0){throw 'Memory strip verification failed.'}
$difference=(Get-Item -LiteralPath $full).Length-(Get-Item -LiteralPath $stripped).Length
if($difference -lt 1024){throw 'No substantial allocation memory section found.'}
$summary=[ordered]@{Trace=$tracePath;CPUZones=$cpu.Count;CPUEvents=($cpu | Measure-Object -Property counts -Sum).Sum;
    InstrumentedThreads=$threads.Count;GpuEvents=$gpu.Count;GpuPasses=@($gpu | Select-Object -ExpandProperty name -Unique);
    AllocationSectionVerified=$true;MemoryStripReductionBytes=$difference;RecordingWorkersRequired=[bool]$RequireRecordingWorkers}
$summary | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $directory 'trace-verification.json')
$summary | ConvertTo-Json -Depth 3
