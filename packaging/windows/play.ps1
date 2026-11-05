#Requires -Version 5.1

<#
.SYNOPSIS
    Prepares the game from the player's own disc and opens its launcher.
    Prepara el juego con el disco del propio jugador y abre su launcher.

.DESCRIPTION
    The game is not in this package. It is recompiled here, on this computer, from the disc image
    that the player owns, so the first run does these things and later runs skip what is done:

      1. Reads the disc image and extracts the game's files (the launcher does it).
      2. Recompiles the game's program into a native module: DolRecomp translates it to C and
         the compiler in toolchain\ builds it. This takes several minutes.
      3. Puts the module where the launcher and the runner look for it.
      4. Builds the mods that are turned on (small changes to the game, listed in game.psd1) with
         the same compiler and puts them in the Mods folder of the player's folder, where the
         runner finds them. A mod that is turned off is taken out of that folder. This is done on
         every run and takes seconds. If it fails, the game starts anyway.
      5. Opens the launcher (or, with -Keyboard, the game itself).

    Which mods are on, and how fast the character is, are chosen in Settings.bat, which keeps the
    choice in settings.json in the player's folder (settings-file.ps1 says what the file holds).
    With no settings.json every mod is on.

    Nothing is installed and no setting of the computer is changed. Everything this script makes
    is in the folder %LOCALAPPDATA%\<UserDirectoryName> (see game.psd1), and a log of the last
    run is in its Logs folder.

    El juego no está en este paquete. Se recompila aquí, en esta computadora, con la imagen del
    disco que el jugador tiene, así que la primera ejecución hace estas cosas y las siguientes
    omiten lo que ya está hecho:

      1. Lee la imagen del disco y extrae los archivos del juego (lo hace el launcher).
      2. Recompila el programa del juego en un módulo nativo: DolRecomp lo traduce a C y el
         compilador de toolchain\ lo construye. Esto tarda varios minutos.
      3. Pone el módulo donde lo buscan el launcher y el runner.
      4. Construye los mods que están activos (pequeños cambios al juego, listados en game.psd1)
         con el mismo compilador y los pone en la carpeta Mods de la carpeta del jugador, donde
         los encuentra el runner. Un mod apagado se quita de esa carpeta. Se hace en cada
         ejecución y tarda segundos. Si falla, el juego arranca igual.
      5. Abre el launcher (o, con -Keyboard, el juego mismo).

    Qué mods están activos, y qué tan rápido es el personaje, se eligen en Settings.bat, que guarda
    la elección en settings.json en la carpeta del jugador (settings-file.ps1 dice qué contiene el
    archivo). Sin settings.json todos los mods están activos.

    No se instala nada ni se cambia ninguna configuración de la computadora. Todo lo que hace este
    script queda en la carpeta %LOCALAPPDATA%\<UserDirectoryName> (ver game.psd1), y el registro
    de la última ejecución está en su carpeta Logs.

.PARAMETER Disc
    The disc image to use (.iso, .gcm, .rvz, ...). If it is left out, the game that was extracted
    the last time is used, and if there is none, a window asks for the image.
    La imagen de disco que se usará (.iso, .gcm, .rvz, ...). Si se omite, se usa el juego que se
    extrajo la vez anterior, y si no hay ninguno, una ventana pide la imagen.

.PARAMETER PrepareOnly
    Prepare the game but do not open the launcher. The tests of the package use it.
    Prepara el juego pero no abre el launcher. Lo usan las pruebas del paquete.

.PARAMETER Keyboard
    Open the game itself, without the launcher, to play with the keyboard. With no controller set
    up the game reads Dolphin's default keyboard layout, which is also what Play does in the
    launcher when no controller is connected; this skips the launcher.
    Abre el juego mismo, sin el launcher, para jugar con el teclado. Sin ningún control
    configurado el juego lee la asignación de teclado que trae Dolphin, que es también lo que hace
    Play en el launcher cuando no hay un control conectado; esto se salta el launcher.

.PARAMETER NoMods
    Play the game as it is on the disc: the mods are not built, the ones that this package
    installed earlier are taken out of the player's folder, and settings.json is not read. Run
    again without it to get them back.
    Juega el juego como viene en el disco: no se construyen los mods, se quitan de la carpeta del
    jugador los que este paquete instaló antes y no se lee settings.json. Ejecuta otra vez sin esto
    para recuperarlos.

