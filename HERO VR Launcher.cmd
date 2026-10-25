@echo off
rem HERO VR: starts the HERO VR launcher (tools\launcher\SM2VRLauncher.exe).
rem If the exe is missing, it builds the launcher from tools\launcher\src first (needs only Windows).
cd /d "%~dp0"
set "EXE=tools\launcher\SM2VRLauncher.exe"
if not exist "%EXE%" set "EXE=tools\launcher\out\SM2VRLauncher.exe"
if not exist "%EXE%" powershell -NoProfile -ExecutionPolicy Bypass -File "tools\launcher\build.ps1" || (pause & exit /b 1)
start "" "%EXE%"
