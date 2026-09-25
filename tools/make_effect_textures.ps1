$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$destination = Join-Path $PSScriptRoot '..\assets\effects'
New-Item -ItemType Directory -Force -Path $destination | Out-Null
$size = 128

function Clamp01([double]$value) { return [Math]::Max(0.0, [Math]::Min(1.0, $value)) }
function Noise([double]$x, [double]$y) {
    return [Math]::Sin($x * 1.83 + [Math]::Sin($y * 2.71)) * 0.48 +
           [Math]::Sin($x * 4.12 - $y * 3.04) * 0.29 +
           [Math]::Sin($x * 9.11 + $y * 7.13) * 0.15
}
foreach ($kind in @('flame', 'smoke', 'flash', 'shockwave')) {
    $bitmap = [System.Drawing.Bitmap]::new($size, $size,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        for ($py = 0; $py -lt $size; $py++) {
            $v = 1.0 - $py / ($size - 1.0)
            for ($px = 0; $px -lt $size; $px++) {
                $u = ($px - ($size - 1.0) * 0.5) / (($size - 1.0) * 0.5)
                $n = Noise ($u * 4.0) ($v * 4.0)
                $alpha = 0.0
                $red = 0; $green = 0; $blue = 0
                if ($kind -eq 'flame') {
                    $bend = 0.09 * [Math]::Sin($v * 13.0) + 0.07 * $n
                    $width = (0.47 * [Math]::Pow(1.0 - $v, 0.68) + 0.04) *
                        (0.86 + 0.16 * [Math]::Sin($v * 19.0 + $n * 4.0))
                    $edge = Clamp01 (1.0 - [Math]::Abs($u - $bend) / [Math]::Max(0.02, $width))
                    $ripple = Clamp01 (0.68 + $n * 0.32 +
                        [Math]::Sin($u * 23.0 + $v * 31.0) * 0.10)
                    $alpha = [Math]::Pow($edge, 0.78) * $ripple *
                        (0.94 - 0.45 * $v) * (Clamp01 ($v * 9.0 + 0.15))
                    $core = Clamp01 ($edge * 1.6 - 0.35)
                    $red = 255
                    $green = [int](66 + 176 * $core * (1.0 - $v * 0.58))
                    $blue = [int](13 + 126 * [Math]::Pow($core, 2.0) *
                        (1.0 - $v))
                } elseif ($kind -eq 'smoke') {
                    $shift = 0.11 * [Math]::Sin($v * 9.0) + 0.07 * $n
                    $radius = [Math]::Sqrt([Math]::Pow(($u - $shift) / 0.83, 2.0) +
                        [Math]::Pow(($v - 0.50) / 0.60, 2.0))
                    $density = Clamp01 (1.0 - $radius + $n * 0.15)
                    $alpha = [Math]::Pow($density, 1.8) * 0.44
                    $tone = [int](69 + 37 * $density + 13 * $n)
                    $red = $tone; $green = $tone + 2; $blue = $tone + 5
                } elseif ($kind -eq 'flash') {
                    $radius = [Math]::Sqrt($u * $u + ($v - 0.5) * ($v - 0.5) * 4.0)
                    $rays = [Math]::Abs($u) * [Math]::Abs($v - 0.5) * 7.0
                    $alpha = [Math]::Max([Math]::Pow((Clamp01 (1.0 - $radius)), 2.3),
                        0.32 * [Math]::Pow((Clamp01 (1.0 - $rays)), 4.0) *
                        (Clamp01 (1.0 - $radius * 0.9)))
                    $alpha *= 0.83
                    $red = 255; $green = 220; $blue = 142
                } else {
                    $radius = [Math]::Sqrt($u * $u + (2.0 * $v - 1.0) *
                        (2.0 * $v - 1.0))
                    $rim = ($radius - 0.70 - 0.025 * $n) / 0.085
                    $alpha = 0.70 * [Math]::Exp(-$rim * $rim) +
                        0.09 * (Clamp01 (1.0 - $radius))
                    $red = 255; $green = 166; $blue = 76
                }
                $color = [System.Drawing.Color]::FromArgb(
                    [int](255 * (Clamp01 $alpha)),
                    [int][Math]::Max(0, [Math]::Min(255, $red)),
                    [int][Math]::Max(0, [Math]::Min(255, $green)),
                    [int][Math]::Max(0, [Math]::Min(255, $blue)))
                $bitmap.SetPixel($px, $py, $color)
            }
        }
        $bitmap.Save((Join-Path $destination "$kind.png"),
            [System.Drawing.Imaging.ImageFormat]::Png)
    } finally { $bitmap.Dispose() }
}
