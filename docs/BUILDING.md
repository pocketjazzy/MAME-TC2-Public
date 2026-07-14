# Building from source (Windows)

This guide compiles the Time Crisis II link-play build of MAME on
Windows. The result is a single file, `mametc2.exe`.

You do not need to be a programmer — every step is a command you can
copy and paste. Expect the first build to take a while (roughly 15–60
minutes depending on your CPU).

## 1. Install MSYS2

MSYS2 provides the compiler toolchain MAME needs on Windows.

1. Download the installer from [msys2.org](https://www.msys2.org/) and
   install it (the default location `C:\msys64` is fine).
2. Open the **MSYS2 UCRT64** shell from the Start menu. (MSYS2 installs
   several shell flavors — use **UCRT64** for everything below.)
3. Update the base system, then install the standard MAME build tools.
   MAME's own documentation ("Compiling MAME", on docs.mamedev.org)
   lists the current package set; it boils down to:

   ```
   pacman -Syu
   pacman -S --needed git make mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-python
   ```

   If `pacman -Syu` asks you to close and reopen the shell, do that and
   run it once more.

## 2. Get the source

Still in the UCRT64 shell, clone this repository (or download and unzip
the source archive from the releases page):

```
git clone <repository-url> tc2-linkplay
cd tc2-linkplay
```

## 3. Build

```
make SUBTARGET=tc2 REGENIE=1 NOWERROR=1 -j8
```

- Replace `8` in `-j8` with the number of CPU cores you want to use
  (more cores = faster build).
- `SUBTARGET=tc2` builds only the hardware Time Crisis II runs on, which
  is much faster than building all of MAME. The full source is present —
  drop `SUBTARGET=tc2` if you want a complete MAME binary instead.
- **If the build stops early complaining it cannot detect a 64-bit
  toolchain**, add `MINGW64=$MINGW_PREFIX` to the command. Some MSYS2
  setups need this in the UCRT64 shell:

  ```
  make SUBTARGET=tc2 REGENIE=1 NOWERROR=1 MINGW64=$MINGW_PREFIX -j8
  ```

## 4. The result

The build produces **`mametc2.exe`** in the source folder's root.

Create a `roms` folder next to it and put your legally-owned
`timecrs2.zip` inside:

```
tc2-linkplay\
  mametc2.exe
  roms\
    timecrs2.zip
```

No ROMs are included with this project, and none are provided.

## 5. Next steps

- Play alone: [SOLO.md](SOLO.md)
- Link play on one PC: [LINKPLAY-LOOPBACK.md](LINKPLAY-LOOPBACK.md)
- Link play on two PCs: [LINKPLAY-LAN.md](LINKPLAY-LAN.md)
