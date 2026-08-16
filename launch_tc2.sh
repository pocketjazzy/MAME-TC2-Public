#!/bin/bash
# launch_tc2.sh - Time Crisis II link play: the all-in-one Linux launcher
# (the Linux sibling of launch_tc2.ps1; any x86_64 distro, Steam Deck included)
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
#   V = per-cabinet DISPLAY settings (monitor / fullscreen / window size for
#       RED and BLUE independently; saved)
#   R = reset the display settings back to the script defaults
#   Q = quit
#   Enter = repeat the last-used choice (saved in launcher_settings.conf)
#
# ZERO-SETUP FIRST RUN: a cabinet folder with no cfg/timecrs2.cfg is seeded
# with the correct identity (main folder = LEFT/RED, instance-b/ = RIGHT/BLUE)
# and the Link Play DIP already ON. Existing cfgs are verified; mismatches are
# fixed only after a Y/n prompt (auto-applied with --unattended).
#
# HEADLESS (front-ends, boot-to-game cabinets, Steam Gaming Mode shortcuts):
#   ./launch_tc2.sh --mode loopback --unattended [--fullscreen] [--stagger 3]
#   ./launch_tc2.sh --mode lanred  --listener-ip 192.168.1.10 --unattended
#   ./launch_tc2.sh --mode lanblue --listener-ip 192.168.1.10 --unattended
#   --unattended never prompts: the choice must come from --mode or the saved
#   settings, and cfg identity/DIP fixes are applied automatically.
#   --log writes MAME's error.log in each cabinet folder (off by default).
#   --stagger <seconds>  loopback only: the RED->BLUE launch delay (default 3;
#       WARNING: values above ~4 miss the in-game pairing window - 5 is
#       already too late (measured). See HOW THE LINK STARTS above.
#   Display params (override the saved V-menu settings for this run only):
#     --red-monitor 0 / --blue-monitor 1     SDL display index (0 = first)
#     --red-resolution 800x600 / --blue-resolution 800x600   windowed size
#     --red-fullscreen / --blue-fullscreen   per cabinet (--fullscreen = both)
#   --background-input   loopback only: both windows keep receiving input
#       while unfocused (two-gun / two-mouse one-PC play; off by default
#       because a shared keyboard would drive both cabinets at once)
#
# HOW THE LINK STARTS (fixed launch stagger - measured; do not "improve"):
# the C139 link is strictly one-shot BOTH ways: the connector dials exactly
# ONCE, and the listener accepts exactly ONE connection EVER. So the script
# must NEVER probe the game port (no /dev/tcp, no netcat, no retry loops) -
# a probe connection BECOMES the peer and silently kills the real link
# (measured). Instead:
#   - Loopback: RED launches, the script sleeps a FIXED stagger, BLUE
#     launches. Default 3 seconds (validated: RED's listener opens in ~1-2s,
#     and the in-game pairing window is tight, so a big offset misses it -
#     3s empirically links, 5s and 10s empirically fail). --stagger overrides.
#   - LAN: human-coordinated. Start RED (option 2) on its PC first, then
#     start BLUE (option 3) on the other PC a few seconds later. This
#     replaces the Windows launcher's control-socket rendezvous on port
#     9875 - on Linux only the game port itself (default 9876) is used, so
#     the RED PC's firewall needs exactly one inbound TCP rule.
#
# Platform notes vs the Windows launcher (Windows-only features, skipped):
#   - No window positioning / borderless conversion: modern Linux desktops
#     (Wayland - SteamOS, GNOME, KDE defaults) do not allow external window
#     control. Windowed cabinets are placed by the compositor; fullscreen
#     uses MAME's native fullscreen, and the V menu can target a specific
#     output (-screen screenN, an SDL display index).
#   - Loopback with BOTH cabinets fullscreen on the SAME display will fight
#     for it (the Windows launcher works around this with borderless
#     windows) - give each cabinet its own monitor in the V menu instead.
#
# Layout expected next to this script (the release bundle ships this):
#   launch_tc2.sh  mametc2  roms/timecrs2.zip  [lib/ bundled libs]
#   cfg/ nvram/ (RED cabinet)  instance-b/cfg/ instance-b/nvram/ (BLUE)
#
# Needs only bash + coreutils (seq, sleep) + sed/grep. ASCII only.

set -u
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
INSTANCE_B="$SCRIPT_DIR/instance-b"
SETTINGS="$SCRIPT_DIR/launcher_settings.conf"
GAME_PORT=9876                        # MAME link port (C139 transport)
[ -d "$SCRIPT_DIR/lib" ] && export LD_LIBRARY_PATH="$SCRIPT_DIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

