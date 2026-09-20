# Drive Gazer Play tour while GAZER_SPLASH_RECORD dumps JPEG frames, then encode splash.mp4.
$ErrorActionPreference = "Stop"
$repo = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$gazer = Join-Path $repo "build\Gazer.exe"
$ff = "C:\Users\adamr\Downloads\ffmpeg-master-latest-win64-gpl\ffmpeg-master-latest-win64-gpl\bin\ffmpeg.exe"
$frames = Join-Path $env:TEMP "gazer-splash-frames"
$out = Join-Path $repo "website\docs\assets\splash.mp4"
if (-not (Test-Path $gazer)) { throw "Missing $gazer" }
if (-not (Test-Path $ff)) { throw "Missing $ff" }
if (-not $env:GAZER_SPLASH_RECORD) {
    throw "Start Gazer.exe with GAZER_SPLASH_RECORD set to a frame directory"
}

Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class SplashNative {
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern int GetSystemMetrics(int nIndex);
}
"@
[SplashNative]::SetProcessDPIAware() | Out-Null
$w = [SplashNative]::GetSystemMetrics(0)
$h = [SplashNative]::GetSystemMetrics(1)
$nextX = [int]($w / 2)
$nextY = [int]($h / 2)

function Park { [SplashNative]::SetCursorPos(80, 80) | Out-Null }
function Dwell-Next {
    [SplashNative]::SetCursorPos($nextX, $nextY) | Out-Null
    Start-Sleep -Milliseconds 2500
    Park
}

if (Test-Path $env:GAZER_SPLASH_RECORD) {
    Remove-Item (Join-Path $env:GAZER_SPLASH_RECORD "f-*.jpg") -ErrorAction SilentlyContinue
}

Park
& $gazer --action command=settings.session.showSplash.play | Out-Null
# GAZER_SPLASH_AUTO holds each step; 6 steps * ~2.2s + intros + fade.
Start-Sleep -Milliseconds 22000

$n = @(Get-ChildItem $env:GAZER_SPLASH_RECORD -Filter "f-*.jpg").Count
if ($n -lt 20) { throw "Too few splash frames: $n in $($env:GAZER_SPLASH_RECORD)" }
& $ff -y -hide_banner -loglevel error -framerate 10 -i (Join-Path $env:GAZER_SPLASH_RECORD "f-%05d.jpg") `
    -vf "crop=1920:1080:(iw-1920)/2:(ih-1080)/2,format=yuv420p" -an -c:v libx264 -profile:v high -pix_fmt yuv420p -crf 20 -movflags +faststart $out
if ($LASTEXITCODE -ne 0) { throw "ffmpeg encode failed" }
Write-Host "Wrote $out frames=$n size=$([math]::Round((Get-Item $out).Length/1MB, 2)) MB"
