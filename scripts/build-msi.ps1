#Requires -Version 5.1
<#
.SYNOPSIS
  Build a Gazer beta MSI for Windows.

.DESCRIPTION
  1. Configures/builds the CMake project (MinGW + Ninja by default).
  2. Stages a clean runtime tree (exe + windeployqt + resources + MinGW runtime + Tobii DLL).
  3. Builds an MSI with WiX Toolset CLI v7.

.PARAMETER Version
  MSI ProductVersion (major.minor.patch, each 0-65535). Default: from CMakeLists or 0.5.0

.PARAMETER Configuration
  CMake build type. Default: Release

.PARAMETER SkipBuild
  Reuse existing build/Gazer.exe (still re-stages and rebuilds MSI).

.PARAMETER OutDir
  Output directory for the MSI. Default: dist/

.EXAMPLE
  .\scripts\build-msi.ps1
  .\scripts\build-msi.ps1 -Version 0.5.1 -SkipBuild
#>
[CmdletBinding()]
param(
    [string]$Version = "",
    [string]$Configuration = "Release",
    [switch]$SkipBuild,
    [string]$OutDir = ""
)

$ErrorActionPreference = "Stop"
$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
Set-Location $RepoRoot

function Find-Tool([string[]]$Candidates, [string]$Name) {
    foreach ($c in $Candidates) {
        if ($c -and (Test-Path $c)) { return (Resolve-Path $c).Path }
    }
    $cmd = Get-Command $Name -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    return $null
}

# --- Tool discovery -----------------------------------------------------------
$Cmake = Find-Tool @(
    "C:\Qt\Tools\CMake_64\bin\cmake.exe",
    "C:\Program Files\CMake\bin\cmake.exe"
) "cmake"

# Prefer Ninja 1.13+ (VS 18). Qt Tools ships 1.12.1, which cannot read the v7
# .ninja_log that 1.13 writes ("build log version is too new; starting over").
$Ninja = Find-Tool @(
    "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe",
    "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe",
    "C:\Qt\Tools\Ninja\ninja.exe"
) "ninja"

$Wix = Find-Tool @(
    "C:\Program Files\WiX Toolset v7.0\bin\wix.exe",
    "C:\Program Files\WiX Toolset v6.0\bin\wix.exe"
) "wix"

$Windeployqt = Find-Tool @(
    "C:\Qt\6.11.1\mingw_64\bin\windeployqt.exe",
    "C:\Qt\6.10.0\mingw_64\bin\windeployqt.exe",
    "C:\Qt\6.9.0\mingw_64\bin\windeployqt.exe"
) "windeployqt"

$MingwBin = $null
if ($Windeployqt) {
    $MingwBin = Split-Path $Windeployqt -Parent
}

if (-not $Cmake) { throw "cmake not found. Install Qt Tools CMake or add cmake to PATH." }
if (-not $Wix) { throw "WiX CLI not found. Install: winget install WiXToolset.WiXCLI" }
if (-not $Windeployqt) { throw "windeployqt not found under C:\Qt\...\mingw_64\bin" }

# Version from CMakeLists if not supplied
if (-not $Version) {
    $cm = Get-Content (Join-Path $RepoRoot "CMakeLists.txt") -Raw
    if ($cm -match 'project\s*\(\s*Gazer\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)') {
        $Version = $Matches[1]
    } else {
        $Version = "0.5.0"
    }
}
if ($Version -notmatch '^\d+\.\d+\.\d+(\.\d+)?$') {
    throw "Version must be major.minor.patch (MSI ProductVersion). Got: $Version"
}

$BuildDir = Join-Path $RepoRoot "build"
$StageDir = Join-Path $RepoRoot "dist\stage"
$DistDir = if ($OutDir) { $OutDir } else { Join-Path $RepoRoot "dist" }
$MsiName = "Gazer-$Version-beta.msi"
$MsiPath = Join-Path $DistDir $MsiName
$PackagingDir = Join-Path $RepoRoot "packaging"

Write-Host "==> Gazer MSI build" -ForegroundColor Cyan
Write-Host "    Version : $Version"
Write-Host "    Stage   : $StageDir"
Write-Host "    Output  : $MsiPath"

# --- Build --------------------------------------------------------------------
if (-not $SkipBuild) {
    if (-not (Test-Path (Join-Path $BuildDir "build.ninja")) -and -not (Test-Path (Join-Path $BuildDir "CMakeCache.txt"))) {
        Write-Host "==> Configuring CMake..." -ForegroundColor Cyan
        New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
        $cfgArgs = @(
            "-S", $RepoRoot, "-B", $BuildDir,
            "-DCMAKE_BUILD_TYPE=$Configuration"
        )
        if ($Ninja) {
            $cfgArgs += @("-G", "Ninja", "-DCMAKE_MAKE_PROGRAM=$Ninja")
        }
        $QtMingw = "C:\Qt\6.11.1\mingw_64"
        if (Test-Path $QtMingw) {
            $cfgArgs += "-DCMAKE_PREFIX_PATH=$QtMingw"
        }
        & $Cmake @cfgArgs
        if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }
    }

    Write-Host "==> Building Gazer..." -ForegroundColor Cyan
    & $Cmake --build $BuildDir --config $Configuration
    if ($LASTEXITCODE -ne 0) { throw "Build failed" }
}

