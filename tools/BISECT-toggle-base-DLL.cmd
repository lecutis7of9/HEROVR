@echo off
rem HERO VR 0.0.79.1 bisect switch (t2155): makes or deletes USE-BASE-DLL.txt in the mod's top folder.
rem With it, the next launch uses the gameplay DLL built from the 0791 base commit 0e94193 (renderer ORIGINAL);
rem without it, the 79.1 web-mechanics test DLL. Close Spider-Man 2 before switching.
set "TOP=%~dp0.."
if exist "%TOP%\USE-BASE-DLL.txt" (
  del "%TOP%\USE-BASE-DLL.txt"
  echo Next launch: the 79.1 TEST gameplay DLL.
) else (
  echo Use the 0791 base 0e94193 gameplay DLL - made by tools\BISECT-toggle-base-DLL.cmd> "%TOP%\USE-BASE-DLL.txt"
  echo Next launch: the 0791 BASE 0e94193 gameplay DLL ^(renderer ORIGINAL^).
)
echo Close Spider-Man 2 if it is running, then press PLAY VR in the launcher as usual.
pause
