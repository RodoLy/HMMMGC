#Requires -Version 5.1

<#
.SYNOPSIS
    Abre el juego con el teclado, como Play-Keyboard.bat, y anota qué tan rápido corre y qué es lo
    que más trabaja mientras tanto.

    Starts the game with the keyboard, like Play-Keyboard.bat, and writes down how fast it runs
    and what is working hardest while it does.

.DESCRIPTION
    El juego informa su propia velocidad. Este script abre el juego, lee ese informe cada pocos
    segundos y agrega una línea a Logs\speed.log, en la carpeta del jugador. Cada línea dice también
    cuánto se usa el chip de gráficos (el motor "3D", solo de este juego), qué hilos del juego usan
    más procesador (el juego les pone nombre: "CPU-GPU thread", ...), el reloj del procesador (un
    procesador que va más lento con la batería, o cuando se calienta, hace que el juego corra más
    lento) y si la computadora funciona con batería o con corriente. La primera línea de cada
    medición dice además el modo de energía de Windows. Al cerrar el juego, el script agrega un
    resumen y, si el juego falló, lo que Windows anotó sobre la falla. Lo que el juego imprime en su
    consola queda al lado, en Logs\measure-runner-output.log y Logs\measure-runner-errors.log.

    El juego debe estar preparado antes: ejecuta Play-Keyboard.bat una vez.

    Una velocidad de 1.00 es la velocidad completa del juego. Por debajo, el juego corre más lento de
    lo que debe y el sonido puede entrecortarse o sonar raro. Si "frames" y "retraces" quedan exactamente
    iguales de una línea a la siguiente, el juego se detuvo un rato (por ejemplo, al cargar); la línea
    termina entonces con STOPPED.

    The game reports its own speed. This script starts the game, reads that report every few seconds
    and adds a line to Logs\speed.log in the player's folder. Each line also says how much the
    graphics chip is used (the "3D" engine, only for this game), which threads of the game use the
    most processor time (the game names them: "CPU-GPU thread", ...), the clock of the processor (a
    processor that runs slower on the battery, or when it heats up, makes the game run slower) and
    whether the computer runs on its battery or on the mains. The first line of each run says the
    power mode of Windows too. When the game is closed the script adds a summary and, if the game
    crashed, what Windows wrote down about the crash. What the game prints on its console is saved
    next to it, in Logs\measure-runner-output.log and Logs\measure-runner-errors.log.

    The game must have been prepared before: run Play-Keyboard.bat once.

    A speed of 1.00 is the full speed of the game. Below it the game runs slower than it should, and
    then the sound can crackle or sound strange. "Frames" and "retraces" that stay exactly the same
    from one line to the next mean that the game stopped for a while (for example while it loads); the
    line then ends with STOPPED.
#>

[Diagnostics.CodeAnalysis.SuppressMessageAttribute(
    'PSAvoidUsingWriteHost', '',
    Justification = 'This script talks to the person who runs it, at the console.')]
