$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$destination = Join-Path $root 'build-tools/directxtex-may2026'
New-Item -ItemType Directory -Force -Path $destination | Out-Null
$tools = @{
    'texconv.exe' = 'dcfdec10244e02cf5037fba089c55fb7e1326b1c8181742d77d15fa5cb5eef06'
    'texdiag.exe' = '411c303c98ba73e4423376f717ac139347dd749bf80a7fc1a22368ab1088ff56'
}
foreach ($name in $tools.Keys) {
    $path = Join-Path $destination $name
    if (!(Test-Path $path) -or (Get-FileHash $path -Algorithm SHA256).Hash.ToLowerInvariant() -ne $tools[$name]) {
        Invoke-WebRequest "https://github.com/microsoft/DirectXTex/releases/download/may2026/$name" -OutFile $path
    }
    if ((Get-FileHash $path -Algorithm SHA256).Hash.ToLowerInvariant() -ne $tools[$name]) { throw "Hash mismatch: $name" }
}
Write-Output "Verified DirectXTex May 2026: $destination"