.PARAMETER PlayerSpeed
    How fast the character walks and runs, in percent of the speed of the original game, from 25
    to 300: 100 is the original speed and 150 is one and a half times as fast. It is for this run
    only and wins over the speed that Settings.bat saved. If it is left out and nothing was saved,
    the player_speed mod uses its own default, which is 150. Above 200 the character may go
    through thin walls (README.md says what was seen). It is given to the mod through an
    environment variable that only lasts while this script and the game it opens run. It has no
    effect with -NoMods, or when the settings turn the player_speed mod off.
    Qué tan rápido camina y corre el personaje, en porcentaje de la velocidad del juego original,
    de 25 a 300: 100 es la velocidad original y 150 es una vez y media más rápido. Vale solo para
    esta ejecución y le gana a la velocidad que guardó Settings.bat. Si se omite y no se guardó
    ninguna, el mod player_speed usa su propio valor por omisión, que es 150. Por encima de 200 el
    personaje podría atravesar paredes delgadas (README.md dice lo que se vio). Se le da al mod con
    una variable de entorno que solo dura mientras corren este script y el juego que abre. No tiene
    efecto con -NoMods, ni cuando los ajustes apagan el mod player_speed.
#>

[Diagnostics.CodeAnalysis.SuppressMessageAttribute(
    'PSAvoidUsingWriteHost', '',
    Justification = 'This script talks to the person who runs it, at the console.')]
[Diagnostics.CodeAnalysis.SuppressMessageAttribute(
    'PSUseShouldProcessForStateChangingFunctions', '',
    Justification = 'The functions are the steps of one run of this script, not commands for others to call.')]
