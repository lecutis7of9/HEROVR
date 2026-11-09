@echo off
setlocal
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-Private-Launcher.ps1"
if errorlevel 1 (
    echo.
    echo Installation did not complete. Your original files were backed up before any replacements.
    pause
    exit /b 1
)
echo.
echo The private launcher should now be open.
pause
