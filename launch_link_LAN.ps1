# launch_link_LAN.ps1  -  Time Crisis II link play on TWO PCs (Ethernet/WiFi)
#
# ONE script, run on BOTH PCs. You pick a role at the prompt:
#
#   Press 1 = RED  cabinet (listener)  -> waits for the connection
#   Press 2 = BLUE cabinet (connector) -> dials in and starts the countdown
#   Press C = set/change the two PCs' IP addresses (validated, then SAVED to
#             lan_settings.json next to this script and reused on later runs;
#             -ListenerIP / -ConnectorIP parameters override + save too)
#   Press M = toggle Ethernet/WiFi mode (saved; use the SAME mode on both PCs)
#
# Full guide (including the firewall rule the RED PC needs): README.md
# (Quick start guide + Troubleshooting); extras in docs\ADVANCED.md
#
# How the two PCs coordinate (no clock sync needed - the game link keeps the
# two machines in step by itself; the in-game partner-search countdown is the
# join window):
#   1. The RED script opens a small control socket (port 9875) and WAITS.
#   2. The BLUE script retries a TCP connect to the RED PC on 9875 until it
#      answers (this doubles as the reachability test), then runs a visible
#      3-2-1 countdown and sends "GO".
#   3. On "GO" the RED script launches its game and confirms it survived its
#      first seconds before replying "LISTENER_UP".
#   4. The BLUE script waits for "LISTENER_UP", pauses a small extra margin
#      (so RED is listening on 9876 first), then launches its game aimed at
#      the RED PC's IP.
#   The control socket is then closed; the game link takes over on port 9876.
#
# BEFORE FIRST USE (once per PC - see README.md):
#   - mametc2.exe + roms\timecrs2.zip in this script's folder on BOTH PCs.
#   - One-time in-game setup on each PC: Link ID (Left/Red on the red PC,
#     Right/Blue on the blue PC) + DIP switch "Link Play Enabled" On.
#   - Windows Firewall on the RED PC must ALLOW inbound TCP 9876 (game link)
#     and 9875 (this script's control channel). The #1 cause of "won't connect".
#
# Windows PowerShell 5.x compatible. ASCII only.

[CmdletBinding()]
param(
    [ValidateSet('1','2','')]
    [string]$Role = '',                 # '1'=RED/listener, '2'=BLUE/connector; empty = prompt
    [int]$StaggerMs = 250,              # BLUE's post-ack delay (RED-first margin; the LISTENER_UP
                                        # ack already means RED's game survived a ~3s alive-check,
                                        # so its port is bound well before this)
    [string]$MameDir = '',              # where mametc2.exe + roms\ live (default: this script's folder)
    [string]$WorkDir = '',              # cfg\ nvram\ error.log folder = this PC's cabinet identity
    [string]$ListenerIP = '',           # RED PC IP; empty = last-saved (lan_settings.json) or built-in default
    [string]$ConnectorIP = '',          # BLUE PC IP; empty = last-saved or built-in default
    [ValidateSet('Ethernet','WiFi','')]
    [string]$Mode = '',                 # network mode; WiFi widens the in-game partner-search
                                        # countdown for extra pairing margin; empty = last-saved or Ethernet
    [switch]$Log                        # write a diagnostic error.log into this PC's instance
                                        # folder (off by default)
)

$ErrorActionPreference = 'Stop'

# Two INDEPENDENT locations:
#   MameDir = the binary + ROMs (one mametc2.exe per PC).
#   WorkDir = the process working folder = where the game reads/writes cfg\
#             nvram\ error.log. THIS folder carries the cabinet identity
#             (RED vs BLUE) and the link DIP setting.
# On a normal install both are simply this script's folder.
$scriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { (Get-Location).Path }
if ([string]::IsNullOrWhiteSpace($WorkDir)) { $WorkDir = $scriptDir }
if ([string]::IsNullOrWhiteSpace($MameDir)) { $MameDir = $scriptDir }

