# Advanced notes

Background and reference material that goes beyond the README's quick-start
guides: how cabinet identity actually works, what the launcher does for you,
how to set everything up manually without it, WiFi mode, solo play, headless
and front-end use, and the realities of playing over the internet.

## One program, two folders (how cabinet identity works)

You only need **one** `mametc2.exe`. What makes two running copies different
cabinets is **the folder each one is started from**. The game saves its
settings (cabinet identity, DIP switches, network settings) into the `cfg`
and `nvram` subfolders of the folder it runs in — so:

On one PC, the launcher
maintains the second folder (`instance-b`):
- \TC2-LinkPlay-vX.X\ = the red cabinet's identity
- \TC2-LinkPlay-vX.X\instance-b\ = the blue cabinet's identity

On two PCs, each PC's \TC2-LinkPlay-vX.X\ folder simply *is* its cabinet
(and picking BLUE in the launcher uses an `instance-b` folder there too, so
one PC can play either side).

Settings stick per folder, and the launcher seeds a folder's identity + link
DIP automatically the first time it uses it. If you delete a folder's `cfg`
and `nvram` subfolders, that cabinet forgets its settings — the launcher will
re-seed the identity on the next run, and the game rebuilds its own defaults
(so things like gun calibration reset).

## Playing solo

The game plays perfectly well as a single cabinet: run `.\mametc2.exe
timecrs2` and play. Two things worth knowing:

- After its power-on self test the game briefly shows a countdown (starting
  at 300 — it lasts a few seconds). That countdown is part of the game's
  normal startup and appears whether the **Link Play Enabled** DIP switch is
  on or off. The switch only controls whether the game actually pairs with a
  partner cabinet during it.
- With the switch **On** and no partner found, the game falls back to solo
  play on its own — nothing breaks. For pure solo play the switch can stay
  off; leaving it on costs you nothing but those few startup seconds.

## What the launcher does (and its extras)

`launch_tc2.ps1` option `1` (Local Link Play) automates the one-PC setup:

1. Creates the second cabinet's folder (`instance-b`) next to the program,
   if it doesn't exist yet, and seeds both folders' identities + link DIP
   on the first run.
2. Starts the RED cabinet from the main folder.
3. Waits a moment (the delay you chose) so red is listening first.
4. Starts the BLUE cabinet from `instance-b`, pointed at the same `roms`
   folder — no ROM copy needed.
5. Places the two windows side by side on your screen.

Options `2`/`3` do the same for a network session: `2` runs the RED cabinet
from the script's folder and waits; `3` runs the BLUE cabinet from
`instance-b` and dials RED. The two PCs coordinate over TCP port 9875 (a
3-2-1 countdown, a health check that RED's game actually started, then BLUE
launches) before the game link itself takes over on port 9876.

Useful extras: `-KillExisting` closes any stuck game windows first;
`-SoloListener` / `-SoloConnector` start just one side of a loopback pair;
`-NoPosition` skips the window placement; `-Log` writes MAME's `error.log`
in each cabinet folder (off by default).

## Display settings (monitors, window size, fullscreen)

Press `V` in the launcher menu to configure the display **per cabinet** —
RED and BLUE independently. It first shows the detected monitors and both
cabinets' current configuration, then walks through each cabinet:

- **Monitor** — which physical display the cabinet uses (pick a number from
  the list, or `a` for automatic).
- **Fullscreen** — y/n per cabinet.
- **Window size** — the game picture size (e.g. `1920x1080`) when windowed.
- **Window position** — `x,y` **on the chosen monitor**, or `a` for the
  automatic side-by-side layout.

Settings are saved and reused on every launch; press `R` in the menu to
reset them to the defaults (windowed 640x480, side by side) — it asks for
confirmation first. Enter on any prompt keeps the current value.

Worth knowing:

