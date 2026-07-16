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

Everything is driven by ONE launcher script, `launch_tc2.ps1`. It configures the cabinets for you on first run — no MAME menu setup is needed.

### Single-PC link play:

1. Unzip the ready-built release containing `mametc2.exe` into a new folder.
2. Add your `timecrs2.zip` to the `roms` folder (do not unzip it).
3. Open a PowerShell prompt and navigate to the folder where `mametc2.exe` and `launch_tc2.ps1` are stored.
4. Run the launcher script `.\launch_tc2.ps1` and choose option `1` (Local Link Play).
   - If you are blocked from running unsigned scripts from unknown sources, use `PowerShell.exe -ExecutionPolicy Bypass -File launch_tc2.ps1`
5. When prompted with `Delay between RED and BLUE launch in seconds (Enter = 0.85):`, press Enter to accept the default setting.
6. The NAMCO parental advisory splash screen will appear to hang while both cabinets synchronize their clocks; then the GASHIN logo should appear in sync, which indicates a healthy link. SOLO / LINK PLAY should appear available on the mode-select screen.

   **NOTE:** If you wish to change the window sizes and locations to a new default, you can edit the launcher script.


### Two-PC link play:

1. Follow steps 1-3 on both PCs
2. Run `launch_tc2.ps1` on both PCs.
3. On the BLUE PC, press `C` and enter the RED PC's IP address (BLUE's own entry can be left as-is — only RED's address is required).
   - If you are on the same LAN, enter the REDs LAN IP. If playing online, enter REDs PUBLIC IP address.
   - If either PC is on WiFi, press `M` to switch the mode to `WiFi` on **both** PCs (the choice is saved for next time). WiFi mode widens the in-game partner-search countdown to give the pairing extra margin — see [docs/ADVANCED.md](docs/ADVANCED.md).
4. On the RED PC, create an inbound Windows Firewall rule that allows any TCP port 9875-9876 traffic. Be sure to apply the rule to the network type that matches your current network adapter profile (e.g., Domain/Private/Public).
   - This is a server/client setup and the script assumes RED is always the host, so **only** the RED player will need to configure a Windows Firewall exception.
   - If you want to play over the internet, create a port-forwarding rule on the RED player's router that forwards any TCP port 9875-9876 traffic to the RED PC's LAN IP.
5. RED player chooses `2` (Server), and BLUE player chooses `3` (Client). Each PC's cabinet folder and settings are created and verified automatically at this point.
6. A connection over TCP port 9875 is established, then a countdown will begin. RED instance will start first, followed by BLUE.
7. The NAMCO parental advisory splash screen will appear to hang while both cabinets synchronize their clocks; then the GASHIN logo should appear in sync, which indicates a healthy link. SOLO / LINK PLAY should appear available on the mode-select screen.

---

# Troubleshooting

1. If you're attempting a LAN/WAN session and you're not getting the countdown, check the following:
   - RED has configured an inbound Windows Firewall rule that allows any TCP port 9875-9876 traffic
   - (If WAN) RED has configured a port-forwarding rule that forwards any TCP port 9875-9876 traffic to the RED PC's LAN IP address
   - The BLUE PC has RED's correct IP saved (press `C` in the launcher to check/change it)
   - Both PCs are using the same network mode (`M` in the launcher — Ethernet or WiFi)
2. The launcher normally guarantees the cabinet settings (each side gets its own identity and the Link Play DIP switch ON, created on first run). If you have edited things manually via the MAME Tab menu, the launcher will detect a wrong side/color or a disabled DIP at launch and offer to fix it — answer `Y`.
3. If a MAME instance fails to start, the launcher window now stays open and shows the error, along with a command you can run to see MAME's own error message. The usual causes are a missing `roms\timecrs2.zip` or an outdated graphics driver.

For more background — how cabinet identity works (one program, two folders), why the staggered start matters, WiFi mode, headless/front-end use, and manual setup without the script — see **[docs/ADVANCED.md](docs/ADVANCED.md)**.

---

## What's new in this build

See **[docs/CHANGELOG.md](docs/CHANGELOG.md)** for the full list. In
brief: the two-cabinet link works (same PC or two PCs), there is an
in-game **Link Play Config** menu for network settings, the link-play
DIP switch is properly labeled now that it has been identified, and a
single all-in-one launcher (`launch_tc2.ps1`) sets the cabinets up
automatically on first run and handles the launch timing.

---

## Appendix: advanced notes & tuning

**[docs/ADVANCED.md](docs/ADVANCED.md)** collects everything that goes
beyond the quick-start guides above: how cabinet identity works (one
program, two folders), what the launcher does behind the scenes, manual
setup without the script, WiFi mode, solo play, headless/front-end
integration (BigBox, boot-to-game cabinets), and playing over the
internet.

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


