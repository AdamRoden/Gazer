@echo off
rem Deferred MSI action (SYSTEM): trust a self-signed Authenticode cert so
rem uiAccess=true Gazer.exe can start from Program Files. No-op when the
rem public cert was not staged (publicly trusted PFX builds).
set "CER=%~dp0gazer-codesign.cer"
if not exist "%CER%" exit /b 0
"%SystemRoot%\System32\certutil.exe" -addstore -f Root "%CER%"
if errorlevel 1 exit /b 1
"%SystemRoot%\System32\certutil.exe" -addstore -f TrustedPublisher "%CER%"
if errorlevel 1 exit /b 1
exit /b 0
