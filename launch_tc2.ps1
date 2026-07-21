# launch_tc2.ps1  -  Time Crisis II link play: the all-in-one launcher (P069)
#
# ONE script for every way to play. Interactive menu:
#
#   1 = LINK PLAY on this PC (two windows, loopback)
#   2 = LINK PLAY over the network - this PC is RED  (host/listener, waits)
#   3 = LINK PLAY over the network - this PC is BLUE (connector, dials RED)
#   C = change the two PCs' IP addresses (validated + saved)
#   M = toggle Ethernet/WiFi network mode (default Ethernet; saved; use the
#       SAME mode on both PCs - WiFi widens the in-game pairing window)
#   P = look up this PC's PUBLIC IP (api.ipify.org; for internet play)
#   V = per-cabinet DISPLAY settings (monitor / fullscreen / window size and
#       position for RED and BLUE independently; saved)
#   R = reset the display settings back to the script defaults
#   Enter = repeat the last-used choice (saved in launcher_settings.json)
#
# ZERO-SETUP FIRST RUN: a cabinet folder with no cfg\timecrs2.cfg is seeded
# with the correct identity (main folder = LEFT/RED, instance folder =
# RIGHT/BLUE) and the Link Play DIP already ON. Existing cfgs are verified;
# mismatches are fixed only after a Y/n prompt (auto-applied with -Unattended).
#
# HEADLESS (front-ends like BigBox, boot-to-game cabinets, custom wrappers):
#   powershell -ExecutionPolicy Bypass -File launch_tc2.ps1 `
#       -Mode Loopback -Unattended [-Fullscreen]
#   powershell -ExecutionPolicy Bypass -File launch_tc2.ps1 `
#       -Mode LanRed -ListenerIP 192.168.1.10 -ConnectorIP 192.168.1.11 `
#       -Network Ethernet -Unattended [-Fullscreen] [-WorkDir <cabinet folder>]
#   -Unattended never prompts: the choice must come from -Mode or the saved
#   settings, and cfg identity/DIP fixes are applied automatically.
#   -Log writes MAME's error.log in each cabinet folder (off by default).
#   Display params (P070; override the saved V-menu settings for the run):
#     -RedMonitor 1 / -BlueMonitor 2   which physical display (a number,
#         DISPLAY2, or \\.\DISPLAY2; MAME needs the exact device name)
#     -RedResolution 800x600 / -BlueResolution 800x600   windowed PICTURE
#         size (the launcher resizes the window itself - MAME treats
#         -resolution as a maximum only in windowed mode)
#     -RedFullscreen / -BlueFullscreen   per cabinet (-Fullscreen = both);
#         fullscreen keeps the desktop video mode (no -switchres by default).
#         LOOPBACK fullscreen cabinets run as BORDERLESS windows sized to the
#         monitor (two native-fullscreen MAME instances on one PC fight for
#         the display); LAN modes use MAME's native fullscreen (one instance
#         per PC, no conflict).
#     -BackgroundInput   loopback only: both windows keep receiving input
#         while unfocused (two-gun / two-mouse one-PC play; off by default
#         because a shared keyboard would drive both cabinets at once)
#
# How the two-PC rendezvous works (no clock/NTP sync needed - MAME's link does
# frame sync itself; the in-game partner-search countdown is the join window:
# counter from 300, a few seconds; WiFi mode widens it to 600/450 via
# NAMCOS23_PATCH_LINK_WAIT; the counter ticks ~60/s, user-measured):
#   1. RED opens a small control socket (port 9875) and WAITS.
#   2. BLUE retries a TCP connect to RED:9875 until it answers (this doubles
#      as the reachability test), runs a visible 3-2-1 countdown, sends "GO".
#   3. On "GO", RED launches its MAME and confirms it survived its first
#      seconds before replying "LISTENER_UP" (or "LISTENER_FAILED").
#   4. BLUE waits for the ack, pauses StaggerMs, then launches its MAME aimed
#      at RED's IP. The control socket closes; the game link runs on 9876.
#
# FIREWALL (one-time, RED PC only): allow inbound TCP 9875-9876.
#
# Windows PowerShell 5.x. ASCII only. Use ; not &&.

[CmdletBinding()]
param(
    [ValidateSet('Loopback','LanRed','LanBlue','')]
    [string]$Mode = '',                 # empty = interactive menu
    [double]$DelaySeconds = -1,         # loopback: RED->BLUE launch delay (prompted if omitted)
    [switch]$KillExisting,              # close any running MAME instances first
    [switch]$KeepLogs,                  # loopback: do not truncate error.log files
    [switch]$SoloListener,              # loopback debug: RED instance only
    [switch]$SoloConnector,             # loopback debug: BLUE instance only
    [int]$GamePort = 9876,              # MAME link port (C139 transport)
    [int]$ListenerX  = 10,              # window placement; relative to the cabinet's
    [int]$ListenerY  = 50,              #   monitor when one is configured
    [int]$ConnectorX = 700,
    [int]$ConnectorY = 50,
    [switch]$NoPosition,
    [int]$StaggerMs = 250,              # LAN blue: post-ack delay (default by -Network below)
    [string]$MameDir = '',              # where mametc2.exe + roms\ live (auto-detected)
    [string]$MameInstanceB = '',        # BLUE cabinet folder; empty = instance-b\ beside the script
    [string]$WorkDir = '',              # LAN: explicit cabinet folder (cfg\ nvram\ identity)
    [string]$ListenerIP = '',           # RED PC IP; empty = saved or built-in default
    [string]$ConnectorIP = '',          # BLUE PC IP; empty = saved or built-in default
    [ValidateSet('Ethernet','WiFi','')]
    [string]$Network = '',              # empty = saved or Ethernet (the default)
    [string]$RedMonitor = '',           # physical display for RED: 1, DISPLAY1, or \\.\DISPLAY1
    [string]$BlueMonitor = '',          # physical display for BLUE
    [string]$RedResolution = '',        # RED windowed size WxH (e.g. 800x600)
    [string]$BlueResolution = '',       # BLUE windowed size WxH
    [switch]$RedFullscreen,             # RED fullscreen (use -RedFullscreen:$false to force windowed)
    [switch]$BlueFullscreen,            # BLUE fullscreen
    [switch]$BackgroundInput,           # loopback: pass -background_input to BOTH instances
    [switch]$Log,                       # write MAME's error.log in each cabinet folder (off by default)
    [switch]$Unattended,                # headless: never prompt
    [switch]$Fullscreen                 # BOTH cabinets fullscreen (per-cabinet switches override)
)

$ErrorActionPreference = 'Stop'

# Keep failures readable when the script is double-clicked (the console would
# otherwise close before the error can be read). Skipped with -Unattended so
# headless runs never block on a prompt.
function Exit-WithPause {
    param([int]$Code = 1)
    if (-not $Unattended) { $null = Read-Host 'Press Enter to close this window' }
    exit $Code
}
trap {
    Write-Host ''
    Write-Host ("FATAL: {0}" -f $_.Exception.Message) -ForegroundColor Red
    if ($_.InvocationInfo -and $_.InvocationInfo.PositionMessage) { Write-Host $_.InvocationInfo.PositionMessage -ForegroundColor DarkGray }
    Exit-WithPause 1
}

if ($SoloListener -and $SoloConnector) { throw '-SoloListener and -SoloConnector are mutually exclusive' }

# ---- Locations -----------------------------------------------------------------
$scriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { (Get-Location).Path }
if ([string]::IsNullOrWhiteSpace($MameDir)) { $MameDir = $scriptDir }
if ([string]::IsNullOrWhiteSpace($MameInstanceB)) { $MameInstanceB = Join-Path $scriptDir 'instance-b' }
$ControlPort = 9875                 # the launcher's two-PC rendezvous channel
$DefaultListenerIP  = '192.168.1.10'   # placeholder - set the real IPs once via the C option
$DefaultConnectorIP = '192.168.1.11'   # placeholder - saved to launcher_settings.json thereafter
$WiFiListenerWaitSetting  = 60      # WiFi: RED counter 600 (NAMCOS23_PATCH_LINK_WAIT, P066+)
$WiFiConnectorWaitSetting = 45      # WiFi: BLUE counter 450 (launches later, ends together)

