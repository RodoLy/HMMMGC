#Requires -Version 5.1

<#
.SYNOPSIS
    Opens a window where the player chooses which mods are on and how fast the character moves.
    Abre una ventana donde el jugador elige qué mods están activos y qué tan rápido se mueve el
    personaje.

.DESCRIPTION
    The choices are saved in settings.json, in the player's folder (%LOCALAPPDATA%\<UserDirectoryName>,
    see game.psd1), which is the file that play.ps1 reads every time it starts the game. They take
    effect the next time the game is started with Play.bat or Play-Keyboard.bat; a game that is
    already open keeps the settings it started with. settings-file.ps1 says what the file holds.

    The window has:

      - One check box for each mod listed in game.psd1. A mod that is turned off is not installed,
        so the game is played without it.
      - The speed of the character: a slider and a number box, from 25% (very slow) to 300% (three
        times as fast), where 100% is the speed of the original game. It is only used while the
        speed mod is on.
      - Buttons to save, to save and start the game (with the keyboard, or through the launcher),
        and to go back to the defaults.

    Las elecciones se guardan en settings.json, en la carpeta del jugador
    (%LOCALAPPDATA%\<UserDirectoryName>, ver game.psd1), que es el archivo que play.ps1 lee cada vez
    que arranca el juego. Surten efecto la próxima vez que se arranca el juego con Play.bat o
    Play-Keyboard.bat; un juego que ya está abierto conserva los ajustes con los que arrancó.
    settings-file.ps1 dice qué contiene el archivo.

    La ventana tiene:

      - Una casilla por cada mod listado en game.psd1. Un mod apagado no se instala, así que el
        juego se juega sin él.
      - La velocidad del personaje: un deslizador y una casilla numérica, de 25% (muy lento) a 300%
        (tres veces más rápido), donde 100% es la velocidad del juego original. Solo se usa mientras
        el mod de velocidad está activo.
      - Botones para guardar, para guardar y arrancar el juego (con el teclado, o con el launcher) y
        para volver a los valores por omisión.
#>

