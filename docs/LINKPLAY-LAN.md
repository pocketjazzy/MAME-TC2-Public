# Link play on TWO PCs (Ethernet or WiFi)

The real deal: two PCs, two screens, one player on each — connected
over your home network. One PC is the **red** cabinet, the other is the
**blue** cabinet.

Red is the side that *waits* for the connection; blue is the side that
*dials in*. Remember that — it decides which PC needs the firewall rule.

## 1. Set up each PC

Each PC gets its own complete folder:

```
tc2-linkplay\
  mametc2.exe
  launch_link_LAN.ps1
  roms\
    timecrs2.zip
```

Then give each PC its one-time identity (do this on both):

1. Run the game once: `.\mametc2.exe timecrs2`
2. Press Tab, open **Machine Configuration**, set **Link ID**:
   - Red PC: **Left/Red**
   - Blue PC: **Right/Blue**
3. Press Tab, open **DIP Switches**, set **Link Play Enabled** to
   **On**.
4. Quit the game. These settings are saved in the folder and stick.

## 2. Open the firewall on the RED PC

**This is the number one cause of "it won't connect."**

The red PC is the one that listens, so Windows must allow the incoming
connections: **allow inbound TCP ports 9875 and 9876** on the red PC.

- Port **9876** carries the game link itself.
- Port **9875** is the small coordination channel the launcher script
  uses to start both PCs at the right moment.

How: open **Windows Defender Firewall with Advanced Security** →
**Inbound Rules** → **New Rule** → Port → TCP → specific ports
`9875-9876` → Allow → apply to your network profile (at least
Private). Alternatively, if Windows pops up an "allow access" prompt
the first time the game runs, allow it for Private networks.

The blue PC needs no firewall change (outgoing connections are allowed
by default).

## 3. Play — the easy way (recommended script)

On **both** PCs, run:

```
.\launch_link_LAN.ps1
```

The script shows a tiny menu:

- Press **1** on the red PC (it becomes the listener and waits).
- Press **2** on the blue PC (it connects and starts the countdown).
- Press **C** to enter the two PCs' IP addresses. You only do this
  once — the script saves them in `lan_settings.json` next to the
  script and reuses them on every later run.
- Press **M** to switch between **Ethernet** and **WiFi** mode (also
  remembered). Use the **same mode on both PCs**.

To find a PC's IP address: run `ipconfig` in a terminal and look for
the "IPv4 Address" of your active network adapter.

What happens next is automatic: the blue PC's script contacts the red
PC's script (over port 9875), counts down 3-2-1, tells red to start its
game, waits until red confirms the game is actually up, then starts the
blue game aimed at the red PC. Within the on-screen partner countdown,
the two games pair up and **LINK PLAY** appears on both mode-select
screens.

## 4. Play — the manual way

You can skip the script and use the in-game **Link Play Config** menu
instead (press Tab; the menu sits between DIP Switches and Machine
Configuration):

- **Red PC:** set *Listener bind IP* (leave `0.0.0.0` to accept from
  anywhere on your network) and *Listener port* (`9876`).
- **Blue PC:** set *Connector target IP* to the **red PC's IP
  address**, and *Connector target port* to `9876`.

Settings are saved in the folder and take effect on the **next**
launch. Then, to play: **start the red PC's game first, wait a few
seconds, then start the blue PC's game** (`.\mametc2.exe timecrs2` on
each). See the next section for why the order matters.

## Why the staggered start?

The red side must already be listening when the blue side dials in — if
blue dials first, nobody answers and the pairing can fail. The script
gets this right for you: it verifies the red game actually started and
survived its first three seconds (a health check) before it releases
the blue side, plus a small extra delay for margin. If you launch
manually, just remember: **red first, wait a few seconds, then blue.**
Both games then show a partner-search countdown (about 30 seconds —
displayed as 300) during which they find each other.

## WiFi

WiFi works. In the script's menu, press **M** to select **WiFi** mode
on **both** PCs. WiFi mode widens the in-game partner-search countdown
(about 60 seconds on the red side, 45 on the blue side) to give the
pairing extra margin on a less predictable connection.

For the smoothest play, wired Ethernet is still the best choice —
but a decent modern WiFi network holds up fine.

## Playing over the internet?

Technically possible: forward TCP port 9876 (and 9875 if you use the
script) on the red player's router to the red PC, and have the blue
player enter the red player's public IP.

But be warned: the two games run in **tight lockstep** — every frame is
coordinated. That makes the link extremely sensitive to latency.
Internet delay slows the game down **for both players**, and
playability over the internet is **not guaranteed**. This build is
designed for a local network; the internet is an experiment you run at
your own risk (of frustration).

## Remember the golden rule

The session runs at the pace of the **slower PC**. If one machine can't
hold full speed on its own, the linked session can't either — both
players feel it. Two reasonably modern PCs make this a non-issue.
