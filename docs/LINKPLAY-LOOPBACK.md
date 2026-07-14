# Link play on ONE PC (two game windows)

Run two copies of the game side by side on the same PC and link them.
One window is the red cabinet, the other is the blue cabinet — grab a
friend and a second controller/mouse, or just try the link out.

## The idea: one program, two folders

You only need **one** `mametc2.exe`. What makes the two running copies
different cabinets is **the folder each one is started from**. The game
saves its settings (cabinet identity, switches, network settings) into
the folder it runs in — so:

- Folder A = the red cabinet's identity
- Folder B = the blue cabinet's identity

Same program, two folders, two cabinets.

## The easy way: the included script (recommended)

From the folder containing `mametc2.exe` and `roms\timecrs2.zip`, run:

```
.\launch_link_loopback.ps1
```

Press Enter at the delay prompt to accept the default.

**First time only:** the two game windows each need their one-time
setup (step 4 and 5 in the manual section below — cabinet identity and
the link switch). Set those in each window, close both, and run the
script again. From then on it links automatically every time.

> **What the script does**
>
> 1. Creates the second cabinet's folder (`instance-b`) next to the
>    program, if it doesn't exist yet.
> 2. Clears the previous session's log files.
> 3. Starts the RED cabinet from the main folder.
> 4. Waits a moment (the delay you chose) so red is listening first.
> 5. Starts the BLUE cabinet from `instance-b`, pointed at the same
>    `roms` folder — no ROM copy needed.
> 6. Places the two windows side by side on your screen.
>
> Useful extras: `-KillExisting` closes any stuck game windows first;
> `-SoloListener` / `-SoloConnector` start just one side;
> `-NoPosition` skips the window placement.

## The manual way (what the script automates)

1. **Folder A** is your main folder: `mametc2.exe` plus
   `roms\timecrs2.zip`.

2. **Create folder B** anywhere you like (for example `instance-b`
   inside folder A). Inside it, create a plain-text file named
   `mame.ini` containing one line that points at folder A's roms —
   for example:

   ```
   rompath C:\games\tc2\roms
   ```

3. **Start both copies.** Open a terminal in folder A and run
   `.\mametc2.exe timecrs2`. Then open a second terminal **in folder
   B** and run the same program from there, e.g.
   `..\mametc2.exe timecrs2`. The folder you are *in* is what counts.

4. **Give each window its identity.** In the first window press Tab,
   open **Machine Configuration**, and set **Link ID** to **Left/Red**.
   In the second window set it to **Right/Blue**.

5. **Switch the link on in BOTH windows.** Press Tab, open **DIP
   Switches**, and set **Link Play Enabled** to **On**.

6. **Close both and start them again** (same folders). On this launch
   the two games find each other automatically, count down briefly
   while they pair up, and **LINK PLAY** appears on the mode-select
   screen. Each player picks it and you're in.

No network setup is needed on one PC — the built-in defaults connect
the two copies through the PC's own internal loopback connection.

## Where the network settings live (you rarely need this)

Press Tab and you'll find a **Link Play Config** menu between DIP
Switches and Machine Configuration. It shows the connection settings
each copy will use (on one PC the defaults are already correct: the red
side listens on port 9876, the blue side connects to 127.0.0.1:9876).
You can edit them here — changes are saved per folder and take effect
the **next** time you start the game. You only need this menu for
two-PC play; see [LINKPLAY-LAN.md](LINKPLAY-LAN.md).

## Tips

- **The session runs at the slower copy's pace.** Two game windows on
  one PC is roughly twice the work — a modern CPU helps a lot. If it
  feels like slow motion on one PC, try two PCs instead.
- Settings stick per folder. If you delete a folder's `cfg` and `nvram`
  subfolders, redo steps 4–5 for that folder.