RED='\033[31m'; CYAN='\033[36m'; YELLOW='\033[33m'; GREEN='\033[32m'; GRAY='\033[90m'; MAGENTA='\033[35m'; NC='\033[0m'

# ---- Saved settings (KEY=VALUE; parsed, not sourced) ------------------------------
DEFAULT_LISTENER_IP='192.168.1.10'    # placeholders - set the real IPs once via C
DEFAULT_CONNECTOR_IP='192.168.1.11'
LISTENER_IP=$DEFAULT_LISTENER_IP      # RED (host/listener) PC
CONNECTOR_IP=$DEFAULT_CONNECTOR_IP    # BLUE (connector) PC
NETWORK='Ethernet'
LAST_CHOICE=''
RED_FULLSCREEN=n; BLUE_FULLSCREEN=n   # defaults: windowed 640x480, auto monitor
RED_RES=640x480;  BLUE_RES=640x480
RED_MON=auto;     BLUE_MON=auto       # SDL display index (MAME: -screen screenN)

valid_ipv4() { [[ "$1" =~ ^([0-9]{1,3}\.){3}[0-9]{1,3}$ ]]; }

load_settings() {
    [ -f "$SETTINGS" ] || return 0
    local key val
    while IFS='=' read -r key val; do
        case "$key" in
            LISTENER_IP)     valid_ipv4 "$val" && LISTENER_IP=$val ;;
            CONNECTOR_IP)    valid_ipv4 "$val" && CONNECTOR_IP=$val ;;
            NETWORK)         [[ "$val" == Ethernet || "$val" == WiFi ]] && NETWORK=$val ;;
            LAST_CHOICE)     case "$val" in
                                 loopback|lanred|lanblue) LAST_CHOICE=$val ;;
                                 local) LAST_CHOICE=loopback ;;   # legacy keys
                                 red)   LAST_CHOICE=lanred ;;
                                 blue)  LAST_CHOICE=lanblue ;;
                             esac ;;
            RED_FULLSCREEN)  [[ "$val" =~ ^[yn]$ ]] && RED_FULLSCREEN=$val ;;
            BLUE_FULLSCREEN) [[ "$val" =~ ^[yn]$ ]] && BLUE_FULLSCREEN=$val ;;
            RED_RES)         [[ "$val" =~ ^[0-9]{2,5}x[0-9]{2,5}$ ]] && RED_RES=$val ;;
            BLUE_RES)        [[ "$val" =~ ^[0-9]{2,5}x[0-9]{2,5}$ ]] && BLUE_RES=$val ;;
            RED_MON)         [[ "$val" =~ ^([0-9]|auto)$ ]] && RED_MON=$val ;;
            BLUE_MON)        [[ "$val" =~ ^([0-9]|auto)$ ]] && BLUE_MON=$val ;;
        esac
    done < "$SETTINGS"
}

save_settings() {
    {
        echo "LISTENER_IP=$LISTENER_IP"
        echo "CONNECTOR_IP=$CONNECTOR_IP"
        echo "NETWORK=$NETWORK"
        echo "LAST_CHOICE=$LAST_CHOICE"
        echo "RED_FULLSCREEN=$RED_FULLSCREEN"
        echo "BLUE_FULLSCREEN=$BLUE_FULLSCREEN"
        echo "RED_RES=$RED_RES"
        echo "BLUE_RES=$BLUE_RES"
        echo "RED_MON=$RED_MON"
        echo "BLUE_MON=$BLUE_MON"
    } > "$SETTINGS" 2>/dev/null || echo -e "${YELLOW}WARNING: could not save settings to $SETTINGS${NC}"
}

