# The game that this package plays. play.ps1 reads it; nothing else in the package does.
# El juego que juega este paquete. Lo lee play.ps1; nada más en el paquete lo usa.
#
# The values that the launcher and the runner were built with (the disc ID they accept and the
# name of the folder where they keep their files) are in branding.cmake. A test checks that the
# two files agree.
# Los valores con los que se construyeron el launcher y el runner (el ID de disco que aceptan y
# el nombre de la carpeta donde guardan sus archivos) están en branding.cmake. Una prueba
# comprueba que los dos archivos coincidan.
@{
    # The six characters that identify the disc: the USA release of the game for GameCube.
    # Los seis caracteres que identifican el disco: la versión USA del juego para GameCube.
    GameId            = 'G4AEE9'

    # The name that is shown to the player.
    # El nombre que se muestra al jugador.
    GameName          = 'Harvest Moon: Magical Melody'

    # The file name of the launcher in bin\, without the .exe.
    # El nombre de archivo del launcher en bin\, sin el .exe.
    LauncherName      = 'HarvestMoonMM'

    # The player's folder inside %LOCALAPPDATA% (the extracted game, the recompiled module, the
    # settings, the saves and the logs).
    # La carpeta del jugador dentro de %LOCALAPPDATA% (el juego extraído, el módulo recompilado,
    # la configuración, las partidas guardadas y los registros).
    UserDirectoryName = 'HarvestMoonMM'

    # The kinds of disc image that the launcher can read (Dolphin's disc reader opens them all).
    # Los tipos de imagen de disco que el launcher puede leer (el lector de discos de Dolphin
    # los abre todos).
    DiscExtensions    = @('.iso', '.gcm', '.rvz', '.wbfs', '.ciso', '.gcz', '.wia', '.tgc')
}
