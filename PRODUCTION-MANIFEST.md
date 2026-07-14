# PRODUCTION MANIFEST — Time Crisis II Link Play for MAME

This file is the contract for what the public (production) branch and the
binary release may and may not contain, and the procedure for updating them
from the private development tree. **Read it before every sync or release.**

---

## INCLUDED (what ships on the production branch)

**The full MAME 0.287 source tree** (as released by mamedev, tag `mame0287`)
**with these modifications and additions:**

| File | Status | What it is |
|---|---|---|
| `src/mame/namco/namco_c139.cpp` | modified | The link device: TCP transport + link protocol emulation |
| `src/mame/namco/namco_c139.h` | modified | Link device header |
| `src/mame/namco/namcos23.cpp` | modified | Driver wiring for the link device; "Link ID" machine-configuration setting; "Link Play Enabled" DIP switch label |
| `src/frontend/mame/ui/linkplay.cpp` | added | The in-game "Link Play Config" menu (per-instance settings, saved to `cfg\timecrs2.cfg`) |
| `src/frontend/mame/ui/linkplay.h` | added | Menu header |
| `src/frontend/mame/ui/mainmenu.cpp` | modified | Adds the Link Play Config entry to the in-game main menu |
| `scripts/src/mame/frontend.lua` | modified | Registers the new UI source files with the build |
| `src/mame/tc2.flt` | added | Driver filter enabling the fast `SUBTARGET=tc2` build |
| `.gitignore` | modified | Keeps ROMs, logs, and local settings out of git |

**Plus the production overlay** (these files, maintained in this layout):

- `README.md` — the public front page (replaces MAME's upstream README)
- `docs/BUILDING.md`, `docs/SOLO.md`, `docs/LINKPLAY-LOOPBACK.md`,
  `docs/LINKPLAY-LAN.md`, `docs/CHANGELOG.md` — the user guides
- `launch_link_loopback.ps1`, `launch_link_LAN.ps1` — the **production**
  launcher scripts (end-user variants, no experiment plumbing)
- `PRODUCTION-MANIFEST.md` — this file

---

## EXCLUDED (must NEVER appear on the production branch or in a release)

| Item | Reason |
|---|---|
| `crouchcrisis-docs/**` | Internal development and reverse-engineering material, **including ROM-derived content**. Must never ship, in any form, ever. |
| `PATCH-LEDGER.csv`, `ROADMAP.md`, `CLAUDE.md`, `PROJECT.md` | Internal development-process documents. |
| `full.txt` (and any disassembly output) | ROM-derived content. Git-ignored on the dev tree; must never be tracked anywhere public. |
| `roms/**` and any `*.zip` game data | Copyrighted game data. No ROMs are ever included, provided, or linked to. |
| `error*.log` and other run logs | Local diagnostic output. |
| `lan_settings.json` | Per-PC user settings written by the LAN script. |
| The dev-branch launcher scripts | The repo-root launchers on development branches carry developer-only experiment plumbing (`-PatchEnv` etc.). Production ships the clean variants listed above instead. |
| `cfg/`, `nvram/`, `instance-b/` and similar | Per-user emulator state. |

---

## UPDATE PROCEDURE (syncing production from development)

**Rule zero: sync only from a development commit the user has personally
validated in link play** (loopback and LAN both confirmed working). Never
sync from an untested tip.

From a checkout of the production branch (PowerShell; `<dev-tip>` = the
validated development commit hash):

```
# 1. Pull the validated dev tree over the working copy.
git checkout <dev-tip> -- .

# 2. Remove everything on the exclusion list that came along.
git rm -r -f crouchcrisis-docs
git rm -f PATCH-LEDGER.csv ROADMAP.md CLAUDE.md PROJECT.md

# 3. Re-apply the production overlay (step 1 overwrote our README and
#    launchers with the dev versions). The overlay is maintained on the dev
#    branch in production-staging/, which mirrors the production layout.
git checkout <dev-tip> -- production-staging
Copy-Item -Recurse -Force production-staging\* .
git rm -r -f production-staging

# 4. Stage the overlay files.
git add README.md PRODUCTION-MANIFEST.md docs launch_link_loopback.ps1 launch_link_LAN.ps1

# 5. Run the VERIFICATION CHECKLIST below. Only commit when every line passes.
```

---

## VERIFICATION CHECKLIST (before every commit/push/release)

Run from the production checkout root. Every command must come back clean.

1. No internal docs directory exists or is tracked:
   `git ls-files crouchcrisis-docs` → no output, and the folder is absent.
2. No disassembly tracked:
   `git ls-files | Select-String -SimpleMatch 'full.txt'` → no output.
3. No game data tracked:
   `git ls-files | Select-String '\.zip$'` → no output.
4. No dev-process docs tracked:
   `git ls-files PATCH-LEDGER.csv ROADMAP.md CLAUDE.md PROJECT.md` → no output.
5. Production scripts carry no experiment plumbing:
   `Select-String -Path launch_link_loopback.ps1,launch_link_LAN.ps1 -Pattern 'PatchEnv'` → no matches.
6. The build succeeds from a clean tree:
   `make SUBTARGET=tc2 REGENIE=1 NOWERROR=1 -j<cores>` (MSYS2 UCRT64;
   add `MINGW64=$MINGW_PREFIX` if needed) → produces `mametc2.exe`.
7. Bare-loopback smoke test: with a local `roms\timecrs2.zip` in place, run
   `.\launch_link_loopback.ps1` — both windows launch, pair up during the
   countdown, and LINK PLAY is selectable on both.
8. Commit identity: every commit on this branch is authored and committed
   as `pocketjazzy <pocketjazzy@users.noreply.github.com>` (no personal
   email). Check: `git log --format='%an <%ae>%n%cn <%ce>' | sort -u`
   shows only that identity. When committing, use
   `git -c user.name=pocketjazzy -c user.email=pocketjazzy@users.noreply.github.com commit ...`

---

## RELEASE BUNDLE (the binary zip)

Layout of the downloadable release archive:

```
TC2-LinkPlay-v<version>\
  mametc2.exe                   (built per docs/BUILDING.md)
  launch_link_loopback.ps1      (production variant)
  launch_link_LAN.ps1           (production variant)
  README.md
  docs\
    BUILDING.md
    SOLO.md
    LINKPLAY-LOOPBACK.md
    LINKPLAY-LAN.md
    CHANGELOG.md
  roms\
    PUT-ROM-HERE.txt            (see below)
  LICENSE.txt                   (see below)
```

`roms\PUT-ROM-HERE.txt` contains only a note along the lines of:
"Place your legally-owned timecrs2.zip in this folder. No ROMs are included
or provided with this project, and none will be."

`LICENSE.txt` contains MAME's license text (the `COPYING` file from the
source tree) plus a statement that the complete corresponding source code
is available at the public repository, as MAME's license requires for
binary distribution.

Before zipping: run the full VERIFICATION CHECKLIST, and confirm the
archive contains **no** `*.zip` game data, no logs, no `lan_settings.json`,
and no `cfg\`/`nvram\` folders.
