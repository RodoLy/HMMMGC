#Requires -Version 5.1

<#
.SYNOPSIS
    Prepares the game from the player's own disc and opens its launcher.
    Prepara el juego con el disco del propio jugador y abre su launcher.

.DESCRIPTION
    The game is not in this package. It is recompiled here, on this computer, from the disc image
    that the player owns, so the first run does three things and later runs skip what is done:

      1. Reads the disc image and extracts the game's files (the launcher does it).
      2. Recompiles the game's program into a native module: DolRecomp translates it to C and
         the compiler in toolchain\ builds it. This takes several minutes.
      3. Puts the module where the launcher and the runner look for it, and opens the launcher.

    Nothing is installed and no setting of the computer is changed. Everything this script makes
    is in the folder %LOCALAPPDATA%\<UserDirectoryName> (see game.psd1), and a log of the last
    run is in its Logs folder.

    El juego no está en este paquete. Se recompila aquí, en esta computadora, con la imagen del
    disco que el jugador tiene, así que la primera ejecución hace tres cosas y las siguientes
    omiten lo que ya está hecho:

      1. Lee la imagen del disco y extrae los archivos del juego (lo hace el launcher).
      2. Recompila el programa del juego en un módulo nativo: DolRecomp lo traduce a C y el
         compilador de toolchain\ lo construye. Esto tarda varios minutos.
      3. Pone el módulo donde lo buscan el launcher y el runner, y abre el launcher.

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

    [switch] $PrepareOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# -- What is where ---------------------------------------------------------------------------------

$PackageDirectory = $PSScriptRoot
$ProgramsDirectory = Join-Path $PackageDirectory 'bin'
$ToolchainDirectory = Join-Path $PackageDirectory 'toolchain'
$Game = Import-PowerShellDataFile -LiteralPath (Join-Path $PackageDirectory 'game.psd1')

$LauncherPath = Join-Path $ProgramsDirectory ($Game.LauncherName + '.exe')
$PortToolPath = Join-Path $ProgramsDirectory 'moderngekko-port.exe'

# The same folders that the launcher and the runner use (the launcher picks %LOCALAPPDATA%\<name>).
$UserDirectory = Join-Path $env:LOCALAPPDATA $Game.UserDirectoryName
$ExtractedGameDirectory = Join-Path (Join-Path $UserDirectory 'games') $Game.GameId
$ExtractedGameProgram = Join-Path $ExtractedGameDirectory 'sys\main.dol'
$InstalledModulesDirectory = Join-Path $UserDirectory 'StaticRecompModules'
$LogPath = Join-Path $UserDirectory 'Logs\play.log'

