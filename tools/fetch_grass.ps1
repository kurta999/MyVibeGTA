$ErrorActionPreference='Stop'
$id='grass_medium_01'
$folder=Join-Path $PSScriptRoot '../assets/models/source/polyhaven/grass_medium_01'
New-Item -ItemType Directory -Force -Path $folder | Out-Null
$listing=Invoke-RestMethod "https://api.polyhaven.com/files/$id"
$gltf=$listing.gltf.'2k'.gltf
$files=@(@{Name="$id.gltf";Url=$gltf.url;Md5=$gltf.md5})
foreach($entry in $gltf.include.PSObject.Properties){
    if($entry.Name -like '*.bin' -or $entry.Name -match '(_diff_|_arm_|_nor_gl_)'){
        $files+=@{Name=$entry.Name;Url=$entry.Value.url;Md5=$entry.Value.md5}
    }
}
foreach($channel in @('Alpha','dry_diff','nor_dx')){
    $format=if($channel -eq 'Alpha'){'png'}else{'jpg'}
    $file=$listing.$channel.'2k'.$format
    $files+=@{Name=('textures/'+($file.url.Split('/')[-1]));Url=$file.url;Md5=$file.md5}
}
foreach($file in $files){
    $target=Join-Path $folder $file.Name
    New-Item -ItemType Directory -Force -Path (Split-Path $target) | Out-Null
    if((Test-Path -LiteralPath $target) -and
       (Get-FileHash -LiteralPath $target -Algorithm MD5).Hash.ToLowerInvariant() -eq $file.Md5){continue}
    Invoke-WebRequest -Uri $file.Url -OutFile $target -TimeoutSec 180
    if((Get-FileHash -LiteralPath $target -Algorithm MD5).Hash.ToLowerInvariant() -ne $file.Md5){throw "Checksum mismatch: $target"}
    Write-Output "Downloaded $($file.Name)"
}
$files | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $folder 'downloads.json')
