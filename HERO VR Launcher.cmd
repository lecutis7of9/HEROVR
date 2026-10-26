@echo off
rem HERO VR - Made by Agent Mulder. Starts the HERO VR launcher (tools\launcher\SM2VRLauncher.exe).
rem First run: builds the launcher from its source in tools\launcher\src with the C# compiler built into Windows
rem (.NET Framework 4.8, nothing to install), next to launcher.ini and NOTICE.
setlocal
cd /d "%~dp0"
set "EXE=tools\launcher\SM2VRLauncher.exe"
if exist "%EXE%" goto run
rem Double-clicked inside the zip (Explorer copies only this file to a temp folder), or a partial unzip.
if not exist "tools\launcher\build.ps1" goto notunzipped
if not exist "tools\launcher\src\Program.cs" goto notunzipped
echo Building the HERO VR launcher from source (first run, a few seconds)...
powershell -NoProfile -ExecutionPolicy Bypass -File "tools\launcher\build.ps1" -Out ".\SM2VRLauncher.exe"
if errorlevel 1 goto buildfailed
if not exist "%EXE%" goto blocked
:run
start "" "%EXE%"
if errorlevel 1 goto blocked
exit /b 0

:notunzipped
echo.
echo HERO VR is not unzipped yet, or only part of it is.
echo Right-click the HERO VR zip, choose "Extract All...", open the new HERO-VR folder it makes
echo and double-click "HERO VR Launcher.cmd" there. Do not run it from inside the zip.
echo.
pause
exit /b 1

:buildfailed
echo.
echo The launcher did not build. See the message above (it is also saved in tools\launcher\build.log).
echo Please send that file to Agent Mulder on Discord.
echo.
pause
exit /b 1

:blocked
echo.
echo Windows did not start tools\launcher\SM2VRLauncher.exe.
echo Your antivirus or Smart App Control may have blocked or removed it. Check Windows Security,
echo Protection history, then run "HERO VR Launcher.cmd" again. Details: tools\launcher\build.log
echo.
pause
exit /b 1
