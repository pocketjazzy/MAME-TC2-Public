# Playing solo (single player)

The simplest way to play: one PC, one game window, no link.

## 1. Folder layout

Put your legally-owned ROM next to the program:

```
your-folder\
  mametc2.exe
  roms\
    timecrs2.zip
```

## 2. Run it

Open a terminal (or PowerShell) in that folder and run:

```
.\mametc2.exe timecrs2
```

The game boots and plays exactly like a single arcade cabinet.

## 3. About the link switch

The game has a DIP switch called **Link Play Enabled**. To see it,
press **Tab** on the keyboard while the game is running to open MAME's
menu, then pick **DIP Switches**.

Either way, after its power-on self test the game briefly shows a
countdown (starting at 300 — it lasts a few seconds). That countdown is
part of the game's normal startup and appears whether the switch is on
or off. What the switch controls is whether the game will actually
pair with a partner cabinet during it:

- **Off** (the default): the game ignores partners and continues into
  normal solo play.
- **On**: the game looks for a partner during the countdown. If it
  finds none, it falls back to solo play on its own — nothing breaks.

For pure solo play the switch can stay off; leaving it on costs you
nothing but those few startup seconds of searching.

## 4. Want a second player?

- Two game windows on this PC: [LINKPLAY-LOOPBACK.md](LINKPLAY-LOOPBACK.md)
- A friend on another PC: [LINKPLAY-LAN.md](LINKPLAY-LAN.md)