# ---- Network settings: built-in defaults <- saved settings <- command line ----
# The built-in IPs below are FIRST-RUN PLACEHOLDERS. Press C at the menu to set
# your real addresses - they persist in lan_settings.json NEXT TO THIS SCRIPT
# (each PC keeps its own copy).
$DefaultListenerIP  = '192.168.1.10'   # RED PC (waits) - placeholder, change via C
$DefaultConnectorIP = '192.168.1.11'   # BLUE PC (dials) - placeholder, change via C
$GamePort    = 9876             # the game link itself
$ControlPort = 9875             # this script's coordination channel
$WiFiListenerWaitSetting  = 60  # WiFi mode: RED cab partner-search window setting (the
                                # on-screen counter starts at 10x this = 600, roughly
                                # double the standard few-second window)
$WiFiConnectorWaitSetting = 45  # WiFi mode: BLUE cab setting (counter starts at 450;
                                # BLUE launches later, so its shorter window ends around
                                # the same time as RED's)

function Test-IPv4 { param([string]$s) $ip=$null; return ([System.Net.IPAddress]::TryParse($s, [ref]$ip) -and $ip.AddressFamily -eq 'InterNetwork') }

$settingsFile = Join-Path $scriptDir 'lan_settings.json'
$saved = $null
if (Test-Path $settingsFile) {
    try { $saved = Get-Content $settingsFile -Raw | ConvertFrom-Json } catch { Write-Host ("WARNING: could not parse {0} - ignoring it." -f $settingsFile) -ForegroundColor Yellow }
}
if ([string]::IsNullOrWhiteSpace($ListenerIP))  { $ListenerIP  = if ($saved -and $saved.ListenerIP  -and (Test-IPv4 $saved.ListenerIP))  { $saved.ListenerIP }  else { $DefaultListenerIP } }
if ([string]::IsNullOrWhiteSpace($ConnectorIP)) { $ConnectorIP = if ($saved -and $saved.ConnectorIP -and (Test-IPv4 $saved.ConnectorIP)) { $saved.ConnectorIP } else { $DefaultConnectorIP } }
if (-not (Test-IPv4 $ListenerIP))  { throw "Invalid -ListenerIP '$ListenerIP'" }
if (-not (Test-IPv4 $ConnectorIP)) { throw "Invalid -ConnectorIP '$ConnectorIP'" }
if ([string]::IsNullOrWhiteSpace($Mode)) { $Mode = if ($saved -and $saved.Mode -and $saved.Mode -in @('Ethernet','WiFi')) { $saved.Mode } else { 'Ethernet' } }

