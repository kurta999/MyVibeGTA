param([string]$Evidence = 'evidence/rendering-refactor-20261009')
$ErrorActionPreference = 'Stop'
$directory = Join-Path (Split-Path -Parent $PSScriptRoot) $Evidence
$results = foreach ($name in @('dx11', 'dx12', 'dx12-fsr2', 'dx12-workers')) {
    $metrics = & "$PSScriptRoot/compare_render_captures.ps1" -PassThru `
        -Reference (Join-Path $directory "$name-baseline.png") `
        -Candidate (Join-Path $directory "$name-candidate.png")
    $metrics | Add-Member -NotePropertyName Configuration -NotePropertyValue $name -PassThru
}
$results | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $directory 'pixel-comparisons.json')
$results | Format-Table Configuration, Width, Height, ChangedPixels, MeanAbsoluteRgbDifference, MaximumChannelDifference
