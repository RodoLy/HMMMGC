# Initial CMake cache of the Windows package: what makes ModernGekko's launcher and runner play
# this game and no other. Use it with `cmake -C packaging/windows/branding.cmake ...`.
# Caché inicial de CMake del paquete para Windows: lo que hace que el launcher y el runner de
# ModernGekko jueguen este juego y ningún otro. Se usa con
# `cmake -C packaging/windows/branding.cmake ...`.
#
# game.psd1 holds the same disc ID and folder name for play.ps1, and a test checks that the two
# files agree.
# game.psd1 tiene el mismo ID de disco y nombre de carpeta para play.ps1, y una prueba comprueba
# que los dos archivos coincidan.

# The title of the launcher's window.
# El título de la ventana del launcher.
set(MODERNGEKKO_FRONTEND_NAME "Harvest Moon: Magical Melody" CACHE STRING "")

# The file name of the launcher (HarvestMoonMM.exe).
# El nombre de archivo del launcher (HarvestMoonMM.exe).
set(MODERNGEKKO_LAUNCHER_OUTPUT_NAME "HarvestMoonMM" CACHE STRING "")

# The player's folder inside %LOCALAPPDATA%.
# La carpeta del jugador dentro de %LOCALAPPDATA%.
set(MODERNGEKKO_USER_DIRECTORY_NAME "HarvestMoonMM" CACHE STRING "")

# The only disc that the launcher and the runner accept: the USA release for GameCube.
# El único disco que aceptan el launcher y el runner: la versión USA para GameCube.
set(MODERNGEKKO_REQUIRED_DISC_ID "G4AEE9" CACHE STRING "")

# The game is for GameCube, so the launcher maps the player's gamepad to a GameCube controller
# and not to a Wii Remote.
# El juego es de GameCube, así que el launcher asigna el control del jugador a un control de
# GameCube y no a un Wii Remote.
set(MODERNGEKKO_GAMECUBE_CONTROLLERS ON CACHE BOOL "")
