#Requires -Version 5.1
<#
.SYNOPSIS
  Regenerate Gazer icon tiles and the site wordmark from the brand SVGs.

.DESCRIPTION
  App tiles are a white mark on a black square:
    resources/icons/gazer-{16,24,32,48,64,128,256,512}.png
    resources/icons/gazer.png (1024 master)
    resources/icons/gazer.ico
  The site name image is a white wordmark on transparency:
    website/docs/assets/wordmark.png

  Source vectors are resources/icons/gazerIcon.svg and gazerWord.svg.
  Rasterizes with Inkscape when it is installed, otherwise with Node
  and @resvg/resvg-js (installed under %TEMP%\gazer-resvg on first use).

.EXAMPLE
  .\scripts\generate-icons.ps1
  .\scripts\generate-icons.ps1 -Source .\resources\icons\gazerIcon.svg
#>
[CmdletBinding()]
param(
    [string]$Source = "",
    [string]$Wordmark = "",
    [string]$OutDir = "",
    [string]$WordmarkOut = "",
    [double]$Pad = 0.08
)

$ErrorActionPreference = "Stop"
$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
if (-not $OutDir) { $OutDir = Join-Path $RepoRoot "resources\icons" }
if (-not $WordmarkOut) {
    $WordmarkOut = Join-Path $RepoRoot "website\docs\assets\wordmark.png"
}
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path $WordmarkOut) | Out-Null

if (-not $Source) {
    $candidates = @(
        (Join-Path $OutDir "gazerIcon.svg"),
        (Join-Path $OutDir "gazer.svg"),
        (Join-Path $OutDir "gazer.png"),
        (Join-Path $env:USERPROFILE "Downloads\icons\gazerIcon.svg"),
        (Join-Path $env:USERPROFILE "Downloads\gazer.svg"),
        (Join-Path $env:USERPROFILE "Downloads\gazer.png")
    )
    foreach ($c in $candidates) {
        if (Test-Path -LiteralPath $c) { $Source = $c; break }
    }
}
if (-not $Source -or -not (Test-Path -LiteralPath $Source)) {
    throw "Source SVG/PNG not found. Pass -Source path\to\gazerIcon.svg"
}
$Source = (Resolve-Path -LiteralPath $Source).Path

if (-not $Wordmark) {
    $wordCandidate = Join-Path $OutDir "gazerWord.svg"
    if (Test-Path -LiteralPath $wordCandidate) { $Wordmark = $wordCandidate }
}
if ($Wordmark -and (Test-Path -LiteralPath $Wordmark)) {
    $Wordmark = (Resolve-Path -LiteralPath $Wordmark).Path
} else {
    $Wordmark = ""
}

Write-Host "Source: $Source"
Write-Host "OutDir: $OutDir"
if ($Wordmark) { Write-Host "Wordmark: $Wordmark" }

Add-Type -AssemblyName System.Drawing