[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [string] $Disc,

    [switch] $PrepareOnly,

    [switch] $Keyboard,

    [switch] $NoMods,

    [ValidateRange(25, 300)]
    [Nullable[int]] $PlayerSpeed
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# -- What is where ---------------------------------------------------------------------------------

$PackageDirectory = $PSScriptRoot
$ProgramsDirectory = Join-Path $PackageDirectory 'bin'
$ToolchainDirectory = Join-Path $PackageDirectory 'toolchain'
$Game = Import-PowerShellDataFile -LiteralPath (Join-Path $PackageDirectory 'game.psd1')

$LauncherPath = Join-Path $ProgramsDirectory ($Game.LauncherName + '.exe')
$RunnerPath = Join-Path $ProgramsDirectory 'moderngekko-run.exe'
$PortToolPath = Join-Path $ProgramsDirectory 'moderngekko-port.exe'

# The same folders that the launcher and the runner use (the launcher picks %LOCALAPPDATA%\<name>).
$UserDirectory = Join-Path $env:LOCALAPPDATA $Game.UserDirectoryName
$ExtractedGameDirectory = Join-Path (Join-Path $UserDirectory 'games') $Game.GameId
$ExtractedGameProgram = Join-Path $ExtractedGameDirectory 'sys\main.dol'
$InstalledModulesDirectory = Join-Path $UserDirectory 'StaticRecompModules'
$LogPath = Join-Path $UserDirectory 'Logs\play.log'

$ClangPath = Join-Path $ToolchainDirectory 'llvm-mingw\bin\clang.exe'
$CMakePath = Join-Path $ToolchainDirectory 'cmake\bin\cmake.exe'

# The mods (see ModIds in game.psd1). Their source and the header that names the game's functions
# are in the package, the headers of ModernGekko that they include are in bin\source, and the built
# mods go to the Mods folder of the player's folder, which is one of the two places where the runner
# looks for mods.
$ModsSourceDirectory = Join-Path $PackageDirectory 'mods'
$ModSymbolsHeader = Join-Path $ModsSourceDirectory ('generated\' + $Game.GameId + '_symbols.h')
$ModernGekkoSourceDirectory = Join-Path $ProgramsDirectory 'source'
$InstalledModsDirectory = Join-Path $UserDirectory 'Mods'

# The environment variable through which the player_speed mod is told how fast the character is (see
# mods\player_speed\mod.c, which reads it when the game starts).
$PlayerSpeedVariable = 'HM_MM_PLAYER_SPEED_PERCENT'

# The settings that the player chose in Settings.bat (which mods are on, how fast the character is).
# The code that reads them is shared with the settings window. It is loaded once the package is known
# to be complete (see the run, at the end), and that is also when $SettingsPath, the path of the
# settings file, is set: the code is what knows the name of the file.
$SettingsLibraryPath = Join-Path $PackageDirectory 'settings-file.ps1'
$SettingsPath = $null

# The mods of this package, as game.psd1 lists them (always a list, even when there are none or one).
$AllModIds = @($Game.ModIds | Where-Object { $_ })

# Every file whose absence means that the package was not extracted completely.
$RequiredFiles = @(
    $LauncherPath,
    $PortToolPath,
    $RunnerPath,
    (Join-Path $ProgramsDirectory 'dolrecomp.exe'),
    $ClangPath,
    $CMakePath,
    (Join-Path $ToolchainDirectory 'ninja\ninja.exe'),
    (Join-Path $ToolchainDirectory 'python\python.exe'),
    (Join-Path $ModsSourceDirectory 'CMakeLists.txt'),
    $ModSymbolsHeader,
    $SettingsLibraryPath
)

# Windows stops at 260 characters in a path, and the files of the recompiled module sit in
# folders named after the hash of the program, which adds about 200 to the length of the folder
# where the module is built. Beyond this length that folder is moved to a shorter place.
$MaximumModulesPathLength = 59
$MaximumPackagePathLength = 90

$script:LogWriter = $null

# -- Messages: English and Spanish, chosen by the language of Windows ------------------------------

$Messages = @{
    en = @{
        WrongComputer       = 'This package is for computers with Windows on ARM (for example with a Snapdragon X processor). This computer is {0}.'
        IncompletePackage   = 'The package is incomplete: {0} is missing. Extract the whole zip file again (all of it, not only some files) and try again.'
        PackagePathIsLong   = 'This package is in a folder with a very long path. If the preparation fails, move the whole folder to a short place such as C:\HarvestMoonMM and try again.'
        AlreadyOpen         = 'Play is already open in another window, or the launcher it opened is still running: it keeps {0} in use. Close that window, or end HarvestMoonMM.exe and moderngekko-run.exe in the Task Manager (Details tab), and try again.'
        ShortFolderUsed     = 'The recompiled game will be built in {0}, because the usual folder has a long path or special characters.'
        ChooseDiscTitle     = 'Choose the disc image of {0}'
        DiscImagesFilter    = 'Disc images'
        AllFilesFilter      = 'All files'
        NoDiscChosen        = 'No disc image was chosen. Run Play.bat again and choose the image of your {0} disc, or drop the file on Play.bat.'
        DiscNotFound        = 'The disc image does not exist: {0}'
        ReadingDisc         = 'Reading the disc image {0} ...'
        ExtractionFailed    = 'The disc image could not be read (see the messages above). This package plays the USA release of {0}, disc ID {1}.'
        Recompiling         = 'Preparing the game for this computer. This is done only once and can take several minutes. Do not close this window.'
        RecompilationFailed = 'The game could not be prepared. The details are in {0}'
        PreparingMods       = 'Preparing the mods ...'
        ModsPrepared        = 'Mods ready: {0}.'
        ModsFailed          = 'The mods could not be prepared. The game starts anyway. The details are in {0}'
        ModsOff             = 'The mods are off (-NoMods): the game is played as it is on the disc.'
        ModsTurnedOff       = 'Mods turned off in the settings: {0}.'
        AllModsTurnedOff    = 'All the mods are turned off in the settings: the game is played as it is on the disc.'
        SettingsHint        = 'Settings.bat lets you turn the mods on and off and choose how fast the character is.'
        SettingsProblems    = 'Some values in {0} could not be used and were left at their defaults: {1}'
        PlayerSpeedSet      = 'The character walks and runs at {0}% of the original speed.'
        PlayerSpeedIgnored  = 'The speed mod is turned off in the settings, so the character moves at the original speed.'
        GamePrepared        = 'The game is ready.'
        OpeningLauncher     = 'Opening the launcher. Press Play in it to start the game.'
        LauncherFailed      = 'The launcher closed with an error (code {0}). The details are in {1}'
        OpeningGame         = 'Opening the game with the keyboard.'
        KeyboardControls    = @'
Keyboard (the default layout of Dolphin, which this game runs on):
  Arrow keys   stick (hold Shift for half tilt)
  X, Z         A and B buttons          C, S    X and Y buttons          D      Z button
  Q, W         L and R triggers         Enter   Start
  T, F, G, H   D-pad up, left, down, right
  I, J, K, L   C-stick up, left, down, right (hold Ctrl for half tilt)
'@
        GameFailed          = 'The game closed with an error (code {0}). The details are in {1}'
        Failed              = 'Something went wrong: {0}'
    }
    es = @{
        WrongComputer       = 'Este paquete es para computadoras con Windows en ARM (por ejemplo con un procesador Snapdragon X). Esta computadora es {0}.'
        IncompletePackage   = 'El paquete está incompleto: falta {0}. Extrae otra vez todo el archivo zip (completo, no solo algunos archivos) e inténtalo de nuevo.'
        PackagePathIsLong   = 'Este paquete está en una carpeta con una ruta muy larga. Si la preparación falla, mueve la carpeta completa a un lugar corto como C:\HarvestMoonMM e inténtalo de nuevo.'
        AlreadyOpen         = 'Play ya está abierto en otra ventana, o el launcher que abrió sigue funcionando: mantiene en uso {0}. Cierra esa ventana, o finaliza HarvestMoonMM.exe y moderngekko-run.exe en el Administrador de tareas (pestaña Detalles), e inténtalo de nuevo.'
        ShortFolderUsed     = 'El juego recompilado se construirá en {0}, porque la carpeta habitual tiene una ruta larga o caracteres especiales.'
        ChooseDiscTitle     = 'Elige la imagen del disco de {0}'
        DiscImagesFilter    = 'Imágenes de disco'
        AllFilesFilter      = 'Todos los archivos'
        NoDiscChosen        = 'No se eligió ninguna imagen de disco. Ejecuta Play.bat otra vez y elige la imagen de tu disco de {0}, o suelta el archivo sobre Play.bat.'
        DiscNotFound        = 'La imagen de disco no existe: {0}'
        ReadingDisc         = 'Leyendo la imagen de disco {0} ...'
        ExtractionFailed    = 'No se pudo leer la imagen de disco (mira los mensajes de arriba). Este paquete juega la versión USA de {0}, ID de disco {1}.'
        Recompiling         = 'Preparando el juego para esta computadora. Se hace una sola vez y puede tardar varios minutos. No cierres esta ventana.'
        RecompilationFailed = 'No se pudo preparar el juego. Los detalles están en {0}'
        PreparingMods       = 'Preparando los mods ...'
        ModsPrepared        = 'Mods listos: {0}.'
        ModsFailed          = 'No se pudieron preparar los mods. El juego arranca igual. Los detalles están en {0}'
        ModsOff             = 'Los mods están apagados (-NoMods): el juego se juega como viene en el disco.'
        ModsTurnedOff       = 'Mods apagados en los ajustes: {0}.'
        AllModsTurnedOff    = 'Todos los mods están apagados en los ajustes: el juego se juega como viene en el disco.'
        SettingsHint        = 'Con Settings.bat puedes activar y apagar los mods y elegir qué tan rápido es el personaje.'
        SettingsProblems    = 'Algunos valores de {0} no se pudieron usar y se quedaron en su valor por omisión: {1}'
        PlayerSpeedSet      = 'El personaje camina y corre al {0}% de la velocidad original.'
        PlayerSpeedIgnored  = 'El mod de velocidad está apagado en los ajustes, así que el personaje se mueve a la velocidad original.'
        GamePrepared        = 'El juego está listo.'
        OpeningLauncher     = 'Abriendo el launcher. Pulsa Play en él para empezar el juego.'
        LauncherFailed      = 'El launcher se cerró con un error (código {0}). Los detalles están en {1}'
        OpeningGame         = 'Abriendo el juego con el teclado.'
        KeyboardControls    = @'
Teclado (la asignación de Dolphin, sobre el que corre este juego):
  Flechas      palanca (con Shift, a media inclinación)
  X, Z         botones A y B            C, S    botones X y Y            D      botón Z
  Q, W         gatillos L y R           Enter   Start
  T, F, G, H   cruceta arriba, izquierda, abajo, derecha
  I, J, K, L   palanca C arriba, izquierda, abajo, derecha (con Ctrl, a media inclinación)
'@
        GameFailed          = 'El juego se cerró con un error (código {0}). Los detalles están en {1}'
        Failed              = 'Algo salió mal: {0}'
    }
}
$Language = if ((Get-UICulture).TwoLetterISOLanguageName -eq 'es') { 'es' } else { 'en' }

function Get-Message {
    param(
        [Parameter(Mandatory)] [string] $Key,
        [object[]] $Values = @()
    )
    return [string]::Format($Messages[$Language][$Key], $Values)
}

# -- The log of this run ---------------------------------------------------------------------------

function Open-Log {
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $LogPath) | Out-Null
    $withoutByteOrderMark = New-Object System.Text.UTF8Encoding($false)
    try {
        $script:LogWriter = New-Object System.IO.StreamWriter($LogPath, $false, $withoutByteOrderMark)
    }
    catch {
        # Another copy of this script keeps the log open for as long as it runs, and it runs for as
        # long as the launcher it started is open. PowerShell wraps the error of the constructor in
        # an exception of its own; the sharing violation is the one inside it.
        if ($_.Exception.InnerException -is [System.IO.IOException]) { throw (Get-Message 'AlreadyOpen' $LogPath) }
        throw
    }
    $script:LogWriter.AutoFlush = $true
}

function Add-LogLine {
    param([string] $Text)
    if ($script:LogWriter) { $script:LogWriter.WriteLine($Text) }
}

function Close-Log {
    if ($script:LogWriter) {
        $script:LogWriter.Dispose()
        $script:LogWriter = $null
    }
}

# -- Running the programs of the package -------------------------------------------------------------

# Shows one line of a program's output and keeps it in the log (with -Quiet it only keeps it). The
# lines of the build that look like "[12/345] Building ..." become a progress bar, because there are
# thousands of them.
function Write-ProgramLine {
    param(
        [string] $Line,
        [switch] $Quiet
    )
    Add-LogLine $Line
    if ($Quiet) { return }
    if ($Line -match '^\[(\d+)/(\d+)\] ') {
        $percent = [int] (100 * [int] $Matches[1] / [int] $Matches[2])
        $status = $Line.Substring(0, [Math]::Min($Line.Length, 90))
        Write-Progress -Activity (Get-Message 'Recompiling') -Status $status -PercentComplete $percent
    }
    else {
        Write-Host $Line
    }
}

# What a program writes on its error output reaches the pipeline as an error record, not as text.
# The text of such a record is the line the program wrote, except for an empty line: PowerShell
# then gives the name of the record's own class ("System.Management.Automation.RemoteException").
# Taking the message of the record gives the line, empty or not.
function ConvertTo-ProgramText {
    param($Output)
    if ($Output -is [System.Management.Automation.ErrorRecord]) { return $Output.Exception.Message }
    return [string] $Output
}

# Runs a program, shows what it says (with -Quiet it only goes to the log), and returns its exit
# code. What a program writes as warnings on its error output is not an error of this script, so
# errors do not stop it here.
function Invoke-Program {
    param(
        [Parameter(Mandatory)] [string] $FilePath,
        [string[]] $Arguments = @(),
        [switch] $Quiet
    )
    Add-LogLine ('> ' + $FilePath + ' ' + ($Arguments -join ' '))
    $previousPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        & $FilePath @Arguments 2>&1 |
            ForEach-Object { Write-ProgramLine (ConvertTo-ProgramText $_) -Quiet:$Quiet }
        return $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previousPreference
        Write-Progress -Activity (Get-Message 'Recompiling') -Completed
    }
}

