@echo off
rem HERO VR - UA expansion: the check that unlocks the REAL Spider-Man 2 body in Stark Tower.
rem Read-only. It looks inside your own installed game's suit .model files (names and sizes only, no geometry is copied)
rem and writes sm2model.json beside this file. Send that file back.
rem Needs: Python 3 (py or python on PATH) and the game installed. Another Steam folder: SM2-BODY-CHECK.cmd "D:\...\Marvel's Spider-Man 2"
setlocal
set "GAME=%~1"
if "%GAME%"=="" set "GAME=C:\Program Files (x86)\Steam\steamapps\common\Marvel's Spider-Man 2"
if not exist "%GAME%" (
  echo The game folder was not found: %GAME%
  echo Run again with the folder in quotes as the first argument.
  pause
  exit /b 1
)
set "PY=py"
where py >nul 2>nul || set "PY=python"
"%PY%" "%~dp0sm2model.py" "%GAME%" "%~dp0sm2model.json" --all
echo.
echo Done: %~dp0sm2model.json  - send this file back.
pause
