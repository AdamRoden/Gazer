@echo off
setlocal
rem Installs an on-demand SYSTEM task that runs GazerUpdate.exe --system-apply.
rem The task is not a user-writable script. Standard users may run it and may
rem not change it. A missing pending MSI makes the helper exit without installing.
if /I "%~1"=="uninstall" goto uninstall

set "DIR=%ProgramData%\Gazer\update"
if not exist "%DIR%" mkdir "%DIR%"
icacls "%DIR%" /inheritance:r /grant:r *S-1-5-18:(OI)(CI)F *S-1-5-32-544:(OI)(CI)F *S-1-5-32-545:(OI)(CI)M
if errorlevel 1 exit /b 1

rem Far-future ONCE keeps the task from expiring and from firing on a schedule.
rem schtasks /Run still starts it. The helper no-ops when pending.msi is absent.
schtasks /Create /F /TN "Gazer\ApplyUpdate" /TR "\"%~dp0GazerUpdate.exe\" --system-apply" /SC ONCE /ST 00:00 /SD 01/01/2099 /RU SYSTEM /RL HIGHEST
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -NonInteractive -Command "$s = New-Object -ComObject Schedule.Service; $s.Connect(); $f = $s.GetFolder('\Gazer'); $d = 'D:(A;;FA;;;BA)(A;;FA;;;SY)(A;;0x1200A9;;;AU)(A;;0x1200A9;;;BU)'; $f.SetSecurityDescriptor($d, 0); $t = $f.GetTask('ApplyUpdate'); $t.SetSecurityDescriptor($d, 0)"
if errorlevel 1 exit /b 1
exit /b 0

:uninstall
schtasks /Delete /F /TN "Gazer\ApplyUpdate" >nul 2>&1
exit /b 0