# ---- Small helpers ---------------------------------------------------------------
function Test-IPv4 { param([string]$s) $ip=$null; return ([System.Net.IPAddress]::TryParse($s, [ref]$ip) -and $ip.AddressFamily -eq 'InterNetwork') }

function Get-LocalIPv4List {
    $addrs = [System.Net.Dns]::GetHostAddresses([System.Net.Dns]::GetHostName())
    return $addrs | Where-Object { $_.AddressFamily -eq 'InterNetwork' } | ForEach-Object { $_.IPAddressToString }
}

# ---- Display helpers (P070) --------------------------------------------------------
# MAME 0.287 facts: -screen wants the EXACT OS device name (\\.\DISPLAY1, not an
# index); -resolution WxH sizes the window (windowed mode); fullscreen = absence
# of -window and keeps the desktop video mode (no -switchres unless asked); MAME
# has NO window-position option, so positioning stays external (SetWindowPos).

function Get-DisplayMonitors {
    if (-not ('System.Windows.Forms.Screen' -as [type])) { Add-Type -AssemblyName System.Windows.Forms }
    return [System.Windows.Forms.Screen]::AllScreens | Sort-Object DeviceName
}

# Accepts '2', 'DISPLAY2', or '\\.\DISPLAY2' -> canonical '\\.\DISPLAY2'; ''/auto -> ''.
function ConvertTo-MonitorDeviceName {
    param([string]$Value)
    $v = ([string]$Value).Trim()
    if ([string]::IsNullOrWhiteSpace($v) -or $v -match '^(?i)a(uto)?$') { return '' }
    if ($v -match '^\d+$') { return ('\\.\DISPLAY' + $v) }
    if ($v -match '^(?i)DISPLAY(\d+)$') { return ('\\.\DISPLAY' + $Matches[1]) }
    if ($v -match '^\\\\\.\\(?i)DISPLAY(\d+)$') { return ('\\.\DISPLAY' + $Matches[1]) }
    throw "Invalid monitor '$Value' - use a number (e.g. 2), DISPLAY2, or \\.\DISPLAY2."
}

function Test-ResolutionString { param([string]$s) return ($s -match '^\d{2,5}x\d{2,5}$') }

# Per-cabinet MAME display flags from a resolved display config hashtable.
function Get-MameDisplayFlags {
    param([hashtable]$Disp)
    $screenFlag = if (-not [string]::IsNullOrWhiteSpace($Disp.Monitor)) { (' -screen "{0}"' -f $Disp.Monitor) } else { '' }
    $logFlag = if ($Log) { ' -log' } else { '' }
    if ($Disp.Fullscreen) {
        return ('-nowindow -skip_gameinfo' + $logFlag + $screenFlag)
    }
    return ('-window -nomaximize -resolution {0}x{1} -prescale 1 -nokeepaspect -skip_gameinfo' -f $Disp.Width, $Disp.Height) + $logFlag + $screenFlag
}

# Where to place a cabinet's window. Positions are RELATIVE TO THE CABINET'S
# MONITOR when one is configured (what a user means by "monitor 2 at 400,50");
# with no monitor set they are plain desktop coordinates. Explicit X/Y wins;
# auto = the classic side-by-side defaults, or $null for LAN (auto = leave the
# window where MAME puts it).
function Get-CabinetWindowPos {
    param([hashtable]$Disp, [int]$DefaultX, [int]$DefaultY, [bool]$AutoUsesDefault)
    $x = $null; $y = $null
    if ($null -ne $Disp.X) {
        $x = [int]$Disp.X; $y = [int]$Disp.Y
    } elseif ($AutoUsesDefault) {
        $x = $DefaultX; $y = $DefaultY
    } else {
        return $null
    }
    if (-not [string]::IsNullOrWhiteSpace($Disp.Monitor)) {
        $s = Get-DisplayMonitors | Where-Object { $_.DeviceName -eq $Disp.Monitor } | Select-Object -First 1
        if ($s) { $x += $s.Bounds.X; $y += $s.Bounds.Y }
    }
    return @{ X = $x; Y = $y }
}

function Resolve-MameExe {
    param([string]$Dir)
    foreach ($name in @('mametc2.exe', 'mame.exe')) {
        $p = Join-Path $Dir $name
        if (Test-Path $p) { return $p }
    }
    throw "No MAME binary (mametc2.exe / mame.exe) found in $Dir - build it first."
}

function Start-MameHidden {
    param([string]$ExePath, [string]$Arguments, [string]$WorkingDirectory, [hashtable]$EnvVars = @{})
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName         = $ExePath
    $psi.Arguments        = $Arguments
    $psi.WorkingDirectory = $WorkingDirectory
    $psi.UseShellExecute  = $false
    $psi.CreateNoWindow   = $true
    foreach ($k in $EnvVars.Keys) { $psi.EnvironmentVariables[$k] = [string]$EnvVars[$k] }
    return [System.Diagnostics.Process]::Start($psi)
}

# After launching, verify the child survived its first seconds. A missing ROM /
# bad rompath kills MAME instantly, but with a hidden console the error is
# invisible and the script would otherwise claim success.
function Test-MameAlive {
    param([System.Diagnostics.Process]$Process, [string]$Label, [string]$ManualCmd)
    Start-Sleep -Seconds 3
    $Process.Refresh()
    if ($Process.HasExited) {
        Write-Host ("ERROR: {0} MAME exited immediately (exit code {1})." -f $Label, $Process.ExitCode) -ForegroundColor Red
        Write-Host '       Usual causes: roms\timecrs2.zip missing next to the exe, or a wrong -rompath.' -ForegroundColor Red
        Write-Host '       See the real error by running MAME with a visible console:' -ForegroundColor Red
        Write-Host ("       {0}" -f $ManualCmd) -ForegroundColor Yellow
        return $false
    }
    return $true
}

# Win32 import for loopback window positioning. Compile once per session.
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

# Wait for a process's main window handle (shared by the placement helpers).
function Wait-ForMainWindow {
    param([System.Diagnostics.Process]$Process, [int]$TimeoutMs = 15000, [string]$Label = '')
    $start = [Environment]::TickCount
    while ($Process.MainWindowHandle -eq [IntPtr]::Zero) {
        if (([Environment]::TickCount - $start) -gt $TimeoutMs) {
            Write-Warning "Timed out waiting for $Label main window (PID $($Process.Id))"
            return [IntPtr]::Zero
        }
        Start-Sleep -Milliseconds 50
        $Process.Refresh()
    }
    return $Process.MainWindowHandle
}

# Window/client rectangle access. Separate class so consoles that loaded an
# older script version do not hit a stale type missing these members.
if (-not ('Win32Metrics' -as [type])) {
    Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class Win32Metrics {
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }
    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);
    [DllImport("user32.dll")]
    public static extern bool GetClientRect(IntPtr hWnd, out RECT lpRect);
}
"@
}

# Place AND size a windowed cabinet. ClientW/H is the game PICTURE size - MAME
# treats -resolution as a maximum only in windowed mode (the window always
# opens at the native 640x480), so the launcher resizes externally, adding the
# measured frame/title margins so the CLIENT area lands on the requested size.
# X/Y $null = keep the window where it is (size-only change).
function Set-CabinetWindow {
    param([System.Diagnostics.Process]$Process, $X, $Y, [int]$ClientW, [int]$ClientH, [int]$TimeoutMs = 15000, [string]$Label = '')
    $hwnd = Wait-ForMainWindow -Process $Process -TimeoutMs $TimeoutMs -Label $Label
    if ($hwnd -eq [IntPtr]::Zero) { return }
    $wr = New-Object Win32Metrics+RECT
    $cr = New-Object Win32Metrics+RECT
    [void][Win32Metrics]::GetWindowRect($hwnd, [ref]$wr)
    [void][Win32Metrics]::GetClientRect($hwnd, [ref]$cr)
    $extraW = ($wr.Right - $wr.Left) - ($cr.Right - $cr.Left)
    $extraH = ($wr.Bottom - $wr.Top) - ($cr.Bottom - $cr.Top)
    $nx = if ($null -ne $X) { [int]$X } else { $wr.Left }
    $ny = if ($null -ne $Y) { [int]$Y } else { $wr.Top }
    $flags = [Win32Window]::SWP_NOZORDER -bor [Win32Window]::SWP_NOACTIVATE
    [void][Win32Window]::SetWindowPos($hwnd, [IntPtr]::Zero, $nx, $ny, ($ClientW + $extraW), ($ClientH + $extraH), $flags)
}