# The programs of the package write UTF-8, and so do the paths in what they say, so the console
# reads UTF-8 while the game is prepared. Returns the encoding to put back at the end, or $null
# when it could not be changed: with no console (the output goes to a file, for example) there is
# nothing to change and nothing is lost.
function Set-ConsoleUtf8 {
    try {
        $previous = [Console]::OutputEncoding
        [Console]::OutputEncoding = New-Object System.Text.UTF8Encoding($false)
        return $previous
    }
    catch {
        return $null
    }
}

# -- Checks before starting --------------------------------------------------------------------------

function Get-ComputerArchitecture {
    try {
        return [string] [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture
    }
    catch {
        # Windows without .NET 4.7.1: the variable of the process is the best that is left.
        return $env:PROCESSOR_ARCHITECTURE
    }
}

function Find-MissingFile {
    foreach ($path in $RequiredFiles) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { return $path }
    }
    return $null
}

# -- Steps -------------------------------------------------------------------------------------------

# Only the tools of the package are on the path while the game is prepared, so that a compiler,
# CMake or Python that happens to be installed in this computer is never used by mistake.
function Set-ToolchainEnvironment {
    $env:Path = @(
        (Join-Path $ToolchainDirectory 'llvm-mingw\bin'),
        (Join-Path $ToolchainDirectory 'cmake\bin'),
        (Join-Path $ToolchainDirectory 'ninja'),
        (Join-Path $ToolchainDirectory 'python'),
        (Join-Path $env:SystemRoot 'System32'),
        $env:SystemRoot
    ) -join ';'
    # CMake looks for Python in the registry before the path; this tells it where to look first.
    $env:Python3_ROOT_DIR = Join-Path $ToolchainDirectory 'python'
}

