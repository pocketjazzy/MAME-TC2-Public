# Time Crisis II — Link Play for MAME

On release, two Time Crisis II cabinets could be linked together for co-op gameplay; however, the MAME project has not yet emulated that inter-cabinet link. This custom fork of **MAME 0.287** aims to add that emulation and make link play possible between two separate instances of MAME, either on the same PC or between two PCs over a local or wide area network (LAN/WAN). 

This fork does **not** account for any other emulated games that use these modified files, so using it for anything other than Time Crisis II is at your own risk.

---

## What you need:

1. **The Time Crisis II ROM**
   - `timecrs2.zip` (specifically the `US, TSS3 Ver.B` release)
   - Other versions may work, but they have not been tested. I am not planning to add support for others at this time.

   > **No ROMs are included with this project. None are provided, linked to, or available on request.** This project is emulator code only.
   > You must own your own legal copy of Time Crisis II — for example, dumped from your own arcade board.
   > I do not condone, promote, or facilitate piracy.

2. **A reasonably modern CPU**
   - The MAME emulator is CPU-based, so having a fast CPU on both linked machines is important. They do not need to be the same model, but **the whole session will run at the pace of the slower machine.**
   - This was developed and tested on an AMD Ryzen 9 7950X3D and an Intel i7-12850HX.

3. **Windows 10/11 x64 and PowerShell**

