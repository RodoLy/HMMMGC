#Requires -Version 5.1

<#
.SYNOPSIS
    The player's settings: which mods are on, how fast the character is and how fast the whole
    game runs. Settings.ps1 writes them and play.ps1 follows them.
    Los ajustes del jugador: qué mods están activos, qué tan rápido es el personaje y qué tan
    rápido corre todo el juego. Settings.ps1 los escribe y play.ps1 los sigue.

.DESCRIPTION
    This file is not run by itself. Settings.ps1 and play.ps1 load it with a dot
    (". .\settings-file.ps1"), so that both read and write the settings in the same way and agree on
    what is allowed.

    The settings are one small JSON file in the player's folder, settings.json, which anybody can
    edit with a text editor:

        {
          "mods": { "jamie_marriage": true, "player_speed": true },
          "playerSpeedPercent": 150,
          "gameSpeedPercent": 200
        }

      mods                a mod that is set to false is not installed, so the game is played without
                          it. A mod that is not listed is on.
      playerSpeedPercent  how fast the character walks and runs, in percent of the speed of the
                          original game (see the player_speed mod). If it is left out, the mod uses
                          its own default.
      gameSpeedPercent    how fast the whole game runs, scenes and dialogues included, in percent of
                          the normal speed: the fast-forward ("turbo") of an emulator. 100 is the
                          normal speed, and it is what a file without this value gets. The computer
                          may not reach a high value; then the game simply runs as fast as it can.
                          It is not a mod: play.ps1 gives it to the emulation core, as the
                          EmulationSpeed of the [Core] section of Config\Dolphin.ini.

    A file that is missing is the same as a file with nothing chosen. A value that makes no sense is
    ignored (and reported), and the rest of the file is still used: a typing mistake in one setting
    never stops the game from starting.

    Este archivo no se ejecuta solo. Settings.ps1 y play.ps1 lo cargan con un punto
    (". .\settings-file.ps1"), para que los dos lean y escriban los ajustes de la misma forma y
    estén de acuerdo en lo que se permite.

    Los ajustes son un archivo JSON pequeño en la carpeta del jugador, settings.json, que se puede
    editar con cualquier editor de texto (el ejemplo está arriba):

      mods                un mod que está en false no se instala, así que el juego se juega sin él.
                          Un mod que no aparece está activo.
      playerSpeedPercent  qué tan rápido camina y corre el personaje, en porcentaje de la velocidad
                          del juego original (ver el mod player_speed). Si se omite, el mod usa su
                          valor por omisión.
      gameSpeedPercent    qué tan rápido corre todo el juego, incluidas las escenas y los diálogos,
                          en porcentaje de la velocidad normal: el avance rápido ("turbo") de un
                          emulador. 100 es la velocidad normal, y es lo que recibe un archivo sin
                          este valor. Puede que la computadora no alcance un valor alto; entonces el
                          juego simplemente corre tan rápido como puede. No es un mod: play.ps1 se lo
                          da al núcleo de emulación, como el EmulationSpeed de la sección [Core] de
                          Config\Dolphin.ini.

    Un archivo que no existe equivale a un archivo sin nada elegido. Un valor que no tiene sentido se
    ignora (y se avisa), y el resto del archivo se sigue usando: un error de dedo en un ajuste nunca
    impide que el juego arranque.
#>

[Diagnostics.CodeAnalysis.SuppressMessageAttribute(
    'PSUseShouldProcessForStateChangingFunctions', '',
    Justification = 'The functions make and write the settings for Settings.ps1 and play.ps1, not commands for others to call.')]
[Diagnostics.CodeAnalysis.SuppressMessageAttribute(
    'PSUseDeclaredVarsMoreThanAssignments', '',
    Justification = 'Settings.ps1 and play.ps1 load this file with a dot and use the values that it sets.')]
param()

# -- What is allowed ---------------------------------------------------------------------------------

# The name of the file, inside the player's folder.
$SettingsFileName = 'settings.json'

# The mod whose speed these settings choose (one of the ModIds of game.psd1).
$PlayerSpeedModId = 'player_speed'

# The speeds that the player_speed mod accepts, in percent of the original speed (a test checks that
# these are the numbers of mods\player_speed\mod.c). 100 is the original speed.
$PlayerSpeedPercentMinimum = 25
$PlayerSpeedPercentMaximum = 300
$PlayerSpeedPercentDefault = 100