# Where the compiled module is built. It is a short folder if the usual one would be too long for
# Windows, or has letters that the programs that build the module cannot pass along.
function Get-ModulesDirectory {
    $usual = Join-Path $UserDirectory 'modules'
    $hasSpecialCharacters = $usual -cmatch '[^\x20-\x7E]'
    if (-not $hasSpecialCharacters -and $usual.Length -le $MaximumModulesPathLength) { return $usual }
    $short = $env:SystemDrive + '\' + $Game.UserDirectoryName + '\modules'
    Write-Host (Get-Message 'ShortFolderUsed' $short)
    return $short
}

function Select-DiscImage {
    Add-Type -AssemblyName System.Windows.Forms
    $patterns = @($Game.DiscExtensions | ForEach-Object { '*' + $_ })
    $dialog = New-Object System.Windows.Forms.OpenFileDialog
    try {
        $dialog.Title = Get-Message 'ChooseDiscTitle' $Game.GameName
        $dialog.Filter = (Get-Message 'DiscImagesFilter') + ' (' + ($patterns -join ', ') + ')|' +
            ($patterns -join ';') + '|' + (Get-Message 'AllFilesFilter') + '|*.*'
        $dialog.InitialDirectory = [Environment]::GetFolderPath('MyDocuments')
        if ($dialog.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) { return $dialog.FileName }
        return $null
    }
    finally {
        $dialog.Dispose()
    }
}

