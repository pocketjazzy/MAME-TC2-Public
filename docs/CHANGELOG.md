# Changelog

All notable changes to the Time Crisis II link-play build, in plain
language.

This is a fan preservation project. It is not affiliated with or
endorsed by Namco or Bandai Namco.

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