# ---- Banner (same art as the Windows launcher) ------------------------------------
show_banner() {
    local art=(
' _____  ___  __  __  _____      ____  ____   ___  ____   ___  ____      ___  ___ '
'|_   _||_ _||  \/  || ____|    / ___||  _ \ |_ _|/ ___| |_ _|/ ___|    |_ _||_ _|'
'  | |   | | | |\/| ||  _|     | |    | |_) | | | \___ \  | | \___ \     | |  | | '
'  | |   | | | |  | || |___    | |___ |  _ <  | |  ___) | | |  ___) |    | |  | | '
'  |_|  |___||_|  |_||_____|    \____||_| \_\|___||____/ |___||____/    |___||___|'
    )
    local link=(
' _      ___  _   _  _  __      ____   _         _    __   __'
'| |    |_ _|| \ | || |/ /     |  _ \ | |       / \   \ \ / /'
'| |     | | |  \| || '"'"' /      | |_) || |      / _ \   \ V / '
'| |___  | | | |\  || . \      |  __/ | |___  / ___ \   | |  '
'|_____||___||_| \_||_|\_\     |_|    |_____|/_/   \_\  |_|  '
    )
    local url='github.com/pocketjazzy/MAME-TC2-Public'
    local w=${#art[0]} line pad
    local border; border=$(printf '=%.0s' $(seq 1 $((w + 6))))
    local blank;  blank="||$(printf ' %.0s' $(seq 1 $((w + 4))))||"
    echo
    printf "${RED}/%s\\\\${NC}\n" "$border"
    printf "${RED}%s${NC}\n" "$blank"
    for line in "${art[@]}"; do printf "${RED}||  %-*s  ||${NC}\n" "$w" "$line"; done
    printf "${RED}%s${NC}\n" "$blank"
    for line in "${link[@]}"; do
        pad=$(( (w - ${#line}) / 2 ))
        printf "${RED}||  %*s%-*s  ||${NC}\n" "$pad" '' "$((w - pad))" "$line"
    done
    printf "${RED}%s${NC}\n" "$blank"
    pad=$(( (w - ${#url}) / 2 ))
    printf "${GRAY}||  %*s%-*s  ||${NC}\n" "$pad" '' "$((w - pad))" "$url"
    printf "${RED}\\\\%s/${NC}\n" "$border"
    echo
}

# ---- Cabinet-cfg bootstrap --------------------------------------------------------
# Seed (first run) or verify (later runs) a cabinet folder's cfg/timecrs2.cfg.
# Identity lives in port :JVS_PLAYER1 mask 16384 (value 16384 = RIGHT/BLUE,
# absent/0 = LEFT/RED); the link DIP is port :DSW mask 8 (value 0 = Link Play
# ON, 8 = OFF, and ABSENT means the default = OFF). Existing cfgs are never
# rewritten without a prompt (auto-yes with --unattended); files without a
# recognizable input section are left alone with a warning.
write_seed_cfg() {  # $1 = cfg file, $2 = red|blue
    local jvs=''
    [ "$2" = blue ] && jvs='
            <port tag=":JVS_PLAYER1" type="CONFIG" mask="16384" defvalue="0" value="16384" />'
    cat > "$1" << EOF
<?xml version="1.0"?>
<mameconfig version="10">
    <system name="timecrs2">
        <input>
            <port tag=":DSW" type="DIPSWITCH" mask="8" defvalue="8" value="0" />${jvs}
        </input>
    </system>
</mameconfig>
EOF
}

ask_yes() {  # $1 = prompt; default YES; --unattended auto-applies
    if [ "$UNATTENDED" = 1 ]; then
        echo -e "${GRAY}      (--unattended: applying automatically)${NC}"
        return 0
    fi
    local a; read -r -p "$1" a
    [[ -z "$a" || "$a" =~ ^[Yy] ]]
}

init_cabinet_cfg() {  # $1 = cabinet dir, $2 = red|blue
    local dir=$1 identity=$2 cfg="$1/cfg/timecrs2.cfg" label
    label=$([ "$identity" = red ] && echo 'LEFT/RED' || echo 'RIGHT/BLUE')
    if [ ! -f "$cfg" ]; then
        mkdir -p "$dir/cfg" "$dir/nvram"
        write_seed_cfg "$cfg" "$identity"
        echo -e "${GREEN}First-run setup: seeded $label cabinet identity (Link Play DIP ON) into $cfg - no Tab-menu setup needed.${NC}"
        return 0
    fi
    if ! grep -q 'name="timecrs2"' "$cfg" || ! grep -q '</input>' "$cfg"; then
        echo -e "${YELLOW}WARNING: could not verify $cfg (no timecrs2 input section) - leaving it alone (cabinet identity unverified).${NC}"
        return 0
    fi
    local cur=red dip_on=n v
    grep -q 'tag=":JVS_PLAYER1"[^>]*mask="16384"[^>]*value="16384"' "$cfg" && cur=blue
    grep -q 'tag=":DSW"[^>]*mask="8"[^>]*value="0"' "$cfg" && dip_on=y
    if [ "$cur" != "$identity" ]; then
        local curlabel; curlabel=$([ "$cur" = red ] && echo 'LEFT/RED' || echo 'RIGHT/BLUE')
        echo -e "${YELLOW}NOTE: $cfg is configured as $curlabel, but this launch needs the $label cabinet.${NC}"
        if ask_yes '      Fix it now? (Y/n) '; then
            v=0; [ "$identity" = blue ] && v=16384
            if grep -q 'tag=":JVS_PLAYER1"[^>]*mask="16384"' "$cfg"; then
                sed -i '/tag=":JVS_PLAYER1"[^>]*mask="16384"/ { s/ value="[0-9]*"//; s| */>| value="'"$v"'" />| }' "$cfg"
            elif [ "$identity" = blue ]; then
                sed -i 's|</input>|    <port tag=":JVS_PLAYER1" type="CONFIG" mask="16384" defvalue="0" value="16384" />\n        </input>|' "$cfg"
            fi
            echo -e "${GREEN}Updated $cfg.${NC}"
        else
            echo -e "${YELLOW}      Left unchanged - the link will fail if both cabinets use the same side.${NC}"
        fi
    fi
    if [ "$dip_on" = n ]; then
        echo -e "${YELLOW}NOTE: the Link Play DIP switch is OFF in $cfg - link play needs it ON.${NC}"
        if ask_yes '      Switch it ON now? (Y/n) '; then
            if grep -q 'tag=":DSW"[^>]*mask="8"' "$cfg"; then
                sed -i '/tag=":DSW"[^>]*mask="8"/ { s/ value="[0-9]*"//; s| */>| value="0" />| }' "$cfg"
            else
                sed -i 's|</input>|    <port tag=":DSW" type="DIPSWITCH" mask="8" defvalue="8" value="0" />\n        </input>|' "$cfg"
            fi
            echo -e "${GREEN}Updated $cfg.${NC}"
        else
            echo -e "${YELLOW}      Left OFF - the cabinets will not pair.${NC}"
        fi
    fi
}

# ---- MAME flag assembly -----------------------------------------------------------
display_flags() {  # $1 = red|blue -> per-cabinet MAME display flags
    local fs res mon scr=''
    if [ "$1" = red ]; then fs=$RED_FULLSCREEN; res=$RED_RES; mon=$RED_MON
    else fs=$BLUE_FULLSCREEN; res=$BLUE_RES; mon=$BLUE_MON; fi
    [ "$mon" != auto ] && scr=" -screen screen$mon"
    if [ "$fs" = y ]; then echo "-nowindow -skip_gameinfo$scr"
    else echo "-window -nomaximize -resolution $res -prescale 1 -nokeepaspect -skip_gameinfo$scr"; fi
}

lan_env() {  # $1 = red|blue: WiFi pairing-window widening (LAN roles only)
    if [ "$NETWORK" = WiFi ]; then
        local wait=45; [ "$1" = red ] && wait=60
        export NAMCOS23_PATCH_LINK_WAIT=$wait
        echo -e "${GRAY}Network mode WiFi: passing NAMCOS23_PATCH_LINK_WAIT=$wait (partner counter starts at ${wait}0; use WiFi mode on the other PC too)${NC}"
    else
        unset NAMCOS23_PATCH_LINK_WAIT 2>/dev/null || true
    fi
}

require_bin() {
    MAME_BIN=''
    local name
    for name in mametc2 mame; do
        [ -x "$SCRIPT_DIR/$name" ] && { MAME_BIN="$SCRIPT_DIR/$name"; break; }
    done
    if [ -z "$MAME_BIN" ]; then
        echo -e "${RED}ERROR: no MAME binary (mametc2 / mame) found in $SCRIPT_DIR - build it first.${NC}"
        pause_exit 1
    fi
    ROMPATH="$SCRIPT_DIR/roms"
    if [ ! -f "$ROMPATH/timecrs2.zip" ]; then
        echo -e "${YELLOW}WARNING: $ROMPATH/timecrs2.zip not found - MAME will exit immediately unless a rompath override (mame.ini) points elsewhere.${NC}"
    fi
}

pause_exit() {  # keep failures readable when double-clicked / run from a shortcut
    [ "$UNATTENDED" = 1 ] || read -r -p 'Press Enter to close ' _
    exit "${1:-1}"
}

# NOTE - no port probing, ever: the C139 listener accepts exactly ONE
# connection for the whole session, so any test connection (/dev/tcp, nc,
# a retrying dialer, ...) becomes the peer and kills the real link
# (measured). Launch ordering is done purely by the fixed stagger below.

report_dead_mame() {  # $1 = label
    echo -e "${RED}ERROR: the $1 MAME instance exited immediately.${NC}"
    echo -e "${RED}       Usual cause: roms/timecrs2.zip missing next to the binary, or a wrong -rompath.${NC}"
    echo -e "${YELLOW}       See the real error by running: cd \"$SCRIPT_DIR\" && ./$(basename "$MAME_BIN") timecrs2 -window -skip_gameinfo -log${NC}"
}

warn_role_ip() {  # $1 = expected IP, $2 = placeholder, $3 = role label
    # Warn (do not hard-fail) if this PC's IPs do not include the expected role
    # IP. Skipped while the expected IP is still the built-in placeholder.
    local ips; ips=$(hostname -I 2>/dev/null || true)
    [ -z "$ips" ] && return 0
    [ "$1" = "$2" ] && return 0
    case " $ips " in *" $1 "*) return 0 ;; esac
    echo -e "${YELLOW}WARNING: this PC's IPv4 addresses ($(echo $ips)) do not include the expected $1 for $3.${NC}"
    echo -e "${YELLOW}         Continuing anyway - make sure you picked the right option on the right PC.${NC}"
}

# ---- Launch modes -----------------------------------------------------------------
run_loopback() {
    init_cabinet_cfg "$SCRIPT_DIR" red
    init_cabinet_cfg "$INSTANCE_B" blue
    if [ "$RED_FULLSCREEN" = y ] && [ "$BLUE_FULLSCREEN" = y ] && [ "$RED_MON" = "$BLUE_MON" ]; then
        echo -e "${YELLOW}WARNING: both cabinets are fullscreen on the same monitor - they will fight for the display. Use the V menu to give each cabinet its own monitor.${NC}"
    fi
    echo -e "${CYAN}Launching RED (port $GAME_PORT, no remote peer)...${NC}"
    ( cd "$SCRIPT_DIR" && exec "$MAME_BIN" timecrs2 $(display_flags red) $LOGFLAG $BGFLAG -comm_localport "$GAME_PORT" -comm_remotehost "" ) &
    local red_pid=$!
    echo -e "${GRAY}Waiting ${STAGGER}s (fixed stagger; --stagger overrides) so RED's listener is up before BLUE's one-shot dial...${NC}"
    sleep "$STAGGER"
    if ! kill -0 "$red_pid" 2>/dev/null; then
        report_dead_mame RED
        pause_exit 1
    fi
    echo -e "${CYAN}Launching BLUE (peer 127.0.0.1:$GAME_PORT)...${NC}"
    ( cd "$INSTANCE_B" && exec "$MAME_BIN" timecrs2 $(display_flags blue) $LOGFLAG $BGFLAG -rompath "$ROMPATH" -comm_remotehost 127.0.0.1 -comm_remoteport "$GAME_PORT" -comm_localhost "" ) &
    local blue_pid=$!
    trap 'kill $red_pid $blue_pid 2>/dev/null' INT TERM
    sleep 3
    if ! kill -0 "$red_pid" 2>/dev/null || ! kill -0 "$blue_pid" 2>/dev/null; then
        report_dead_mame 'RED or BLUE'
        kill "$red_pid" "$blue_pid" 2>/dev/null
        pause_exit 1
    fi
    echo -e "${GREEN}Both launched. The game link runs on TCP $GAME_PORT. Close the game windows when done.${NC}"
    [ -n "$LOGFLAG" ] && echo -e "${GRAY}Logs: $SCRIPT_DIR/error.log  +  $INSTANCE_B/error.log${NC}"
    wait "$red_pid" "$blue_pid"
}

run_lanred() {
    init_cabinet_cfg "$SCRIPT_DIR" red
    warn_role_ip "$LISTENER_IP" "$DEFAULT_LISTENER_IP" 'RED (option 2)'
    lan_env red
    echo
    echo -e "${MAGENTA}RED server: MAME will listen on 0.0.0.0:$GAME_PORT.${NC}"
    echo -e "${MAGENTA}Now run option 3 on the BLUE PC, a few seconds behind this one - the in-game${NC}"
    echo -e "${MAGENTA}pairing windows must overlap (WiFi mode widens them; use it on BOTH PCs).${NC}"
    echo -e "${GRAY}(Firewall, one time: this PC must allow inbound TCP $GAME_PORT.)${NC}"
    cd "$SCRIPT_DIR" && exec "$MAME_BIN" timecrs2 $(display_flags red) $LOGFLAG -comm_localport "$GAME_PORT" -comm_remotehost "" -rompath "$ROMPATH"
}

run_lanblue() {
    mkdir -p "$INSTANCE_B"
    init_cabinet_cfg "$INSTANCE_B" blue
    warn_role_ip "$CONNECTOR_IP" "$DEFAULT_CONNECTOR_IP" 'BLUE (option 3)'
    lan_env blue
    echo
    echo -e "${MAGENTA}BLUE client: dialing RED at $LISTENER_IP:$GAME_PORT.${NC}"
    echo -e "${MAGENTA}The RED PC must ALREADY be running (option 2, started a few seconds ago) -${NC}"
    echo -e "${MAGENTA}the connector dials exactly once, so if RED is not up yet the link is lost:${NC}"
    echo -e "${MAGENTA}close both, start RED first, then BLUE.${NC}"
    echo -e "${CYAN}Launching BLUE -> $LISTENER_IP:$GAME_PORT...${NC}"
    cd "$INSTANCE_B" && exec "$MAME_BIN" timecrs2 $(display_flags blue) $LOGFLAG -comm_remotehost "$LISTENER_IP" -comm_remoteport "$GAME_PORT" -comm_localhost "" -rompath "$ROMPATH"
}

# ---- Command line -----------------------------------------------------------------
usage() {
    # Print this file's top comment block (everything up to the first
    # non-comment line) as the help text.
    sed -n '2,/^[^#]/p' "$0" | sed '$d' | sed 's/^# \{0,1\}//'
    exit "${1:-0}"
}

MODE=''; UNATTENDED=0; LOGFLAG=''; BGFLAG=''; STAGGER=3
CLI_RED_FS=''; CLI_BLUE_FS=''; CLI_RED_RES=''; CLI_BLUE_RES=''; CLI_RED_MON=''; CLI_BLUE_MON=''
CLI_LISTENER_IP=''; CLI_CONNECTOR_IP=''; CLI_NETWORK=''
while [ $# -gt 0 ]; do
    case "$1" in
        --mode)           case "${2:-}" in
                              loopback|local) MODE=loopback ;;
                              lanred|red)     MODE=lanred ;;
                              lanblue|blue)   MODE=lanblue ;;
                              *) echo "Invalid --mode '${2:-}' (loopback|lanred|lanblue)"; exit 2 ;;
                          esac; shift 2 ;;
        --listener-ip|--ip) CLI_LISTENER_IP="${2:-}"; shift 2 ;;
        --connector-ip)   CLI_CONNECTOR_IP="${2:-}"; shift 2 ;;
        --network)        case "${2:-}" in
                              [Ee]thernet) CLI_NETWORK=Ethernet ;;
                              [Ww]i[Ff]i)  CLI_NETWORK=WiFi ;;
                              *) echo "Invalid --network '${2:-}' (Ethernet|WiFi)"; exit 2 ;;
                          esac; shift 2 ;;
        --port)           GAME_PORT="${2:-}"; shift 2 ;;
        --stagger)        STAGGER="${2:-}"; shift 2 ;;
        --fullscreen)     CLI_RED_FS=y; CLI_BLUE_FS=y; shift ;;
        --red-fullscreen)  CLI_RED_FS=y; shift ;;
        --blue-fullscreen) CLI_BLUE_FS=y; shift ;;
        --red-resolution)  CLI_RED_RES="${2:-}"; shift 2 ;;
        --blue-resolution) CLI_BLUE_RES="${2:-}"; shift 2 ;;
        --red-monitor)     CLI_RED_MON="${2:-}"; shift 2 ;;
        --blue-monitor)    CLI_BLUE_MON="${2:-}"; shift 2 ;;
        --background-input) BGFLAG='-background_input'; shift ;;
        --log)            LOGFLAG='-log'; shift ;;
        --unattended)     UNATTENDED=1; shift ;;
        -h|--help)        usage 0 ;;
        *) echo "Unknown option: $1 (see --help)"; exit 2 ;;
    esac