# The disc image to read, or $null when the game was already extracted and no image was given.
function Get-DiscToExtract {
    param([string] $RequestedDisc)
    if ($RequestedDisc) {
        $resolved = Resolve-Path -LiteralPath $RequestedDisc -ErrorAction SilentlyContinue
        if (-not $resolved) { throw (Get-Message 'DiscNotFound' $RequestedDisc) }
        return $resolved.ProviderPath
    }
    if (Test-Path -LiteralPath $ExtractedGameProgram -PathType Leaf) { return $null }
    $chosen = Select-DiscImage
    if (-not $chosen) { throw (Get-Message 'NoDiscChosen' $Game.GameName) }
    return $chosen
}

function Export-Disc {
    param([Parameter(Mandatory)] [string] $DiscPath)
    Write-Host (Get-Message 'ReadingDisc' (Split-Path -Leaf $DiscPath))
    $exitCode = Invoke-Program -FilePath $LauncherPath -Arguments @('--extract', $DiscPath)
    if ($exitCode -ne 0 -or -not (Test-Path -LiteralPath $ExtractedGameProgram -PathType Leaf)) {
        throw (Get-Message 'ExtractionFailed' @($Game.GameName, $Game.GameId))
    }
}

# Recompiles the game's program and returns the path of the module. The tool keeps what it builds
# and builds nothing when the program, the tools and the settings are the same as the last time.
function New-GameModule {
    param([Parameter(Mandatory)] [string] $ModulesDirectory)
    Write-Host (Get-Message 'Recompiling')
    $arguments = @(
        'build', $ExtractedGameDirectory,
        '--backend', 'c',
        '--toolchain', 'clang',
        '--output', $ModulesDirectory
    )
    $exitCode = Invoke-Program -FilePath $PortToolPath -Arguments $arguments
    if ($exitCode -ne 0) { throw (Get-Message 'RecompilationFailed' $LogPath) }
    # The tool writes the path of the module it considers current into this file.
    $activeModuleFile = Join-Path (Join-Path $ModulesDirectory $Game.GameId) 'active-module.txt'
    $modulePath = (Get-Content -LiteralPath $activeModuleFile -Encoding UTF8 -TotalCount 1).Trim()
    if (-not (Test-Path -LiteralPath $modulePath -PathType Leaf)) {
        throw (Get-Message 'RecompilationFailed' $LogPath)
    }
    return $modulePath
}

# Copies the module to the folder where the runner looks for it, unless it is already there.
function Install-GameModule {
    param([Parameter(Mandatory)] [string] $ModulePath)
    New-Item -ItemType Directory -Force -Path $InstalledModulesDirectory | Out-Null
    $installedPath = Join-Path $InstalledModulesDirectory (Split-Path -Leaf $ModulePath)
    $isInstalled = (Test-Path -LiteralPath $installedPath -PathType Leaf) -and
        ((Get-FileHash -LiteralPath $installedPath).Hash -eq (Get-FileHash -LiteralPath $ModulePath).Hash)
    if (-not $isInstalled) { Copy-Item -LiteralPath $ModulePath -Destination $installedPath -Force }
    return $installedPath
}

# Reads the settings that the player chose in Settings.bat. Whatever cannot be used is reported and
# left at its default, so that a typing mistake in settings.json never stops the game from starting.
# While the player has not saved any settings (there is no file) a line says where to choose them.
function Get-PlayerSettings {
    if (-not (Test-Path -LiteralPath $SettingsPath -PathType Leaf)) { Write-Host (Get-Message 'SettingsHint') }
    $read = Read-PlayerSettings -Path $SettingsPath -ModIds $AllModIds
    if ($read.Problems.Count -gt 0) {
        $problems = $read.Problems -join '; '
        Add-LogLine ('Settings problems: ' + $problems)
        Write-Host (Get-Message 'SettingsProblems' @($SettingsPath, $problems)) -ForegroundColor Yellow
    }
    return $read.Settings
}