if (-not ("GazerBrandRaster" -as [type])) {
    Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @"
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

public static class GazerBrandRaster
{
    public static void PasteWhiteOnBlackSquare(string srcPath, string destPath, int size)
    {
        using (Bitmap src = new Bitmap(srcPath))
        {
            int ox = (size - src.Width) / 2;
            int oy = (size - src.Height) / 2;
            BlitWhiteOnBlack(src, destPath, size, ox, oy);
        }
    }

    public static void SaveWhiteTransparent(string srcPath, string destPath, double padFraction)
    {
        using (Bitmap src = new Bitmap(srcPath))
        {
            int padX = (int)Math.Round(src.Width * padFraction);
            int padY = (int)Math.Round(src.Height * padFraction);
            int w = src.Width + padX * 2;
            int h = src.Height + padY * 2;
            using (Bitmap dst = new Bitmap(w, h, PixelFormat.Format32bppArgb))
            {
                Rectangle rect = new Rectangle(0, 0, w, h);
                BitmapData data = dst.LockBits(rect, ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
                int stride = data.Stride;
                byte[] buf = new byte[stride * h];
                byte[] sbuf = ReadBgra(src);
                int sstride = sbuf.Length / src.Height;
                for (int y = 0; y < src.Height; y++)
                {
                    for (int x = 0; x < src.Width; x++)
                    {
                        int si = y * sstride + x * 4;
                        byte a = sbuf[si + 3];
                        int di = (y + padY) * stride + (x + padX) * 4;
                        buf[di] = 255;
                        buf[di + 1] = 255;
                        buf[di + 2] = 255;
                        buf[di + 3] = a;
                    }
                }
                Marshal.Copy(buf, 0, data.Scan0, buf.Length);
                dst.UnlockBits(data);
                dst.Save(destPath, ImageFormat.Png);
            }
        }
    }

    static void BlitWhiteOnBlack(Bitmap src, string destPath, int size, int ox, int oy)
    {
        using (Bitmap dst = new Bitmap(size, size, PixelFormat.Format32bppArgb))
        {
            Rectangle rect = new Rectangle(0, 0, size, size);
            BitmapData data = dst.LockBits(rect, ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
            int stride = data.Stride;
            byte[] buf = new byte[stride * size];
            for (int y = 0; y < size; y++)
            {
                for (int x = 0; x < size; x++)
                {
                    int di = y * stride + x * 4;
                    buf[di] = 0;
                    buf[di + 1] = 0;
                    buf[di + 2] = 0;
                    buf[di + 3] = 255;
                }
            }
            byte[] sbuf = ReadBgra(src);
            int sstride = sbuf.Length / src.Height;
            for (int y = 0; y < src.Height; y++)
            {
                int dy = oy + y;
                if (dy < 0 || dy >= size) continue;
                for (int x = 0; x < src.Width; x++)
                {
                    int dx = ox + x;
                    if (dx < 0 || dx >= size) continue;
                    int si = y * sstride + x * 4;
                    byte a = sbuf[si + 3];
                    int di = dy * stride + dx * 4;
                    buf[di] = a;
                    buf[di + 1] = a;
                    buf[di + 2] = a;
                    buf[di + 3] = 255;
                }
            }
            Marshal.Copy(buf, 0, data.Scan0, buf.Length);
            dst.UnlockBits(data);
            dst.Save(destPath, ImageFormat.Png);
        }
    }

    static byte[] ReadBgra(Bitmap src)
    {
        Rectangle rect = new Rectangle(0, 0, src.Width, src.Height);
        BitmapData data = src.LockBits(rect, ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
        int stride = data.Stride;
        byte[] buf = new byte[stride * src.Height];
        Marshal.Copy(data.Scan0, buf, 0, buf.Length);
        src.UnlockBits(data);
        return buf;
    }
}
"@
}

function Find-Inkscape {
    $candidates = @(
        "C:\Program Files\Inkscape\bin\inkscape.exe",
        "C:\Program Files\Inkscape\inkscape.exe"
    )
    foreach ($c in $candidates) {
        if (Test-Path -LiteralPath $c) { return $c }
    }
    $cmd = Get-Command inkscape -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    return $null
}

function Ensure-ResvgRoot {
    $root = Join-Path $env:TEMP "gazer-resvg"
    $pkg = Join-Path $root "node_modules\@resvg\resvg-js\package.json"
    if (Test-Path -LiteralPath $pkg) { return $root }
    $npm = Get-Command npm.cmd -ErrorAction SilentlyContinue
    if (-not $npm) { return $null }
    New-Item -ItemType Directory -Force -Path $root | Out-Null
    & $npm.Source install --prefix $root "@resvg/resvg-js" --no-fund --no-audit
    if (Test-Path -LiteralPath $pkg) { return $root }
    return $null
}

function Export-SvgPng {
    param([string]$Svg, [string]$Png, [int]$Width)
    $ink = Find-Inkscape
    if ($ink) {
        & $ink --export-type=png --export-filename="$Png" --export-width=$Width --export-background-opacity=0 "$Svg"
        if (-not (Test-Path -LiteralPath $Png)) { throw "Inkscape failed to export $Png" }
        return
    }
    $node = Get-Command node -ErrorAction SilentlyContinue
    $resvgRoot = Ensure-ResvgRoot
    if (-not $node -or -not $resvgRoot) {
        throw "Rasterizing SVG needs Inkscape, or Node with npm so @resvg/resvg-js can be installed."
    }
    $js = Join-Path $env:TEMP "gazer-rasterize-svg.mjs"
    @'
import { createRequire } from "module";
import { writeFileSync, readFileSync } from "fs";
const require = createRequire(import.meta.url);
const { Resvg } = require(process.env.RESVG_ROOT + "/node_modules/@resvg/resvg-js");
const svg = readFileSync(process.argv[2]);
const width = Number(process.argv[4]);
const rendered = new Resvg(svg, {
  fitTo: { mode: "width", value: width },
  background: "rgba(0,0,0,0)",
}).render();
writeFileSync(process.argv[3], rendered.asPng());
console.log(rendered.width + "x" + rendered.height);
'@ | Set-Content -LiteralPath $js -Encoding utf8
    $env:RESVG_ROOT = $resvgRoot
    & $node.Source $js $Svg $Png "$Width"
    if (-not (Test-Path -LiteralPath $Png)) { throw "resvg failed to export $Png" }
}

function Save-PngSize([System.Drawing.Image]$src, [int]$size, [string]$path) {
    $bmp = New-Object System.Drawing.Bitmap $size, $size
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.Clear([System.Drawing.Color]::Black)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
    $g.DrawImage($src, 0, 0, $size, $size)
    $g.Dispose()
    $ms = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    [System.IO.File]::WriteAllBytes($path, $ms.ToArray())
    $ms.Dispose(); $bmp.Dispose()
}

$ext = [System.IO.Path]::GetExtension($Source).ToLowerInvariant()
$masterPath = Join-Path $OutDir "gazer.png"
$scratch = Join-Path $env:TEMP ("gazer-icon-src-" + [guid]::NewGuid().ToString("N") + ".png")

if ($ext -eq ".svg") {
    $svgDest = Join-Path $OutDir "gazer.svg"
    $srcFull = [System.IO.Path]::GetFullPath($Source)
    $destFull = [System.IO.Path]::GetFullPath($svgDest)
    if ($srcFull -ne $destFull) {
        Copy-Item -LiteralPath $Source -Destination $svgDest -Force
    }
    # Render the long side near the padded inner box so the recolor step barely scales.
    # Rasterize the long side to the padded inner box, then center it. Avoid a second scale.
    $inner = [int][Math]::Round(1024 * (1.0 - 2.0 * $Pad))
    if ($inner -lt 64) { $inner = 64 }
    Export-SvgPng -Svg $Source -Png $scratch -Width $inner
    [GazerBrandRaster]::PasteWhiteOnBlackSquare($scratch, $masterPath, 1024)
    Remove-Item -LiteralPath $scratch -Force -ErrorAction SilentlyContinue
} elseif ($ext -eq ".png" -or $ext -eq ".jpg" -or $ext -eq ".jpeg") {
    $srcImg = [System.Drawing.Image]::FromFile($Source)
    try {
        $side = [Math]::Max($srcImg.Width, $srcImg.Height)
        $side = [Math]::Max($side, 512)
        $bmp = New-Object System.Drawing.Bitmap $side, $side
        $g = [System.Drawing.Graphics]::FromImage($bmp)
        $g.Clear([System.Drawing.Color]::Transparent)
        $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
        $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $scale = [Math]::Min($side / [double]$srcImg.Width, $side / [double]$srcImg.Height)
        $nw = [int]($srcImg.Width * $scale)
        $nh = [int]($srcImg.Height * $scale)
        $x = [int](($side - $nw) / 2)
        $y = [int](($side - $nh) / 2)
        $g.DrawImage($srcImg, $x, $y, $nw, $nh)
        $g.Dispose()
        $ms = New-Object System.IO.MemoryStream
        $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
        [System.IO.File]::WriteAllBytes($masterPath, $ms.ToArray())
        $ms.Dispose(); $bmp.Dispose()
    } finally {
        $srcImg.Dispose()
    }
} else {
    throw "Unsupported source type: $ext (use .svg or .png)"
}

$masterImg = [System.Drawing.Image]::FromFile((Resolve-Path -LiteralPath $masterPath))
try {
    foreach ($s in @(16, 24, 32, 48, 64, 128, 256, 512)) {
        $out = Join-Path $OutDir "gazer-$s.png"
        Save-PngSize $masterImg $s $out
        Write-Host ("  gazer-{0}.png ({1} bytes)" -f $s, (Get-Item -LiteralPath $out).Length)
    }
} finally {
    $masterImg.Dispose()
}
Write-Host ("  gazer.png ({0} bytes)" -f (Get-Item -LiteralPath $masterPath).Length)

function New-MultiSizeIco {
    param([string[]]$PngPaths, [string]$OutPath)
    $pngs = foreach ($p in $PngPaths) {
        $bytes = [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $p))
        $w = ([int]$bytes[16] -shl 24) -bor ([int]$bytes[17] -shl 16) -bor ([int]$bytes[18] -shl 8) -bor [int]$bytes[19]
        $h = ([int]$bytes[20] -shl 24) -bor ([int]$bytes[21] -shl 16) -bor ([int]$bytes[22] -shl 8) -bor [int]$bytes[23]
        [pscustomobject]@{ Bytes = $bytes; W = $w; H = $h }
    }
    $count = $pngs.Count
    $offset = 6 + (16 * $count)
    $ms = New-Object System.IO.MemoryStream
    $bw = New-Object System.IO.BinaryWriter $ms
    $bw.Write([uint16]0)
    $bw.Write([uint16]1)
    $bw.Write([uint16]$count)
    foreach ($img in $pngs) {
        $bw.Write([byte]$(if ($img.W -ge 256) { 0 } else { $img.W }))
        $bw.Write([byte]$(if ($img.H -ge 256) { 0 } else { $img.H }))
        $bw.Write([byte]0)
        $bw.Write([byte]0)
        $bw.Write([uint16]1)
        $bw.Write([uint16]32)
        $bw.Write([uint32]$img.Bytes.Length)
        $bw.Write([uint32]$offset)
        $offset += $img.Bytes.Length
    }
    foreach ($img in $pngs) { $bw.Write($img.Bytes) }
    $bw.Flush()
    [System.IO.File]::WriteAllBytes($OutPath, $ms.ToArray())
    $bw.Dispose(); $ms.Dispose()
}

$icoPngs = @(16, 24, 32, 48, 64, 128, 256) | ForEach-Object { Join-Path $OutDir "gazer-$_.png" }
$icoPath = Join-Path $OutDir "gazer.ico"
New-MultiSizeIco -PngPaths $icoPngs -OutPath $icoPath
Write-Host ("  gazer.ico ({0} bytes)" -f (Get-Item -LiteralPath $icoPath).Length)

if ($Wordmark) {
    $wordScratch = Join-Path $env:TEMP ("gazer-word-src-" + [guid]::NewGuid().ToString("N") + ".png")
    Export-SvgPng -Svg $Wordmark -Png $wordScratch -Width 3200
    [GazerBrandRaster]::SaveWhiteTransparent($wordScratch, $WordmarkOut, 0.025)
    Remove-Item -LiteralPath $wordScratch -Force -ErrorAction SilentlyContinue
    Write-Host ("  wordmark.png ({0} bytes)" -f (Get-Item -LiteralPath $WordmarkOut).Length)
}

Write-Host "Done." -ForegroundColor Green
