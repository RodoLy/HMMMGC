#Requires -Version 5.1

<#
.SYNOPSIS
    The launcher of the native version (without Dolphin): a window to turn the mods on and off,
    choose the speed of the character and of the whole game, and start the game.
    El launcher de la versión nativa (sin Dolphin): una ventana para activar y desactivar los mods,
    elegir la velocidad del personaje y la de todo el juego, y arrancar el juego.

.DESCRIPTION
    The choices are kept in settings.json, in the player's folder (%LOCALAPPDATA%\HarvestMoonMM),
    the same file that the settings window of the ModernGekko version (Settings.ps1) writes, so both
    versions follow the same choices. Values this window does not show are kept as they are.

    "Play" saves the choices and starts hmmm-native.exe, next to this script, with:
      --mods       the mods that are on (or "none");
      --game-speed the speed of the whole game (100 to 400%; Tab held in the game: at least 300%);
      HM_MM_PLAYER_SPEED_PERCENT, which the player_speed mod reads;
      --disc       the disc that the ModernGekko package extracted (%LOCALAPPDATA%\HarvestMoonMM\games\G4AEE9).
    What the game prints goes to native.log, next to the program.

    Las elecciones se guardan en settings.json, en la carpeta del jugador, el mismo archivo que
    escribe la ventana de ajustes de la versión ModernGekko, así que las dos versiones siguen las
    mismas elecciones. "Jugar" guarda y arranca hmmm-native.exe; lo que imprime el juego va a
    native.log.
#>

