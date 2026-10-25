@echo off
rem HERO VR - Made by Agent Mulder. Starts the HERO VR launcher (tools\launcher\SM2VRLauncher.exe).
rem First run: builds the launcher from its source in tools\launcher\src with the C# compiler built into Windows
rem (.NET Framework 4.8, nothing to install), next to launcher.ini and NOTICE.
cd /d "%~dp0"
set "EXE=tools\launcher\SM2VRLauncher.exe"
if exist "%EXE%" goto run
echo Building the HERO VR launcher from source (first run, a few seconds)...
powershell -NoProfile -ExecutionPolicy Bypass -File "tools\launcher\build.ps1" -Out ".\SM2VRLauncher.exe" || (echo The launcher did not build. See the message above. & pause & exit /b 1)
:run
start "" "%EXE%"
