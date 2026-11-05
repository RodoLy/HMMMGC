@echo off
rem Starts the game with the keyboard, like Play-Keyboard.bat, and writes how fast it runs to
rem Logs\speed.log in the player's folder (%LOCALAPPDATA%\HarvestMoonMM). Close the game to stop.
rem Run Play-Keyboard.bat once first, so that the game is prepared. All the work is done by
rem Measure-Speed.ps1. See README.md (English and Spanish).
rem
rem Arranca el juego con el teclado, como Play-Keyboard.bat, y anota que tan rapido corre en
rem Logs\speed.log, en la carpeta del jugador (%LOCALAPPDATA%\HarvestMoonMM). Cierra el juego para
rem terminar. Ejecuta Play-Keyboard.bat una vez antes, para que el juego este preparado. Todo lo
rem hace Measure-Speed.ps1. Ver README.md (ingles y espanol).
rem
rem -ExecutionPolicy Bypass applies to this one run only, so that Windows does not refuse a
rem script that came in a download. No setting of the computer is changed.

"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "%~dp0Measure-Speed.ps1"
echo.
pause
