#Requires -Version 5.1
<#
.SYNOPSIS
  Authenticode helpers for Gazer UIAccess.

.DESCRIPTION
  Dot-source from build-msi.ps1. UIAccess requires a signed PE in Program Files;
  this script stamps the uiAccess manifest, signs, and (for a self-signed cert)
  exports the public .cer the MSI trusts at install time.

  Optional env:
    GAZER_SIGN_PFX            path to a .pfx
    GAZER_SIGN_PFX_PASSWORD   PFX password
#>

function Find-WindowsKitTool {
    param([Parameter(Mandatory)][string]$Name)
    $cmd = Get-Command $Name -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    $roots = @(
        "${env:ProgramFiles(x86)}\Windows Kits\10\bin",
        "$env:ProgramFiles\Windows Kits\10\bin"
    )
    $hits = @()
    foreach ($root in $roots) {
        if (-not (Test-Path $root)) { continue }
        $hits += Get-ChildItem $root -Recurse -Filter $Name -ErrorAction SilentlyContinue |
            Where-Object { $_.DirectoryName -match '\\x64$' }
    }
    if (-not $hits) { return $null }
    return ($hits | Sort-Object { $_.Directory.Parent.Name } -Descending | Select-Object -First 1).FullName
}

function Get-GazerSigningStorePath {
    $dir = Join-Path $env:LOCALAPPDATA "Gazer\signing"
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    return @{
        Dir = $dir
        Pfx = Join-Path $dir "GazerCodeSign.pfx"
        Cer = Join-Path $dir "GazerCodeSign.cer"
    }
}

function Get-GazerSigningCert {
    <#
      Returns @{ Cert = X509Certificate2; SelfSigned = bool }
      Prefers GAZER_SIGN_PFX, else a persistent local code-signing cert.
    #>
    $pfxPath = $env:GAZER_SIGN_PFX
    $pfxPass = $env:GAZER_SIGN_PFX_PASSWORD
    if ($pfxPath) {
        if (-not (Test-Path $pfxPath)) {
            throw "GAZER_SIGN_PFX not found: $pfxPath"
        }
        $pass = if ($pfxPass) {
            ConvertTo-SecureString $pfxPass -AsPlainText -Force
        } else {
            New-Object Security.SecureString
        }
        $cert = New-Object System.Security.Cryptography.X509Certificates.X509Certificate2
        $flags = [Security.Cryptography.X509Certificates.X509KeyStorageFlags]::Exportable -bor
                 [Security.Cryptography.X509Certificates.X509KeyStorageFlags]::PersistKeySet
        $cert.Import((Resolve-Path $pfxPath).Path, $pass, $flags)
        if (-not $cert.HasPrivateKey) {
            throw "GAZER_SIGN_PFX has no private key: $pfxPath"
        }
        return @{ Cert = $cert; SelfSigned = ($cert.Subject -eq $cert.Issuer) }
    }

    $subject = "CN=Gazer Code Signing"
    $existing = Get-ChildItem Cert:\CurrentUser\My -ErrorAction SilentlyContinue |
        Where-Object {
            $_.Subject -eq $subject -and $_.HasPrivateKey -and $_.NotAfter -gt (Get-Date).AddDays(30)
        } |
        Sort-Object NotAfter -Descending |
        Select-Object -First 1
    if ($existing) {
        return @{ Cert = $existing; SelfSigned = $true }
    }

    $paths = Get-GazerSigningStorePath
    if (Test-Path $paths.Pfx) {
        $empty = New-Object Security.SecureString
        $cert = New-Object System.Security.Cryptography.X509Certificates.X509Certificate2
        $flags = [Security.Cryptography.X509Certificates.X509KeyStorageFlags]::Exportable -bor
                 [Security.Cryptography.X509Certificates.X509KeyStorageFlags]::PersistKeySet
        $cert.Import($paths.Pfx, $empty, $flags)
        if ($cert.HasPrivateKey -and $cert.NotAfter -gt (Get-Date).AddDays(30)) {
            $store = New-Object System.Security.Cryptography.X509Certificates.X509Store("My", "CurrentUser")
            $store.Open("ReadWrite")
            $store.Add($cert)
            $store.Close()
            return @{ Cert = $cert; SelfSigned = $true }
        }
    }

    Write-Host "    Creating self-signed Authenticode cert ($subject)"
    $cert = New-SelfSignedCertificate `
        -Type CodeSigningCert `
        -Subject $subject `
        -KeyAlgorithm RSA `
        -KeyLength 2048 `
        -HashAlgorithm SHA256 `
        -CertStoreLocation "Cert:\CurrentUser\My" `
        -KeyExportPolicy Exportable `
        -NotAfter (Get-Date).AddYears(10)
    $empty = New-Object Security.SecureString
    Export-PfxCertificate -Cert $cert -FilePath $paths.Pfx -Password $empty | Out-Null
    return @{ Cert = $cert; SelfSigned = $true }
}