# How fast the whole game may run, in percent of the normal speed. 100 is the normal speed. The
# limit above is a choice and not a rule of the emulation core: four times the normal speed is more
# than most computers reach with this game, and more would only make the slider harder to use.
$GameSpeedPercentMinimum = 100
$GameSpeedPercentMaximum = 400
$GameSpeedPercentDefault = 100

# Where the emulation core keeps its own settings, inside the player's folder, and the one that
# holds how fast it runs (1 is the normal speed, 2 twice as fast).
$CoreSettingsRelativePath = 'Config\Dolphin.ini'
$CoreSettingsSection = 'Core'
$EmulationSpeedKey = 'EmulationSpeed'

# -- Functions ---------------------------------------------------------------------------------------

function Get-SettingsPath {
    param([Parameter(Mandatory)] [string] $UserDirectory)
    return Join-Path $UserDirectory $SettingsFileName
}

# The settings of a player who has chosen nothing: every mod is on, the speed of the character is
# not chosen ($null, which leaves it to the mod's default) and the game runs at its normal speed.
# The settings are an object with three properties: Mods (a table from mod id to $true or $false),
# PlayerSpeedPercent (a whole number or $null) and GameSpeedPercent (a whole number).
function New-DefaultSettingsSet {
    param([Parameter(Mandatory)] [AllowEmptyCollection()] [string[]] $ModIds)
    $mods = [ordered] @{}
    foreach ($modId in $ModIds) { $mods[$modId] = $true }
    return [pscustomobject] @{
        Mods               = $mods
        PlayerSpeedPercent = $null
        GameSpeedPercent   = $GameSpeedPercentDefault
    }
}

# True when $Value is a whole number from $Minimum to $Maximum. ConvertFrom-Json gives whole numbers
# as [int] or [long], and numbers with a decimal point as [double], which are refused.
function Test-WholeNumberInRange {
    param($Value, [int] $Minimum, [int] $Maximum)
    $isWholeNumber = ($Value -is [int]) -or ($Value -is [long])
    return $isWholeNumber -and $Value -ge $Minimum -and $Value -le $Maximum
}

# Reads the settings file. Returns an object with the Settings that were read and the Problems that
# were found (a list of texts, empty when all is well). Whatever cannot be used is left at its
# default, and the rest of the file is kept.
function Read-SettingsFile {
    param(
        [Parameter(Mandatory)] [string] $Path,
        [Parameter(Mandatory)] [AllowEmptyCollection()] [string[]] $ModIds
    )
    $settings = New-DefaultSettingsSet -ModIds $ModIds
    $problems = New-Object System.Collections.Generic.List[string]

    if (Test-Path -LiteralPath $Path -PathType Leaf) {
        try {
            $file = Get-Content -LiteralPath $Path -Raw -Encoding UTF8 | ConvertFrom-Json

            $modsProperty = $file.PSObject.Properties['mods']
            if ($modsProperty -and $modsProperty.Value) {
                foreach ($modId in $ModIds) {
                    $entry = $modsProperty.Value.PSObject.Properties[$modId]
                    if (-not $entry) { continue }
                    if ($entry.Value -is [bool]) {
                        $settings.Mods[$modId] = $entry.Value
                    }
                    else {
                        $problems.Add("mods.$modId must be true or false")
                    }
                }
            }

            $speedProperty = $file.PSObject.Properties['playerSpeedPercent']
            if ($speedProperty -and $null -ne $speedProperty.Value) {
                $speed = $speedProperty.Value
                if (Test-WholeNumberInRange -Value $speed -Minimum $PlayerSpeedPercentMinimum -Maximum $PlayerSpeedPercentMaximum) {
                    $settings.PlayerSpeedPercent = [int] $speed
                }
                else {
                    $problems.Add("playerSpeedPercent must be a whole number from $PlayerSpeedPercentMinimum to $PlayerSpeedPercentMaximum")
                }
            }

            $gameSpeedProperty = $file.PSObject.Properties['gameSpeedPercent']
            if ($gameSpeedProperty -and $null -ne $gameSpeedProperty.Value) {
                $gameSpeed = $gameSpeedProperty.Value
                if (Test-WholeNumberInRange -Value $gameSpeed -Minimum $GameSpeedPercentMinimum -Maximum $GameSpeedPercentMaximum) {
                    $settings.GameSpeedPercent = [int] $gameSpeed
                }
                else {
                    $problems.Add("gameSpeedPercent must be a whole number from $GameSpeedPercentMinimum to $GameSpeedPercentMaximum")
                }
            }
        }
        catch {
            $problems.Add($_.Exception.Message)
        }
    }

    return [pscustomobject] @{
        Settings = $settings
        Problems = $problems.ToArray()
    }
}