# Style access for the borderless conversion (separate class from Win32Window so
# consoles that loaded an older script version don't hit a stale type).
if (-not ('Win32Style' -as [type])) {
    Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class Win32Style {
    [DllImport("user32.dll", SetLastError=true)]
    public static extern int GetWindowLong(IntPtr hWnd, int nIndex);
    [DllImport("user32.dll", SetLastError=true)]
    public static extern int SetWindowLong(IntPtr hWnd, int nIndex, int dwNewLong);
}
"@
}

# Turn a MAME window into a borderless window filling the given monitor bounds.
# Used for loopback "fullscreen": two native-fullscreen MAME instances on one PC
# fight for the display (user-found), borderless windows coexist cleanly.
function Set-BorderlessWindow {
    param([System.Diagnostics.Process]$Process, [int]$X, [int]$Y, [int]$W, [int]$H, [int]$TimeoutMs = 15000, [string]$Label = '')
    $hwnd = Wait-ForMainWindow -Process $Process -TimeoutMs $TimeoutMs -Label $Label
    if ($hwnd -eq [IntPtr]::Zero) { return }
    $GWL_STYLE = -16
    # Strip WS_CAPTION|WS_THICKFRAME|WS_SYSMENU|WS_MINIMIZEBOX|WS_MAXIMIZEBOX = 0x00CF0000
    $style = [Win32Style]::GetWindowLong($hwnd, $GWL_STYLE)
    [void][Win32Style]::SetWindowLong($hwnd, $GWL_STYLE, ($style -band (-bnot 0x00CF0000)))
    $SWP_FRAMECHANGED = 0x0020
    $SWP_SHOWWINDOW   = 0x0040
    $flags = $SWP_FRAMECHANGED -bor $SWP_SHOWWINDOW -bor [Win32Window]::SWP_NOZORDER -bor [Win32Window]::SWP_NOACTIVATE
    [void][Win32Window]::SetWindowPos($hwnd, [IntPtr]::Zero, $X, $Y, $W, $H, $flags)
}

# The monitor a cabinet's display config lands on (configured one if attached,
# else the primary).
function Get-CabinetMonitor {
    param([hashtable]$Disp)
    $screens = @(Get-DisplayMonitors)
    if (-not [string]::IsNullOrWhiteSpace($Disp.Monitor)) {
        $s = $screens | Where-Object { $_.DeviceName -eq $Disp.Monitor } | Select-Object -First 1
        if ($s) { return $s }
    }
    return ($screens | Where-Object { $_.Primary } | Select-Object -First 1)
}

