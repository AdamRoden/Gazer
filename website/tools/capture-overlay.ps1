# Rasterize overlay-scene.html into website/docs/assets/overlay.png
$repo = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$edge = "C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"
$html = Join-Path $PSScriptRoot "overlay-scene.html"
$out = Join-Path $repo "website\docs\assets\overlay.png"
$profile = Join-Path $PSScriptRoot ".edge-profile"
$uri = ([Uri](Resolve-Path $html).Path).AbsoluteUri
New-Item -ItemType Directory -Force -Path $profile | Out-Null
& $edge --headless=new --user-data-dir="$profile" --disable-gpu --allow-file-access-from-files --hide-scrollbars --force-device-scale-factor=1 --window-size=1600,900 "--screenshot=$out" $uri
Write-Host "Wrote $out"
