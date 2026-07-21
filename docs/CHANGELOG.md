# Changelog

All notable changes to the Time Crisis II link-play build, in plain
language.

This is a fan preservation project. It is not affiliated with or
endorsed by Namco or Bandai Namco.

## v0.3 — 2026-07-20 — Per-cabinet display settings

- **Display settings per cabinet.** New `V` option in the launcher menu:
  choose which monitor each cabinet uses, its window size and position,
  or fullscreen — separately for RED and BLUE. Settings are shown before
  editing, saved for every later launch, and `R` resets them to the
  defaults (with a confirmation). Matching command-line options
  (`-RedMonitor`, `-BlueFullscreen`, `-RedResolution`, ...) cover
  headless and front-end use.
- **Dual fullscreen on one PC.** Both cabinets can run fullscreen on
  separate monitors. The launcher uses borderless windows for this —
  testing showed two true-fullscreen games on one PC fight over the
  display, while borderless looks identical and coexists cleanly.
  Network play keeps real fullscreen (one instance per PC).
- **Two guns, one PC.** The opt-in `-BackgroundInput` option keeps both
  game windows receiving input at the same time — for two lightgun or
  two mouse players on a single PC.
- **Placement fixes.** Window positions are relative to the chosen
  monitor, and configured window sizes are applied exactly.
- **New docs.** ADVANCED.md gains display-settings and per-cabinet
  settings guides (how to give each cabinet its own controls and
  calibration).

The game build (`mametc2.exe`) is unchanged — launcher and
documentation only.

## v0.2 — 2026-07-16 — One launcher, zero setup

- **One script for everything.** `launch_tc2.ps1` replaces the two
  previous launcher scripts. A simple menu covers every way to play:
  `1` link play on one PC, `2` network play as RED (the host), `3`
  network play as BLUE. It remembers your IPs, network mode, and last
  choice, can look up your public IP for internet play, and pressing
  Enter repeats what you did last time.
- **Zero-setup first run.** The launcher now configures the cabinets
  for you: on a fresh install it creates each cabinet's folder, assigns
  the red/blue identities, and switches **Link Play Enabled** on. The
  old routine — open the Tab menu on both windows, set identities and
  the DIP switch, close, relaunch — is gone. Unzip, add your ROM, run
  the script, play.
- **No more same-side mixups.** Picking RED or BLUE in the menu now
  selects the matching cabinet folder automatically, so two PCs can no
  longer accidentally both join as red (previously BLUE had to flip
  its identity by hand in the Tab menu).
- **Front-end and dedicated-cabinet friendly.** New `-Unattended`
  (never prompts) and `-Fullscreen` options let launchers like BigBox,
  boot-to-game arcade cabinets, and custom scripts start link play
  headlessly. See the new headless section in
  [ADVANCED.md](ADVANCED.md).
- **Readable errors.** If something fails to start, the launcher
  window now stays open, explains what went wrong, and shows the
  command to see MAME's own error message.

The game build (`mametc2.exe`) is unchanged from v0.1 — only the
launcher and documentation changed.

## v0.1 — 2026-07-14 — Initial public release

- **Time Crisis II two-cabinet link play works in MAME** — for the
  first time. The original arcade link, never emulated upstream, is now
  playable: two games pair up, LINK PLAY appears on the mode-select
  screen, and two players fight through the campaign together.
- **Play on one PC or two.** Run two game windows on the same PC, or
  link two PCs over your home network — Ethernet or WiFi both work.
- **In-game Link Play Config menu.** Network settings (who listens,
  who connects, on which address and port) are edited right inside
  MAME: press **Tab** on the keyboard while the game is running to open
  MAME's menu, then pick **Link Play Config**. Settings save
  automatically — no config files to edit.
- **The link switch has a proper name.** The DIP switch that turns
  link play on is now labeled "Link Play Enabled" instead of "Unknown".
  You'll find it in the same MAME Tab menu, under **DIP Switches**,
  while the game is running.
- **Smoother motion over the link.** A series of netcode improvements
  keeps the partner's movement and the shared action smooth and in
  step throughout the game.
- **Launcher scripts included.** `launch_link_loopback.ps1` (one PC)
  and `launch_link_LAN.ps1` (two PCs) handle the start-up order,
  timing, and window placement so linking "just works".

Based on MAME 0.287.