function Export-GazerPublicCert {
    param(
        [Parameter(Mandatory)]$Cert,
        [Parameter(Mandatory)][string]$Path
    )
    $dir = Split-Path $Path -Parent
    if ($dir) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }
    $bytes = $Cert.Export([Security.Cryptography.X509Certificates.X509ContentType]::Cert)
    [IO.File]::WriteAllBytes($Path, $bytes)
}

function Set-GazerUiAccessManifest {
    param(
        [Parameter(Mandatory)][string]$Exe,
        [Parameter(Mandatory)][string]$TemplatePath,
        [Parameter(Mandatory)][string]$Version
    )
    if (-not (Test-Path $Exe)) { throw "Exe not found: $Exe" }
    if (-not (Test-Path $TemplatePath)) { throw "Manifest template not found: $TemplatePath" }
    if ($Version -notmatch '^\d+\.\d+\.\d+') {
        throw "Version must be major.minor.patch. Got: $Version"
    }
    $four = if ($Version -match '^\d+\.\d+\.\d+\.\d+') { $Version } else { "$Version.0" }
    $xml = [IO.File]::ReadAllText($TemplatePath)
    $xml = $xml.Replace('@GAZER_UIACCESS_XML@', 'true')
    $xml = $xml.Replace('@GAZER_VERSION_FOUR@', $four)

    $mt = Find-WindowsKitTool "mt.exe"
    if (-not $mt) {
        throw "mt.exe not found. Install the Windows 10 SDK (Windows Kits\10\bin\...\x64\mt.exe)."
    }
    $manifestFile = Join-Path ([IO.Path]::GetTempPath()) ("gazer-uiaccess-{0}.manifest" -f [guid]::NewGuid().ToString("n"))
    try {
        # UTF-8 no BOM — windres/mt both accept this.
        $utf8 = New-Object System.Text.UTF8Encoding $false
        [IO.File]::WriteAllText($manifestFile, $xml, $utf8)
        $outRes = "-outputresource:$Exe;1"
        $prev = $ErrorActionPreference
        $ErrorActionPreference = "Continue"
        & $mt -nologo -manifest $manifestFile $outRes 2>&1 | ForEach-Object { Write-Host $_ }
        $code = $LASTEXITCODE
        $ErrorActionPreference = $prev
        if ($code -ne 0) { throw "mt.exe failed (exit $code) embedding uiAccess manifest" }
    } finally {
        Remove-Item $manifestFile -Force -ErrorAction SilentlyContinue
    }
}

function Sign-GazerExecutable {
    param(
        [Parameter(Mandatory)][string]$Exe,
        [Parameter(Mandatory)]$Cert
    )
    if (-not (Test-Path $Exe)) { throw "Exe not found: $Exe" }

    $signed = $null
    try {
        $signed = Set-AuthenticodeSignature -FilePath $Exe -Certificate $Cert -HashAlgorithm SHA256 `
            -TimestampServer "http://timestamp.digicert.com"
    } catch {
        $signed = $null
    }
    if (-not $signed -or -not $signed.SignerCertificate) {
        Write-Warning "Timestamped sign failed; signing without timestamp."
        $signed = Set-AuthenticodeSignature -FilePath $Exe -Certificate $Cert -HashAlgorithm SHA256
    }
    if (-not $signed -or -not $signed.SignerCertificate) {
        throw "Authenticode signature missing on $Exe"
    }
    $note = [string]$signed.Status
    if ($note -eq "UnknownError") {
        $note = "signed (cert not in LocalMachine Root yet; MSI trusts it at install)"
    }
    Write-Host ("    Signed {0} ({1})" -f (Split-Path $Exe -Leaf), $note)
}