function Save-LanSettings {
    param([string]$LIP, [string]$CIP, [string]$NetMode, [string]$Path)
    try {
        @{ ListenerIP = $LIP; ConnectorIP = $CIP; Mode = $NetMode; SavedAt = (Get-Date -Format 'yyyy-MM-dd HH:mm:ss') } | ConvertTo-Json | Set-Content -Path $Path -Encoding ASCII
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

# ------------------------------------------------------------------------------

function Get-LocalIPv4List {
    $addrs = [System.Net.Dns]::GetHostAddresses([System.Net.Dns]::GetHostName())
    return $addrs | Where-Object { $_.AddressFamily -eq 'InterNetwork' } | ForEach-Object { $_.IPAddressToString }
}

function Resolve-MameExe {
    param([string]$Dir)
    foreach ($name in @('mametc2.exe', 'mame.exe')) {
        $p = Join-Path $Dir $name
        if (Test-Path $p) { return $p }
    }
    throw "No MAME binary (mametc2.exe / mame.exe) found in $Dir - see docs\BUILDING.md."
}

function Start-MameHidden {
    param(
        [string]$ExePath,
        [string]$Arguments,
        [string]$WorkingDirectory,
        [hashtable]$EnvVars = @{}
    )
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName         = $ExePath
    $psi.Arguments        = $Arguments
    $psi.WorkingDirectory = $WorkingDirectory
    $psi.UseShellExecute  = $false
    $psi.CreateNoWindow   = $true
    foreach ($k in $EnvVars.Keys) { $psi.EnvironmentVariables[$k] = [string]$EnvVars[$k] }
    return [System.Diagnostics.Process]::Start($psi)
}

# ---- Role selection (with in-menu IP editing) ---------------------------------
if ([string]::IsNullOrWhiteSpace($Role)) {
    while ($true) {
        Write-Host ''
        Write-Host 'Time Crisis II LAN link - choose this PC''s role:' -ForegroundColor Cyan
        Write-Host ("  1 = RED  cabinet (this PC = {0}) - waits for the connection" -f $ListenerIP)
        Write-Host ("  2 = BLUE cabinet (this PC = {0}) - dials in + countdown" -f $ConnectorIP)
        Write-Host  '  C = change the IPs (saved for next time)'
        Write-Host ("  M = network mode: currently {0} (saved; WiFi widens the in-game partner-search countdown for extra margin - use the SAME mode on both PCs)" -f $Mode)
        $pick = Read-Host 'Press 1, 2, C or M'
        if ($pick -eq '1' -or $pick -eq '2') { $Role = $pick; break }
        if ($pick -match '^[Cc]$') {
            $ListenerIP  = Read-IPWithDefault -Label 'RED PC (listener) IP' -Current $ListenerIP
            $ConnectorIP = Read-IPWithDefault -Label 'BLUE PC (connector) IP' -Current $ConnectorIP
            Save-LanSettings -LIP $ListenerIP -CIP $ConnectorIP -NetMode $Mode -Path $settingsFile
            Write-Host ("Saved to {0}" -f $settingsFile) -ForegroundColor Green
            continue
        }
        if ($pick -match '^[Mm]$') {
            $Mode = if ($Mode -eq 'WiFi') { 'Ethernet' } else { 'WiFi' }
            Save-LanSettings -LIP $ListenerIP -CIP $ConnectorIP -NetMode $Mode -Path $settingsFile
            Write-Host ("Network mode -> {0} (saved)" -f $Mode) -ForegroundColor Green
            continue
        }
        Write-Host 'Invalid choice.' -ForegroundColor Yellow
    }
}
if ($Role -ne '1' -and $Role -ne '2') { throw "Invalid role '$Role' (expected 1 or 2)." }

# Persist the values actually used this run (captures command-line overrides too).
Save-LanSettings -LIP $ListenerIP -CIP $ConnectorIP -NetMode $Mode -Path $settingsFile

# Mode -> partner-search window. WiFi mode roughly DOUBLES the game's built-in
# few-second partner-search countdown for extra pairing margin on a less
# predictable connection (the on-screen counter starts at 600 on RED / 450 on
# BLUE instead of the standard 300 - BLUE launches later, so both windows end
# around the same time). The widening is passed to the game via the
# NAMCOS23_PATCH_LINK_WAIT environment variable (counter = 10x the value);
# Ethernet mode passes nothing and uses the game's default. Run the SAME mode
# on BOTH PCs - mismatched windows let the shorter-window cabinet fall back to
# solo early.
$LinkWaitSetting = ''
if ($Mode -eq 'WiFi') {
    $LinkWaitSetting = if ($Role -eq '1') { [string]$WiFiListenerWaitSetting } else { [string]$WiFiConnectorWaitSetting }
    Write-Host ("Network mode WiFi: widened partner-search window (counter starts at {0}0 on this PC; use WiFi mode on the other PC too)" -f $LinkWaitSetting) -ForegroundColor DarkGray
} else {
    Write-Host 'Network mode Ethernet: standard partner-search window (counter starts at 300)' -ForegroundColor DarkGray
}

# Mode-aware stagger default (explicit -StaggerMs always wins): the LISTENER_UP
# ack already means RED's game survived a 3s alive-check (port long since
# bound), so WiFi needs no extra margin at all; Ethernet keeps a small one.
if (-not $PSBoundParameters.ContainsKey('StaggerMs')) {
    $StaggerMs = if ($Mode -eq 'WiFi') { 0 } else { 250 }
}

# Warn (do not hard-fail) if the local IP does not match the expected role IP.
$localIPs   = Get-LocalIPv4List
$expectedIP = if ($Role -eq '1') { $ListenerIP } else { $ConnectorIP }
if ($localIPs -notcontains $expectedIP) {
    Write-Host ("WARNING: this PC's IPv4 addresses ({0}) do not include the expected {1} for this role." -f ($localIPs -join ', '), $expectedIP) -ForegroundColor Yellow
    Write-Host '         Continuing anyway - check the C menu option if the IPs are wrong (ipconfig shows this PC''s address).' -ForegroundColor Yellow
}

$mameExe = Resolve-MameExe -Dir $MameDir
$rompath = Join-Path $MameDir 'roms'

Write-Host ("Binary + ROMs : {0}" -f $MameDir) -ForegroundColor DarkGray
Write-Host ("Settings folder ({0} cabinet identity) : {1}" -f $(if ($Role -eq '1') { 'RED' } else { 'BLUE' }), $WorkDir) -ForegroundColor DarkGray

# WiFi mode passes the widened partner-search window to the game; Ethernet
# passes nothing (game default).
$mameEnv = @{}
if (-not [string]::IsNullOrWhiteSpace($LinkWaitSetting)) { $mameEnv['NAMCOS23_PATCH_LINK_WAIT'] = [string]$LinkWaitSetting }

# Pre-flight: the ROM zip is expected beside the binary.
$romZip = Join-Path $rompath 'timecrs2.zip'
if (-not (Test-Path $romZip)) {
    Write-Host ("WARNING: {0} not found - the game will exit immediately if the ROM is not reachable via -rompath." -f $romZip) -ForegroundColor Yellow
}

# After launching, verify the child survived its first seconds. A missing ROM /
# bad rompath kills the game instantly, but with a hidden console the error is
# invisible and the script would otherwise claim success.
function Test-MameAlive {
    param([System.Diagnostics.Process]$Process, [string]$Label, [string]$ManualCmd)
    Start-Sleep -Seconds 3
    $Process.Refresh()
    if ($Process.HasExited) {
        Write-Host ("ERROR: {0} game exited immediately (exit code {1})." -f $Label, $Process.ExitCode) -ForegroundColor Red
        Write-Host '       Usual causes: roms\timecrs2.zip missing next to the exe, or a wrong -rompath.' -ForegroundColor Red
        Write-Host '       See the real error by running the game with a visible console:' -ForegroundColor Red
        Write-Host ("       {0}" -f $ManualCmd) -ForegroundColor Yellow
        return $false
    }
    return $true
}

# Logging is OFF by default; -Log adds MAME's -log (error.log in this PC's instance folder).
$commonFlags = '-window -nomaximize -resolution 640x480 -prescale 1 -nokeepaspect -skip_gameinfo'
if ($Log) { $commonFlags += ' -log' }

if ($Role -eq '1') {
    # ===================== RED cabinet (listener, waits) ======================
    $listenerArgs = "timecrs2 $commonFlags -comm_localport $GamePort -comm_remotehost `"`" -rompath `"$rompath`""

    $srv = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Any, $ControlPort)
    $srv.Start()
    Write-Host ''
    Write-Host ("RED ready. Waiting for the BLUE PC ({0}) on control port {1}..." -f $ConnectorIP, $ControlPort) -ForegroundColor Magenta
    Write-Host '(Ctrl+C to abort. Not connecting? Check the firewall rule - see README.md Troubleshooting.)' -ForegroundColor DarkGray

    $client = $srv.AcceptTcpClient()          # blocks until the BLUE PC dials in
    $srv.Stop()
    $stream = $client.GetStream()
    $reader = New-Object System.IO.StreamReader($stream)
    $writer = New-Object System.IO.StreamWriter($stream)
    $writer.AutoFlush = $true

    Write-Host ("BLUE PC connected from {0}." -f $client.Client.RemoteEndPoint) -ForegroundColor Green
    $msg = $reader.ReadLine()
    if ($msg -ne 'GO') { Write-Host "WARNING: expected 'GO', got '$msg' - launching anyway." -ForegroundColor Yellow }

    Write-Host 'GO received - launching the RED cabinet...' -ForegroundColor Cyan
    $proc = Start-MameHidden -ExePath $mameExe -Arguments $listenerArgs -WorkingDirectory $WorkDir -EnvVars $mameEnv
    $manual = ('& "{0}" timecrs2 -window -skip_gameinfo -log -comm_localport {1} -comm_remotehost "" -rompath "{2}"' -f $mameExe, $GamePort, $rompath)
    if (Test-MameAlive -Process $proc -Label 'RED' -ManualCmd $manual) {
        Write-Host ("RED cabinet up (PID {0}), listening on port {1}." -f $proc.Id, $GamePort) -ForegroundColor Green
        $writer.WriteLine('LISTENER_UP')
    } else {
        $writer.WriteLine('LISTENER_FAILED')
    }
    Start-Sleep -Milliseconds 200
    $client.Close()
    if ($Log) { Write-Host ("Log: {0}\error.log" -f $WorkDir) -ForegroundColor DarkGray }
}
else {
    # ===================== BLUE cabinet (connector, dials) ====================
    $connectorArgs = "timecrs2 $commonFlags -comm_remotehost $ListenerIP -comm_remoteport $GamePort -comm_localhost `"`" -rompath `"$rompath`""

    Write-Host ''
    Write-Host ("BLUE: dialing the RED PC {0}:{1} (retrying until it answers)..." -f $ListenerIP, $ControlPort) -ForegroundColor Magenta
    $client = $null
    $attempt = 0
    while ($null -eq $client) {
        $attempt++
        try {
            $c = New-Object System.Net.Sockets.TcpClient
            $c.Connect($ListenerIP, $ControlPort)
            $client = $c
        } catch {
            if (($attempt % 10) -eq 0) { Write-Host ("  ...still waiting (attempt {0}). Is the RED PC running this script with option 1? Firewall open? (see README.md Troubleshooting)" -f $attempt) -ForegroundColor DarkGray }
            Start-Sleep -Milliseconds 500
        }
    }
    $stream = $client.GetStream()
    $reader = New-Object System.IO.StreamReader($stream)
    $writer = New-Object System.IO.StreamWriter($stream)
    $writer.AutoFlush = $true
    Write-Host 'Connected to the RED PC.' -ForegroundColor Green

    Write-Host 'Starting countdown...' -ForegroundColor Cyan
    foreach ($n in 3,2,1) { Write-Host ("  {0}..." -f $n) -ForegroundColor Yellow; Start-Sleep -Seconds 1 }
    $writer.WriteLine('GO')

    # Wait for the RED PC to confirm it launched (the ack means its game
    # actually SURVIVED its first seconds, so allow ~5s beyond that check).
    $client.ReceiveTimeout = 12000
    $ack = $null
    try { $ack = $reader.ReadLine() } catch { }
    if ($ack -eq 'LISTENER_UP') {
        Write-Host 'RED cabinet confirmed up.' -ForegroundColor Green
    } elseif ($ack -eq 'LISTENER_FAILED') {
        Write-Host 'ABORT: the RED PC reports its game exited immediately (see the RED PC console for the diagnostic). Not launching the BLUE cabinet.' -ForegroundColor Red
        $client.Close()
        exit 1
    } else {
        Write-Host ("WARNING: no LISTENER_UP ack (got '{0}') - launching anyway." -f $ack) -ForegroundColor Yellow
    }

    if ($StaggerMs -gt 0) {
        Write-Host ("Waiting {0} ms so the RED cabinet is listening first..." -f $StaggerMs) -ForegroundColor DarkGray
        Start-Sleep -Milliseconds $StaggerMs
    } else {
        Write-Host 'No extra stagger (WiFi mode / -StaggerMs 0) - RED already verified alive.' -ForegroundColor DarkGray
    }

    Write-Host ("Launching the BLUE cabinet -> {0}:{1}..." -f $ListenerIP, $GamePort) -ForegroundColor Cyan
    $proc = Start-MameHidden -ExePath $mameExe -Arguments $connectorArgs -WorkingDirectory $WorkDir -EnvVars $mameEnv
    $manual = ('& "{0}" timecrs2 -window -skip_gameinfo -log -comm_remotehost {1} -comm_remoteport {2} -comm_localhost "" -rompath "{3}"' -f $mameExe, $ListenerIP, $GamePort, $rompath)
    if (Test-MameAlive -Process $proc -Label 'BLUE' -ManualCmd $manual) {
        Write-Host ("BLUE cabinet up (PID {0})." -f $proc.Id) -ForegroundColor Green
    }
    $client.Close()
    if ($Log) { Write-Host ("Log: {0}\error.log" -f $WorkDir) -ForegroundColor DarkGray }
}

Write-Host ''
$windowLabel = if ($Mode -eq 'WiFi') { ('counter from {0}0, WiFi-widened' -f $LinkWaitSetting) } else { 'counter from 300, standard' }
Write-Host ("Launched. The game link now establishes over TCP {0} during the partner-search window ({1})." -f $GamePort, $windowLabel) -ForegroundColor Green
Write-Host 'Close the game window when done.' -ForegroundColor Green