4. This custom MAME build (mametc2.exe)
   - **Download the latest pre-built release** — [`TC2-LinkPlay-vX.X-win64.zip`](https://github.com/pocketjazzy/MAME-TC2-Public/releases/)
   - **Build it from source** — clone this repository and compile it yourself. See **[docs/BUILDING.md](docs/BUILDING.md)** for the step-by-step Windows guide.

---

## Quick start guide

### Single-PC link play:

1. Unzip the ready-built release containing `mametc2.exe` into a new folder.
2. Add your `timecrs2.zip` to the `roms` folder (do not unzip it).
3. Open a PowerShell prompt and navigate to the folder where `mametc2.exe` and `launch_link_loopback.ps1` are stored.
   - If you are blocked from running unsigned scripts from unknown sources, use `PowerShell.exe -ExecutionPolicy Bypass -File launch_link_loopback.ps1`
4. Run the single-PC launcher script `launch_link_loopback.ps1`.
5. When prompted with `Delay between RED and BLUE launch in seconds (Enter = 0.85):`, press Enter to accept the default setting.
6. In each MAME instance, press TAB to access the MAME/game menu.
   - Go to "Machine Configuration" on both and assign each instance a different cabinet identity. The left window should be `left/red`. The right window should be `right/blue`.
   - Go to "Dip Switches" on both and switch **Link Play Enabled** to ON.
   - Close both instances.
7. Re-run the launcher script. The individual MAME instance settings you just configured are saved in their respective folders' cfg files.
8. The NAMCO parental advisory splash screen will appear to hang while both cabinets synchronize their clocks; then the GASHIN logo should appear in sync, which indicates a healthy link. SOLO / LINK PLAY should appear available on the mode-select screen.

   **NOTE:** If you wish to change the window sizes and locations to a new default, you can copy the launcher script and make any changes you need.


### Two-PC link play:

1. Follow steps 1-6 in the **single-PC link play** instructions above for initial setup.
2. Run the two-PC launcher script `launch_link_LAN.ps1` on both PCs.
3. Press `C` to set each PC's LAN IP address in the interactive PowerShell script.
   - If you are on the same LAN, enter the IP address of the other player's PC. If playing online, enter the PUBLIC IP address of the other PC and your own local LAN address.
   - If either PC is on WiFi, press `M` to switch the mode to `WiFi` on **both** PCs (the choice is saved for next time). WiFi mode widens the in-game partner-search countdown to give the pairing extra margin — see [docs/ADVANCED.md](docs/ADVANCED.md).
4. On the RED PC, create an inbound Windows Firewall rule that allows any TCP port 9875-9876 traffic. Be sure to apply the rule to the network type that matches your current network adapter profile (e.g., Domain/Private/Public).
   - This is a server/client setup and the script assumes RED is always the host, so **only** the RED player will need to configure a Windows Firewall exception.
   - If you want to play over the internet, create a port-forwarding rule on the RED player's router that forwards any TCP port 9875-9876 traffic to the RED PC's LAN IP.
5. RED player chooses `1`, and BLUE player chooses `2`. Hit `Enter` to accept, and then wait for the other player to connect.
   - BLUE player may need to temporarily set their `left/red` instance to `right/blue` in the machine configuration menu. See the Troubleshooting section below for more information. 
7. A connection over TCP port 9875 is established, then a countdown will begin. RED instance will start first, followed by BLUE.
8. The NAMCO parental advisory splash screen will appear to hang while both cabinets synchronize their clocks; then the GASHIN logo should appear in sync, which indicates a healthy link. SOLO / LINK PLAY should appear available on the mode-select screen.

---

# Troubleshooting

1. If you're attempting a LAN/WAN session and you're not getting the countdown, check the following:
   - Both instances have the Link Play DIP switch set to `ON`
   - Both instances have *different* sides/colors configured in Machine Configuration
   - RED has configured an inbound Windows Firewall rule that allows any TCP port 9875-9876 traffic
   - (If WAN) RED has configured a port-forwarding rule that forwards any TCP port 9875-9876 traffic to the RED PC's LAN IP address
   - The LAN script currently only uses the main folder's mametc2.exe and cfg file. This means that if you followed the Single-PC instructions exactly, you will end up with two players attempting to connect to each other using left/red settings. For this reason, the player who has decided to be BLUE will need to go back into Machine Configuration and temporarily change their `left/red` instance to `right/blue`. If both players attempt to start a link-mode session using the same side/color, the session will drop both cabinets back to solo mode, or fail to start the Stage 1 cutscene. I am working on a fix for this to make the script more dynamic and out-of-the-box ready.

For more background — how cabinet identity works (one program, two folders), why the staggered start matters, WiFi mode, and manual setup without the scripts — see **[docs/ADVANCED.md](docs/ADVANCED.md)**.

---

## What's new in this build

See **[docs/CHANGELOG.md](docs/CHANGELOG.md)** for the full list. In
brief: the two-cabinet link works (same PC or two PCs), there is an
in-game **Link Play Config** menu for network settings, the link-play
DIP switch is properly labeled now that it has been identified, and two
launcher scripts help streamline the launch timing.

---

## Appendix: advanced notes & tuning

**[docs/ADVANCED.md](docs/ADVANCED.md)** collects everything that goes
beyond the quick-start guides above: how cabinet identity works (one
program, two folders), what the launcher scripts do behind the scenes,
manual setup without the scripts, WiFi mode, solo play, and playing over
the internet.

The link code ships with sensible defaults that just work — you should
not need to change anything. For experts, a small number of tuning
overrides exist as environment variables (for things like network-timing
behavior). They are deliberately not documented here; if you know what
you are doing, the relevant comments in the source code (the link device
under `src/mame/namco/`) explain what is available. Everyone else can
safely ignore that they exist.

---

## Legal

This is a fan preservation project. It is not affiliated with, endorsed
by, or connected to Namco or Bandai Namco in any way. Time Crisis is a
trademark of its respective owner.

The emulator source code is available under MAME's license (see the
`COPYING` file in the source repository). No game data is included.

**Disclaimer of warranty and liability:** This software is provided
"as is", without warranty of any kind, express or implied. You download,
install, configure, and use it entirely **at your own risk**. In no event
shall the author or contributors be liable for any claim, damages, or
other liability arising from the use of — or inability to use — this
software. This includes, without limitation, any consequences of
network-configuration changes made to enable link play (such as Windows
Firewall exceptions or router port forwarding): those changes are made at
your own discretion and risk, and you are solely responsible for the
security of your own network.


