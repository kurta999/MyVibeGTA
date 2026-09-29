# Deliver the original authoring sources independently of the full game package.
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$staging=Join-Path $root ('build-tools/modern-assets-package-'+[guid]::NewGuid().ToString('N'))
$models=Join-Path $staging 'assets/models'
New-Item -ItemType Directory -Path (Join-Path $models 'baked'),(Join-Path $models 'source'),(Join-Path $staging 'tools'),(Join-Path $staging 'tests') -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'assets/models/baked/modern') -Destination (Join-Path $models 'baked') -Recurse
Copy-Item -LiteralPath (Join-Path $root 'assets/models/source/modern') -Destination (Join-Path $models 'source') -Recurse
Copy-Item -LiteralPath (Join-Path $root 'assets/models/MODERN_ASSETS.md') -Destination (Join-Path $staging 'README.md')
foreach ($name in @('build_modern_assets.py','build_modern_materials.py','cook_modern_textures.py','bootstrap_texconv.ps1')) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination (Join-Path $staging 'tools')
}
Copy-Item -LiteralPath (Join-Path $root 'tests/modern_assets.py') -Destination (Join-Path $staging 'tests')
Copy-Item -LiteralPath (Join-Path $root 'tests/modern_texture_quality.py') -Destination (Join-Path $staging 'tests')
Copy-Item -LiteralPath (Join-Path $root 'evidence/modern-assets-20260928/modern-assets-overview.jpg') -Destination $staging
$destination=Join-Path $root 'dist/MiniCity3D-original-modern-assets-20260928.zip'
New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
Compress-Archive -Path (Join-Path $staging 'assets'),(Join-Path $staging 'tools'),(Join-Path $staging 'tests'),(Join-Path $staging 'README.md'),(Join-Path $staging 'modern-assets-overview.jpg') -DestinationPath $destination -Force
Write-Output $destination
Write-Output ('SHA256 '+(Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash)