done
for v in "$CLI_RED_RES" "$CLI_BLUE_RES"; do
    [ -n "$v" ] && ! [[ "$v" =~ ^[0-9]{2,5}x[0-9]{2,5}$ ]] && { echo "Invalid resolution '$v' (use WxH, e.g. 800x600)"; exit 2; }
done
for v in "$CLI_RED_MON" "$CLI_BLUE_MON"; do
    [ -n "$v" ] && ! [[ "$v" =~ ^([0-9]|auto)$ ]] && { echo "Invalid monitor '$v' (SDL display index 0-9, or auto)"; exit 2; }
done
{ [[ "$GAME_PORT" =~ ^[0-9]+$ ]] && [ "$GAME_PORT" -gt 0 ]; } || { echo "Invalid --port '$GAME_PORT'"; exit 2; }
[[ "$STAGGER" =~ ^[0-9]+([.][0-9]+)?$ ]] || { echo "Invalid --stagger '$STAGGER' (seconds, e.g. 3 or 2.5)"; exit 2; }

load_settings
# Command-line IPs/network override the saved values and are persisted (the
# saved V-menu display settings stay untouched by per-run display overrides).
if [ -n "$CLI_LISTENER_IP" ]; then
    valid_ipv4 "$CLI_LISTENER_IP" || { echo "Invalid --listener-ip '$CLI_LISTENER_IP'"; exit 2; }
    LISTENER_IP=$CLI_LISTENER_IP