- **Two fullscreen cabinets on one PC use borderless windows.** Two true
  fullscreen games on one PC fight over the display (found in testing), so
  in Local Link Play the launcher runs "fullscreen" cabinets as borderless
  windows sized to the monitor — visually identical, no conflict. Network
  play uses MAME's native fullscreen (one instance per PC).
- **Two players on one PC:** normally only the focused window receives
  input. The opt-in `-BackgroundInput` option keeps BOTH windows live at
  once — right for two lightguns or two mice, wrong for a shared keyboard
  (it would drive both cabinets simultaneously).
- The launcher sizes and places the windows itself (MAME alone treats a
  windowed `-resolution` only as a maximum and has no position option).

## Per-cabinet settings (controls, calibration)

Each cabinet folder keeps its own settings. Anything you change in the MAME
Tab menu **while an instance is running** is saved to THAT instance's
folder when it exits: `cfg\` holds the input mappings, DIP switches, and
machine configuration; `nvram\` holds the game's own service-menu data,
including gun calibration.

To give cabinet A and cabinet B different settings (say, different control
mappings for player 1 and player 2 on one PC), configure one cabinet at a
time:

1. Start ONLY the red cabinet: `.\launch_tc2.ps1 -Mode Loopback -SoloListener`
2. Press Tab, open **Input Settings -> Input (this Machine)**, map the
   controls, then close the game.
3. Start ONLY the blue cabinet: `.\launch_tc2.ps1 -Mode Loopback -SoloConnector`
   and repeat.

From then on every linked launch uses each cabinet's own mappings. The same
approach works for anything per-cabinet: volume, video options, service-menu
settings, gun calibration.

## Manual setup without the scripts

### One PC

1. **Folder A** is your main folder: `mametc2.exe` plus `roms\timecrs2.zip`.
2. **Create folder B** anywhere you like (for example `instance-b` inside
   folder A). Inside it, create a plain-text file named `mame.ini`
   containing one line that points at folder A's roms — for example:

   ```
   rompath C:\games\tc2\roms
   ```

3. **Start both copies.** Open a terminal in folder A and run
   `.\mametc2.exe timecrs2`. Then open a second terminal **in folder B** and
   run the same program from there, e.g. `..\mametc2.exe timecrs2`. The
   folder you are *in* is what counts.
4. **Give each window its identity** (Tab → Machine Configuration →
   left/red in one, right/blue in the other), **switch Link Play Enabled on
   in both** (Tab → DIP Switches), close both, and start them again — red
   first, then blue a moment later.

No network setup is needed on one PC — the built-in defaults connect the two
copies through the PC's own internal loopback connection.

### Two PCs

You can skip the launcher and use the in-game **Link Play Config** menu
instead (press Tab; the menu sits between DIP Switches and Machine
Configuration):

- **Red PC:** set *Listener bind IP* (leave `0.0.0.0` to accept from
  anywhere on your network) and *Listener port* (`9876`).
- **Blue PC:** set *Connector target IP* to the **red PC's IP address**, and
  *Connector target port* to `9876`.

Settings are saved in the folder and take effect on the **next** launch.
Then start the red PC's game first, wait a few seconds, and start the blue
PC's game. The red PC still needs the firewall rule described in the README.

## Why the staggered start?

The red side must already be listening when the blue side dials in — if blue
dials first, nobody answers and the pairing can fail. The launcher gets
this right for you: it verifies the red game actually started and survived
its first three seconds (a health check) before it releases the blue side,
plus a small extra delay for margin. If you launch manually, just remember:
**red first, wait a few seconds, then blue.**

## WiFi mode

WiFi works. In the launcher's menu, press **M** to select **WiFi** mode on
**both** PCs (the choice is remembered, alongside the IPs and your last menu
choice, in `launcher_settings.json`). WiFi mode widens the in-game
partner-search countdown
(the counter starts at 600 on the red side and 450 on the blue side —
roughly double the standard 300) to give the pairing extra margin on a less
predictable connection. For the smoothest play, wired Ethernet is still the
best choice — but a decent modern WiFi network holds up fine.

## Playing over the internet

Technically possible, and it has been done: forward TCP ports 9875-9876 on
the red player's router to the red PC, and have the blue player enter the
red player's public IP. But be warned: the two games run in **tight
lockstep** — every frame is coordinated. That makes the link sensitive to
latency; internet delay slows the game down **for both players**, and
playability over the internet is **not guaranteed**. This build is designed
for a local network; the internet is an experiment you run at your own risk
(of frustration).

## Headless & front-end integration

For front-ends (LaunchBox/BigBox and friends), boot-to-game dedicated
cabinets, or your own wrapper scripts, the launcher runs fully
non-interactive. `-Unattended` guarantees it never stops at a prompt, and
`-Fullscreen` gives the dedicated-cabinet look:

```
powershell -ExecutionPolicy Bypass -File launch_tc2.ps1 -Mode Loopback -Unattended
powershell -ExecutionPolicy Bypass -File launch_tc2.ps1 -Mode LanRed  -Unattended -Fullscreen
powershell -ExecutionPolicy Bypass -File launch_tc2.ps1 -Mode LanBlue -ListenerIP 192.168.1.10 -Unattended -Fullscreen
```

- `-Mode Loopback | LanRed | LanBlue` picks what the menu's `1`/`2`/`3` pick.
  With `-Unattended` and no `-Mode`, the last saved menu choice is reused.
- Everything the menu can set has a parameter: `-ListenerIP` (RED's address),
  `-ConnectorIP`, `-Network Ethernet|WiFi`, `-DelaySeconds`, `-StaggerMs`,
  `-WorkDir <folder>` (use a specific cabinet folder), `-Log` (write MAME's
  error.log, off by default).
- Display parameters (override the saved `V`-menu settings for that run):
  `-RedMonitor 1` / `-BlueMonitor 2` (a number, `DISPLAY2`, or
  `\\.\DISPLAY2`), `-RedResolution 800x600` / `-BlueResolution`,
  `-RedFullscreen` / `-BlueFullscreen` (per cabinet; plain `-Fullscreen` =
  both), `-ListenerX/-ListenerY` / `-ConnectorX/-ConnectorY` (window
  position, relative to the cabinet's monitor), and `-BackgroundInput`
  (both loopback windows keep receiving input - two-gun/two-mouse setups).
- With `-Unattended`, first-run cabinet seeding still happens, and any
  identity/DIP fix that would normally prompt is applied automatically.
- Exit code is `0` on a successful launch and `1` on any failure, so a
  wrapper can react.

### Launching mametc2.exe directly (no script at all)

The game itself is front-end friendly. A cabinet folder that has been set up
once (by the launcher or the Tab menu) boots straight into link play with a
bare command from that folder:

```
.\mametc2.exe timecrs2
```

Network settings resolve in this order: command line, then the folder's
saved **Link Play Config**, then loopback defaults — and the cabinet's
red/blue identity decides whether it listens or connects. Useful arguments:

- `-comm_localport 9876 -comm_remotehost ""` — force the RED/listener role.
- `-comm_remotehost <redIP> -comm_remoteport 9876` — force the BLUE/connector
  role, aimed at the red PC.
- `-rompath <folder>` — where `timecrs2.zip` lives.
- `-cfg_directory <path>` and `-nvram_directory <path>` — point one exe at
  any cabinet identity folder **without** changing the working directory
  (handy when a front-end fixes the working directory).
- The WiFi pairing-window widening is an environment variable, settable in
  any wrapper: `NAMCOS23_PATCH_LINK_WAIT=60` on red / `45` on blue.

One caveat: direct launches skip the launcher's rendezvous and health check,
so the ordering is on you — **start red first, wait a few seconds, then
start blue** within the partner-search countdown.

## The golden rule

The session runs at the pace of the **slower machine**. If one PC can't hold
full speed on its own, the linked session can't either — both players feel
it. On one PC, remember that two game windows are roughly twice the work.
