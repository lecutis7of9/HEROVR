@echo off
rem HERO VR 0.0.72 PRIVATE TEST BUILD - double-click this file. It finds your HERO VR mod folder,
rem copies the test build into it, checks every file and starts that folder's own launcher.
title HERO VR 0.0.72 test build - installer
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0INSTALL-TEST-BUILD.ps1" %*
echo.
pause