[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# -- What is where ---------------------------------------------------------------------------------

$Game = Import-PowerShellDataFile -LiteralPath (Join-Path $PSScriptRoot 'game.psd1')
$RunnerPath = Join-Path $PSScriptRoot 'bin\moderngekko-run.exe'
$UserDirectory = Join-Path $env:LOCALAPPDATA $Game.UserDirectoryName
$GameDirectory = Join-Path (Join-Path $UserDirectory 'games') $Game.GameId
$LogsDirectory = Join-Path $UserDirectory 'Logs'
$SpeedLogPath = Join-Path $LogsDirectory 'speed.log'
$RunnerOutputPath = Join-Path $LogsDirectory 'measure-runner-output.log'
$RunnerErrorsPath = Join-Path $LogsDirectory 'measure-runner-errors.log'

# While it runs, the game writes how it is doing into status.txt in this folder, several times a second.
$AutomationDirectory = Join-Path $env:TEMP 'HarvestMoonMM-automation'
$StatusPath = Join-Path $AutomationDirectory 'status.txt'

$SecondsBetweenSamples = 2
# A sample below this fraction of the full speed counts as slow.
$SlowSpeedLimit = 0.95
# A speed above this is not a speed: the game's report shows absurd values for a moment right after the
# game stood still. Such readings are left out of the summary and counted apart.
$ImpossibleSpeedLimit = 2.0
# How many of the busiest threads each line names.
$BusiestThreadsShown = 3
# A thread that used less than this fraction of a processor core is not worth naming.
$IdleThreadLimit = 0.03
$InvariantCulture = [System.Globalization.CultureInfo]::InvariantCulture

# -- Reading the names of threads ------------------------------------------------------------------------

# Windows lets a program give a name to each of its threads, and the game does. PowerShell cannot
# read those names by itself, so a small piece of code asks Windows for them.
$ThreadNamesAvailable = $true
try {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;

public static class ThreadNameReader
{
    private const uint QueryLimitedInformation = 0x0800;

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern IntPtr OpenThread(uint desiredAccess, bool inheritHandle, uint threadId);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool CloseHandle(IntPtr handle);

    [DllImport("kernel32.dll")]
    private static extern int GetThreadDescription(IntPtr thread, out IntPtr description);

    [DllImport("kernel32.dll")]
    private static extern IntPtr LocalFree(IntPtr memory);

    // The name of a thread, or an empty text if it has none or cannot be read.
    public static string Read(int threadId)
    {
        IntPtr threadHandle = OpenThread(QueryLimitedInformation, false, (uint)threadId);
        if (threadHandle == IntPtr.Zero) return "";
        try
        {
            IntPtr description;
            if (GetThreadDescription(threadHandle, out description) < 0) return "";
            try { return Marshal.PtrToStringUni(description) ?? ""; }
            finally { LocalFree(description); }
        }
        finally { CloseHandle(threadHandle); }
    }
}
'@
}
catch {
    $ThreadNamesAvailable = $false
    Write-Host "The names of the threads cannot be read here ($($_.Exception.Message)); the threads will be shown by number."
}

# -- Reading the power mode of Windows ---------------------------------------------------------------

# The "power mode" of Windows 11 (Settings > System > Power & battery) is not a power plan but a layer
# over it, and powercfg does not show it. Windows can tell it, so a small piece of code asks.
$PowerModeAvailable = $true
try {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;

public static class PowerModeReader
{
    [DllImport("powrprof.dll")]
    private static extern uint PowerGetEffectiveOverlayScheme(out Guid overlayScheme);

    // The identifier of the power mode in use. It throws if this Windows cannot say.
    public static Guid Read()
    {
        Guid overlayScheme;
        uint result = PowerGetEffectiveOverlayScheme(out overlayScheme);
        if (result != 0) throw new InvalidOperationException("Windows answered " + result);
        return overlayScheme;
    }
}
'@
}
catch {
    $PowerModeAvailable = $false
}

# The names Windows 11 gives to its power modes, by the identifier of each one.
$PowerModeNames = @{
    '961cc777-2547-4f9d-8174-7d86181b8a7a' = 'best power efficiency'
    '00000000-0000-0000-0000-000000000000' = 'balanced'
    '3af9b8d9-7c97-431d-ad78-34a8bfea439f' = 'better performance'
    'ded574b5-45a0-4f42-8737-46345c09c238' = 'best performance'
}

# -- Helpers -----------------------------------------------------------------------------------------

# Shows a line and adds it to the speed log.
function Write-SpeedLine {
    param([string] $Text)
    Write-Host $Text
    Add-Content -LiteralPath $SpeedLogPath -Value $Text
}

# The status of the game as a table of names and values, or $null while the file cannot be read
# (the game rewrites it all the time, so it can be locked for a moment).
function Read-GameStatus {
    try { $lines = Get-Content -LiteralPath $StatusPath -ErrorAction Stop }
    catch { return $null }
    $status = @{}
    foreach ($line in $lines) {
        $separatorIndex = $line.IndexOf('=')
        if ($separatorIndex -gt 0) {
            $status[$line.Substring(0, $separatorIndex)] = $line.Substring($separatorIndex + 1)
        }
    }
    return $status
}

# A number written by the game (it always uses a dot), or $null if the text is not a number.
function ConvertTo-GameNumber {
    param([string] $Text)
    $number = 0.0
    $styles = [System.Globalization.NumberStyles]::Float
    if ([double]::TryParse($Text, $styles, $InvariantCulture, [ref] $number)) { return $number }
    return $null
}

# A number with two decimals and a dot, or a dash when there is no number.
function Format-GameNumber {
    param($Number)
    if ($null -eq $Number) { return '-' }
    return $Number.ToString('0.00', $InvariantCulture)
}

# Whether the computer runs on its battery, or on the mains: it changes how fast it can go.
function Get-PowerSource {
    try {
        $battery = Get-CimInstance -ClassName Win32_Battery -ErrorAction Stop | Select-Object -First 1
        if ($null -eq $battery) { return 'no battery' }
        # 1 = discharging; the other values mean that the mains are connected.
        if ($battery.BatteryStatus -eq 1) { return 'battery' }
        return 'mains'
    }
    catch { return 'unknown' }
}

# The power mode in use ("balanced", "best performance"...), or "unknown" if Windows does not say.
function Get-PowerMode {
    if (-not $PowerModeAvailable) { return 'unknown' }
    try { $identifier = ([PowerModeReader]::Read()).ToString() }
    catch { return 'unknown' }
    if ($PowerModeNames.ContainsKey($identifier)) { return $PowerModeNames[$identifier] }
    return $identifier
}

# The clock of the processor as text such as "3417 MHz (100 %)": its speed in MHz and how much of its
# top speed that is. A processor that runs slower on the battery, or when it heats up, makes the game
# run slower. A dash if Windows does not say.
function Get-ProcessorClockText {
    try {
        $clock = Get-CimInstance -ClassName Win32_PerfFormattedData_Counters_ProcessorInformation `
            -Filter "Name = '_Total'" -ErrorAction Stop | Select-Object -First 1
        if ($null -eq $clock) { return '-' }
        return '{0} MHz ({1} %)' -f $clock.ProcessorFrequency, $clock.PercentProcessorPerformance
    }
    catch { return '-' }
}

# The middle value of some numbers.
function Get-Median {
    param([double[]] $Numbers)
    $sorted = @($Numbers | Sort-Object)
    $middle = [int][math]::Floor($sorted.Count / 2)
    if ($sorted.Count % 2 -eq 1) { return $sorted[$middle] }
    return ($sorted[$middle - 1] + $sorted[$middle]) / 2
}

# Adds to the speed log what Windows wrote down about a game that crashed (the "Application Error" entry
# of the event log), so that the cause can be looked up. The entry can take a moment to appear.
function Write-CrashReport {
    param([datetime] $Since)
    Start-Sleep -Seconds 3
    try {
        $entries = Get-WinEvent -ErrorAction Stop -FilterHashtable @{
            LogName = 'Application'; ProviderName = 'Application Error'; StartTime = $Since
        }
        $entry = @($entries | Where-Object { $_.Message -like '*moderngekko-run*' }) | Select-Object -First 1
    }
    catch { $entry = $null }
    if ($null -eq $entry) {
        Write-SpeedLine '--- Windows wrote nothing about this in the Application log'
        return
    }
    Write-SpeedLine '--- what Windows wrote down about the crash:'
    foreach ($reportLine in ($entry.Message -split "`r?`n")) {
        if ($reportLine.Trim() -ne '') { Write-SpeedLine ('    ' + $reportLine.Trim()) }
    }
}

# How many seconds of processor time each thread of the game has used so far, by thread number.
function Get-ThreadProcessorTime {
    param([System.Diagnostics.Process] $Process)
    $secondsByThread = @{}
    try {
        $Process.Refresh()
        foreach ($thread in $Process.Threads) {
            # A thread can end between the moment it is listed and the moment it is read.
            try { $secondsByThread[$thread.Id] = $thread.TotalProcessorTime.TotalSeconds }
            catch { continue }
        }
    }
    catch { return $secondsByThread }
    return $secondsByThread
}

# The name of a thread, "thread 1234" if it has none or the names cannot be read. Names are kept once
# they are known; a thread that had none when it was first seen is asked again, because a program
# names its threads a moment after it starts them.
$ThreadNames = @{}
function Get-ThreadName {
    param([int] $ThreadId)
    if ($ThreadNamesAvailable -and ((-not $ThreadNames.ContainsKey($ThreadId)) -or $ThreadNames[$ThreadId] -eq '')) {
        $ThreadNames[$ThreadId] = [ThreadNameReader]::Read($ThreadId)
    }
    if ($ThreadNames.ContainsKey($ThreadId) -and $ThreadNames[$ThreadId] -ne '') { return $ThreadNames[$ThreadId] }
    return "thread $ThreadId"
}

# The threads that used the most processor time between two readings, as text such as
# "CPU thread 0.97, GPU thread 0.05" (the number is the processor cores that the thread used).
function Format-BusiestThreadList {
    param($SecondsNow, $SecondsBefore, [double] $WallSeconds)
    # The first reading has nothing to be compared with.
    if ($SecondsBefore.Count -eq 0) { return 'first reading' }
    $usage = @()
    foreach ($threadId in $SecondsNow.Keys) {
        if (-not $SecondsBefore.ContainsKey($threadId)) { continue }
        $coresUsed = ($SecondsNow[$threadId] - $SecondsBefore[$threadId]) / $WallSeconds
        if ($coresUsed -ge $IdleThreadLimit) {
            $usage += [pscustomobject]@{ ThreadId = $threadId; Cores = $coresUsed }
        }
    }
    $busiest = @($usage | Sort-Object -Property Cores -Descending | Select-Object -First $BusiestThreadsShown)
    if ($busiest.Count -eq 0) { return 'all threads idle' }
    $descriptions = foreach ($thread in $busiest) {
        '{0} {1}' -f (Get-ThreadName $thread.ThreadId), (Format-GameNumber $thread.Cores)
    }
    return $descriptions -join ', '
}

# How much of the graphics chip the game uses, in percent of its 3D engine, or $null if Windows does
# not say (it needs the "GPU Engine" counters of Windows).
function Get-GameGraphicsPercent {
    param([int] $ProcessId)
    try {
        $samples = (Get-Counter -Counter '\GPU Engine(*)\Utilization Percentage' -ErrorAction Stop).CounterSamples
    }
    catch { return $null }
    $total = 0.0
    $found = $false
    foreach ($sample in $samples) {
        # The name of each counter says which program and which engine it is about.
        if ($sample.InstanceName -like "pid_${ProcessId}_*engtype_3D") {
            $total += $sample.CookedValue
            $found = $true
        }
    }
    if ($found) { return $total }
    return $null
}

# -- The run -----------------------------------------------------------------------------------------

foreach ($requiredPath in @($RunnerPath, $GameDirectory)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Not found: $requiredPath. Run Play-Keyboard.bat once first."
    }
}
New-Item -ItemType Directory -Force -Path $LogsDirectory, $AutomationDirectory | Out-Null
# A status left by an earlier run would look like a game that is running.
if (Test-Path -LiteralPath $StatusPath) { Remove-Item -LiteralPath $StatusPath }

$runnerArguments = @(
    '--game', $GameDirectory,
    '--user-dir', $UserDirectory,
    '--graphics', $Game.GraphicsBackend,
    '--automation-dir', $AutomationDirectory
) | ForEach-Object { '"' + $_ + '"' }

Write-SpeedLine ('--- {0}  power: {1}  power mode: {2}  graphics: {3}' -f `
    (Get-Date -Format 's'), (Get-PowerSource), (Get-PowerMode), $Game.GraphicsBackend)
Write-SpeedLine ('Close the game when you want to stop measuring. Columns: time, speed (1.00 = full), frames, retraces, ' +
    'processor cores in use, graphics chip use (3D engine, percent), processor clock (MHz and percent of its top ' +
    'speed), power source, busiest threads (cores each).')

$startedAt = Get-Date
$gameProcess = Start-Process -FilePath $RunnerPath -ArgumentList $runnerArguments -NoNewWindow -PassThru `
    -RedirectStandardOutput $RunnerOutputPath -RedirectStandardError $RunnerErrorsPath
# Asking for the handle keeps the exit code available once the game has closed (a quirk of Windows PowerShell 5.1).
$null = $gameProcess.Handle

$speeds = New-Object System.Collections.Generic.List[double]
$clock = [System.Diagnostics.Stopwatch]::StartNew()
$lastProcessorSeconds = 0.0
$lastClockSeconds = 0.0
$lastThreadSeconds = @{}
$lastFramesText = ''
$lastRetracesText = ''
$impossibleReadings = 0

while (-not $gameProcess.HasExited) {
    Start-Sleep -Seconds $SecondsBetweenSamples
    $status = Read-GameStatus
    if ($null -eq $status -or $status['state'] -ne 'running' -or $status['booted'] -ne '1') { continue }

    # The game can close between the check of the loop and here, and then its times cannot be read.
    try {
        $gameProcess.Refresh()
        $processorSeconds = $gameProcess.TotalProcessorTime.TotalSeconds
    }
    catch { break }
    $clockSeconds = $clock.Elapsed.TotalSeconds
    $wallSeconds = $clockSeconds - $lastClockSeconds
    $coresInUse = ($processorSeconds - $lastProcessorSeconds) / $wallSeconds
    $threadSeconds = Get-ThreadProcessorTime $gameProcess
    $busiestThreads = Format-BusiestThreadList $threadSeconds $lastThreadSeconds $wallSeconds
    $lastProcessorSeconds = $processorSeconds
    $lastClockSeconds = $clockSeconds
    $lastThreadSeconds = $threadSeconds

    $graphicsPercent = Get-GameGraphicsPercent $gameProcess.Id
    $graphicsText = if ($null -eq $graphicsPercent) { '-' } else { $graphicsPercent.ToString('0', $InvariantCulture) }

    # The game rewrites its frames and retraces per second all the time: the very same text twice in a
    # row means that nothing was drawn in between.
    $stopped = ($status['fps'] -eq $lastFramesText) -and ($status['vps'] -eq $lastRetracesText)
    $lastFramesText = $status['fps']
    $lastRetracesText = $status['vps']
    if ($stopped) { $busiestThreads = "$busiestThreads   STOPPED" }

    $speed = ConvertTo-GameNumber $status['speed']
    if ($null -ne $speed) {
        if ($speed -gt $ImpossibleSpeedLimit) { $impossibleReadings++ } else { $speeds.Add($speed) }
    }
    $line = ('{0,7:N1} s   speed {1,5}   frames {2,5}   retraces {3,5}   cores {4,5}   gpu {5,3} %   ' +
        'clock {6}   power {7}   {8}') -f `
        $clockSeconds, (Format-GameNumber $speed), (Format-GameNumber (ConvertTo-GameNumber $status['fps'])),
        (Format-GameNumber (ConvertTo-GameNumber $status['vps'])), (Format-GameNumber $coresInUse),
        $graphicsText, (Get-ProcessorClockText), (Get-PowerSource), $busiestThreads
    Write-SpeedLine $line
}

# -- Summary -----------------------------------------------------------------------------------------

$exitCode = $gameProcess.ExitCode
# A crash code is easier to look up in hexadecimal (0xC000041D) than as the negative number that
# Windows PowerShell shows.
$exitCodeHex = '0x{0:X8}' -f [BitConverter]::ToUInt32([BitConverter]::GetBytes([int] $exitCode), 0)
Write-SpeedLine ('--- the game closed with code {0} ({1})' -f $exitCode, $exitCodeHex)
if ($exitCode -ne 0) { Write-CrashReport $startedAt }
if ($speeds.Count -gt 0) {
    $summary = $speeds | Measure-Object -Average -Minimum -Maximum
    $slowSamples = @($speeds | Where-Object { $_ -lt $SlowSpeedLimit }).Count
    # The text is built first and shown after: the -f operator belongs to the text, not to Write-SpeedLine.
    $summaryText = ('--- {0} samples; speed lowest {1}, median {2}, average {3}, highest {4}; ' +
        '{5} of them below {6}') -f `
        $speeds.Count, (Format-GameNumber $summary.Minimum), (Format-GameNumber (Get-Median $speeds)),
        (Format-GameNumber $summary.Average), (Format-GameNumber $summary.Maximum), $slowSamples,
        (Format-GameNumber $SlowSpeedLimit)
    Write-SpeedLine $summaryText
}
else {
    Write-SpeedLine '--- no samples: the game closed before it started running'
}
if ($impossibleReadings -gt 0) {
    $impossibleText = ('--- {0} reading(s) above {1} were left out: the game reports absurd speeds right after ' +
        'it stood still') -f $impossibleReadings, (Format-GameNumber $ImpossibleSpeedLimit)
    Write-SpeedLine $impossibleText
}
