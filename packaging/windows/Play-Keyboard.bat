@echo off
rem Starts the game and plays it with the keyboard. Use it when no controller is connected: the
rem launcher that Play.bat opens does not let the game start without one, and this file skips it.
rem The first time it asks for the disc image and prepares the game, like Play.bat does. Drop a
rem disc image on this file to use that image. All the work is done by play.ps1.
rem See README.md (English and Spanish).
rem
rem Arranca el juego y lo juega con el teclado. Usalo cuando no hay un control conectado: el
rem launcher que abre Play.bat no deja empezar el juego sin uno, y este archivo se lo salta.
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
