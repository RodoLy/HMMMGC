@echo off
rem Starts the game and plays it with the keyboard, without opening the launcher. Play.bat does
rem the same when no controller is connected, but it shows the launcher first; this file skips it.
rem The first time it asks for the disc image and prepares the game, like Play.bat does. Drop a
rem disc image on this file to use that image. All the work is done by play.ps1.
rem See README.md (English and Spanish).
rem
rem Arranca el juego y lo juega con el teclado, sin abrir el launcher. Play.bat hace lo mismo
rem cuando no hay un control conectado, pero antes muestra el launcher; este archivo se lo salta.
rem La primera vez pide la imagen del disco y prepara el juego, igual que Play.bat. Suelta una
rem imagen de disco sobre este archivo para usarla. Todo lo hace play.ps1.
rem Ver README.md (ingles y espanol).
rem
rem -ExecutionPolicy Bypass applies to this one run only, so that Windows does not refuse a
rem script that came in a download. No setting of the computer is changed.

"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -STA -File "%~dp0play.ps1" -Keyboard %*
if errorlevel 1 (
    echo.
    pause
)
