@echo off
rem Opens the settings window: which mods are on, and how fast the character is. The choices are
rem saved in settings.json, in the player's folder, and Play.bat and Play-Keyboard.bat follow them
rem the next time the game is started. All the work is done by Settings.ps1.
rem See README.md (English and Spanish).
rem
rem Abre la ventana de ajustes: que mods estan activos y que tan rapido es el personaje. Las
rem elecciones se guardan en settings.json, en la carpeta del jugador, y Play.bat y Play-Keyboard.bat
rem las siguen la proxima vez que se arranca el juego. Todo lo hace Settings.ps1.
rem Ver README.md (ingles y espanol).
rem
rem -ExecutionPolicy Bypass applies to this one run only, so that Windows does not refuse a
rem script that came in a download. No setting of the computer is changed.

"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -STA -File "%~dp0Settings.ps1"
if errorlevel 1 (
    echo.
    pause
)
