# launch_link_loopback.ps1  -  Time Crisis II link play on ONE PC
#
# Starts two copies of the game side by side (RED cabinet + BLUE cabinet) and
# links them on this machine. Run it from the folder that contains
# mametc2.exe and roms\timecrs2.zip. Full guide: docs\LINKPLAY-LOOPBACK.md
#
# Usage:
#   .\launch_link_loopback.ps1                     # prompts for the launch delay
#   .\launch_link_loopback.ps1 -DelaySeconds 0.85  # RED starts 850 ms before BLUE
#   .\launch_link_loopback.ps1 -KillExisting       # close stuck game windows first
#   .\launch_link_loopback.ps1 -SoloListener       # RED instance only, no partner
#   .\launch_link_loopback.ps1 -SoloConnector      # BLUE instance only, no partner
#
# What it expects:
#   - mametc2.exe (or mame.exe) in the same folder as this script.
#   - roms\timecrs2.zip next to it (you supply the ROM - none is included).
#   - A second working folder for the BLUE cabinet (default: instance-b under
#     this folder). Created automatically on first run.
#
# FIRST-TIME SETUP (once per folder, inside the game):
#   - RED window:  Tab -> Machine Configuration -> Link ID -> Left/Red
#   - BLUE window: Tab -> Machine Configuration -> Link ID -> Right/Blue
#   - BOTH:        Tab -> DIP Switches -> Link Play Enabled -> On
#   Then close both windows and run this script again.
#
# Windows PowerShell 5.x compatible. ASCII only.

[CmdletBinding()]
param(
    [double]$DelaySeconds = -1,      # RED-before-BLUE launch stagger; -1 = ask
    [switch]$KillExisting,           # close any running MAME processes first
    [switch]$Log,                    # write diagnostic error.log files (off by default;
                                     # each run with -Log starts a fresh log per instance)
    [switch]$SoloListener,           # launch only the RED (listening) instance
    [switch]$SoloConnector,          # launch only the BLUE (connecting) instance
    [int]$ListenerPort = 9876,       # TCP port the RED instance listens on
    [int]$ListenerX  = 10,           # window positions (RED left, BLUE right)
    [int]$ListenerY  = 50,
    [int]$ConnectorX = 700,
    [int]$ConnectorY = 50,
    [switch]$NoPosition,             # skip the window placement
    [string]$MameDir = '',           # folder with mametc2.exe + roms\ (default: this script's folder)
    [string]$MameInstanceB = ''      # BLUE cabinet's working folder (default: <MameDir>\instance-b)
)

if ($SoloListener -and $SoloConnector) {
    throw "-SoloListener and -SoloConnector are mutually exclusive"
}
$soloMode = $SoloListener -or $SoloConnector

$ErrorActionPreference = 'Stop'

# Default locations: everything lives next to this script.
$scriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { (Get-Location).Path }
if ([string]::IsNullOrWhiteSpace($MameDir))       { $MameDir = $scriptDir }
if ([string]::IsNullOrWhiteSpace($MameInstanceB)) { $MameInstanceB = Join-Path $MameDir 'instance-b' }

# Win32 import for window positioning. Compile once per session.
if (-not ('Win32Window' -as [type])) {
    Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class Win32Window {
    [DllImport("user32.dll", SetLastError=true)]
    public static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter,
        int X, int Y, int cx, int cy, uint uFlags);
    public const uint SWP_NOSIZE     = 0x0001;
    public const uint SWP_NOZORDER   = 0x0004;
    public const uint SWP_NOACTIVATE = 0x0010;
}
"@
}

function Wait-AndMoveWindow {
    param(
        [System.Diagnostics.Process]$Process,
        [int]$X,
        [int]$Y,
        [int]$TimeoutMs = 15000,
        [string]$Label = ''
    )
    $start = [Environment]::TickCount
    while ($Process.MainWindowHandle -eq [IntPtr]::Zero) {
        if (([Environment]::TickCount - $start) -gt $TimeoutMs) {
            Write-Warning "Timed out waiting for $Label main window (PID $($Process.Id))"
            return
        }
        Start-Sleep -Milliseconds 50
        $Process.Refresh()
    }
    $flags = [Win32Window]::SWP_NOSIZE -bor `
             [Win32Window]::SWP_NOZORDER -bor `
             [Win32Window]::SWP_NOACTIVATE
    [void][Win32Window]::SetWindowPos($Process.MainWindowHandle, [IntPtr]::Zero, $X, $Y, 0, 0, $flags)
}

# Launch MAME with no console window but a normal game window.
function Start-MameHidden {
    param(
        [string]$ExePath,
        [string]$Arguments,
        [string]$WorkingDirectory
    )
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName         = $ExePath
    $psi.Arguments        = $Arguments
    $psi.WorkingDirectory = $WorkingDirectory
    $psi.UseShellExecute  = $false
    $psi.CreateNoWindow   = $true
    return [System.Diagnostics.Process]::Start($psi)
}

# Resolve the built MAME binary: the Time Crisis II build is mametc2.exe; a full
# MAME build is mame.exe. BOTH cabinets share this ONE binary (from MameDir); the
# BLUE instance differs only by its working folder, so its settings and log stay
# separate. It finds the ROM via -rompath (set below to MameDir\roms), so the
# second folder needs no exe and no ROM copy of its own.
function Resolve-MameExe {
    param([string]$Dir)
    foreach ($name in @('mametc2.exe', 'mame.exe')) {
        $p = Join-Path $Dir $name
        if (Test-Path $p) { return $p }
    }
    throw "No MAME binary (mametc2.exe / mame.exe) found in $Dir - see docs\BUILDING.md."
}

