param([Parameter(Mandatory)][string]$Reference,[Parameter(Mandatory)][string]$Candidate)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
if(-not ('RenderCaptureDiff' -as [type])){
$drawingReferences=@([System.Drawing.Bitmap].Assembly.Location,[System.Drawing.Size].Assembly.Location,
    (Join-Path $PSHOME 'System.Runtime.dll'),(Join-Path $PSHOME 'System.Runtime.InteropServices.dll'))
$drawingReferences+=@(Get-ChildItem -LiteralPath $PSHOME -Filter 'System.Private.Windows*.dll' | ForEach-Object FullName)
Add-Type -ReferencedAssemblies $drawingReferences -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
public static class RenderCaptureDiff {
    static byte[] Read(Bitmap image) {
        var data=image.LockBits(new Rectangle(0,0,image.Width,image.Height),ImageLockMode.ReadOnly,PixelFormat.Format32bppArgb);
        try { var result=new byte[image.Width*image.Height*4];
            for(int y=0;y<image.Height;y++) Marshal.Copy(IntPtr.Add(data.Scan0,y*data.Stride),result,y*image.Width*4,image.Width*4);
            return result;
        } finally {image.UnlockBits(data);}
    }
    public static string Compare(string reference,string candidate) {
        using(var a=new Bitmap(reference)) using(var b=new Bitmap(candidate)) {
            if(a.Size!=b.Size) throw new Exception("Capture dimensions differ");
            var x=Read(a);var y=Read(b);long changed=0,over2=0,over16=0;double total=0,squares=0;int maximum=0;
            for(int i=0;i<x.Length;i+=4){int largest=0;
                for(int c=0;c<3;c++){int d=Math.Abs(x[i+c]-y[i+c]);total+=d;squares+=d*d;largest=Math.Max(largest,d);}
                maximum=Math.Max(maximum,largest);if(largest>0)changed++;if(largest>2)over2++;if(largest>16)over16++;
            }
            double pixels=a.Width*a.Height;
            return String.Format(System.Globalization.CultureInfo.InvariantCulture,
                "{0}x{1}: RGB MAE {2:F6}/255, RMS {3:F6}/255, max {4}, changed {5:F4}%, >2 {6:F4}%, >16 {7:F4}%",
                a.Width,a.Height,total/(pixels*3),Math.Sqrt(squares/(pixels*3)),maximum,100*changed/pixels,100*over2/pixels,100*over16/pixels);
        }
    }
}
'@
}
[RenderCaptureDiff]::Compare([IO.Path]::GetFullPath($Reference),[IO.Path]::GetFullPath($Candidate))
