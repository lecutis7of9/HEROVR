@echo off
rem HERO VR 0.0.72 GAME BODY (EXPERIMENTAL), test 2: the checks, then the game's body driven by scripted hands.
rem Start Spider-Man 2 from Steam (not through HERO VR), load into free roam, then double-click this file.
rem Close the game afterwards: start it again (through HERO VR) before you play.
"%~dp0..\python\python.exe" -B -X utf8 "%~dp0probe_game_body.py" --phases find,solve
echo.
echo Done. Send the FIND and SOLVE lines above, reports\body-probe.json and reports\body-probe\eye-body.png.
echo Close the game now and start it again before you play.
pause
