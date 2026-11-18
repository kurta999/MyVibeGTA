$ErrorActionPreference = 'Stop'
$root = Join-Path $PSScriptRoot '../assets/models/source/polyhaven'
$foliage = @{
    tree_small_02 = 'leaves'
    island_tree_01 = 'leaves'
    jacaranda_tree = 'leaves'
    fir_sapling = 'twigs'
    pine_sapling_small = 'twig'
    quiver_tree_01 = 'leaf'
}
foreach ($id in ($foliage.Keys | Sort-Object)) {
    $listing = Invoke-RestMethod "https://api.polyhaven.com/files/$id" -TimeoutSec 30
    $prefix = $foliage[$id]
    $records = foreach ($type in 'diff', 'alpha', 'nor_dx') {
        $extension = if ($type -eq 'alpha') { 'png' } else { 'jpg' }
        $file = $listing."${prefix}_$type".'2k'.$extension
        if (!$file) {
            if ($type -eq 'alpha') { continue }
            throw "Missing $id 2K ${prefix}_$type material"
        }
        $name = $file.url.Split('/')[-1]
        $directory = Join-Path $root "$id/textures"
        $target = Join-Path $directory $name
        New-Item -ItemType Directory -Path $directory -Force | Out-Null
        if (!(Test-Path -LiteralPath $target) -or
            (Get-FileHash -LiteralPath $target -Algorithm MD5).Hash -ne $file.md5) {
            $temporary = "$target.download"
            Invoke-WebRequest -Uri $file.url -OutFile $temporary -TimeoutSec 60
            if ((Get-FileHash -LiteralPath $temporary -Algorithm MD5).Hash -ne $file.md5) {
                throw "Checksum mismatch for $name"
            }
            Move-Item -LiteralPath $temporary -Destination $target -Force
        }
        [ordered]@{name = $name; url = $file.url; md5 = $file.md5}
    }
    @($records) | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $root "$id/leaf-maps.json") -Encoding utf8
    Write-Output "$id $($records.Count) maps"
}
