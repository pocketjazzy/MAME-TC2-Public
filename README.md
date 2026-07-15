# Time Crisis II — Link Play for MAME

On release, two Time Crisis II cabinets could be linked together for co-op gameplay; however, the MAME project has not yet emulated that inter-cabinet link. This custom fork of **MAME 0.287** aims to add that emulation and make link play possible between two separate instances of MAME, either on the same PC or between two PCs over a local or wide area network (LAN/WAN). This fork does **not** account for any other emulated games that use these modified files, so using it for anything other than Time Crisis II is at your own risk.

---

## What you need

1. **The Time Crisis II ROM** `timecrs2.zip` (specifically the `US, TSS3 Ver.B` release)
   — This was developed using only the release mentioned above. While other releases may work, I cannot guarantee it, nor will I be adding support for others at this time.

   > **No ROMs are included with this project. None are provided, linked to, or available on request.** This project is emulator code only.

   — You must own your own legal copy of Time Crisis II — for example, dumped from your own arcade board.
   — I do not condone, promote, or facilitate piracy.

2. **A reasonably modern CPU**
   The MAME emulator is CPU-based, so having a fast CPU on both linked machines is important. They do not need to be the same model, but **the whole session will run at the pace of the slower machine.**
   — This was developed and tested on an AMD Ryzen 9 7950X3D and an Intel i7-12850HX.

3. **Windows 10 or 11 (64-bit)** with PowerShell (if you wish to use the included launcher scripts).

---

## Two ways to get this custom MAME build (mametc2.exe)

- **Download the pre-built release** — download `TC2-LinkPlay-vX.X-win64.zip`, unzip it, and add your own ROM to the `\roms\` folder.
- **Build it from source** — clone this repository and compile it yourself. See **[docs/BUILDING.md](docs/BUILDING.md)** for the step-by-step Windows guide.

---

## Quick start

Pick the guide that matches how you want to play:

| I want to... | Read this |
|---|---|
| Play alone, one screen | **[docs/SOLO.md](docs/SOLO.md)** |
| Play link play on ONE PC (two game windows) | **[docs/LINKPLAY-LOOPBACK.md](docs/LINKPLAY-LOOPBACK.md)** |
| Play link play on TWO PCs (Ethernet or WiFi) | **[docs/LINKPLAY-LAN.md](docs/LINKPLAY-LAN.md)** |

The TL;DR for **single-PC link play**:

1. Unzip the ready-built release containing `mametc2.exe` into a new folder.
2. Add your `timecrs2.zip` to the `roms` folder (do not unzip it).
3. Open a PowerShell prompt and navigate to the folder where `mametc2.exe` and `launch_link_loopback.ps1` are stored.
   — If you are blocked from running unsigned scripts from unknown sources, use `PowerShell.exe -ExecutionPolicy Bypass -File launch_link_loopback.ps1`
4. Run the single-PC launcher script `launch_link_loopback.ps1`.
5. When prompted with `Delay between RED and BLUE launch in seconds (Enter = 0.85):`, press Enter to accept the default setting.
6. In each MAME instance, press TAB to access the MAME/game menu.
   - Go to "Machine Configuration" on both and assign each instance a different cabinet identity (left/red, right/blue).
   - Go to "Dip Switches" on both and switch **Link Play Enabled** to ON.
   - Close both instances.
7. Re-run the launcher script. The individual MAME instance settings you just configured are saved in their respective folders' cfg files.
8. The red NAMCO parental advisory splash screen will appear to hang while both cabinets synchronize their clocks; then the GASHIN logo should appear in sync, which indicates a healthy link. SOLO / LINK PLAY should appear available on the mode-select screen.

   **NOTE:** If you wish to change the window sizes and locations to a new default, you can copy the launcher script and make any changes you need.

The TL;DR for **two-PC link play**:

1. Follow steps 1-6 in the **single-PC link play** instructions above for initial setup.
2. Run the two-PC launcher script `launch_link_LAN.ps1` on both PCs.
3. Press `C` to set each PC's LAN IP address in the interactive PowerShell script.
   — If you are on the same LAN, enter the IP address of the other player's PC. If playing online, enter the PUBLIC IP address of the other PC and your own local LAN address.
4. On the RED PC, create an inbound Windows Firewall rule that allows any TCP port 9875-9876 traffic. Be sure to apply the rule to the network type that matches your current LAN profile (e.g., Domain/Private/Public).
   — This is a server/client setup, so **only** the RED player needs to configure a Windows Firewall exception (and a port-forwarding rule if playing online).
   — If you want to play over the internet, create a port-forwarding rule on your router that forwards any TCP port 9875-9876 traffic to the RED PC's LAN IP.
5. Choose either `1` or `2` in the script to match your color/side, and wait.
6. In each MAME instance, press TAB to access the MAME/game menu.
   - Go to "Machine Configuration" and assign each instance a different cabinet identity (left/red, right/blue).
   - Go to "Dip Switches" and switch **Link Play Enabled** to ON.
7. Close both instances and re-run the `launch_link_LAN.ps1` launcher script on both PCs.
8. The red NAMCO parental advisory splash screen will appear to hang while both cabinets synchronize their clocks; then the GASHIN logo should appear in sync, which indicates a healthy link. SOLO / LINK PLAY should appear available on the mode-select screen.

---

## What's new in this build

See **[docs/CHANGELOG.md](docs/CHANGELOG.md)** for the full list. In
brief: the two-cabinet link works (same PC or two PCs), there is an
in-game **Link Play Config** menu for network settings, the link-play
DIP switch is properly labeled now that it has been identified, and two
launcher scripts help streamline the launch timing.

---

## Appendix: advanced tuning

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