$Exe = Join-Path $BuildDir "Gazer.exe"
if (-not (Test-Path $Exe)) {
    throw "Gazer.exe not found at $Exe - build first or omit -SkipBuild"
}

# --- Stage clean payload ------------------------------------------------------
Write-Host "==> Staging payload..." -ForegroundColor Cyan
if (Test-Path $StageDir) {
    Remove-Item -Recurse -Force $StageDir
}
New-Item -ItemType Directory -Force -Path $StageDir | Out-Null

Copy-Item $Exe $StageDir

$ResSrc = Join-Path $RepoRoot "resources"
if (-not (Test-Path $ResSrc)) { throw "resources/ missing" }
Copy-Item -Recurse $ResSrc (Join-Path $StageDir "resources")

Write-Host "==> windeployqt..." -ForegroundColor Cyan
# Skip debug QML tooling plugins for a leaner beta MSI. Native stderr warnings
# (e.g. missing dxcompiler) must not abort the script under ErrorAction Stop.
$prevEap = $ErrorActionPreference
$ErrorActionPreference = "Continue"
& $Windeployqt --release --no-translations --no-compiler-runtime `
    --skip-plugin-types qmltooling `
    --dir $StageDir (Join-Path $StageDir "Gazer.exe") 2>&1 | ForEach-Object { Write-Host $_ }
$wdExit = $LASTEXITCODE
$ErrorActionPreference = $prevEap
if ($wdExit -ne 0) { throw "windeployqt failed (exit $wdExit)" }

if ($MingwBin) {
    foreach ($dll in @("libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll")) {
        $src = Join-Path $MingwBin $dll
        if (Test-Path $src) {
            Copy-Item $src $StageDir -Force
        }
    }
}

# Tobii Stream Engine (optional - mouse tracker still works without it)
$TobiiCandidates = @(
    (Join-Path $BuildDir "tobii_stream_engine.dll"),
    "$env:ProgramFiles\Tobii\Tobii EyeX\tobii_stream_engine.dll",
    "C:\Program Files\Tobii\Tobii EyeX\tobii_stream_engine.dll"
)
$TobiiFound = $false
foreach ($t in $TobiiCandidates) {
    if ($t -and (Test-Path $t)) {
        Copy-Item $t (Join-Path $StageDir "tobii_stream_engine.dll") -Force
        Write-Host "    Included tobii_stream_engine.dll from $t"
        $TobiiFound = $true
        break
    }
}
if (-not $TobiiFound) {
    Write-Warning "tobii_stream_engine.dll not found - MSI will use mouse tracker only until Tobii is present."
}

# Strip build artifacts only. Do NOT remove resources/models/*.obj — those are
# Wavefront mesh assets (head preview), not compiler objects.
Get-ChildItem $StageDir -Recurse -Include *.pdb,*.ilk,*.exp,*.lib,gazer.log,gazer_*.log -ErrorAction SilentlyContinue |
    Remove-Item -Force -ErrorAction SilentlyContinue
# Compiler .obj only at stage root (next to Gazer.exe), never under resources/
Get-ChildItem $StageDir -File -Filter *.obj -ErrorAction SilentlyContinue |
    Remove-Item -Force -ErrorAction SilentlyContinue

if (-not (Test-Path (Join-Path $StageDir "platforms\qwindows.dll"))) {
    throw "Staging incomplete: platforms\qwindows.dll missing after windeployqt"
}

$fileCount = (Get-ChildItem $StageDir -Recurse -File).Count
$stageBytes = (Get-ChildItem $StageDir -Recurse -File | Measure-Object -Property Length -Sum).Sum
Write-Host ("    Staged {0} files ({1:N1} MB)" -f $fileCount, ($stageBytes / 1MB))

# --- WiX MSI ------------------------------------------------------------------
Write-Host "==> Building MSI (WiX)..." -ForegroundColor Cyan
New-Item -ItemType Directory -Force -Path $DistDir | Out-Null

& $Wix extension add WixToolset.UI.wixext 2>$null | Out-Null

$wxs = Join-Path $PackagingDir "Gazer.wxs"
if (-not (Test-Path $wxs)) { throw "Missing $wxs" }

$wixArgs = @(
    "build",
    "-acceptEula", "wix7",
    "-ext", "WixToolset.UI.wixext",
    "-bindpath", "Payload=$StageDir",
    "-bindpath", "Packaging=$PackagingDir",
    "-d", "Version=$Version",
    "-arch", "x64",
    "-o", $MsiPath,
    $wxs
)

& $Wix @wixArgs
if ($LASTEXITCODE -ne 0) { throw "wix build failed (exit $LASTEXITCODE)" }

if (-not (Test-Path $MsiPath)) { throw "MSI was not produced: $MsiPath" }

$msiSize = (Get-Item $MsiPath).Length
Write-Host ""
Write-Host "==> Done" -ForegroundColor Green
Write-Host ("    MSI : {0} ({1:N1} MB)" -f $MsiPath, ($msiSize / 1MB))
Write-Host "    Install: msiexec /i `"$MsiPath`""
Write-Host "    Quiet  : msiexec /i `"$MsiPath`" /qn"
if (-not $TobiiFound) {
    Write-Host "    Note  : Tobii DLL was not bundled; testers need Tobii drivers + DLL for eye tracking." -ForegroundColor Yellow
}

return $MsiPath