[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

# -- What is where ---------------------------------------------------------------------------------

$ProgramDirectory = $PSScriptRoot
$Program = Join-Path $ProgramDirectory 'hmmm-native.exe'
$LogFile = Join-Path $ProgramDirectory 'native.log'
$Symbols = Join-Path $ProgramDirectory 'symbols.txt'
$PlayerDirectory = Join-Path $env:LOCALAPPDATA 'HarvestMoonMM'
$SettingsFile = Join-Path $PlayerDirectory 'settings.json'
$DiscDirectory = Join-Path $PlayerDirectory 'games\G4AEE9'

# The mods compiled into hmmm-native.exe (NATIVE_MODS in runtime/CMakeLists.txt).
$Mods = @(
    @{ Id = 'jamie_marriage'
       English = 'Marry Jamie: marrying Jamie no longer ends the game'
       Spanish = 'Casarse con Jamie: casarse con Jamie ya no termina el juego' },
    @{ Id = 'player_speed'
       English = 'Faster character: walks and runs at the speed below'
       Spanish = 'Personaje más rápido: camina y corre a la velocidad de abajo' }
)
$PlayerSpeedModId = 'player_speed'

# The same limits as the player_speed mod and Settings.ps1: 100 is the original game's speed.
$PlayerSpeedMinimum = 25
$PlayerSpeedMaximum = 300
$PlayerSpeedDefault = 150
# The same limits as Settings.ps1 and the runtime (game_clock.h): 100 is the console's speed.
$GameSpeedMinimum = 100
$GameSpeedMaximum = 400
$GameSpeedDefault = 100

# -- Texts -----------------------------------------------------------------------------------------

$Texts = @{
    en = @{
        Title       = 'Harvest Moon: Magical Melody (native)'
        ModsLabel   = 'Mods'
        SpeedLabel  = 'Speed of the character (100% is the original game):'
        GameSpeedLabel = 'Speed of the whole game (100% is normal; hold Tab in the game for turbo):'
        Play        = 'Play'
        Save        = 'Save'
        Close       = 'Close'
        Saved       = 'Saved.'
        NoProgram   = 'hmmm-native.exe is not next to this launcher.'
        NoDisc      = "The game's disc has not been extracted yet ({0}). Start the game once with Play.bat first."
    }
    es = @{
        Title       = 'Harvest Moon: Magical Melody (nativo)'
        ModsLabel   = 'Mods'
        SpeedLabel  = 'Velocidad del personaje (100% es la del juego original):'
        GameSpeedLabel = 'Velocidad de todo el juego (100% es la normal; mantén Tab en el juego para el turbo):'
        Play        = 'Jugar'
        Save        = 'Guardar'
        Close       = 'Cerrar'
        Saved       = 'Guardado.'
        NoProgram   = 'hmmm-native.exe no está junto a este launcher.'
        NoDisc      = 'El disco del juego todavía no se ha extraído ({0}). Arranca primero el juego una vez con Play.bat.'
    }
}
$Language = if ((Get-UICulture).TwoLetterISOLanguageName -eq 'es') { 'es' } else { 'en' }
$Text = $Texts[$Language]

# -- The settings file -----------------------------------------------------------------------------

function Test-WholeNumberInRange($Value, $Minimum, $Maximum) {
    return ($Value -is [int] -or $Value -is [long]) -and $Value -ge $Minimum -and $Value -le $Maximum
}

# Reads settings.json as a hashtable: mods (id -> true/false), playerSpeedPercent, and whatever
# else the file holds, which is written back unchanged.
function Read-Settings {
    $settings = [ordered] @{ mods = [ordered] @{}; playerSpeedPercent = $PlayerSpeedDefault; gameSpeedPercent = $GameSpeedDefault }
    foreach ($mod in $Mods) { $settings.mods[$mod.Id] = $true }
    if (-not (Test-Path -LiteralPath $SettingsFile)) {
        return $settings
    }
    try {
        $saved = Get-Content -LiteralPath $SettingsFile -Raw -Encoding UTF8 | ConvertFrom-Json
    } catch {
        return $settings  # an unreadable file: start from the defaults, and replace it on saving
    }
    foreach ($property in $saved.PSObject.Properties) {
        if ($property.Name -eq 'mods') {
            foreach ($entry in $property.Value.PSObject.Properties) {
                $settings.mods[$entry.Name] = [bool] $entry.Value
            }
        } else {
            $settings[$property.Name] = $property.Value
        }
    }
    if (-not (Test-WholeNumberInRange $settings.playerSpeedPercent $PlayerSpeedMinimum $PlayerSpeedMaximum)) {
        $settings.playerSpeedPercent = $PlayerSpeedDefault
    }
    if (-not (Test-WholeNumberInRange $settings.gameSpeedPercent $GameSpeedMinimum $GameSpeedMaximum)) {
        $settings.gameSpeedPercent = $GameSpeedDefault
    }
    return $settings
}

function Write-Settings($Settings) {
    New-Item -ItemType Directory -Force -Path $PlayerDirectory | Out-Null
    $json = ($Settings | ConvertTo-Json -Depth 3) + "`r`n"
    $withoutByteOrderMark = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($SettingsFile, $json, $withoutByteOrderMark)
}

# -- Starting the game -----------------------------------------------------------------------------

function Start-Game($Settings) {
    if (-not (Test-Path -LiteralPath $Program)) {
        [void] [System.Windows.Forms.MessageBox]::Show($Text.NoProgram, $Text.Title)
        return $false
    }
    if (-not (Test-Path -LiteralPath (Join-Path $DiscDirectory 'sys\main.dol'))) {
        [void] [System.Windows.Forms.MessageBox]::Show(($Text.NoDisc -f $DiscDirectory), $Text.Title)
        return $false
    }
    $enabled = @($Mods | Where-Object { $Settings.mods[$_.Id] } | ForEach-Object { $_.Id })
    $modList = if ($enabled.Count -gt 0) { $enabled -join ',' } else { 'none' }
    # The game inherits this process's environment: the player_speed mod reads the variable.
    $env:HM_MM_PLAYER_SPEED_PERCENT = [string] $Settings.playerSpeedPercent
    # cmd.exe sends both of the game's output streams to one log file.
    $gameSpeed = [int] $Settings.gameSpeedPercent
    $command = "`"`"$Program`" --disc `"$DiscDirectory`" --symbols `"$Symbols`" --mods $modList --game-speed $gameSpeed > `"$LogFile`" 2>&1`""
    Start-Process -FilePath "$env:SystemRoot\System32\cmd.exe" -ArgumentList '/c', $command `
        -WorkingDirectory $ProgramDirectory -WindowStyle Hidden
    return $true
}

# -- The window ------------------------------------------------------------------------------------

$Settings = Read-Settings

$Form = New-Object System.Windows.Forms.Form
$Form.Text = $Text.Title
$Form.FormBorderStyle = 'FixedDialog'
$Form.MaximizeBox = $false
$Form.StartPosition = 'CenterScreen'
$Form.AutoSize = $true
$Form.AutoSizeMode = 'GrowAndShrink'
$Form.Padding = New-Object System.Windows.Forms.Padding(12)

$Layout = New-Object System.Windows.Forms.FlowLayoutPanel
$Layout.FlowDirection = 'TopDown'
$Layout.AutoSize = $true
$Layout.WrapContents = $false
$Form.Controls.Add($Layout)

$ModsLabel = New-Object System.Windows.Forms.Label
$ModsLabel.Text = $Text.ModsLabel
$ModsLabel.AutoSize = $true
$ModsLabel.Font = New-Object System.Drawing.Font($Form.Font, [System.Drawing.FontStyle]::Bold)
$Layout.Controls.Add($ModsLabel)

$CheckBoxes = @{}
foreach ($mod in $Mods) {
    $box = New-Object System.Windows.Forms.CheckBox
    $box.Text = if ($Language -eq 'es') { $mod.Spanish } else { $mod.English }
    $box.AutoSize = $true
    $box.Checked = [bool] $Settings.mods[$mod.Id]
    $CheckBoxes[$mod.Id] = $box
    $Layout.Controls.Add($box)
}

$SpeedLabel = New-Object System.Windows.Forms.Label
$SpeedLabel.Text = $Text.SpeedLabel
$SpeedLabel.AutoSize = $true
$SpeedLabel.Margin = New-Object System.Windows.Forms.Padding(0, 12, 0, 4)
$Layout.Controls.Add($SpeedLabel)

# A row with a slider and a number box that follow each other, for a percentage. Returns the
# row; its number box (the value to read) is the row's Tag.
function New-PercentRow($Minimum, $Maximum, $Value) {
    $row = New-Object System.Windows.Forms.FlowLayoutPanel
    $row.AutoSize = $true
    $slider = New-Object System.Windows.Forms.TrackBar
    $slider.Minimum = $Minimum
    $slider.Maximum = $Maximum
    $slider.TickFrequency = 25
    $slider.SmallChange = 5
    $slider.LargeChange = 25
    $slider.Width = 280
    $slider.Value = $Value
    $number = New-Object System.Windows.Forms.NumericUpDown
    $number.Minimum = $Minimum
    $number.Maximum = $Maximum
    $number.Increment = 5
    $number.Width = 60
    $number.Value = $Value
    $percent = New-Object System.Windows.Forms.Label
    $percent.Text = '%'
    $percent.AutoSize = $true
    $percent.Margin = New-Object System.Windows.Forms.Padding(0, 6, 0, 0)
    $slider.Add_ValueChanged({ if ($number.Value -ne $slider.Value) { $number.Value = $slider.Value } }.GetNewClosure())
    $number.Add_ValueChanged({ if ($slider.Value -ne $number.Value) { $slider.Value = [int] $number.Value } }.GetNewClosure())
    $row.Controls.AddRange(@($slider, $number, $percent))
    $row.Tag = $number
    return $row
}

$SpeedRow = New-PercentRow $PlayerSpeedMinimum $PlayerSpeedMaximum ([int] $Settings.playerSpeedPercent)
$Layout.Controls.Add($SpeedRow)

$GameSpeedLabel = New-Object System.Windows.Forms.Label
$GameSpeedLabel.Text = $Text.GameSpeedLabel
$GameSpeedLabel.AutoSize = $true
$GameSpeedLabel.Margin = New-Object System.Windows.Forms.Padding(0, 12, 0, 4)
$Layout.Controls.Add($GameSpeedLabel)
$GameSpeedRow = New-PercentRow $GameSpeedMinimum $GameSpeedMaximum ([int] $Settings.gameSpeedPercent)
$Layout.Controls.Add($GameSpeedRow)

# The speed only matters while the speed mod is on.
$UpdateSpeedEnabled = { $SpeedRow.Enabled = $CheckBoxes[$PlayerSpeedModId].Checked }
$CheckBoxes[$PlayerSpeedModId].Add_CheckedChanged($UpdateSpeedEnabled)
& $UpdateSpeedEnabled

function Get-Choices {
    foreach ($mod in $Mods) { $Settings.mods[$mod.Id] = $CheckBoxes[$mod.Id].Checked }
    $Settings.playerSpeedPercent = [int] $SpeedRow.Tag.Value
    $Settings.gameSpeedPercent = [int] $GameSpeedRow.Tag.Value
    return $Settings
}

$Buttons = New-Object System.Windows.Forms.FlowLayoutPanel
$Buttons.AutoSize = $true
$Buttons.Margin = New-Object System.Windows.Forms.Padding(0, 12, 0, 0)
$PlayButton = New-Object System.Windows.Forms.Button
$PlayButton.Text = $Text.Play
$PlayButton.AutoSize = $true
$PlayButton.Add_Click({
    $choices = Get-Choices
    Write-Settings $choices
    if (Start-Game $choices) { $Form.Close() }
})
$SaveButton = New-Object System.Windows.Forms.Button
$SaveButton.Text = $Text.Save
$SaveButton.AutoSize = $true
$SaveButton.Add_Click({
    Write-Settings (Get-Choices)
    [void] [System.Windows.Forms.MessageBox]::Show($Text.Saved, $Text.Title)
})
$CloseButton = New-Object System.Windows.Forms.Button
$CloseButton.Text = $Text.Close
$CloseButton.AutoSize = $true
$CloseButton.Add_Click({ $Form.Close() })
$Buttons.Controls.AddRange(@($PlayButton, $SaveButton, $CloseButton))
$Layout.Controls.Add($Buttons)
$Form.AcceptButton = $PlayButton
$Form.CancelButton = $CloseButton

[void] $Form.ShowDialog()