$mameExe = Resolve-MameExe -Dir $MameDir

# Create the BLUE cabinet's working folder if it does not exist yet.
if (-not $SoloListener -and -not (Test-Path $MameInstanceB)) {
    New-Item -ItemType Directory -Force $MameInstanceB | Out-Null
    Write-Host "Created second-cabinet folder $MameInstanceB" -ForegroundColor DarkGray
    Write-Host "(Remember the one-time setup in that window: Link ID Right/Blue + Link Play Enabled On.)" -ForegroundColor DarkGray
}

# Prompt for delay if not provided. Solo modes have one instance, so delay is moot.
if ($soloMode) {
    $DelaySeconds = 0
} elseif ($DelaySeconds -lt 0) {
    $reply = Read-Host 'Delay between RED and BLUE launch in seconds (Enter = 0.85)'
    if ([string]::IsNullOrWhiteSpace($reply)) { $DelaySeconds = 0.85 } else { $DelaySeconds = [double]$reply }
}

if ($KillExisting) {
    $running = Get-Process -Name 'mame','mametc2' -ErrorAction SilentlyContinue
    if ($running) {
        Write-Host "Closing $($running.Count) running MAME process(es)..." -ForegroundColor Yellow
        $running | Stop-Process -Force
        Start-Sleep -Milliseconds 500
    }
}

# Logging is OFF by default. With -Log, MAME writes diagnostic error.log files
# into each instance folder, and each run starts them fresh.
if ($Log) {
    $dirsToTruncate = if ($SoloListener)      { @($MameDir) }
                      elseif ($SoloConnector) { @($MameInstanceB) }
                      else                    { @($MameDir, $MameInstanceB) }
    foreach ($dir in $dirsToTruncate) {
        $log = Join-Path $dir 'error.log'
        Set-Content -Path $log -Value '' -NoNewline -Force
        Write-Host "Cleared $log" -ForegroundColor DarkGray
    }
}

# Window flags put both instances in the same compact 640x480 window.
$rompath       = Join-Path $MameDir 'roms'
$commonFlags   = '-window -nomaximize -resolution 640x480 -prescale 1 -nokeepaspect -skip_gameinfo'
if ($Log) { $commonFlags += ' -log' }
$listenerArgs  = "timecrs2 $commonFlags -comm_localport $ListenerPort -comm_remotehost `"`""
$connectorArgs = "timecrs2 $commonFlags -comm_remotehost 127.0.0.1 -comm_remoteport $ListenerPort -comm_localhost `"`" -rompath `"$rompath`""

Write-Host ''
if ($SoloListener)      { Write-Host "MODE: RED ONLY (no partner)" -ForegroundColor Magenta }
elseif ($SoloConnector) { Write-Host "MODE: BLUE ONLY (no partner)" -ForegroundColor Magenta }
else                    { Write-Host "MODE: LINKED (RED + BLUE on this PC)" -ForegroundColor Magenta }

$listenerProc = $null
$connectorProc = $null

if (-not $SoloConnector) {
    Write-Host "Launching RED cabinet (listens on port $ListenerPort)..." -ForegroundColor Cyan
    $listenerProc = Start-MameHidden -ExePath $mameExe -Arguments $listenerArgs -WorkingDirectory $MameDir
}

if (-not $soloMode -and $DelaySeconds -gt 0) {
    $delayMs = [int][math]::Round($DelaySeconds * 1000)
    Write-Host ("Waiting {0} ms so RED is listening first..." -f $delayMs) -ForegroundColor Yellow
    Start-Sleep -Milliseconds $delayMs
}

if (-not $SoloListener) {
    Write-Host "Launching BLUE cabinet (connects to 127.0.0.1:$ListenerPort)..." -ForegroundColor Cyan
    $connectorProc = Start-MameHidden -ExePath $mameExe -Arguments $connectorArgs -WorkingDirectory $MameInstanceB
}

if (-not $NoPosition) {
    Write-Host "Positioning window(s)..." -ForegroundColor DarkCyan
    if ($listenerProc)  { Wait-AndMoveWindow -Process $listenerProc  -X $ListenerX  -Y $ListenerY  -Label 'RED' }
    if ($connectorProc) { Wait-AndMoveWindow -Process $connectorProc -X $ConnectorX -Y $ConnectorY -Label 'BLUE' }
}

Write-Host ''
if ($SoloListener) {
    Write-Host 'RED cabinet launched (solo). Close the game window when done.' -ForegroundColor Green
    if ($Log) { Write-Host "Log: $MameDir\error.log" -ForegroundColor DarkGray }
} elseif ($SoloConnector) {
    Write-Host 'BLUE cabinet launched (solo). Close the game window when done.' -ForegroundColor Green
    if ($Log) { Write-Host "Log: $MameInstanceB\error.log" -ForegroundColor DarkGray }
} else {
    Write-Host 'Both cabinets launched - they pair up during the on-screen countdown.' -ForegroundColor Green
    Write-Host 'Close the game windows when done.' -ForegroundColor Green
    if ($Log) { Write-Host "Logs: $MameDir\error.log  +  $MameInstanceB\error.log" -ForegroundColor DarkGray }
}