[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# -- What is where ---------------------------------------------------------------------------------

$PackageDirectory = $PSScriptRoot
. (Join-Path $PackageDirectory 'settings-file.ps1')
$Game = Import-PowerShellDataFile -LiteralPath (Join-Path $PackageDirectory 'game.psd1')

$UserDirectory = Join-Path $env:LOCALAPPDATA $Game.UserDirectoryName
$SettingsPath = Get-SettingsPath -UserDirectory $UserDirectory
$PlayKeyboardPath = Join-Path $PackageDirectory 'Play-Keyboard.bat'
$PlayWithLauncherPath = Join-Path $PackageDirectory 'Play.bat'

# -- Texts: English and Spanish, chosen by the language of Windows -----------------------------------

$Messages = @{
    en = @{
        WindowTitle           = 'Settings - {0}'
        ModsHeading           = 'Mods'
        ModsExplanation       = 'A mod is a small change to the game. Turn off the ones you do not want.'
        SpeedHeading          = 'Character speed'
        SpeedExplanation      = '100% is the speed of the original game: more is faster, less is slower.'
        SpeedScale            = '{0}% slowest   -   100% original   -   {1}% fastest'
        SpeedWarning          = 'Above 200% the character may walk through thin walls.'
        SpeedModIsOff         = 'The speed mod is off, so the character moves at the original speed.'
        WhenItApplies         = 'The changes apply the next time the game is started with Play.bat or Play-Keyboard.bat.'
        ResetButton           = 'Defaults'
        SaveButton            = 'Save'
        SaveAndPlayButton     = 'Save and play (keyboard)'
        SaveAndLauncherButton = 'Save and open the launcher'
        CancelButton          = 'Close'
        Saved                 = 'Saved.'
        SaveFailed            = 'The settings could not be saved: {0}'
        ProblemsInFile        = 'Some values in {0} could not be used, and the defaults are shown for them: {1}'
        Failed                = 'Something went wrong: {0}'
    }
    es = @{
        WindowTitle           = 'Ajustes - {0}'
        ModsHeading           = 'Mods'
        ModsExplanation       = 'Un mod es un pequeño cambio al juego. Apaga los que no quieras.'
        SpeedHeading          = 'Velocidad del personaje'
        SpeedExplanation      = '100% es la velocidad del juego original: más es más rápido, menos es más lento.'
        SpeedScale            = '{0}% más lento   -   100% original   -   {1}% más rápido'
        SpeedWarning          = 'Por encima de 200% el personaje puede atravesar paredes delgadas.'
        SpeedModIsOff         = 'El mod de velocidad está apagado, así que el personaje se mueve a la velocidad original.'
        WhenItApplies         = 'Los cambios se aplican la próxima vez que se arranca el juego con Play.bat o Play-Keyboard.bat.'
        ResetButton           = 'Valores originales'
        SaveButton            = 'Guardar'
        SaveAndPlayButton     = 'Guardar y jugar (teclado)'
        SaveAndLauncherButton = 'Guardar y abrir el launcher'
        CancelButton          = 'Cerrar'
        Saved                 = 'Guardado.'
        SaveFailed            = 'No se pudieron guardar los ajustes: {0}'
        ProblemsInFile        = 'Algunos valores de {0} no se pudieron usar, y se muestran los valores por omisión para ellos: {1}'
        Failed                = 'Algo salió mal: {0}'
    }
}

# What each mod is called and does, for the check boxes. A mod that is not here (one added to
# game.psd1 and not yet described) is shown with its id.
$ModTexts = @{
    en = @{
        jamie_marriage = @{
            Name        = 'Marrying Jamie does not end the game'
            Description = 'After the wedding with Jamie the game goes on, instead of showing the epilogue and going back to the title screen.'
        }
        player_speed   = @{
            Name        = 'Faster character'
            Description = 'The character walks and runs at the speed chosen below, in free play.'
        }
    }
    es = @{
        jamie_marriage = @{
            Name        = 'Casarse con Jamie no termina el juego'
            Description = 'Después de la boda con Jamie el juego continúa, en lugar de mostrar el epílogo y volver a la pantalla del título.'
        }
        player_speed   = @{
            Name        = 'Personaje más rápido'
            Description = 'El personaje camina y corre a la velocidad que se elige abajo, en el juego libre.'
        }
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

function Get-ModText {
    param(
        [Parameter(Mandatory)] [string] $ModId,
        [Parameter(Mandatory)] [ValidateSet('Name', 'Description')] [string] $Field
    )
    $texts = $ModTexts[$Language]
    if ($texts.ContainsKey($ModId)) { return $texts[$ModId][$Field] }
    if ($Field -eq 'Name') { return $ModId }
    return ''
}

# -- The window --------------------------------------------------------------------------------------

function Show-ErrorBox {
    param([Parameter(Mandatory)] [string] $Text)
    [void] [System.Windows.Forms.MessageBox]::Show(
        $Text, $Form.Text,
        [System.Windows.Forms.MessageBoxButtons]::OK,
        [System.Windows.Forms.MessageBoxIcon]::Error)
}

# A label of text that wraps at $Width. The text may be empty: the line that says what the last click
# did starts empty, and PowerShell refuses an empty string for a mandatory parameter unless this
# says that it is allowed.
function New-TextLabel {
    param(
        [Parameter(Mandatory)] [AllowEmptyString()] [string] $Text,
        [Parameter(Mandatory)] [int] $Width,
        [switch] $Bold,
        [switch] $Muted,
        [int] $LeftMargin = 0,
        [int] $TopMargin = 0
    )
    $label = New-Object System.Windows.Forms.Label
    $label.Text = $Text
    $label.AutoSize = $true
    $label.MaximumSize = New-Object System.Drawing.Size($Width, 0)
    $label.Margin = New-Object System.Windows.Forms.Padding($LeftMargin, $TopMargin, 0, 4)
    if ($Bold) { $label.Font = New-Object System.Drawing.Font($Form.Font, [System.Drawing.FontStyle]::Bold) }
    if ($Muted) { $label.ForeColor = [System.Drawing.SystemColors]::GrayText }
    return $label
}

function New-WindowButton {
    param(
        [Parameter(Mandatory)] [string] $Text,
        [Parameter(Mandatory)] [scriptblock] $OnClick
    )
    $button = New-Object System.Windows.Forms.Button
    $button.Text = $Text
    $button.AutoSize = $true
    $button.Margin = New-Object System.Windows.Forms.Padding(0, 0, 8, 0)
    $button.Add_Click($OnClick)
    return $button
}

# The settings that the window shows at this moment.
function Get-ChosenSettings {
    $mods = [ordered] @{}
    foreach ($modId in $Game.ModIds) { $mods[$modId] = [bool] $ModCheckBoxes[$modId].Checked }
    return [pscustomobject] @{
        Mods               = $mods
        PlayerSpeedPercent = [int] $SpeedBox.Value
    }
}

# The speed controls only mean something while the speed mod is on.
function Update-SpeedControls {
    $isOn = $true
    if ($ModCheckBoxes.ContainsKey($PlayerSpeedModId)) { $isOn = [bool] $ModCheckBoxes[$PlayerSpeedModId].Checked }
    $SpeedSlider.Enabled = $isOn
    $SpeedBox.Enabled = $isOn
    $SpeedModIsOffLabel.Visible = -not $isOn
}

# Saves what the window shows. With $Then = 'Stay' the window stays open; with another value it
# closes after saving and the script starts what $script:AfterClosing says.
function Save-ChosenSettings {
    param([Parameter(Mandatory)] [ValidateSet('Stay', 'PlayWithKeyboard', 'OpenLauncher')] [string] $Then)
    try {
        Save-PlayerSettings -Path $SettingsPath -Settings (Get-ChosenSettings)
    }
    catch {
        Show-ErrorBox (Get-Message 'SaveFailed' $_.Exception.Message)
        return
    }
    if ($Then -eq 'Stay') {
        $StatusLabel.Text = Get-Message 'Saved'
        return
    }
    $script:AfterClosing = $Then
    $Form.Close()
}

function Reset-ChosenSettings {
    foreach ($modId in $Game.ModIds) { $ModCheckBoxes[$modId].Checked = $true }
    $SpeedBox.Value = $PlayerSpeedPercentDefault
    $StatusLabel.Text = ''
}

try {
    Add-Type -AssemblyName System.Windows.Forms
    Add-Type -AssemblyName System.Drawing
    [System.Windows.Forms.Application]::EnableVisualStyles()

    $loaded = Read-PlayerSettings -Path $SettingsPath -ModIds $Game.ModIds
    $initialSpeed = $PlayerSpeedPercentDefault
    if ($null -ne $loaded.Settings.PlayerSpeedPercent) { $initialSpeed = $loaded.Settings.PlayerSpeedPercent }

    $ContentWidth = 480
    $script:AfterClosing = 'Nothing'

    $Form = New-Object System.Windows.Forms.Form
    $Form.Text = Get-Message 'WindowTitle' $Game.GameName
    $Form.StartPosition = [System.Windows.Forms.FormStartPosition]::CenterScreen
    $Form.FormBorderStyle = [System.Windows.Forms.FormBorderStyle]::FixedDialog
    $Form.MaximizeBox = $false
    $Form.MinimizeBox = $false
    $Form.AutoSize = $true
    $Form.AutoSizeMode = [System.Windows.Forms.AutoSizeMode]::GrowAndShrink
    $Form.Padding = New-Object System.Windows.Forms.Padding(16)
    $Form.Font = New-Object System.Drawing.Font('Segoe UI', 9.5)

    # Everything goes in one column, from the top down.
    $Layout = New-Object System.Windows.Forms.FlowLayoutPanel
    $Layout.FlowDirection = [System.Windows.Forms.FlowDirection]::TopDown
    $Layout.WrapContents = $false
    $Layout.AutoSize = $true
    $Layout.AutoSizeMode = [System.Windows.Forms.AutoSizeMode]::GrowAndShrink
    $Form.Controls.Add($Layout)

    # The mods.
    $Layout.Controls.Add((New-TextLabel -Text (Get-Message 'ModsHeading') -Width $ContentWidth -Bold))
    $Layout.Controls.Add((New-TextLabel -Text (Get-Message 'ModsExplanation') -Width $ContentWidth -Muted))
    $ModCheckBoxes = @{}
    foreach ($modId in $Game.ModIds) {
        $checkBox = New-Object System.Windows.Forms.CheckBox
        $checkBox.AutoSize = $true
        $checkBox.Text = Get-ModText -ModId $modId -Field 'Name'
        $checkBox.Checked = [bool] $loaded.Settings.Mods[$modId]
        $checkBox.Margin = New-Object System.Windows.Forms.Padding(0, 8, 0, 0)
        $checkBox.Add_CheckedChanged({ Update-SpeedControls })
        $Layout.Controls.Add($checkBox)
        $ModCheckBoxes[$modId] = $checkBox
        $description = Get-ModText -ModId $modId -Field 'Description'
        if ($description) {
            $Layout.Controls.Add((New-TextLabel -Text $description -Width ($ContentWidth - 24) -Muted -LeftMargin 24))
        }
    }

    # The speed of the character.
    $Layout.Controls.Add((New-TextLabel -Text (Get-Message 'SpeedHeading') -Width $ContentWidth -Bold -TopMargin 16))
    $Layout.Controls.Add((New-TextLabel -Text (Get-Message 'SpeedExplanation') -Width $ContentWidth -Muted))

    $SpeedRow = New-Object System.Windows.Forms.FlowLayoutPanel
    $SpeedRow.FlowDirection = [System.Windows.Forms.FlowDirection]::LeftToRight
    $SpeedRow.WrapContents = $false
    $SpeedRow.AutoSize = $true
    $SpeedRow.AutoSizeMode = [System.Windows.Forms.AutoSizeMode]::GrowAndShrink

    $SpeedSlider = New-Object System.Windows.Forms.TrackBar
    $SpeedSlider.Minimum = $PlayerSpeedPercentMinimum
    $SpeedSlider.Maximum = $PlayerSpeedPercentMaximum
    $SpeedSlider.TickFrequency = 25
    $SpeedSlider.SmallChange = 5
    $SpeedSlider.LargeChange = 25
    $SpeedSlider.Width = 340
    $SpeedSlider.Value = $initialSpeed

    $SpeedBox = New-Object System.Windows.Forms.NumericUpDown
    $SpeedBox.Minimum = $PlayerSpeedPercentMinimum
    $SpeedBox.Maximum = $PlayerSpeedPercentMaximum
    $SpeedBox.Increment = 5
    $SpeedBox.Width = 64
    $SpeedBox.Value = $initialSpeed
    $SpeedBox.Margin = New-Object System.Windows.Forms.Padding(8, 8, 0, 0)

    $PercentLabel = New-TextLabel -Text '%' -Width 20 -TopMargin 12

    # The slider and the number box always show the same speed.
    $SpeedSlider.Add_ValueChanged({ $SpeedBox.Value = $SpeedSlider.Value })
    $SpeedBox.Add_ValueChanged({ $SpeedSlider.Value = [int] $SpeedBox.Value })

    $SpeedRow.Controls.Add($SpeedSlider)
    $SpeedRow.Controls.Add($SpeedBox)
    $SpeedRow.Controls.Add($PercentLabel)
    $Layout.Controls.Add($SpeedRow)

    $scale = Get-Message 'SpeedScale' @($PlayerSpeedPercentMinimum, $PlayerSpeedPercentMaximum)
    $Layout.Controls.Add((New-TextLabel -Text $scale -Width $ContentWidth -Muted))
    $Layout.Controls.Add((New-TextLabel -Text (Get-Message 'SpeedWarning') -Width $ContentWidth -Muted))
    $SpeedModIsOffLabel = New-TextLabel -Text (Get-Message 'SpeedModIsOff') -Width $ContentWidth -Muted
    $Layout.Controls.Add($SpeedModIsOffLabel)
    Update-SpeedControls

    # What happens with the choices, and what the last click did.
    $Layout.Controls.Add((New-TextLabel -Text (Get-Message 'WhenItApplies') -Width $ContentWidth -Muted -TopMargin 16))
    $StatusText = ''
    if ($loaded.Problems.Count -gt 0) {
        $StatusText = Get-Message 'ProblemsInFile' @($SettingsPath, ($loaded.Problems -join '; '))
    }
    $StatusLabel = New-TextLabel -Text $StatusText -Width $ContentWidth -Bold
    $Layout.Controls.Add($StatusLabel)

    # The buttons.
    $Buttons = New-Object System.Windows.Forms.FlowLayoutPanel
    $Buttons.FlowDirection = [System.Windows.Forms.FlowDirection]::LeftToRight
    $Buttons.WrapContents = $true
    $Buttons.AutoSize = $true
    $Buttons.AutoSizeMode = [System.Windows.Forms.AutoSizeMode]::GrowAndShrink
    $Buttons.MaximumSize = New-Object System.Drawing.Size($ContentWidth, 0)
    $Buttons.Margin = New-Object System.Windows.Forms.Padding(0, 12, 0, 0)

    $Buttons.Controls.Add((New-WindowButton -Text (Get-Message 'SaveButton') -OnClick { Save-ChosenSettings -Then 'Stay' }))
    $Buttons.Controls.Add((New-WindowButton -Text (Get-Message 'SaveAndPlayButton') -OnClick { Save-ChosenSettings -Then 'PlayWithKeyboard' }))
    $Buttons.Controls.Add((New-WindowButton -Text (Get-Message 'SaveAndLauncherButton') -OnClick { Save-ChosenSettings -Then 'OpenLauncher' }))
    $Buttons.Controls.Add((New-WindowButton -Text (Get-Message 'ResetButton') -OnClick { Reset-ChosenSettings }))
    $CloseButton = New-WindowButton -Text (Get-Message 'CancelButton') -OnClick { $Form.Close() }
    $Buttons.Controls.Add($CloseButton)
    $Layout.Controls.Add($Buttons)
    $Form.CancelButton = $CloseButton

    [void] $Form.ShowDialog()
    $Form.Dispose()

    # The window is closed: start the game if that is what the player asked for.
    if ($script:AfterClosing -eq 'PlayWithKeyboard') {
        Start-Process -FilePath $PlayKeyboardPath -WorkingDirectory $PackageDirectory
    }
    elseif ($script:AfterClosing -eq 'OpenLauncher') {
        Start-Process -FilePath $PlayWithLauncherPath -WorkingDirectory $PackageDirectory
    }
}
catch {
    Write-Host (Get-Message 'Failed' $_.Exception.Message) -ForegroundColor Red
    exit 1
}
exit 0
