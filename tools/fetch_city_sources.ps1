param([string[]]$AssetIds = @(
    'tree_small_02', 'fir_sapling', 'quiver_tree_01',
    'island_tree_01', 'island_tree_02', 'pine_sapling_small',
    'modular_urban_apartments_facade', 'modular_factory_facade',
    'shrub_01', 'shrub_02', 'shrub_03', 'shrub_04', 'shrub_sorrel_01',
    'wild_rooibos_bush', 'fern_02', 'nettle_plant', 'periwinkle_plant',
    'weed_plant_02', 'crystalline_iceplant'))
$ErrorActionPreference = 'Stop'
$root = Join-Path $PSScriptRoot '../assets/models/source/polyhaven'
foreach ($id in $AssetIds) {
    $listing = Invoke-RestMethod "https://api.polyhaven.com/files/$id"
    $gltf = $listing.gltf.'1k'.gltf
    if (-not $gltf) { throw "No 1K glTF for $id" }
    $folder = Join-Path $root $id
    New-Item -ItemType Directory -Force -Path $folder | Out-Null
    $files = @(@{ Name = "$id.gltf"; Url = $gltf.url; Md5 = $gltf.md5 })
    foreach ($entry in $gltf.include.PSObject.Properties) {
        if ($entry.Name -like '*.bin' -or $entry.Name -match '(_diff_|_alpha_)') {
            $files += @{ Name = $entry.Name; Url = $entry.Value.url; Md5 = $entry.Value.md5 }
        }
    }
    foreach ($entry in $files) {
        $target = Join-Path $folder $entry.Name
        New-Item -ItemType Directory -Force -Path (Split-Path $target) | Out-Null
        if (Test-Path -LiteralPath $target) {
            $hash = (Get-FileHash -LiteralPath $target -Algorithm MD5).Hash.ToLowerInvariant()
            if ($hash -eq $entry.Md5) { continue }
        }
        Invoke-WebRequest -Uri $entry.Url -OutFile $target -TimeoutSec 180
        $hash = (Get-FileHash -LiteralPath $target -Algorithm MD5).Hash.ToLowerInvariant()
        if ($hash -ne $entry.Md5) { throw "Checksum mismatch: $target" }
        Write-Host "Downloaded $id/$($entry.Name)"
    }
}

# Smaller, individually authored CC0 biome trees from the 3DAssets catalog.
$slugs = @(
    'desert-mosque-and-madrasa-courtyard-date-palm-c311e205',
    'exotic-wildlife-hd-umbrella-acacia-850a62bb',
    'exotic-wildlife-hd-baobab-tree-fb196805',
    'japanese-school-and-city-street-cherry-tree-blossom-618a9f6d',
    'greek-island-village-and-harbour-olive-tree-b7baf3c7'
)
$smallRoot = Join-Path $PSScriptRoot '../assets/models/source/3dassets'
New-Item -ItemType Directory -Force -Path $smallRoot | Out-Null
foreach ($slug in $slugs) {
    $asset = (Invoke-RestMethod "https://3dassets.dev/api/v1/assets/$slug").data
    if ($asset.license.slug -ne 'cc0-1.0') { throw "Unexpected license for $slug" }
    $target = Join-Path $smallRoot "$slug.glb"
    if (-not (Test-Path -LiteralPath $target) -or
        (Get-Item -LiteralPath $target).Length -ne $asset.stats.fileSize) {
        Invoke-WebRequest -Uri $asset.cdnUrl -OutFile $target -TimeoutSec 90
    }
    if ((Get-Item -LiteralPath $target).Length -ne $asset.stats.fileSize) {
        throw "Unexpected length for $slug"
    }
    Write-Host "Downloaded $slug"
}