# Builds the given mods, with the compiler of the package, and puts each one in the Mods folder of
# the player's folder as <id>.mgm\mod.dll, where the runner looks for mods. A mod is one small file
# of C, so this takes seconds, and it is done again on every run: that way the mods are always the
# ones of this package, built for this computer, even after the package was replaced by a newer one.
# The build folder is made anew each time because CMake remembers where it found the package and
# would refuse to build in a folder that was made for a package that has been moved.
#
# The mods are an addition to the game, so a failure here is a warning and not an error: the game
# starts anyway, and the log says what went wrong. What the tools say goes to the log only.
function Install-Mods {
    param(
        [Parameter(Mandatory)] [string] $BuildDirectory,
        [Parameter(Mandatory)] [string[]] $ModIds
    )
    Write-Host (Get-Message 'PreparingMods')
    try {
        if (Test-Path -LiteralPath $BuildDirectory) { Remove-Item -LiteralPath $BuildDirectory -Recurse -Force }
        $configureArguments = @(
            '-S', $ModsSourceDirectory,
            '-B', $BuildDirectory,
            '-G', 'Ninja',
            '-DCMAKE_BUILD_TYPE=Release',
            "-DCMAKE_C_COMPILER=$ClangPath",
            "-DMODERNGEKKO_SOURCE_DIR=$ModernGekkoSourceDirectory",
            "-DMOD_SYMBOL_HEADER=$ModSymbolsHeader"
        )
        $exitCode = Invoke-Program -FilePath $CMakePath -Arguments $configureArguments -Quiet
        if ($exitCode -ne 0) { throw "CMake could not set up the build of the mods (exit code $exitCode)" }

        # Only the mods that are asked for: the folder also holds mods that are not part of the game.
        $buildArguments = @('--build', $BuildDirectory, '--target') + @($ModIds)
        $exitCode = Invoke-Program -FilePath $CMakePath -Arguments $buildArguments -Quiet
        if ($exitCode -ne 0) { throw "CMake could not build the mods (exit code $exitCode)" }

        New-Item -ItemType Directory -Force -Path $InstalledModsDirectory | Out-Null
        foreach ($modId in $ModIds) {
            $builtMod = Join-Path $BuildDirectory ($modId + '.mgm')
            $installedMod = Join-Path $InstalledModsDirectory ($modId + '.mgm')
            # An older copy goes completely, so that a file the mod no longer has does not stay.
            if (Test-Path -LiteralPath $installedMod) { Remove-Item -LiteralPath $installedMod -Recurse -Force }
            Copy-Item -LiteralPath $builtMod -Destination $installedMod -Recurse
            Add-LogLine ('Mod: ' + (Join-Path $installedMod 'mod.dll'))
        }
        Write-Host (Get-Message 'ModsPrepared' ($ModIds -join ', '))
    }
    catch {
        Add-LogLine ($_ | Out-String)
        Write-Host (Get-Message 'ModsFailed' $LogPath) -ForegroundColor Yellow
    }
}

# Takes the given mods out of the player's folder, so that the game is played without them. They
# are always mods of this package (ids from game.psd1): a mod that the player put there by hand
# stays.
function Remove-InstalledMods {
    param([AllowEmptyCollection()] [string[]] $ModIds = @())
    foreach ($modId in $ModIds) {
        $installedMod = Join-Path $InstalledModsDirectory ($modId + '.mgm')
        if (Test-Path -LiteralPath $installedMod) { Remove-Item -LiteralPath $installedMod -Recurse -Force }
    }
}

# Tells the player_speed mod how fast the character is to be, in percent. The mod reads an
# environment variable, and both the launcher and the runner start the program they open with the
# environment of their parent, so the variable reaches the mod whether the game is opened through
# the launcher or with -Keyboard. With no speed ($null) nothing is set, and the mod uses its own
# default (or what the player put in the variable by hand). While the mod is off there is nothing to
# tell, and a speed that was asked for on the command line is reported as without effect.
function Set-PlayerSpeed {
    param(
        [Nullable[int]] $Percent,
        [Parameter(Mandatory)] [bool] $ModIsOn
    )
    if (-not $ModIsOn) {
        if ($null -ne $PlayerSpeed) { Write-Host (Get-Message 'PlayerSpeedIgnored') }
        return
    }
    if ($null -eq $Percent) { return }
    [Environment]::SetEnvironmentVariable($PlayerSpeedVariable, [string] $Percent, 'Process')
    Add-LogLine ($PlayerSpeedVariable + '=' + $Percent)
    Write-Host (Get-Message 'PlayerSpeedSet' $Percent)
}