fi
if [ -n "$CLI_CONNECTOR_IP" ]; then
    valid_ipv4 "$CLI_CONNECTOR_IP" || { echo "Invalid --connector-ip '$CLI_CONNECTOR_IP'"; exit 2; }
    CONNECTOR_IP=$CLI_CONNECTOR_IP
fi
[ -n "$CLI_NETWORK" ] && NETWORK=$CLI_NETWORK
require_bin

# ---- Banner + choice --------------------------------------------------------------
LOCAL_IPS=$(hostname -I 2>/dev/null | tr -s ' ' | sed 's/ $//' || true)

if [ "$UNATTENDED" = 1 ] && [ -z "$MODE" ]; then
    if [ -n "$LAST_CHOICE" ]; then
        MODE=$LAST_CHOICE
        SAVED_CHOICE_NOTE=1
    else
        echo '--unattended needs --mode loopback|lanred|lanblue (no saved choice found).'
        exit 2
    fi
fi

if [ -n "$MODE" ]; then
    # Non-menu path (--mode given or --unattended): plain banner, no screen clear.
    show_banner
    [ -n "$LOCAL_IPS" ] && echo -e "${GRAY}This PC's local IPv4: ${LOCAL_IPS// /, }${NC}"
    [ "${SAVED_CHOICE_NOTE:-0}" = 1 ] && echo -e "${GRAY}--unattended: using the saved choice $MODE from launcher_settings.conf.${NC}"
