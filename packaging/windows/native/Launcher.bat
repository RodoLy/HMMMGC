@echo off
rem Opens the launcher of the native version (without Dolphin): turn the mods on and off, choose
rem the speed of the character, and play. All the work is done by Launcher.ps1.
rem
rem Abre el launcher de la version nativa (sin Dolphin): activar y desactivar los mods, elegir la
rem velocidad del personaje y jugar. Todo lo hace Launcher.ps1.
rem
rem -ExecutionPolicy Bypass applies to this one run only, so that Windows does not refuse a
rem script that came in a download. No setting of the computer is changed.

"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -STA -File "%~dp0Launcher.ps1"
if errorlevel 1 (
    echo.
    pause
)
