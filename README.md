# Time Crisis II — Link Play for MAME

Two cabinets. Two players. One mission.

On release, two Time Crisis II cabinets could link together for Co-op gameplay. MAME — the arcade emulator — has never emulated that link for Time Crisis II. Officially, it still doesn't.

This project makes it work. It is a modified build of **MAME 0.287** that
emulates the cabinet-to-cabinet link for Time Crisis II. You can link two copies of the game on one PC, or two PCs
over your home network, and play the full co-op campaign.

---

## What you need

1. **The Time Crisis II ROM set** (`timecrs2`), which you must own legally
   — for example, dumped from your own arcade board.

   > **No ROMs are included with this project. None are provided, linked
   > to, or available on request — and that will not change.** This
   > project is emulator code only.

2. **A reasonably modern PC** for each running copy of the game.
   Time Crisis II is demanding to emulate, and the two linked games run in
   tight lockstep: **the whole session runs at the pace of the slower
   machine.** One weak PC slows both players down.

3. **Windows** with PowerShell (any recent Windows 10 or 11 is fine) if
   you want to use the included launcher scripts.

---

## Two ways to get it

- **Download the pre-built release** — grab the release zip, unzip it,
  add your own `roms\timecrs2.zip`, and play. Easiest.
- **Build it from source** — clone this repository and compile it
  yourself. See **[docs/BUILDING.md](docs/BUILDING.md)** for the
  step-by-step Windows guide.

---

## Quick start

Pick the guide that matches how you want to play:

| I want to... | Read this |
|---|---|
| Play alone, one screen | **[docs/SOLO.md](docs/SOLO.md)** |
| Play link play on ONE PC (two game windows) | **[docs/LINKPLAY-LOOPBACK.md](docs/LINKPLAY-LOOPBACK.md)** |
| Play link play on TWO PCs (Ethernet or WiFi) | **[docs/LINKPLAY-LAN.md](docs/LINKPLAY-LAN.md)** |

The short version for link play:

1. Put `mametc2.exe` and your `roms\timecrs2.zip` in place.
2. Run the matching launcher script — `launch_link_loopback.ps1` for one
   PC, `launch_link_LAN.ps1` for two PCs.
3. In each MAME instance, press (TAB) to access the game menu.
      a. Go to "Machine Configuration" and assign each instance a cabinet identity (one left/red, one
   right/blue)
      b. Go to "Dip Switches" and switch **Link Play Enabled** to ON.
4. Close both instances and re-run the matching launcher script — `launch_link_loopback.ps1` for one
   PC, `launch_link_LAN.ps1` for two PCs.
5. The GASHIN logo should appear in sync which means you have a healthy link. SOLO/LINK PLAY should appear available on the mode-select screen.

---

## What's new in this build

See **[docs/CHANGELOG.md](docs/CHANGELOG.md)** for the full list. In
brief: the two-cabinet link works (same PC or two PCs), there is an
in-game **Link Play Config** menu for network settings, the link-play
DIP switch is properly labeled, and two launcher scripts handle all the
timing for you.

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