else
    # Interactive menu: redraw on a cleared screen each cycle (no scroll creep);
    # the previous action's result is shown as a status line above the prompt.
    NOTICE=''; NOTICE_COLOR=$GREEN
    while true; do
        clear
        show_banner
        [ -n "$LOCAL_IPS" ] && echo -e "${GRAY}This PC's local IPv4: ${LOCAL_IPS// /, }${NC}"
        echo
        echo -e "${CYAN}What do you want to do?${NC}"
        echo '  1 = Local Link Play (two windows on this PC)'
        echo '  2 = Network Link Play - RED  (Server - waits for BLUE)'
        echo "  3 = Network Link Play - BLUE (Client - dials RED at $LISTENER_IP)"
        echo "  C = change saved IPs (RED $LISTENER_IP / BLUE $CONNECTOR_IP)"
        echo "  M = network mode: currently $NETWORK"
        echo '  P = show this PC'"'"'s public IP'
        echo "  V = display settings (RED: fs=$RED_FULLSCREEN $RED_RES mon=$RED_MON | BLUE: fs=$BLUE_FULLSCREEN $BLUE_RES mon=$BLUE_MON)"
        echo '  R = reset display settings to defaults'
        echo '  Q = quit'
        echo
        [ -n "$NOTICE" ] && { echo -e "${NOTICE_COLOR}${NOTICE}${NC}"; echo; }
        prompt='Press 1, 2, 3, C, M, P, V, R or Q'
        [ -n "$LAST_CHOICE" ] && prompt="$prompt (Enter = last: $LAST_CHOICE)"
        read -r -p "$prompt: " pick
        case "$pick" in
            '') if [ -n "$LAST_CHOICE" ]; then MODE=$LAST_CHOICE; break; fi
                NOTICE='Invalid choice.'; NOTICE_COLOR=$YELLOW ;;
            1) MODE=loopback; break ;;
            2) MODE=lanred;   break ;;
            3) MODE=lanblue;  break ;;
            [Cc])
                read -r -p "RED (host/listener) PC IP [$LISTENER_IP]: " ip
                if [ -n "$ip" ]; then
                    if valid_ipv4 "$ip"; then LISTENER_IP=$ip
                    else NOTICE='Not a valid IPv4 address - nothing changed.'; NOTICE_COLOR=$YELLOW; continue; fi
                fi
                read -r -p "BLUE (connector) PC IP [$CONNECTOR_IP]: " ip
                if [ -n "$ip" ]; then
                    if valid_ipv4 "$ip"; then CONNECTOR_IP=$ip
                    else NOTICE='Not a valid IPv4 address - BLUE IP unchanged.'; NOTICE_COLOR=$YELLOW; save_settings; continue; fi
                fi
                save_settings
                NOTICE="Saved: RED $LISTENER_IP / BLUE $CONNECTOR_IP"; NOTICE_COLOR=$GREEN ;;
            [Mm])
                if [ "$NETWORK" = WiFi ]; then NETWORK=Ethernet; else NETWORK=WiFi; fi
                save_settings; NOTICE="Network mode -> $NETWORK (saved)"; NOTICE_COLOR=$GREEN ;;
            [Pp])
                echo -e "${GRAY}Looking up the public IP (api.ipify.org, 5s timeout)...${NC}"
                pub=$(curl -s --max-time 5 https://api.ipify.org 2>/dev/null || wget -qO- -T 5 https://api.ipify.org 2>/dev/null || true)
                if valid_ipv4 "${pub:-}"; then
                    NOTICE="Public IP: $pub   (for internet play the RED player shares this with the BLUE player)"; NOTICE_COLOR=$GREEN
                else
                    NOTICE='Public IP lookup failed (no internet, or curl/wget missing).'; NOTICE_COLOR=$YELLOW
                fi ;;
            [Vv])
                echo
                echo '(Monitor numbers are SDL display indexes: 0 = first, 1 = second, ...)'
                echo '(Window position is placed by your desktop - positioning is a Windows-launcher-only feature.)'
                for side in RED BLUE; do
                    if [ "$side" = RED ]; then fsv=$RED_FULLSCREEN; resv=$RED_RES; monv=$RED_MON
                    else fsv=$BLUE_FULLSCREEN; resv=$BLUE_RES; monv=$BLUE_MON; fi
                    echo "--- $side cabinet ---"
                    read -r -p "  Monitor number, or a = auto [$monv]: " a
                    if [[ "$a" =~ ^[0-9]$ ]]; then monv=$a; elif [[ "$a" =~ ^[Aa] ]]; then monv=auto; fi
                    read -r -p "  Fullscreen? y/n [$fsv]: " a
                    [[ "$a" =~ ^[YyNn]$ ]] && fsv=${a,,}
                    if [ "$fsv" = n ]; then
                        read -r -p "  Window size WxH [$resv]: " a
                        [[ "$a" =~ ^[0-9]{2,5}x[0-9]{2,5}$ ]] && resv=$a
                    fi
                    if [ "$side" = RED ]; then RED_FULLSCREEN=$fsv; RED_RES=$resv; RED_MON=$monv
                    else BLUE_FULLSCREEN=$fsv; BLUE_RES=$resv; BLUE_MON=$monv; fi
                done
                save_settings; NOTICE='Display settings saved.'; NOTICE_COLOR=$GREEN ;;
            [Rr])
                read -r -p 'Reset display settings to defaults? (y/N) ' a
                if [[ "$a" =~ ^[Yy]$ ]]; then
                    RED_FULLSCREEN=n; BLUE_FULLSCREEN=n; RED_RES=640x480; BLUE_RES=640x480; RED_MON=auto; BLUE_MON=auto
                    save_settings; NOTICE='Display settings reset to defaults (windowed 640x480, auto monitor).'; NOTICE_COLOR=$GREEN
                else NOTICE='Reset cancelled - display settings unchanged.'; NOTICE_COLOR=$YELLOW; fi ;;
            [Qq]) exit 0 ;;
            *) NOTICE='Invalid choice.'; NOTICE_COLOR=$YELLOW ;;
        esac
    done
fi

# Persist the values actually used this run (captures command-line IP/network
# overrides too). Per-run display overrides are applied AFTER the save so the
# saved V-menu settings stay as configured.
LAST_CHOICE=$MODE
save_settings
[ -n "$CLI_RED_FS" ]    && RED_FULLSCREEN=$CLI_RED_FS
[ -n "$CLI_BLUE_FS" ]   && BLUE_FULLSCREEN=$CLI_BLUE_FS
[ -n "$CLI_RED_RES" ]   && RED_RES=$CLI_RED_RES
[ -n "$CLI_BLUE_RES" ]  && BLUE_RES=$CLI_BLUE_RES
[ -n "$CLI_RED_MON" ]   && RED_MON=$CLI_RED_MON
[ -n "$CLI_BLUE_MON" ]  && BLUE_MON=$CLI_BLUE_MON

case "$MODE" in
    loopback) run_loopback ;;
    lanred)   run_lanred ;;
    lanblue)  run_lanblue ;;
esac
