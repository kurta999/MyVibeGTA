param(
    [string]$Executable=(Join-Path $PSScriptRoot '../build-msvc-ninja/MiniCity3D.exe'),
    [string]$OutputDirectory=(Join-Path $PSScriptRoot '../evidence/threading-20260928/rendering'),
    [switch]$CompareOnly
)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
public static class ThreadedPixelComparison {
    public static string Compare(byte[] a,byte[] b,int width,int height,int stride) {
        int pixels=0,max=0,minX=width,minY=height,maxX=-1,maxY=-1;long total=0;
        for(int y=0;y<height;y++)for(int x=0;x<width;x++) {
            bool different=false;
            for(int c=0;c<4;c++) {
                int delta=Math.Abs(a[y*stride+x*4+c]-b[y*stride+x*4+c]);
                total+=delta;max=Math.Max(max,delta);different|=delta!=0;
            }
            if(different){pixels++;minX=Math.Min(minX,x);minY=Math.Min(minY,y);maxX=Math.Max(maxX,x);maxY=Math.Max(maxY,y);}
        }
        return $"differing pixels {pixels}/{width*height}; max channel delta {max}; total delta {total}; bounds {minX},{minY}..{maxX},{maxY}";
    }
}
'@
function Read-Pixels([string]$Path) {
    $bitmap=[Drawing.Bitmap]::new((Resolve-Path -LiteralPath $Path).Path)
    try {
        $bits=$bitmap.LockBits([Drawing.Rectangle]::new(0,0,$bitmap.Width,$bitmap.Height),
            [Drawing.Imaging.ImageLockMode]::ReadOnly,[Drawing.Imaging.PixelFormat]::Format32bppArgb)
        try {
            $bytes=[byte[]]::new([Math]::Abs($bits.Stride)*$bitmap.Height)
            [Runtime.InteropServices.Marshal]::Copy($bits.Scan0,$bytes,0,$bytes.Length)
            return @{Bytes=$bytes;Width=$bitmap.Width;Height=$bitmap.Height;Stride=$bits.Stride}
        } finally {$bitmap.UnlockBits($bits)}
    } finally {$bitmap.Dispose()}
}
$executablePath=(Resolve-Path -LiteralPath $Executable).Path
$runtimeDirectory=Split-Path -Parent $executablePath
$log=Join-Path $runtimeDirectory 'MiniCity3D.log'
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$scenes=@(
    @{Name='day';Arguments='--day'},
    @{Name='night';Arguments='--night --ragdoll --high-shadows'},
    @{Name='woods';Arguments='--day --woods'}
)
foreach($scene in $scenes){
    $hashes=@()
    foreach($worker in 1,4){
        $destination=Join-Path $OutputDirectory "$($scene.Name)-workers-$worker.png"
        if(-not $CompareOnly){
            $before=(Get-Content -LiteralPath $log).Count
            $arguments="--smoke --1080p --screenshot --loader-workers=$worker --scene-workers=$worker $($scene.Arguments)"
            $process=Start-Process -FilePath $executablePath -WorkingDirectory $runtimeDirectory `
                -ArgumentList $arguments -WindowStyle Hidden -PassThru
            $process.WaitForExit()
            if($process.ExitCode -ne 0){throw "Rendering failed: $($scene.Name), $worker workers"}
            $lines=@(Get-Content -LiteralPath $log | Select-Object -Skip $before)
            $lines | Set-Content -LiteralPath (Join-Path $OutputDirectory "$($scene.Name)-workers-$worker.txt")
            $capture=@($lines | Select-String 'Screenshot saved: screenshots/(.+\.png)')
            if($capture.Count -ne 1){throw 'Missing rendering capture'}
            $image=Join-Path $runtimeDirectory "screenshots/$($capture[0].Matches[0].Groups[1].Value)"
            Copy-Item -LiteralPath $image -Destination $destination
        }
        $hashes+=(Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash
    }
    $result="$($scene.Name): serial/parallel PNG identical = $($hashes[0] -eq $hashes[1]); SHA-256 $($hashes[0]) / $($hashes[1])"
    $result | Add-Content -LiteralPath (Join-Path $OutputDirectory 'comparison.txt')
    Write-Output $result
    $first=Read-Pixels (Join-Path $OutputDirectory "$($scene.Name)-workers-1.png")
    $second=Read-Pixels (Join-Path $OutputDirectory "$($scene.Name)-workers-4.png")
    if($first.Width -ne $second.Width -or $first.Height -ne $second.Height -or $first.Stride -ne $second.Stride){throw 'Capture dimensions differ'}
    $pixelResult="$($scene.Name): $([ThreadedPixelComparison]::Compare($first.Bytes,$second.Bytes,$first.Width,$first.Height,$first.Stride))"
    $pixelResult | Add-Content -LiteralPath (Join-Path $OutputDirectory 'pixel-comparison.txt')
    Write-Output $pixelResult
}