# ---- TC2 banner ------------------------------------------------------------------
function Show-TC2Banner {
    $title = @(
        ' _____  ___  __  __  _____      ____  ____   ___  ____   ___  ____      ___  ___ ',
        '|_   _||_ _||  \/  || ____|    / ___||  _ \ |_ _|/ ___| |_ _|/ ___|    |_ _||_ _|',
        '  | |   | | | |\/| ||  _|     | |    | |_) | | | \___ \  | | \___ \     | |  | | ',
        '  | |   | | | |  | || |___    | |___ |  _ <  | |  ___) | | |  ___) |    | |  | | ',
        '  |_|  |___||_|  |_||_____|    \____||_| \_\|___||____/ |___||____/    |___||___|'
    )
    $link = @(
        ' _      ___  _   _  _  __      ____   _         _    __   __',
        '| |    |_ _|| \ | || |/ /     |  _ \ | |       / \   \ \ / /',
        '| |     | | |  \| || '' /      | |_) || |      / _ \   \ V / ',
        '| |___  | | | |\  || . \      |  __/ | |___  / ___ \   | |  ',
        '|_____||___||_| \_||_|\_\     |_|    |_____|/_/   \_\  |_|  '
    )
    $url = 'github.com/pocketjazzy/MAME-TC2-Public'
    $w = $title[0].Length
    $border = '=' * ($w + 6)
    $blank  = '||' + (' ' * ($w + 4)) + '||'
    Write-Host ''
    Write-Host ('/' + $border + '\') -ForegroundColor Red
    Write-Host $blank -ForegroundColor Red
    foreach ($line in $title) { Write-Host ('||  ' + $line.PadRight($w) + '  ||') -ForegroundColor Red }
    Write-Host $blank -ForegroundColor Red
    foreach ($line in $link) {
        $pad = $w - $line.Length
        $l = [int][math]::Floor($pad / 2)
        Write-Host ('||  ' + (' ' * $l) + $line + (' ' * ($pad - $l)) + '  ||') -ForegroundColor Red
    }
    Write-Host $blank -ForegroundColor Red
    $pad = $w - $url.Length
    $l = [int][math]::Floor($pad / 2)
    Write-Host ('||  ' + (' ' * $l) + $url + (' ' * ($pad - $l)) + '  ||') -ForegroundColor DarkGray
    Write-Host ('\' + $border + '/') -ForegroundColor Red
    Write-Host ''
}

# ---- Cabinet-cfg bootstrap --------------------------------------------------------
# Seed (first run) or verify (later runs) a cabinet folder's cfg\timecrs2.cfg.
# Identity lives in port :JVS_PLAYER1 mask 16384 (value 16384 = RIGHT/BLUE,
# absent/0 = LEFT/RED); the link DIP is port :DSW mask 8 (value 0 = Link Play ON,
# 8 = OFF, and ABSENT means the default = OFF). Verified against the known-good
# red/blue dev pair 2026-07-15. Existing cfgs are never rewritten without a
# prompt; unparseable ones are left alone with a warning.
function Initialize-CabinetCfg {
    param(
        [string]$Dir,
        [ValidateSet('red','blue')][string]$Identity,
        [switch]$AutoYes               # headless: apply fixes without prompting
    )
    $label   = if ($Identity -eq 'red') { 'LEFT/RED' } else { 'RIGHT/BLUE' }
    $cfgDir  = Join-Path $Dir 'cfg'
    $cfgFile = Join-Path $cfgDir 'timecrs2.cfg'

    if (-not (Test-Path $cfgFile)) {
        if (-not (Test-Path $cfgDir)) { New-Item -ItemType Directory -Force $cfgDir | Out-Null }
        $lines = @(
            '<?xml version="1.0"?>',
            '<mameconfig version="10">',
            '    <system name="timecrs2">',
            '        <input>',
            '            <port tag=":DSW" type="DIPSWITCH" mask="8" defvalue="8" value="0" />'
        )
        if ($Identity -eq 'blue') {
            $lines += '            <port tag=":JVS_PLAYER1" type="CONFIG" mask="16384" defvalue="0" value="16384" />'
        }
        $lines += @('        </input>', '    </system>', '</mameconfig>')
        Set-Content -Path $cfgFile -Value $lines -Encoding UTF8
        Write-Host ("First-run setup: seeded {0} cabinet identity (Link Play DIP ON) into {1} - no Tab-menu setup needed." -f $label, $cfgFile) -ForegroundColor Green
        return
    }

    $xml = $null
    try { $xml = [xml](Get-Content $cfgFile -Raw) } catch {
        Write-Host ("WARNING: could not parse {0} - leaving it alone (cabinet identity unverified)." -f $cfgFile) -ForegroundColor Yellow
        return
    }
    $sys = $xml.SelectSingleNode("/mameconfig/system[@name='timecrs2']")
    if ($null -eq $sys) {
        Write-Host ("WARNING: {0} has no timecrs2 section - leaving it alone (cabinet identity unverified)." -f $cfgFile) -ForegroundColor Yellow
        return
    }
    $inputNode = $sys.SelectSingleNode('input')
    $jvs = if ($inputNode) { $inputNode.SelectSingleNode("port[@tag=':JVS_PLAYER1' and @mask='16384']") } else { $null }
    $dsw = if ($inputNode) { $inputNode.SelectSingleNode("port[@tag=':DSW' and @mask='8']") } else { $null }
    $curIdentity = if ($jvs -and $jvs.GetAttribute('value') -eq '16384') { 'blue' } else { 'red' }
    $dipOn = ($dsw -and $dsw.GetAttribute('value') -eq '0')
    $dirty = $false

    if ($curIdentity -ne $Identity) {
        $curLabel = if ($curIdentity -eq 'red') { 'LEFT/RED' } else { 'RIGHT/BLUE' }
        Write-Host ("NOTE: {0} is configured as {1}, but this launch needs the {2} cabinet." -f $cfgFile, $curLabel, $label) -ForegroundColor Yellow
        $r = if ($AutoYes) { Write-Host '      (-Unattended: fixing automatically)' -ForegroundColor DarkGray; 'Y' } else { Read-Host '      Fix it now? (Y/n)' }
        if ([string]::IsNullOrWhiteSpace($r) -or $r -match '^[Yy]') {
            if ($null -eq $inputNode) { $inputNode = $sys.AppendChild($xml.CreateElement('input')) }
            if ($null -eq $jvs) {
                $jvs = $xml.CreateElement('port')
                $jvs.SetAttribute('tag', ':JVS_PLAYER1')
                $jvs.SetAttribute('type', 'CONFIG')
                $jvs.SetAttribute('mask', '16384')
                $jvs.SetAttribute('defvalue', '0')
                [void]$inputNode.AppendChild($jvs)
            }
            $jvs.SetAttribute('value', $(if ($Identity -eq 'blue') { '16384' } else { '0' }))
            $dirty = $true
        } else {
            Write-Host '      Left unchanged - the link will fail if both cabinets use the same side.' -ForegroundColor Yellow
        }
    }
    if (-not $dipOn) {
        Write-Host ("NOTE: the Link Play DIP switch is OFF in {0} - link play needs it ON." -f $cfgFile) -ForegroundColor Yellow
        $r = if ($AutoYes) { Write-Host '      (-Unattended: switching it ON automatically)' -ForegroundColor DarkGray; 'Y' } else { Read-Host '      Switch it ON now? (Y/n)' }
        if ([string]::IsNullOrWhiteSpace($r) -or $r -match '^[Yy]') {
            if ($null -eq $inputNode) { $inputNode = $sys.AppendChild($xml.CreateElement('input')) }
            if ($null -eq $dsw) {
                $dsw = $xml.CreateElement('port')
                $dsw.SetAttribute('tag', ':DSW')
                $dsw.SetAttribute('type', 'DIPSWITCH')
                $dsw.SetAttribute('mask', '8')
                $dsw.SetAttribute('defvalue', '8')
                [void]$inputNode.AppendChild($dsw)
            }
            $dsw.SetAttribute('value', '0')
            $dirty = $true
        } else {
            Write-Host '      Left OFF - the cabinets will not pair.' -ForegroundColor Yellow
        }
    }
    if ($dirty) {
        try {
            $xml.Save($cfgFile)
            Write-Host ("Updated {0}." -f $cfgFile) -ForegroundColor Green
        } catch {
            Write-Host ("WARNING: could not write {0}: {1}" -f $cfgFile, $_.Exception.Message) -ForegroundColor Yellow
        }
    }
}

# ---- Settings: built-in defaults <- saved <- command line -------------------------
# Persist in launcher_settings.json NEXT TO THIS SCRIPT (each PC keeps its own).
# The old lan_settings.json (pre-merge) is read once as a fallback.
$settingsFile = Join-Path $scriptDir 'launcher_settings.json'
$saved = $null
if (Test-Path $settingsFile) {
    try { $saved = Get-Content $settingsFile -Raw | ConvertFrom-Json } catch { Write-Host ("WARNING: could not parse {0} - ignoring it." -f $settingsFile) -ForegroundColor Yellow }
} elseif (Test-Path (Join-Path $scriptDir 'lan_settings.json')) {
    try { $saved = Get-Content (Join-Path $scriptDir 'lan_settings.json') -Raw | ConvertFrom-Json } catch { }
}
if ([string]::IsNullOrWhiteSpace($ListenerIP))  { $ListenerIP  = if ($saved -and $saved.ListenerIP  -and (Test-IPv4 $saved.ListenerIP))  { $saved.ListenerIP }  else { $DefaultListenerIP } }
if ([string]::IsNullOrWhiteSpace($ConnectorIP)) { $ConnectorIP = if ($saved -and $saved.ConnectorIP -and (Test-IPv4 $saved.ConnectorIP)) { $saved.ConnectorIP } else { $DefaultConnectorIP } }
if (-not (Test-IPv4 $ListenerIP))  { throw "Invalid -ListenerIP '$ListenerIP'" }
if (-not (Test-IPv4 $ConnectorIP)) { throw "Invalid -ConnectorIP '$ConnectorIP'" }
if ([string]::IsNullOrWhiteSpace($Network)) {
    $savedNet = $null
    if ($saved) {
        if ($saved.PSObject.Properties['Network']) { $savedNet = [string]$saved.Network }
        elseif ($saved.PSObject.Properties['Mode']) { $savedNet = [string]$saved.Mode }   # legacy key
    }
    $Network = if ($savedNet -in @('Ethernet','WiFi')) { $savedNet } else { 'Ethernet' }
}
$savedChoice = if ($saved -and $saved.PSObject.Properties['Choice'] -and ([string]$saved.Choice) -in @('Loopback','LanRed','LanBlue')) { [string]$saved.Choice } else { '' }

# Saved per-cabinet display settings (P070). Defaults reproduce the classic
# behavior: windowed 640x480, auto position (side-by-side), auto monitor.
# Old settings files without a Display block load fine (all-defaults).
$dispSaved = @{
    Red  = @{ Monitor = ''; Fullscreen = $false; Width = 640; Height = 480; X = $null; Y = $null }
    Blue = @{ Monitor = ''; Fullscreen = $false; Width = 640; Height = 480; X = $null; Y = $null }
}
if ($saved -and $saved.PSObject.Properties['Display']) {
    foreach ($side in @('Red','Blue')) {
        if (-not $saved.Display.PSObject.Properties[$side]) { continue }
        $node = $saved.Display.$side
        $d = $dispSaved[$side]
        if ($node.PSObject.Properties['Monitor']) {
            try { $d.Monitor = ConvertTo-MonitorDeviceName ([string]$node.Monitor) } catch { $d.Monitor = '' }
        }
        if ($node.PSObject.Properties['Fullscreen']) { $d.Fullscreen = [bool]$node.Fullscreen }
        if ($node.PSObject.Properties['Width']  -and ([int]$node.Width)  -ge 160 -and ([int]$node.Width)  -le 7680) { $d.Width  = [int]$node.Width }
        if ($node.PSObject.Properties['Height'] -and ([int]$node.Height) -ge 120 -and ([int]$node.Height) -le 4320) { $d.Height = [int]$node.Height }
        if ($node.PSObject.Properties['X'] -and $null -ne $node.X) { $d.X = [int]$node.X }
        if ($node.PSObject.Properties['Y'] -and $null -ne $node.Y) { $d.Y = [int]$node.Y }
    }
}

function Save-LauncherSettings {
    param([string]$LIP, [string]$CIP, [string]$Net, [string]$ChoiceSel, [hashtable]$DisplayCfg, [string]$Path)
    try {
        # -Depth 4: the nested Display block sits at depth 3; PS 5.x default
        # depth 2 would silently stringify it.
        @{ ListenerIP = $LIP; ConnectorIP = $CIP; Network = $Net; Choice = $ChoiceSel; Display = $DisplayCfg; SavedAt = (Get-Date -Format 'yyyy-MM-dd HH:mm:ss') } | ConvertTo-Json -Depth 4 | Set-Content -Path $Path -Encoding ASCII
    } catch { Write-Host ("WARNING: could not save settings to {0}: {1}" -f $Path, $_.Exception.Message) -ForegroundColor Yellow }
}

function Read-IPWithDefault {
    param([string]$Label, [string]$Current)
    while ($true) {
        $entry = Read-Host ("{0} [{1}]" -f $Label, $Current)
        if ([string]::IsNullOrWhiteSpace($entry)) { return $Current }
        if (Test-IPv4 $entry) { return $entry }
        Write-Host '  Not a valid IPv4 address - try again (Enter keeps the current value).' -ForegroundColor Yellow
    }
}

# One-line summary of a cabinet's saved display config.
function Format-DispSummary {
    param([hashtable]$Disp)
    $mon = if ($Disp.Monitor) { $Disp.Monitor } else { 'auto' }
    if ($Disp.Fullscreen) { return ("monitor {0}, FULLSCREEN" -f $mon) }
    $pos = if ($null -ne $Disp.X) { ('{0},{1}' -f $Disp.X, $Disp.Y) } else { 'auto' }
    return ("monitor {0}, windowed {1}x{2} at {3}" -f $mon, $Disp.Width, $Disp.Height, $pos)
}

# Interactive per-cabinet display editor (menu option V). Mutates $dispSaved;
# the caller saves. Enter keeps the current value on every prompt.
function Edit-DisplaySettings {
    $screens = @(Get-DisplayMonitors)
    Write-Host ''
    Write-Host 'Detected monitors:' -ForegroundColor Cyan
    for ($i = 0; $i -lt $screens.Count; $i++) {
        $s = $screens[$i]
        $primary = if ($s.Primary) { '  (primary)' } else { '' }
        Write-Host ("  {0} = {1}  {2}x{3} at {4},{5}{6}" -f ($i + 1), $s.DeviceName, $s.Bounds.Width, $s.Bounds.Height, $s.Bounds.X, $s.Bounds.Y, $primary)
    }
    Write-Host ''
    Write-Host 'Currently configured:' -ForegroundColor Cyan
    Write-Host ("  RED : {0}" -f (Format-DispSummary -Disp $dispSaved.Red))  -ForegroundColor Red
    Write-Host ("  BLUE: {0}" -f (Format-DispSummary -Disp $dispSaved.Blue)) -ForegroundColor Cyan
    foreach ($side in @('Red','Blue')) {
        $d = $dispSaved[$side]
        $color = if ($side -eq 'Red') { 'Red' } else { 'Cyan' }
        Write-Host ''
        Write-Host ("--- {0} cabinet ---" -f $side.ToUpper()) -ForegroundColor $color
        while ($true) {
            $cur = if ($d.Monitor) { $d.Monitor } else { 'auto' }
            $e = Read-Host ("  Monitor number, or a = auto [{0}]" -f $cur)
            if ([string]::IsNullOrWhiteSpace($e)) { break }
            try { $d.Monitor = ConvertTo-MonitorDeviceName $e; break }
            catch { Write-Host '    Enter a number from the list above, or a for auto.' -ForegroundColor Yellow }
        }
        $cur = if ($d.Fullscreen) { 'y' } else { 'n' }
        $e = Read-Host ("  Fullscreen? y/n [{0}]" -f $cur)
        if ($e -match '^[Yy]') { $d.Fullscreen = $true } elseif ($e -match '^[Nn]') { $d.Fullscreen = $false }
        if (-not $d.Fullscreen) {
            while ($true) {
                $e = Read-Host ("  Window size WxH [{0}x{1}]" -f $d.Width, $d.Height)
                if ([string]::IsNullOrWhiteSpace($e)) { break }
                if (Test-ResolutionString $e) {
                    $p = $e -split 'x'
                    $d.Width = [int]$p[0]; $d.Height = [int]$p[1]
                    break
                }
                Write-Host '    Use WxH, e.g. 800x600.' -ForegroundColor Yellow
            }
            while ($true) {
                $cur = if ($null -ne $d.X) { ('{0},{1}' -f $d.X, $d.Y) } else { 'auto' }
                $e = Read-Host ("  Window position x,y on that monitor, or a = auto [{0}]" -f $cur)
                if ([string]::IsNullOrWhiteSpace($e)) { break }
                if ($e -match '^(?i)a(uto)?$') { $d.X = $null; $d.Y = $null; break }
                if ($e -match '^\s*(-?\d+)\s*,\s*(-?\d+)\s*$') { $d.X = [int]$Matches[1]; $d.Y = [int]$Matches[2]; break }
                Write-Host '    Use x,y (e.g. 0,0 or 1920,50) or a for auto.' -ForegroundColor Yellow
            }
        }
    }
}

# ---- Banner + choice --------------------------------------------------------------
$localIPs = Get-LocalIPv4List
$ipHeader = ("This PC's local IPv4: {0}" -f ($localIPs -join ', '))

if ($Unattended -and [string]::IsNullOrWhiteSpace($Mode)) {
    if ($savedChoice) {
        $Mode = $savedChoice
    } else {
        throw '-Unattended needs -Mode Loopback|LanRed|LanBlue (no saved choice found).'
    }
}

if ([string]::IsNullOrWhiteSpace($Mode)) {
    # Interactive menu: redraw on a cleared screen each cycle (no scroll creep);
    # the previous action's result is shown as a status line above the prompt.
    $menuMap = @{ '1' = 'Loopback'; '2' = 'LanRed'; '3' = 'LanBlue' }
    $notice = ''
    $noticeColor = 'Green'
    while ($true) {
        Clear-Host
        Show-TC2Banner
        Write-Host $ipHeader -ForegroundColor DarkGray
        Write-Host ''
        Write-Host 'What do you want to do?' -ForegroundColor Cyan
        Write-Host  '  1 = Local Link Play (two windows on this PC)'
        Write-Host  '  2 = Network Link Play - RED  (Server - waits for BLUE)'
        Write-Host ("  3 = Network Link Play - BLUE (Client - dials RED at {0})" -f $ListenerIP)
        Write-Host ("  C = change saved IPs (RED {0} / BLUE {1})" -f $ListenerIP, $ConnectorIP)
        Write-Host ("  M = network mode: currently {0} (See ADVANCED.md for details)" -f $Network)
        Write-Host  '  P = show this PC''s public IP'
        Write-Host  '  V = display settings (monitor / fullscreen / window size + position per cabinet)'
        Write-Host  '  R = reset display settings to defaults'
        Write-Host ''
        if ($notice) {
            Write-Host $notice -ForegroundColor $noticeColor
            Write-Host ''
        }
        $promptTxt = if ($savedChoice) { ("Press 1, 2, 3, C, M, P, V or R (Enter = last: {0})" -f $savedChoice) } else { 'Press 1, 2, 3, C, M, P, V or R' }
        $pick = Read-Host $promptTxt
        if ([string]::IsNullOrWhiteSpace($pick) -and $savedChoice) { $Mode = $savedChoice; break }
        if ($menuMap.ContainsKey($pick)) { $Mode = $menuMap[$pick]; break }
        if ($pick -match '^[Cc]$') {
            $ListenerIP  = Read-IPWithDefault -Label 'RED (host/listener) PC IP' -Current $ListenerIP
            $ConnectorIP = Read-IPWithDefault -Label 'BLUE (connector) PC IP' -Current $ConnectorIP
            Save-LauncherSettings -LIP $ListenerIP -CIP $ConnectorIP -Net $Network -ChoiceSel $savedChoice -DisplayCfg $dispSaved -Path $settingsFile
            $notice = ("Saved: RED {0} / BLUE {1}" -f $ListenerIP, $ConnectorIP)
            $noticeColor = 'Green'
            continue
        }
        if ($pick -match '^[Mm]$') {
            $Network = if ($Network -eq 'WiFi') { 'Ethernet' } else { 'WiFi' }
            Save-LauncherSettings -LIP $ListenerIP -CIP $ConnectorIP -Net $Network -ChoiceSel $savedChoice -DisplayCfg $dispSaved -Path $settingsFile
            $notice = ("Network mode -> {0} (saved)" -f $Network)
            $noticeColor = 'Green'
            continue
        }
        if ($pick -match '^[Vv]$') {
            Edit-DisplaySettings
            Save-LauncherSettings -LIP $ListenerIP -CIP $ConnectorIP -Net $Network -ChoiceSel $savedChoice -DisplayCfg $dispSaved -Path $settingsFile
            $notice = 'Display settings saved.'
            $noticeColor = 'Green'
            continue
        }
        if ($pick -match '^[Rr]$') {
            $confirm = Read-Host 'Reset display settings to defaults? (y/N)'
            if ($confirm -match '^[Yy]') {
                foreach ($side in @('Red','Blue')) {
                    $dispSaved[$side] = @{ Monitor = ''; Fullscreen = $false; Width = 640; Height = 480; X = $null; Y = $null }
                }
                Save-LauncherSettings -LIP $ListenerIP -CIP $ConnectorIP -Net $Network -ChoiceSel $savedChoice -DisplayCfg $dispSaved -Path $settingsFile
                $notice = 'Display settings reset to defaults (windowed 640x480, side by side, auto monitor).'
                $noticeColor = 'Green'
            } else {
                $notice = 'Reset cancelled - display settings unchanged.'
                $noticeColor = 'Yellow'
            }
            continue
        }
        if ($pick -match '^[Pp]$') {
            Write-Host 'Looking up the public IP (api.ipify.org, 5s timeout)...' -ForegroundColor DarkGray
            $pub = $null
            try { $pub = [string](Invoke-RestMethod -Uri 'https://api.ipify.org' -TimeoutSec 5) } catch { }
            if ($pub -and (Test-IPv4 $pub.Trim())) {
                $notice = ("Public IP: {0}   (for internet play the RED player shares this with the BLUE player)" -f $pub.Trim())
                $noticeColor = 'Green'
            } else {
                $notice = 'Public IP lookup failed (no internet, or the service is unreachable).'
                $noticeColor = 'Yellow'
            }
            continue
        }
        $notice = 'Invalid choice.'
        $noticeColor = 'Yellow'
    }
} else {
    # Non-menu path (-Mode given or -Unattended): plain banner, no screen clear.
    Show-TC2Banner
    Write-Host $ipHeader -ForegroundColor DarkGray
    if ($Unattended -and -not $PSBoundParameters.ContainsKey('Mode')) {
        Write-Host ("-Unattended: using the saved choice {0} from launcher_settings.json." -f $Mode) -ForegroundColor DarkGray
    }
}

# Persist the values actually used this run (captures command-line overrides too).
Save-LauncherSettings -LIP $ListenerIP -CIP $ConnectorIP -Net $Network -ChoiceSel $Mode -DisplayCfg $dispSaved -Path $settingsFile

# ---- Effective per-cabinet display config (P070): CLI > saved > defaults ----------
$redDisp  = $dispSaved.Red.Clone()
$blueDisp = $dispSaved.Blue.Clone()
if ($PSBoundParameters.ContainsKey('RedMonitor'))  { $redDisp.Monitor  = ConvertTo-MonitorDeviceName $RedMonitor }
if ($PSBoundParameters.ContainsKey('BlueMonitor')) { $blueDisp.Monitor = ConvertTo-MonitorDeviceName $BlueMonitor }
if ($PSBoundParameters.ContainsKey('RedResolution')) {
    if (-not (Test-ResolutionString $RedResolution)) { throw "Invalid -RedResolution '$RedResolution' (use WxH, e.g. 800x600)." }
    $p = $RedResolution -split 'x'; $redDisp.Width = [int]$p[0]; $redDisp.Height = [int]$p[1]
}
if ($PSBoundParameters.ContainsKey('BlueResolution')) {
    if (-not (Test-ResolutionString $BlueResolution)) { throw "Invalid -BlueResolution '$BlueResolution' (use WxH, e.g. 800x600)." }
    $p = $BlueResolution -split 'x'; $blueDisp.Width = [int]$p[0]; $blueDisp.Height = [int]$p[1]
}
$redDisp.Fullscreen  = if ($PSBoundParameters.ContainsKey('RedFullscreen'))  { [bool]$RedFullscreen }  elseif ($Fullscreen) { $true } else { $dispSaved.Red.Fullscreen }
$blueDisp.Fullscreen = if ($PSBoundParameters.ContainsKey('BlueFullscreen')) { [bool]$BlueFullscreen } elseif ($Fullscreen) { $true } else { $dispSaved.Blue.Fullscreen }
if ($PSBoundParameters.ContainsKey('ListenerX'))  { $redDisp.X  = $ListenerX }
if ($PSBoundParameters.ContainsKey('ListenerY'))  { $redDisp.Y  = $ListenerY }
if ($PSBoundParameters.ContainsKey('ConnectorX')) { $blueDisp.X = $ConnectorX }
if ($PSBoundParameters.ContainsKey('ConnectorY')) { $blueDisp.Y = $ConnectorY }

# A configured monitor that is not attached right now: warn and fall back to
# MAME's auto assignment instead of letting MAME die on a hidden console.
$attached = @(Get-DisplayMonitors | ForEach-Object { $_.DeviceName })
foreach ($pair in @(@('RED', $redDisp), @('BLUE', $blueDisp))) {
    if (-not [string]::IsNullOrWhiteSpace($pair[1].Monitor) -and $attached -notcontains $pair[1].Monitor) {
        Write-Host ("WARNING: {0} cabinet monitor {1} is not attached (found: {2}) - using auto." -f $pair[0], $pair[1].Monitor, ($attached -join ', ')) -ForegroundColor Yellow
        $pair[1].Monitor = ''
    }
}

# ---- Common preflight --------------------------------------------------------------
$mameExe = Resolve-MameExe -Dir $MameDir
$rompath = Join-Path $MameDir 'roms'
if (-not (Test-Path (Join-Path $rompath 'timecrs2.zip'))) {
    Write-Host ("WARNING: {0}\timecrs2.zip not found - MAME will exit immediately unless a rompath override (mame.ini) points elsewhere." -f $rompath) -ForegroundColor Yellow
}

if ($KillExisting) {
    $running = Get-Process -Name 'mame','mametc2' -ErrorAction SilentlyContinue
    if ($running) {
        Write-Host "Killing $($running.Count) running MAME process(es)..." -ForegroundColor Yellow
        $running | Stop-Process -Force
        Start-Sleep -Milliseconds 500
    }
}

# Env for the launched instance(s): WiFi pairing-window widening (LAN roles only,
# P066+ builds; older builds ignore it harmlessly).
$mameEnv = @{}
if ($Mode -ne 'Loopback' -and $Network -eq 'WiFi') {
    $wait = if ($Mode -eq 'LanRed') { $WiFiListenerWaitSetting } else { $WiFiConnectorWaitSetting }
    $mameEnv['NAMCOS23_PATCH_LINK_WAIT'] = [string]$wait
    Write-Host ("Network mode WiFi: passing NAMCOS23_PATCH_LINK_WAIT={0} (partner counter starts at {0}0; use WiFi mode on the other PC too)" -f $wait) -ForegroundColor DarkGray
}

$redFlags  = Get-MameDisplayFlags -Disp $redDisp
$blueFlags = Get-MameDisplayFlags -Disp $blueDisp

# ====================================================================================
if ($Mode -eq 'Loopback') {
    # ----- LINK PLAY on this PC: RED from MameDir, BLUE from MameInstanceB ----------
    $soloMode = $SoloListener -or $SoloConnector
    if (-not $SoloListener -and -not (Test-Path $MameInstanceB)) {
        New-Item -ItemType Directory -Force $MameInstanceB | Out-Null
        Write-Host "Created second-instance directory $MameInstanceB" -ForegroundColor DarkGray
    }
    if (-not $SoloConnector) { Initialize-CabinetCfg -Dir $MameDir       -Identity red  -AutoYes:$Unattended }
    if (-not $SoloListener)  { Initialize-CabinetCfg -Dir $MameInstanceB -Identity blue -AutoYes:$Unattended }

    if (-not $soloMode -and $redDisp.Fullscreen -and $blueDisp.Fullscreen -and ($redDisp.Monitor -eq $blueDisp.Monitor)) {
        Write-Host 'WARNING: both cabinets are fullscreen on the same monitor - the borderless windows will overlap and only the top one is visible. Use the V menu to give each cabinet its own monitor.' -ForegroundColor Yellow
    }

    if ($soloMode) {
        $DelaySeconds = 0
    } elseif ($DelaySeconds -lt 0) {
        if ($Unattended) {
            $DelaySeconds = 0.85
        } else {
            $reply = Read-Host 'Delay between RED and BLUE launch in seconds (Enter = 0.85)'
            if ([string]::IsNullOrWhiteSpace($reply)) { $DelaySeconds = 0.85 } else { $DelaySeconds = [double]$reply }
        }
    }

    if ($Log -and -not $KeepLogs) {
        $dirsToTruncate = if ($SoloListener)      { @($MameDir) }
                          elseif ($SoloConnector) { @($MameInstanceB) }
                          else                    { @($MameDir, $MameInstanceB) }
        foreach ($dir in $dirsToTruncate) {
            $log = Join-Path $dir 'error.log'
            Set-Content -Path $log -Value '' -NoNewline -Force
            Write-Host "Cleared $log" -ForegroundColor DarkGray
        }
    }

    # -background_input (opt-in): both windows keep receiving input while
    # unfocused - for two-gun / two-mouse play on one PC.
    $bgFlag = if ($BackgroundInput) { ' -background_input' } else { '' }

    # Loopback "fullscreen" = BORDERLESS window at the monitor's resolution.
    # Two native-fullscreen (-nowindow) MAME instances on one PC fight for the
    # display - one takes precedence (user-found) - so fullscreen cabinets are
    # launched WINDOWED at the monitor size and the frame is stripped after
    # launch (Set-BorderlessWindow). LAN modes keep native fullscreen (one
    # instance per PC, no conflict).
    $redMon  = Get-CabinetMonitor -Disp $redDisp
    $blueMon = Get-CabinetMonitor -Disp $blueDisp
    $bdLogFlag = if ($Log) { ' -log' } else { '' }
    $redLaunchFlags  = if ($redDisp.Fullscreen)  { ('-window -nomaximize -resolution {0}x{1} -skip_gameinfo' -f $redMon.Bounds.Width,  $redMon.Bounds.Height) + $bdLogFlag }  else { $redFlags }
    $blueLaunchFlags = if ($blueDisp.Fullscreen) { ('-window -nomaximize -resolution {0}x{1} -skip_gameinfo' -f $blueMon.Bounds.Width, $blueMon.Bounds.Height) + $bdLogFlag } else { $blueFlags }

    $listenerArgs  = "timecrs2 $redLaunchFlags$bgFlag -comm_localport $GamePort -comm_remotehost `"`""
    $connectorArgs = "timecrs2 $blueLaunchFlags$bgFlag -comm_remotehost 127.0.0.1 -comm_remoteport $GamePort -comm_localhost `"`" -rompath `"$rompath`""

    $listenerProc = $null
    $connectorProc = $null
    if (-not $SoloConnector) {
        Write-Host "Launching RED (port $GamePort, no remote peer)..." -ForegroundColor Cyan
        $listenerProc = Start-MameHidden -ExePath $mameExe -Arguments $listenerArgs -WorkingDirectory $MameDir -EnvVars $mameEnv
    }
    if (-not $soloMode -and $DelaySeconds -gt 0) {
        $delayMs = [int][math]::Round($DelaySeconds * 1000)
        Write-Host ("Waiting {0} ms..." -f $delayMs) -ForegroundColor Yellow
        Start-Sleep -Milliseconds $delayMs
    }
    if (-not $SoloListener) {
        Write-Host "Launching BLUE (peer 127.0.0.1:$GamePort)..." -ForegroundColor Cyan
        $connectorProc = Start-MameHidden -ExePath $mameExe -Arguments $connectorArgs -WorkingDirectory $MameInstanceB -EnvVars $mameEnv
    }
    # Borderless conversion happens even with -NoPosition (it IS the display
    # mode); only plain windowed placement respects -NoPosition.
    $positioned = $false
    if ($listenerProc) {
        if ($redDisp.Fullscreen) {
            Write-Host ("Sizing RED borderless on {0}..." -f $redMon.DeviceName) -ForegroundColor DarkCyan
            Set-BorderlessWindow -Process $listenerProc -X $redMon.Bounds.X -Y $redMon.Bounds.Y -W $redMon.Bounds.Width -H $redMon.Bounds.Height -Label 'RED'
            $positioned = $true
        } elseif (-not $NoPosition) {
            $p = Get-CabinetWindowPos -Disp $redDisp -DefaultX $ListenerX -DefaultY $ListenerY -AutoUsesDefault $true
            Write-Host ("Placing RED window ({0}x{1})..." -f $redDisp.Width, $redDisp.Height) -ForegroundColor DarkCyan
            Set-CabinetWindow -Process $listenerProc -X $p.X -Y $p.Y -ClientW $redDisp.Width -ClientH $redDisp.Height -Label 'RED'
            $positioned = $true
        }
    }
    if ($connectorProc) {
        if ($blueDisp.Fullscreen) {
            Write-Host ("Sizing BLUE borderless on {0}..." -f $blueMon.DeviceName) -ForegroundColor DarkCyan
            Set-BorderlessWindow -Process $connectorProc -X $blueMon.Bounds.X -Y $blueMon.Bounds.Y -W $blueMon.Bounds.Width -H $blueMon.Bounds.Height -Label 'BLUE'
            $positioned = $true
        } elseif (-not $NoPosition) {
            $p = Get-CabinetWindowPos -Disp $blueDisp -DefaultX $ConnectorX -DefaultY $ConnectorY -AutoUsesDefault $true
            Write-Host ("Placing BLUE window ({0}x{1})..." -f $blueDisp.Width, $blueDisp.Height) -ForegroundColor DarkCyan
            Set-CabinetWindow -Process $connectorProc -X $p.X -Y $p.Y -ClientW $blueDisp.Width -ClientH $blueDisp.Height -Label 'BLUE'
            $positioned = $true
        }
    }
    if (-not $positioned) {
        Start-Sleep -Seconds 2      # give MAME a moment before the liveness peek below
    }
    $anyDead = $false
    foreach ($p in @($listenerProc, $connectorProc)) {
        if ($null -ne $p) {
            $p.Refresh()
            if ($p.HasExited) {
                $anyDead = $true
                Write-Host ("ERROR: a MAME instance exited immediately (PID {0}, exit code {1})." -f $p.Id, $p.ExitCode) -ForegroundColor Red
            }
        }
    }
    if ($anyDead) {
        Write-Host '       Usual cause: roms\timecrs2.zip missing next to the exe. See the real error with a visible console:' -ForegroundColor Red
        Write-Host ("       & `"{0}`" timecrs2 -window -skip_gameinfo -log" -f $mameExe) -ForegroundColor Yellow
        Exit-WithPause 1
    }
    Write-Host ''
    if ($SoloListener)      { Write-Host 'RED launched (solo).' -ForegroundColor Green }
    elseif ($SoloConnector) { Write-Host 'BLUE launched (solo).' -ForegroundColor Green }
    else                    { Write-Host 'Both launched.' -ForegroundColor Green }
    if ($Log) { Write-Host "Logs: $MameDir\error.log  +  $MameInstanceB\error.log" -ForegroundColor DarkGray }
    Write-Host 'Close the MAME window(s) when done.' -ForegroundColor Green
    return
}

# ====================================================================================
# ----- LINK PLAY over the network ---------------------------------------------------
# Role -> cabinet folder: RED runs from the script's folder; BLUE from the blue
# instance dir (-MameInstanceB, default instance-b\ beside the script). An
# explicit -WorkDir wins for either role.
if ([string]::IsNullOrWhiteSpace($WorkDir)) {
    if ($Mode -eq 'LanRed') {
        $WorkDir = $scriptDir
    } else {
        # Prefer an already-configured blue folder; else fall back to the default
        # (created + seeded below). NOTE: $WorkDir is a [string] parameter, so
        # $null assignments coerce to '' - test with IsNullOrWhiteSpace, never -eq $null.
        $WorkDir = ''
        foreach ($c in @($MameInstanceB, (Join-Path $scriptDir 'instance-b'))) {
            if (Test-Path (Join-Path $c 'cfg\timecrs2.cfg')) { $WorkDir = $c; break }
        }
        if ([string]::IsNullOrWhiteSpace($WorkDir)) { $WorkDir = $MameInstanceB }
    }
}
if (-not (Test-Path $WorkDir)) {
    New-Item -ItemType Directory -Force $WorkDir | Out-Null
    Write-Host ("Created cabinet directory {0}" -f $WorkDir) -ForegroundColor DarkGray
}
Initialize-CabinetCfg -Dir $WorkDir -Identity $(if ($Mode -eq 'LanRed') { 'red' } else { 'blue' }) -AutoYes:$Unattended

# Post-ack stagger default by network mode (explicit -StaggerMs always wins): the
# LISTENER_UP ack already means RED's MAME survived a 3s alive-check, so WiFi
# needs no extra margin; Ethernet keeps a small one.
if (-not $PSBoundParameters.ContainsKey('StaggerMs')) {
    $StaggerMs = if ($Network -eq 'WiFi') { 0 } else { 250 }
}

# Warn (do not hard-fail) if the local IP does not match the expected role IP.
# Skipped while the expected IP is still the built-in placeholder (not yet set
# via C) - e.g. BLUE never needs its own IP entered, only RED's.
$expectedIP  = if ($Mode -eq 'LanRed') { $ListenerIP } else { $ConnectorIP }
$placeholder = if ($Mode -eq 'LanRed') { $DefaultListenerIP } else { $DefaultConnectorIP }
if ($expectedIP -ne $placeholder -and $localIPs -notcontains $expectedIP) {
    Write-Host ("WARNING: this PC's IPv4 addresses ({0}) do not include the expected {1} for {2}." -f ($localIPs -join ', '), $expectedIP, $Mode) -ForegroundColor Yellow
    Write-Host '         Continuing anyway - make sure you picked the right option on the right PC.' -ForegroundColor Yellow
}

Write-Host ("Binary + ROMs : {0}" -f $MameDir) -ForegroundColor DarkGray
Write-Host ("Config (cfg\ nvram\ = {0} identity) : {1}" -f $(if ($Mode -eq 'LanRed') { 'RED' } else { 'BLUE' }), $WorkDir) -ForegroundColor DarkGray

if ($Mode -eq 'LanRed') {
    # ----- RED (host/listener, waits) ----------------------------------------------
    $listenerArgs = "timecrs2 $redFlags -comm_localport $GamePort -comm_remotehost `"`" -rompath `"$rompath`""

    $srv = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Any, $ControlPort)
    $srv.Start()
    Write-Host ''
    Write-Host ("RED ready. Waiting for the BLUE PC ({0}) on control port {1}..." -f $ConnectorIP, $ControlPort) -ForegroundColor Magenta
    Write-Host '(Ctrl+C to abort. Not connecting? Check the RED firewall rule: inbound TCP 9875-9876.)' -ForegroundColor DarkGray

    $client = $srv.AcceptTcpClient()          # blocks until the BLUE PC dials in
    $srv.Stop()
    $stream = $client.GetStream()
    $reader = New-Object System.IO.StreamReader($stream)
    $writer = New-Object System.IO.StreamWriter($stream)
    $writer.AutoFlush = $true

    Write-Host ("BLUE connected from {0}." -f $client.Client.RemoteEndPoint) -ForegroundColor Green
    $msg = $reader.ReadLine()
    if ($msg -ne 'GO') { Write-Host "WARNING: expected 'GO', got '$msg' - launching anyway." -ForegroundColor Yellow }

    Write-Host 'GO received - launching the RED instance...' -ForegroundColor Cyan
    $proc = Start-MameHidden -ExePath $mameExe -Arguments $listenerArgs -WorkingDirectory $WorkDir -EnvVars $mameEnv
    $manual = ('& "{0}" timecrs2 -window -skip_gameinfo -log -comm_localport {1} -comm_remotehost "" -rompath "{2}"' -f $mameExe, $GamePort, $rompath)
    if (Test-MameAlive -Process $proc -Label 'RED' -ManualCmd $manual) {
        Write-Host ("RED up (PID {0}), bound on 0.0.0.0:{1}." -f $proc.Id, $GamePort) -ForegroundColor Green
        if (-not $NoPosition -and -not $redDisp.Fullscreen) {
            $pos = Get-CabinetWindowPos -Disp $redDisp -DefaultX $ListenerX -DefaultY $ListenerY -AutoUsesDefault $false
            if ($pos -or $redDisp.Width -ne 640 -or $redDisp.Height -ne 480) {
                Set-CabinetWindow -Process $proc -X $(if ($pos) { $pos.X } else { $null }) -Y $(if ($pos) { $pos.Y } else { $null }) -ClientW $redDisp.Width -ClientH $redDisp.Height -Label 'RED'
            }
        }
        $writer.WriteLine('LISTENER_UP')
    } else {
        $writer.WriteLine('LISTENER_FAILED')
        Start-Sleep -Milliseconds 200
        $client.Close()
        Exit-WithPause 1
    }
    Start-Sleep -Milliseconds 200
    $client.Close()
    if ($Log) { Write-Host ("Log: {0}\error.log" -f $WorkDir) -ForegroundColor DarkGray }
}
else {
    # ----- BLUE (connector, dials RED) ----------------------------------------------
    $connectorArgs = "timecrs2 $blueFlags -comm_remotehost $ListenerIP -comm_remoteport $GamePort -comm_localhost `"`" -rompath `"$rompath`""

    Write-Host ''
    Write-Host ("BLUE: dialing RED {0}:{1} (retrying until it answers)..." -f $ListenerIP, $ControlPort) -ForegroundColor Magenta
    $client = $null
    $attempt = 0
    while ($null -eq $client) {
        $attempt++
        try {
            $c = New-Object System.Net.Sockets.TcpClient
            $c.Connect($ListenerIP, $ControlPort)
            $client = $c
        } catch {
            if (($attempt % 10) -eq 0) { Write-Host ("  ...still waiting (attempt {0}). Is the RED PC running this script with option 2? Firewall open (inbound TCP 9875-9876 on RED)?" -f $attempt) -ForegroundColor DarkGray }
            Start-Sleep -Milliseconds 500
        }
    }
    $stream = $client.GetStream()
    $reader = New-Object System.IO.StreamReader($stream)
    $writer = New-Object System.IO.StreamWriter($stream)
    $writer.AutoFlush = $true
    Write-Host 'Connected to RED.' -ForegroundColor Green

    Write-Host 'Starting countdown...' -ForegroundColor Cyan
    foreach ($n in 3,2,1) { Write-Host ("  {0}..." -f $n) -ForegroundColor Yellow; Start-Sleep -Seconds 1 }
    $writer.WriteLine('GO')

    # Wait for RED to confirm it launched (the ack means its MAME actually
    # SURVIVED its first seconds, so allow ~5s beyond that check).
    $client.ReceiveTimeout = 12000
    $ack = $null
    try { $ack = $reader.ReadLine() } catch { }
    if ($ack -eq 'LISTENER_UP') {
        Write-Host 'RED confirmed up.' -ForegroundColor Green
    } elseif ($ack -eq 'LISTENER_FAILED') {
        Write-Host 'ABORT: the RED PC reports its MAME exited immediately (see the RED console for the diagnostic). Not launching BLUE.' -ForegroundColor Red
        $client.Close()
        Exit-WithPause 1
    } else {
        Write-Host ("WARNING: no LISTENER_UP ack (got '{0}') - launching anyway." -f $ack) -ForegroundColor Yellow
    }

    if ($StaggerMs -gt 0) {
        Write-Host ("Waiting {0} ms so RED binds first..." -f $StaggerMs) -ForegroundColor DarkGray
        Start-Sleep -Milliseconds $StaggerMs
    } else {
        Write-Host 'No stagger (WiFi mode / -StaggerMs 0) - RED already verified alive.' -ForegroundColor DarkGray
    }

    Write-Host ("Launching BLUE -> {0}:{1}..." -f $ListenerIP, $GamePort) -ForegroundColor Cyan
    $proc = Start-MameHidden -ExePath $mameExe -Arguments $connectorArgs -WorkingDirectory $WorkDir -EnvVars $mameEnv
    $manual = ('& "{0}" timecrs2 -window -skip_gameinfo -log -comm_remotehost {1} -comm_remoteport {2} -comm_localhost "" -rompath "{3}"' -f $mameExe, $ListenerIP, $GamePort, $rompath)
    if (Test-MameAlive -Process $proc -Label 'BLUE' -ManualCmd $manual) {
        Write-Host ("BLUE up (PID {0})." -f $proc.Id) -ForegroundColor Green
        if (-not $NoPosition -and -not $blueDisp.Fullscreen) {
            $pos = Get-CabinetWindowPos -Disp $blueDisp -DefaultX $ConnectorX -DefaultY $ConnectorY -AutoUsesDefault $false
            if ($pos -or $blueDisp.Width -ne 640 -or $blueDisp.Height -ne 480) {
                Set-CabinetWindow -Process $proc -X $(if ($pos) { $pos.X } else { $null }) -Y $(if ($pos) { $pos.Y } else { $null }) -ClientW $blueDisp.Width -ClientH $blueDisp.Height -Label 'BLUE'
            }
        }
    } else {
        $client.Close()
        Exit-WithPause 1
    }
    $client.Close()
    if ($Log) { Write-Host ("Log: {0}\error.log" -f $WorkDir) -ForegroundColor DarkGray }
}

Write-Host ''
$windowLabel = if ($Network -eq 'WiFi') { 'counter widened by WiFi mode' } else { 'counter from 300, ROM default' }
Write-Host ("Launched. The game link now establishes over TCP {0} during the discovery window ({1})." -f $GamePort, $windowLabel) -ForegroundColor Green
Write-Host 'Close the MAME window when done.' -ForegroundColor Green