# Writes the settings file (UTF-8 without a byte order mark, like every file of this package), and
# creates the player's folder if it does not exist yet. A speed that was not chosen is left out of
# the file, so that the mod keeps using its default.
function Save-SettingsFile {
    param(
        [Parameter(Mandatory)] [string] $Path,
        [Parameter(Mandatory)] $Settings
    )
    $content = [ordered] @{ mods = $Settings.Mods }
    if ($null -ne $Settings.PlayerSpeedPercent) { $content['playerSpeedPercent'] = [int] $Settings.PlayerSpeedPercent }
    $content['gameSpeedPercent'] = [int] $Settings.GameSpeedPercent

    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Path) | Out-Null
    $withoutByteOrderMark = New-Object System.Text.UTF8Encoding($false)
    $json = ($content | ConvertTo-Json -Depth 3) + "`r`n"
    [System.IO.File]::WriteAllText($Path, $json, $withoutByteOrderMark)
}

# The ids of the mods that are on, in the order the settings have them.
function Get-EnabledModId {
    param([Parameter(Mandatory)] $Settings)
    return @($Settings.Mods.Keys | Where-Object { $Settings.Mods[$_] })
}

# -- How fast the whole game runs ----------------------------------------------------------------------

# The emulation speed that the core expects for a speed in percent: 100 -> "1", 250 -> "2.5". It is
# written with a point whatever the language of Windows is (a Spanish Windows would write "2,5",
# which the core does not read as a number).
function ConvertTo-EmulationSpeedText {
    param([Parameter(Mandatory)] [int] $Percent)
    return ($Percent / 100).ToString('0.##', [System.Globalization.CultureInfo]::InvariantCulture)
}

# Gives back the lines of an INI file with "$Key = $Value" in the section [$Section]: the line of
# that key is replaced if the section has one, it is added at the end of the section if not, and the
# section is added at the end of the file if the file does not have it. Every other line stays as it
# was. Section and key names are compared without regard to upper and lower case, as the core does.
function Set-IniValue {
    param(
        [Parameter(Mandatory)] [AllowEmptyCollection()] [AllowEmptyString()] [string[]] $Lines,
        [Parameter(Mandatory)] [string] $Section,
        [Parameter(Mandatory)] [string] $Key,
        [Parameter(Mandatory)] [string] $Value
    )
    $newLine = "$Key = $Value"
    $result = New-Object System.Collections.Generic.List[string]
    $inSection = $false
    $done = $false
    foreach ($line in $Lines) {
        $isHeader = $line -match '^\s*\[(.+)\]\s*$'
        if ($isHeader) {
            # Leaving the section without having found the key: the key goes at its end.
            if ($inSection -and -not $done) { $result.Add($newLine); $done = $true }
            $inSection = ($Matches[1].Trim() -eq $Section)
        }
        elseif ($inSection -and -not $done -and $line -match '^\s*([^=]+?)\s*=') {
            if ($Matches[1] -eq $Key) { $result.Add($newLine); $done = $true; continue }
        }
        $result.Add($line)
    }
    if (-not $done) {
        if (-not $inSection) { $result.Add("[$Section]") }
        $result.Add($newLine)
    }
    return , $result.ToArray()
}

# Tells the emulation core how fast to run the game, by writing EmulationSpeed in the core's own
# settings file in the player's folder (Config\Dolphin.ini). The core reads that file when the game
# starts, so the speed applies from the next start on. The file is created if the game has never
# been started; the core adds the rest of its settings to it then.
function Set-CoreEmulationSpeed {
    param(
        [Parameter(Mandatory)] [string] $UserDirectory,
        [Parameter(Mandatory)] [int] $Percent
    )
    $path = Join-Path $UserDirectory $CoreSettingsRelativePath
    $lines = @()
    if (Test-Path -LiteralPath $path -PathType Leaf) {
        $lines = @(Get-Content -LiteralPath $path -Encoding UTF8)
    }
    $speedText = ConvertTo-EmulationSpeedText -Percent $Percent
    $newLines = Set-IniValue -Lines $lines -Section $CoreSettingsSection -Key $EmulationSpeedKey -Value $speedText

    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $path) | Out-Null
    $withoutByteOrderMark = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($path, (($newLines -join "`r`n") + "`r`n"), $withoutByteOrderMark)
}
