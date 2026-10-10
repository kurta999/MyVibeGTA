param([Parameter(Mandatory)][string]$Reference,
      [Parameter(Mandatory)][string]$Candidate,
      [switch]$PassThru)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
if (-not ('RenderCaptureComparison' -as [type])) {
$drawingReferences = @([System.Drawing.Bitmap].Assembly.Location, [System.Drawing.Rectangle].Assembly.Location)
foreach ($assemblyReference in [System.Drawing.Bitmap].Assembly.GetReferencedAssemblies()) {
    if ($assemblyReference.Name -like 'System.Private.Windows.*') {
        $drawingReferences += [Reflection.Assembly]::Load($assemblyReference).Location
    }
}
Add-Type -ReferencedAssemblies $drawingReferences -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
public static class RenderCaptureComparison {
    public static double[] Compare(string beforePath, string afterPath) {
        using (var before = new Bitmap(beforePath))
        using (var after = new Bitmap(afterPath)) {
            if (before.Size != after.Size) throw new InvalidOperationException("Capture dimensions differ");
            var area = new Rectangle(0, 0, before.Width, before.Height);
            var a = before.LockBits(area, ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
            var b = after.LockBits(area, ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
            try {
                var rowA = new byte[before.Width * 4];
                var rowB = new byte[rowA.Length];
                long total = 0, changed = 0, over2 = 0, over16 = 0;
                double squares = 0;
                int maximum = 0;
                int minX = before.Width, minY = before.Height, maxX = -1, maxY = -1;
                for (int y = 0; y < before.Height; ++y) {
                    Marshal.Copy(IntPtr.Add(a.Scan0, y * a.Stride), rowA, 0, rowA.Length);
                    Marshal.Copy(IntPtr.Add(b.Scan0, y * b.Stride), rowB, 0, rowB.Length);
                    for (int x = 0; x < before.Width; ++x) {
                        bool different = false;
                        int largest = 0;
                        for (int channel = 0; channel < 3; ++channel) {
                            int difference = Math.Abs(rowA[x * 4 + channel] - rowB[x * 4 + channel]);
                            total += difference;
                            squares += difference * difference;
                            largest = Math.Max(largest, difference);
                            maximum = Math.Max(maximum, difference);
                            different |= difference != 0;
                        }
                        if (different) {
                            ++changed;
                            minX = Math.Min(minX, x); minY = Math.Min(minY, y);
                            maxX = Math.Max(maxX, x); maxY = Math.Max(maxY, y);
                        }
                        if (largest > 2) ++over2;
                        if (largest > 16) ++over16;
                    }
                }
                return new double[] { before.Width, before.Height, changed,
                    (double)total / (before.Width * before.Height * 3), maximum,
                    minX, minY, maxX, maxY,
                    Math.Sqrt(squares / (before.Width * before.Height * 3)), over2, over16 };
            } finally { before.UnlockBits(a); after.UnlockBits(b); }
        }
    }
}
'@
}
$metrics = [RenderCaptureComparison]::Compare([IO.Path]::GetFullPath($Reference), [IO.Path]::GetFullPath($Candidate))
if ($PassThru) {
    [PSCustomObject]@{ Width = $metrics[0]; Height = $metrics[1]; ChangedPixels = $metrics[2]; MeanAbsoluteRgbDifference = $metrics[3]; MaximumChannelDifference = $metrics[4]; DifferenceBounds = @($metrics[5], $metrics[6], $metrics[7], $metrics[8]); RootMeanSquareDifference = $metrics[9]; PixelsOver2 = $metrics[10]; PixelsOver16 = $metrics[11] }
} else {
    $pixels = $metrics[0] * $metrics[1]
    [string]::Format([Globalization.CultureInfo]::InvariantCulture,
        '{0}x{1}: RGB MAE {2:F6}/255, RMS {3:F6}/255, max {4}, changed {5:F4}%, >2 {6:F4}%, >16 {7:F4}%',
        $metrics[0], $metrics[1], $metrics[3], $metrics[9], $metrics[4],
        (100 * $metrics[2] / $pixels), (100 * $metrics[10] / $pixels), (100 * $metrics[11] / $pixels))
}
