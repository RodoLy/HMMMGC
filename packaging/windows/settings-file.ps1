#Requires -Version 5.1

<#
.SYNOPSIS
    The player's settings: which mods are on and how fast the character is. Settings.ps1 writes
    them and play.ps1 follows them.
    Los ajustes del jugador: qué mods están activos y qué tan rápido es el personaje.
    Settings.ps1 los escribe y play.ps1 los sigue.

.DESCRIPTION
    This file is not run by itself. Settings.ps1 and play.ps1 load it with a dot
    (". .\settings-file.ps1"), so that both read and write the settings in the same way and agree on
    what is allowed.

    The settings are one small JSON file in the player's folder, settings.json, which anybody can
    edit with a text editor:

        {
          "mods": { "jamie_marriage": true, "player_speed": true },
          "playerSpeedPercent": 150
        }

      mods                a mod that is set to false is not installed, so the game is played without
                          it. A mod that is not listed is on.
      playerSpeedPercent  how fast the character walks and runs, in percent of the speed of the
                          original game (see the player_speed mod). If it is left out, the mod uses
                          its own default.

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

    Un archivo que no existe equivale a un archivo sin nada elegido. Un valor que no tiene sentido se
    ignora (y se avisa), y el resto del archivo se sigue usando: un error de dedo en un ajuste nunca
    impide que el juego arranque.
#>

# -- What is allowed ---------------------------------------------------------------------------------

# The name of the file, inside the player's folder.
$SettingsFileName = 'settings.json'

# The mod whose speed these settings choose (one of the ModIds of game.psd1).
$PlayerSpeedModId = 'player_speed'

# The speeds that the player_speed mod accepts, in percent of the original speed (a test checks that
# these are the numbers of mods\player_speed\mod.c). 100 is the original speed.
$PlayerSpeedPercentMinimum = 25
$PlayerSpeedPercentMaximum = 300
$PlayerSpeedPercentDefault = 150

# -- Functions ---------------------------------------------------------------------------------------

function Get-SettingsPath {
    param([Parameter(Mandatory)] [string] $UserDirectory)
    return Join-Path $UserDirectory $SettingsFileName
}

# The settings of a player who has chosen nothing: every mod is on and the speed is not chosen
# ($null), which leaves the speed to the mod's default. The settings are an object with two
# properties: Mods (a table from mod id to $true or $false) and PlayerSpeedPercent (a whole number
# or $null).
function New-DefaultSettings {
    param([Parameter(Mandatory)] [AllowEmptyCollection()] [string[]] $ModIds)
    $mods = [ordered] @{}
    foreach ($modId in $ModIds) { $mods[$modId] = $true }
    return [pscustomobject] @{
        Mods               = $mods
        PlayerSpeedPercent = $null
    }
}

# Reads the settings file. Returns an object with the Settings that were read and the Problems that
# were found (a list of texts, empty when all is well). Whatever cannot be used is left at its
# default, and the rest of the file is kept.
function Read-PlayerSettings {
    param(
        [Parameter(Mandatory)] [string] $Path,
        [Parameter(Mandatory)] [AllowEmptyCollection()] [string[]] $ModIds
    )
    $settings = New-DefaultSettings -ModIds $ModIds
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
                $isWholeNumber = ($speed -is [int]) -or ($speed -is [long])
                if ($isWholeNumber -and $speed -ge $PlayerSpeedPercentMinimum -and $speed -le $PlayerSpeedPercentMaximum) {
                    $settings.PlayerSpeedPercent = [int] $speed
                }
                else {
                    $problems.Add("playerSpeedPercent must be a whole number from $PlayerSpeedPercentMinimum to $PlayerSpeedPercentMaximum")
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
function Save-PlayerSettings {
    param(
        [Parameter(Mandatory)] [string] $Path,
        [Parameter(Mandatory)] $Settings
    )
    $content = [ordered] @{ mods = $Settings.Mods }
    if ($null -ne $Settings.PlayerSpeedPercent) { $content['playerSpeedPercent'] = [int] $Settings.PlayerSpeedPercent }

    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Path) | Out-Null
    $withoutByteOrderMark = New-Object System.Text.UTF8Encoding($false)
    $json = ($content | ConvertTo-Json -Depth 3) + "`r`n"
    [System.IO.File]::WriteAllText($Path, $json, $withoutByteOrderMark)
}

# The ids of the mods that are on, in the order the settings have them.
function Get-EnabledModIds {
    param([Parameter(Mandatory)] $Settings)
    return @($Settings.Mods.Keys | Where-Object { $Settings.Mods[$_] })
}
