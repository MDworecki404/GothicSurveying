@echo off
where pwsh.exe >nul 2>&1
if %errorlevel%==0 (
  pwsh.exe -NoProfile -File "%~dp0Convert-Gothic-Names.ps1" -Filename %1 -OutputFilename %2
) else (
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Convert-Gothic-Names.ps1" -Filename %1 -OutputFilename %2
)
