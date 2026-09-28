# Original vector-drawn city mark. Rebuild the committed Windows icon without
# external art tools; each size is drawn separately for a crisp small icon.
param([string]$OutputDirectory=(Join-Path $PSScriptRoot '..\assets\app'))
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$sizes=@(16,24,32,48,64,128,256)
$images=@()
foreach($size in $sizes){
    $bitmap=[System.Drawing.Bitmap]::new($size,$size)
    $graphics=[System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.SmoothingMode=[System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.ScaleTransform($size/256.0,$size/256.0)
    $graphics.Clear([System.Drawing.Color]::Transparent)
    $background=[System.Drawing.SolidBrush]::new([System.Drawing.ColorTranslator]::FromHtml('#13283e'))
    $path=[System.Drawing.Drawing2D.GraphicsPath]::new()
    $path.AddArc(8,8,56,56,180,90);$path.AddArc(192,8,56,56,270,90)
    $path.AddArc(192,192,56,56,0,90);$path.AddArc(8,192,56,56,90,90);$path.CloseFigure()
    $graphics.FillPath($background,$path)
    $sun=[System.Drawing.SolidBrush]::new([System.Drawing.ColorTranslator]::FromHtml('#ffb568'))
    $graphics.FillEllipse($sun,160,36,52,52)
    $city=[System.Drawing.SolidBrush]::new([System.Drawing.ColorTranslator]::FromHtml('#34cfbc'))
    $graphics.FillRectangle($city,36,113,46,83)
    $graphics.FillRectangle($city,91,66,47,130)
    $graphics.FillRectangle($city,147,104,35,92)
    $graphics.FillRectangle($city,191,137,29,59)
    $graphics.FillRectangle($city,111,48,7,23)
    $windows=[System.Drawing.SolidBrush]::new([System.Drawing.ColorTranslator]::FromHtml('#13283e'))
    if($size -ge 24){
        foreach($x in @(47,65,101,120,157,170,200)){
            $top=if($x -lt 90){126}elseif($x -lt 140){82}elseif($x -lt 190){117}else{150}
            for($y=$top;$y -lt 181;$y+=22){$graphics.FillRectangle($windows,$x,$y,7,10)}
        }
    }
    $graphics.FillRectangle($sun,36,207,184,9)
    $graphics.FillRectangle($background,78,206,18,11)
    $graphics.FillRectangle($background,140,206,18,11)
    $stream=[System.IO.MemoryStream]::new()
    $bitmap.Save($stream,[System.Drawing.Imaging.ImageFormat]::Png)
    $images+=,@($stream.ToArray())
    if($size -eq 256){$bitmap.Save((Join-Path $OutputDirectory 'minicity.png'),[System.Drawing.Imaging.ImageFormat]::Png)}
    $stream.Dispose();$graphics.Dispose();$bitmap.Dispose()
    $path.Dispose();$background.Dispose();$sun.Dispose();$city.Dispose();$windows.Dispose()
}
$output=[System.IO.File]::Create((Join-Path $OutputDirectory 'minicity.ico'))
$writer=[System.IO.BinaryWriter]::new($output)
try{
    $writer.Write([uint16]0);$writer.Write([uint16]1);$writer.Write([uint16]$sizes.Count)
    $offset=6+16*$sizes.Count
    for($i=0;$i -lt $sizes.Count;$i++){
        $dimension=if($sizes[$i] -eq 256){0}else{$sizes[$i]}
        $writer.Write([byte]$dimension);$writer.Write([byte]$dimension)
        $writer.Write([byte]0);$writer.Write([byte]0)
        $writer.Write([uint16]1);$writer.Write([uint16]32)
        $writer.Write([uint32]$images[$i].Length);$writer.Write([uint32]$offset)
        $offset+=$images[$i].Length
    }
    foreach($bytes in $images){$writer.Write([byte[]]$bytes)}
}finally{$writer.Dispose();$output.Dispose()}
Write-Host "Created Mini City 3D icon ($($sizes -join ', ') px) in $OutputDirectory"