# Puts the mods in the state that was asked for. With -NoMods none is installed, and the settings are
# not read. Otherwise the mods that the settings turn on are built and installed, the ones that they
# turn off are taken out of the player's folder (so a mod that was on before stops being on), and the
# speed of the character is given to the speed mod. -PlayerSpeed is for this run only and wins over
# the speed that was saved.
function Initialize-Mods {
    param([Parameter(Mandatory)] [string] $BuildDirectory)

    if ($NoMods) {
        Remove-InstalledMods -ModIds $AllModIds
        Write-Host (Get-Message 'ModsOff')
        return
    }

    $settings = Get-PlayerSettings
    $enabledModIds = @(Get-EnabledModIds -Settings $settings)
    $disabledModIds = @($AllModIds | Where-Object { $enabledModIds -notcontains $_ })
    Add-LogLine ('Mods turned on in the settings: ' + ($enabledModIds -join ', '))

    # The mods that are off are taken out first, so that they are not in the game even if the
    # building of the others fails.
    Remove-InstalledMods -ModIds $disabledModIds
    if ($enabledModIds.Count -eq 0 -and $disabledModIds.Count -gt 0) {
        Write-Host (Get-Message 'AllModsTurnedOff')
    }
    elseif ($disabledModIds.Count -gt 0) {
        Write-Host (Get-Message 'ModsTurnedOff' ($disabledModIds -join ', '))
    }
    if ($enabledModIds.Count -gt 0) { Install-Mods -BuildDirectory $BuildDirectory -ModIds $enabledModIds }

    $percent = $settings.PlayerSpeedPercent
    if ($null -ne $PlayerSpeed) { $percent = $PlayerSpeed }
    Set-PlayerSpeed -Percent $percent -ModIsOn ($enabledModIds -contains $PlayerSpeedModId)
}

function Start-Launcher {
    Write-Host (Get-Message 'OpeningLauncher')
    & $LauncherPath
    if ($LASTEXITCODE -ne 0) { throw (Get-Message 'LauncherFailed' @($LASTEXITCODE, $LogPath)) }
}

# Opens the game without the launcher, which is what the launcher itself does when Play is pressed:
# it starts the runner with the game and the player's folder. The runner does not need a controller
# (with none set up the game reads Dolphin's keyboard layout), so this plays with the keyboard
# without opening the launcher. What the runner says is shown and kept in the log, like what the
# tools say.
# The graphics API is given on purpose, so that the game does not depend on what the runner picks
# when it is not told: Vulkan in an unpatched ModernGekko, which draws nothing on some computers
# (see GraphicsBackend in game.psd1).
function Start-GameWithKeyboard {
    Write-Host (Get-Message 'KeyboardControls')
    Write-Host (Get-Message 'OpeningGame')
    $exitCode = Invoke-Program -FilePath $RunnerPath -Arguments @(
        '--game', $ExtractedGameDirectory,
        '--user-dir', $UserDirectory,
        '--graphics', $Game.GraphicsBackend
    )
    if ($exitCode -ne 0) { throw (Get-Message 'GameFailed' @($exitCode, $LogPath)) }
}

# -- The run -----------------------------------------------------------------------------------------

$previousOutputEncoding = Set-ConsoleUtf8
try {
    $architecture = Get-ComputerArchitecture
    if ($architecture -ne 'Arm64') { throw (Get-Message 'WrongComputer' $architecture) }
    $missingFile = Find-MissingFile
    if ($missingFile) { throw (Get-Message 'IncompletePackage' $missingFile) }
    # The code of the settings is loaded here, with a dot so that its functions and values become
    # those of this script, and not at the top: a package without that file gets the message above.
    . $SettingsLibraryPath
    $SettingsPath = Get-SettingsPath -UserDirectory $UserDirectory
    if ($PackageDirectory.Length -gt $MaximumPackagePathLength) { Write-Host (Get-Message 'PackagePathIsLong') }

    Open-Log
    Set-ToolchainEnvironment

    $discToExtract = Get-DiscToExtract -RequestedDisc $Disc
    if ($discToExtract) { Export-Disc -DiscPath $discToExtract }

    # The mods are built beside the modules, in the same folder or in the short one that replaces it.
    $modulesDirectory = Get-ModulesDirectory
    $builtModule = New-GameModule -ModulesDirectory $modulesDirectory
    $installedModule = Install-GameModule -ModulePath $builtModule
    Add-LogLine ('Module: ' + $installedModule)
    Initialize-Mods -BuildDirectory (Join-Path (Split-Path -Parent $modulesDirectory) 'mods-build')
    Write-Host (Get-Message 'GamePrepared')

    if (-not $PrepareOnly) {
        if ($Keyboard) { Start-GameWithKeyboard } else { Start-Launcher }
    }
}
catch {
    Add-LogLine ($_ | Out-String)
    Write-Host ''
    Write-Host (Get-Message 'Failed' $_.Exception.Message) -ForegroundColor Red
    exit 1
}
finally {
    Close-Log
    if ($previousOutputEncoding) { [Console]::OutputEncoding = $previousOutputEncoding }
}
exit 0
