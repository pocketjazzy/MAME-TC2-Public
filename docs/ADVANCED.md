# Advanced notes

Background and reference material that goes beyond the README's quick-start
guides: how cabinet identity actually works, what the launcher scripts do for
you, how to set everything up manually without them, WiFi mode, solo play, and
the realities of playing over the internet.

## One program, two folders (how cabinet identity works)

You only need **one** `mametc2.exe`. What makes two running copies different
cabinets is **the folder each one is started from**. The game saves its
settings (cabinet identity, DIP switches, network settings) into the `cfg`
and `nvram` subfolders of the folder it runs in — so:

On one PC, the loopback script
maintains the second folder (`instance-b`):
- \TC2-LinkPlay-v0.1\ = the red cabinet's identity
- \TC2-LinkPlay-v0.1\instance-b\ = the blue cabinet's identity

On two PCs, each PC's \TC2-LinkPlay-v0.1\ folder simply *is* its cabinet.

Settings stick per folder. If you delete a folder's `cfg` and `nvram`
subfolders, that cabinet forgets its identity and switches — just redo the
one-time Tab-menu setup for that folder.

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

## What the loopback script does (and its extras)

`launch_link_loopback.ps1` automates the one-PC setup:

1. Creates the second cabinet's folder (`instance-b`) next to the program,
   if it doesn't exist yet.
2. Starts the RED cabinet from the main folder.
3. Waits a moment (the delay you chose) so red is listening first.
4. Starts the BLUE cabinet from `instance-b`, pointed at the same `roms`
   folder — no ROM copy needed.
5. Places the two windows side by side on your screen.

Useful extras: `-KillExisting` closes any stuck game windows first;
`-SoloListener` / `-SoloConnector` start just one side; `-NoPosition` skips
the window placement.

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

You can skip the LAN script and use the in-game **Link Play Config** menu
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
dials first, nobody answers and the pairing can fail. The LAN script gets
this right for you: it verifies the red game actually started and survived
its first three seconds (a health check) before it releases the blue side,
plus a small extra delay for margin. If you launch manually, just remember:
**red first, wait a few seconds, then blue.**

## WiFi mode

WiFi works. In the LAN script's menu, press **M** to select **WiFi** mode on
**both** PCs (the choice is remembered, alongside the IPs, in
`lan_settings.json`). WiFi mode widens the in-game partner-search countdown
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

## The golden rule

The session runs at the pace of the **slower machine**. If one PC can't hold
full speed on its own, the linked session can't either — both players feel
it. On one PC, remember that two game windows are roughly twice the work.
