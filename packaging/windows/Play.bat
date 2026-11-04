@echo off
rem Starts the game. The first time it asks for the disc image and prepares the game.
rem Drop a disc image on this file to use that image. All the work is done by play.ps1.
rem See README.md (English and Spanish).
rem
rem Arranca el juego. La primera vez pide la imagen del disco y prepara el juego.
rem Suelta una imagen de disco sobre este archivo para usarla. Todo lo hace play.ps1.
rem Ver README.md (ingles y espanol).
rem
rem -ExecutionPolicy Bypass applies to this one run only, so that Windows does not refuse a
rem script that came in a download. No setting of the computer is changed.

"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -STA -File "%~dp0play.ps1" %*
if errorlevel 1 (
    echo.
    pause
)
