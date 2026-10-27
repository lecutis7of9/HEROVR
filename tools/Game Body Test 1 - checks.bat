@echo off
rem HERO VR 0.0.72 GAME BODY (EXPERIMENTAL), test 1: the read-only checks. Nothing is injected or hooked.
rem Start Spider-Man 2 from Steam (not through HERO VR), load into free roam, then double-click this file.
"%~dp0..\python\python.exe" -B -X utf8 "%~dp0probe_game_body.py" --phases find
echo.
echo Done. Send the FIND line above and the file reports\body-probe.json.
pause