# Every file whose absence means that the package was not extracted completely.
$RequiredFiles = @(
    $LauncherPath,
    $PortToolPath,
    (Join-Path $ProgramsDirectory 'moderngekko-run.exe'),
    (Join-Path $ProgramsDirectory 'dolrecomp.exe'),
    (Join-Path $ToolchainDirectory 'llvm-mingw\bin\clang.exe'),
    (Join-Path $ToolchainDirectory 'cmake\bin\cmake.exe'),
    (Join-Path $ToolchainDirectory 'ninja\ninja.exe'),
    (Join-Path $ToolchainDirectory 'python\python.exe')
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
        GamePrepared        = 'The game is ready.'
        OpeningLauncher     = 'Opening the launcher. Press Play in it to start the game.'
        LauncherFailed      = 'The launcher closed with an error (code {0}). The details are in {1}'
        Failed              = 'Something went wrong: {0}'
    }
    es = @{
        WrongComputer       = 'Este paquete es para computadoras con Windows en ARM (por ejemplo con un procesador Snapdragon X). Esta computadora es {0}.'
        IncompletePackage   = 'El paquete está incompleto: falta {0}. Extrae otra vez todo el archivo zip (completo, no solo algunos archivos) e inténtalo de nuevo.'
        PackagePathIsLong   = 'Este paquete está en una carpeta con una ruta muy larga. Si la preparación falla, mueve la carpeta completa a un lugar corto como C:\HarvestMoonMM e inténtalo de nuevo.'
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
        GamePrepared        = 'El juego está listo.'
        OpeningLauncher     = 'Abriendo el launcher. Pulsa Play en él para empezar el juego.'
        LauncherFailed      = 'El launcher se cerró con un error (código {0}). Los detalles están en {1}'
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
    $script:LogWriter = New-Object System.IO.StreamWriter($LogPath, $false, $withoutByteOrderMark)
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

# Shows one line of a program's output and keeps it in the log. The lines of the build that look
# like "[12/345] Building ..." become a progress bar, because there are thousands of them.
function Write-ProgramLine {
    param([string] $Line)
    Add-LogLine $Line
    if ($Line -match '^\[(\d+)/(\d+)\] ') {
        $percent = [int] (100 * [int] $Matches[1] / [int] $Matches[2])
        $status = $Line.Substring(0, [Math]::Min($Line.Length, 90))
        Write-Progress -Activity (Get-Message 'Recompiling') -Status $status -PercentComplete $percent
    }
    else {
        Write-Host $Line
    }
}

# Runs a program, shows what it says, and returns its exit code. What a program writes as
# warnings on its error output is not an error of this script, so errors do not stop it here.
function Invoke-Program {
    param(
        [Parameter(Mandatory)] [string] $FilePath,
        [string[]] $Arguments = @()
    )
    Add-LogLine ('> ' + $FilePath + ' ' + ($Arguments -join ' '))
    $previousPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        & $FilePath @Arguments 2>&1 | ForEach-Object { Write-ProgramLine ([string] $_) }
        return $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previousPreference
        Write-Progress -Activity (Get-Message 'Recompiling') -Completed
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
    Write-Host (Get-Message 'Recompiling')
    $modulesDirectory = Get-ModulesDirectory
    $arguments = @(
        'build', $ExtractedGameDirectory,
        '--backend', 'c',
        '--toolchain', 'clang',
        '--output', $modulesDirectory
    )
    $exitCode = Invoke-Program -FilePath $PortToolPath -Arguments $arguments
    if ($exitCode -ne 0) { throw (Get-Message 'RecompilationFailed' $LogPath) }
    # The tool writes the path of the module it considers current into this file.
    $activeModuleFile = Join-Path (Join-Path $modulesDirectory $Game.GameId) 'active-module.txt'
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

function Start-Launcher {
    Write-Host (Get-Message 'OpeningLauncher')
    & $LauncherPath
    if ($LASTEXITCODE -ne 0) { throw (Get-Message 'LauncherFailed' @($LASTEXITCODE, $LogPath)) }
}

# -- The run -----------------------------------------------------------------------------------------

$previousOutputEncoding = [Console]::OutputEncoding
try {
    # The programs of the package write UTF-8, and so do the paths in what they say.
    [Console]::OutputEncoding = New-Object System.Text.UTF8Encoding($false)

    $architecture = Get-ComputerArchitecture
    if ($architecture -ne 'Arm64') { throw (Get-Message 'WrongComputer' $architecture) }
    $missingFile = Find-MissingFile
    if ($missingFile) { throw (Get-Message 'IncompletePackage' $missingFile) }
    if ($PackageDirectory.Length -gt $MaximumPackagePathLength) { Write-Host (Get-Message 'PackagePathIsLong') }

    Open-Log
    Set-ToolchainEnvironment

    $discToExtract = Get-DiscToExtract -RequestedDisc $Disc
    if ($discToExtract) { Export-Disc -DiscPath $discToExtract }

    $builtModule = New-GameModule
    $installedModule = Install-GameModule -ModulePath $builtModule
    Add-LogLine ('Module: ' + $installedModule)
    Write-Host (Get-Message 'GamePrepared')

    if (-not $PrepareOnly) { Start-Launcher }
}
catch {
    Add-LogLine ($_ | Out-String)
    Write-Host ''
    Write-Host (Get-Message 'Failed' $_.Exception.Message) -ForegroundColor Red
    exit 1
}
finally {
    Close-Log
    [Console]::OutputEncoding = $previousOutputEncoding
}
exit 0
