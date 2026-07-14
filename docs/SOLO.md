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

The game has a DIP switch called **Link Play Enabled** (press Tab in
the game window, then open **DIP Switches** to see it).

- **Off** (the default): the game goes straight to normal solo play.
- **On**: at startup the game spends about 30 seconds searching for a
  partner cabinet (you'll see a countdown). If none is found, it gives
  up and falls back to solo play on its own. Nothing breaks — it just
  wastes half a minute.

So for pure solo play, leave the switch off. If you've been playing
link play and the switch is still on, you can either wait out the
countdown or switch it off and restart.

## 4. Want a second player?

- Two game windows on this PC: [LINKPLAY-LOOPBACK.md](LINKPLAY-LOOPBACK.md)
- A friend on another PC: [LINKPLAY-LAN.md](LINKPLAY-LAN.md)
