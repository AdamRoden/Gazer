#Requires -Version 5.1
<#
.SYNOPSIS
  Regenerate Gazer multi-size PNG + ICO assets from the brand SVG (or a master PNG).

.DESCRIPTION
  Writes resources/icons/gazer-{16,24,32,48,64,128,256,512}.png and gazer.ico.
  Prefer an SVG path; falls back to a high-res PNG. Uses System.Drawing for resize
  and packs a Vista-style multi-size ICO (PNG frames).

.EXAMPLE
  .\scripts\generate-icons.ps1 -Source .\resources\icons\gazer.png
  .\scripts\generate-icons.ps1 -Source .\resources\icons\gazer.svg
#>
[CmdletBinding()]
param(
    [string]$Source = "",
    [string]$OutDir = ""
)

$ErrorActionPreference = "Stop"
$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
if (-not $OutDir) { $OutDir = Join-Path $RepoRoot "resources\icons" }
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

if (-not $Source) {
    $candidates = @(
        (Join-Path $OutDir "gazer.png"),
        (Join-Path $OutDir "gazer.svg"),
        (Join-Path $env:USERPROFILE "Downloads\gazer.svg"),
        (Join-Path $env:USERPROFILE "Downloads\gazer.png")
    )
    foreach ($c in $candidates) {
        if (Test-Path $c) { $Source = $c; break }
    }
}
if (-not $Source -or -not (Test-Path $Source)) {
    throw "Source SVG/PNG not found. Pass -Source path\to\gazer.svg"
}
$Source = (Resolve-Path $Source).Path
Write-Host "Source: $Source"
Write-Host "OutDir: $OutDir"

Add-Type -AssemblyName System.Drawing

$masterPath = Join-Path $OutDir "_master_tmp.png"
$ext = [System.IO.Path]::GetExtension($Source).ToLowerInvariant()

if ($ext -eq ".svg") {
    $ink = @(
        "C:\Program Files\Inkscape\bin\inkscape.exe",
        "C:\Program Files\Inkscape\inkscape.exe"
    ) | Where-Object { Test-Path $_ } | Select-Object -First 1
    if (-not $ink) {
        throw "Inkscape required to rasterize SVG. Install Inkscape or pass a PNG -Source."
    }
    Copy-Item -LiteralPath $Source -Destination (Join-Path $OutDir "gazer.svg") -Force
    & $ink --export-type=png --export-filename="$masterPath" --export-width=1024 --export-height=1024 "$Source"
    if (-not (Test-Path $masterPath)) { throw "Inkscape failed to export master PNG" }
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

function Save-PngSize([System.Drawing.Image]$src, [int]$size, [string]$path) {
    $bmp = New-Object System.Drawing.Bitmap $size, $size
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.Clear([System.Drawing.Color]::Transparent)
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

$masterImg = [System.Drawing.Image]::FromFile((Resolve-Path $masterPath))
try {
    foreach ($s in @(16, 24, 32, 48, 64, 128, 256, 512)) {
        $out = Join-Path $OutDir "gazer-$s.png"
        Save-PngSize $masterImg $s $out
        Write-Host ("  gazer-{0}.png ({1} bytes)" -f $s, (Get-Item $out).Length)
    }
} finally {
    $masterImg.Dispose()
}
Remove-Item -LiteralPath $masterPath -Force -ErrorAction SilentlyContinue

function New-MultiSizeIco {
    param([string[]]$PngPaths, [string]$OutPath)
    $pngs = foreach ($p in $PngPaths) {
        $bytes = [System.IO.File]::ReadAllBytes((Resolve-Path $p))
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
Write-Host ("  gazer.ico ({0} bytes)" -f (Get-Item $icoPath).Length)
Write-Host "Done." -ForegroundColor Green
