$ErrorActionPreference = 'Stop'
$taskRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../assets/models/source/polyhaven'))
$taskIds = @('tree_small_02','island_tree_01','island_tree_02','island_tree_03','fir_sapling','pine_sapling_small','jacaranda_tree','quiver_tree_01','quiver_tree_02')
foreach ($taskId in $taskIds) {
    $taskFolder = Join-Path $taskRoot $taskId
    $taskGltf = Join-Path $taskFolder "$taskId.gltf"
    $taskDocument = Get-Content -Raw -LiteralPath $taskGltf | ConvertFrom-Json
    $taskMissing = @($taskDocument.buffers | Where-Object { -not (Test-Path -LiteralPath (Join-Path $taskFolder $_.uri)) })
    if ($taskMissing.Count -eq 0) { continue }
    $taskListing = Invoke-RestMethod "https://api.polyhaven.com/files/$taskId"
    $taskModel = $taskListing.gltf.'1k'.gltf
    if ((Get-FileHash -LiteralPath $taskGltf -Algorithm MD5).Hash.ToLowerInvariant() -ne $taskModel.md5) {
        $taskRemote = Invoke-RestMethod $taskModel.url
        if (($taskDocument | ConvertTo-Json -Compress -Depth 100) -ne ($taskRemote | ConvertTo-Json -Compress -Depth 100)) { throw "Source glTF changed for $taskId; inspect before importing a different version." }
    }
    foreach ($taskBuffer in $taskMissing) {
        $taskEntry = $taskModel.include.PSObject.Properties[$taskBuffer.uri].Value
        if (-not $taskEntry) { throw "Missing source buffer metadata: $taskId/$($taskBuffer.uri)" }
        $taskUrl = [Uri]$taskEntry.url
        if ($taskUrl.Scheme -ne 'https' -or $taskUrl.Host -ne 'dl.polyhaven.org') { throw 'Unexpected source download host' }
        $taskPath = [IO.Path]::GetFullPath((Join-Path $taskFolder $taskBuffer.uri))
        if (-not $taskPath.StartsWith($taskFolder+[IO.Path]::DirectorySeparatorChar) -or [IO.Path]::GetExtension($taskPath) -ne '.bin') { throw 'Unexpected source buffer path' }
        Invoke-WebRequest $taskUrl.AbsoluteUri -OutFile $taskPath
        if ((Get-FileHash -LiteralPath $taskPath -Algorithm MD5).Hash.ToLowerInvariant() -ne $taskEntry.md5) { throw "Source buffer checksum failed: $taskId" }
        Write-Host "Restored source buffer $taskId ($((Get-Item -LiteralPath $taskPath).Length) bytes)"
    }
}
