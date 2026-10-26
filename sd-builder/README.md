# SD card builder (Windows)

One download and one double-click that put the game on a 3DS SD card, with nothing to install. The game is either one file (`PokeRogue.3dsx` or an installable `PokeRogue.cia`, with the packs inside it: see **One file instead of loose packs**) or the app plus the loose packs. The packs (`gfx`, `snd`, `bgm`, `font` and `data.pak`, 848 MB) are Pokémon and PokeRogue content, so they can't be shared: each person builds their own from PokeRogue's public files. This wraps [`assets-pipeline/build_sd.py`](../assets-pipeline/) so a non-developer can do that.

**The zip holds no Pokémon or PokeRogue content**, only the tool: the launcher, this folder's Python, the pipeline scripts, the Windows build of the audio encoder (45 KB), the 17 JoJo trainer sprites, the release notes and the game app (`game/`: the 3.9 MB `.3dsx`, its stripped ELF, icon and HOME-menu banner, which is drawn by `tools/make_banner_art.py` from plain colours and a pixel font). The user's steps are in [`READ-ME-FIRST.txt`](READ-ME-FIRST.txt), which is also in the zip.

## The project's own pictures (JoJo trainers)

The JoJo challengers' sprites (17 PNGs, third-party art of unclear licence, committed at lorenzo's decision of 2026-10-25) live in
`sd-builder/extra-images/` and `make-zip.sh` copies them into the zip. The builder packs any PNG it finds under
`files/extra-images/` (next to `builder.py`) into `gfx.pak`, laid out like upstream's `images/` folder, so
`files/extra-images/trainer/custom/kira.png` becomes the picture `images/trainer/custom/kira` that the game asks for. A file of
the same name as an upstream picture replaces it. The folder's files are part of the packs' recipe, so adding or changing one
re-runs the pictures step (the longest step with the music). Tested by `assets-pipeline/tests/extra_images_test.py`.

## What is in a build, and updating

Each builder zip says what its game contains in [`WHATS-NEW.txt`](WHATS-NEW.txt) (also in the zip, next to `READ-ME-FIRST.txt`), including what has not been seen on a real console. Write it again for each refresh, and say there when the converted files (the five packs) change: the builder redoes them whenever the pipeline scripts, the encoder, the upstream pins or `extra-images/` differ from what made the packs it already has, and says "Converting" when it does. The game revision is in `files/VERSION.txt`; the builder prints it when it starts and writes it to `build-log.txt`.

**Updating is "extract over the old folder".** The zip holds none of the folders the builder creates (`files/runtime`, `files/downloads`, `files/work`, `files/build`), so unzipping a newer zip on top of an earlier builder folder replaces only the tool and the game app. The next run then skips each pack whose recipe hasn't changed (`recipe()` in `builder.py`), and only makes the new `PokeRogue.3dsx`/`.cia` and copies it. When no pack is pending the builder fetches nothing at all (`pending_stages()`), so `files/work` (the 780 MB of unpacked upstream files) can be deleted after a finished build without costing the next update a download; with any pack pending it fetches what that needs. That is the normal path for a game-only refresh and is what `READ-ME-FIRST.txt` tells the user. The workflow checks it on Windows (step "Update over an earlier folder"). Dragging the files over the old folder in Explorer and choosing "Replace the files in the destination" should have the same effect as the workflow's `Expand-Archive -Force`, but only the latter is run in CI. One limit: files the newer zip no longer has (a removed `extra-images/` picture) stay in the old folder, so remove those by hand.

The only slow thing a game refresh can force is a pack rebuild, and it is all or nothing: `recipe()` is one hash for all five packs, so a change to one picture re-converts the music too (about 9 minutes on four cores by the asset pipeline's own timings; not measured on lorenzo's PC). Per-pack recipes would save most of that; they were left out because `build_sd.py`'s stages share modules (`font.py` and the data stage import `gfx.py`, the data stage reads `audio.py`) and a wrong dependency list would leave a stale pack on a card without any message.

## What it does
1. `BUILD-SD-CARD.bat` runs `setup.ps1`, which downloads a portable Python (python-build-standalone, 45 MB) with `curl.exe`, checks its SHA-256 and unpacks it with `tar.exe`. Both ship with Windows 10 1803 and later.
2. `builder.py` does the rest, using only the standard library:
   - Finds the SD card: drives with a `3ds` folder, or removable ones. It skips the system drive, avoids "insert a disk" pop-ups from empty slots, and asks before using a drive with no `3ds` folder or one that isn't FAT32. Before the long build it writes a test file, so a locked card is caught at the start.
   - Downloads the pipeline's libraries (4 wheels, plus FriBiDi next to `python.exe`: see **Same packs on every PC** below), ffmpeg (GyanD essentials build) and upstream's files at the pins below. Every tool is checked by SHA-256. The upstream archives are checked by file count and total size after unpacking, because GitHub doesn't promise stable archive bytes. Interrupted downloads resume where the server allows it, and are retried 5 times.
   - Runs `build_sd.py` once per pack (data, font, gfx, snd, bgm) at below-normal priority, using all cores but one. It shows the percentage for gfx and bgm and a "still working" line every minute. A finished pack is marked with a hash of the pins and the pipeline code, so a rerun skips it; a pack stopped halfway is rebuilt. Each pack's SHA-256 goes in the log.
   - Asks how the game should go on the 3DS (a menu; `--format 3dsx|cia|both|folder` skips it, and `--yes` alone means `folder`, as before). For the two one-file formats it then makes the files in `build\bundle\` (see below). A file whose inputs (the packs, the app, `bundle.py`, the makerom pin) haven't changed is kept.
   - Copies the result to the card. Each file goes to a `.part` file first, is synced, and is renamed only when complete; files already on the card are skipped. `folder` puts the packs in `3ds\pokerogue\` and the plain app in `3ds\PokeRogue.3dsx`; `3dsx` puts `3ds\PokeRogue.3dsx` (the one with the packs inside); `cia` puts `cias\PokeRogue.cia`.
   - If `3ds\dspfirm.cdc` is missing, it puts DSP1 (zoogie, MIT) in `3ds\` and tells the user to run it once on the 3DS.
   - Copies any `.3dsx` shipped in `files\apps\` to `3ds\` (the current zip carries the graphics bench, for the next console check).
   - Without a card (typing SKIP, or `--no-sd`), it puts exactly what goes on the card in `SD-CARD-FILES\`, as hard links, for copying by hand.
3. It never deletes anything on the card. It only writes `3ds\pokerogue\*.pak` (folder format), `3ds\PokeRogue.3dsx`, `cias\PokeRogue.cia`, `3ds\DSP1.3dsx` and the shipped `.3dsx` files. After a one-file build it says the old loose packs, if they're there, can be deleted, and that `3ds\pokerogue\save` must be kept.

Everything is logged to `build-log.txt` next to the .bat. It stops with a plain explanation for the cases a user can fix: no internet, a locked or full card, low disk space, a damaged download, or the folder being inside OneDrive (which would upload 2 GB). Anything else keeps the traceback in the log.

## One file instead of loose packs
`PokeRogue.3dsx` and `PokeRogue.cia` are made on the user's PC, from the packs that were just built and the game app that came in the zip. Neither the packs nor a finished file ever enter Git or a CI artifact.

| | `PokeRogue.3dsx` (default) | `PokeRogue.cia` |
|---|---|---|
| What it is | the app, then the five packs as a RomFS appended to it, offset in the header: what `3dsxtool --romfs` writes | an installable title, made by makerom from the app's ELF with the packs as the title's RomFS |
| Made by | `bundle.py` (standard library), checked byte for byte against `3dsxtool` and `mkromfs3ds` | makerom v0.18.4 (MIT, pinned and SHA-256 checked, 240 KB, downloaded only for this) with `game/pokerogue.rsf` |
| Size | the packs + 3.9 MB (about 852 MB) | the packs + 4 to 5 MB (about 853 MB); the card needs about 1 GB free while FBI installs it |
| Start | Homebrew Launcher, like any other homebrew | HOME menu, after a one-time install with FBI |
| Needs | what you needed before | a CFW with signature patches (Luma3DS with them on) and FBI: the title is signed with makerom's test keys (`-target t`), as homebrew CIAs are |
| Saves | `sdmc:/3ds/pokerogue/save`, unchanged | the same folder: not the title's own save data, so uninstalling keeps them |

The game finds its packs in `romfs:/` first and in `sdmc:/3ds/pokerogue/` second (`assets-pipeline/app-src/source/pak.c`), so one app build serves all three formats and an old card with loose packs keeps working. Inside a `.3dsx`, `romfs:/` exists only if the file has a RomFS; `pak_3dsx_has_romfs()` checks that first, because libctru's `romfsMountSelf` reads the header's RomFS offset without checking for zero.

**3DS limits that apply here.**
- **Both one-file formats stop below 4 GB.** Each is one file on the card, and the 3DS reads only FAT32, where a file can't be 4 GiB or more (a `.3dsx` also keeps its RomFS offset in 32 bits). The packs are 0.85 GB now. `bundle.py` refuses a one-file build whose packs are within 16 MB of that, with a message pointing to the loose format; there each pack is its own file, so the total is limited only by the card. The CIA's own format would allow far more (NCCH sizes are 32-bit counts of 0x200-byte units, up to 2 TB), but the card and the installed title's content file don't. Installing also needs the CIA and the installed copy on the card at once: about 1 GB free now, 2x the packs later.
- Memory is unchanged by bundling: both formats reserve the Old 3DS's 64 MB application memory (`SystemMode: 64MB` in the spec). Nothing is loaded into RAM because of the RomFS; the game still streams the packs from the card.
- An installed CIA's service and kernel permission lists are fixed at install time (`cia/pokerogue.rsf`). A service the game starts using later and the spec lacks fails on the console without a clear message: the spec lists what the game uses today, and `ctrtool_check.sh` confirms the file's lists are well formed, not that they are enough.

**How big it is, and how it grows.** The five packs from the pinned upstream (`pins.json`: assets `87426a7`) are 848 MB, measured from `build_sd.py`'s output by the asset-pipeline thread and the SD builder's reference build, not an estimate; I haven't rebuilt them since. Entry counts are the `expect` values in `pins.json`:

| pack | size | entries | average entry |
|---|---|---|---|
| `bgm.pak` (music) | 649 MB (76%) | 178 | 3.6 MB per track |
| `gfx.pak` | 126 MB | 19,483 | 6.5 KB |
| `snd.pak` | 53 MB | 2,557 | 21 KB |
| `font.pak` | 15 MB | 65 | |
| `data.pak` | 5.6 MB | 2,765 | 2 KB |

What the size depends on: music dominates, so new tracks are the only thing that can move the total quickly (20 more tracks is about 70 MB). Sprites and data are small (100 more sprite entries is under 1 MB). Upstream growth is not predictable from here: the pin stays at `87426a7` until the game is playable, then the pin is updated by hand. Room left before a one-file build stops: about 3.1 GB, roughly 3.7 times today's packs. RAM is not the limit: `pak.c` keeps a 24-byte record per entry in memory (about 600 KB for today's 25,048 entries, so 10,000 more entries cost about 240 KB); the data stays on the card and is read when a scene needs it. The limits that bite sooner are per scene (the textures a battle has loaded at once: measured with stand-in sheets on a console, and not yet with the real packs) and the work of adding the content to the game, not the storage.

**What was checked, and what wasn't.**
- `bundle_test.py --reference` (CI): `bundle.py`'s RomFS and `.3dsx` equal `mkromfs3ds`'s and `3dsxtool`'s output byte for byte, for 28 files including an accented name.
- `check_bundle.py` (CI, Windows and Linux): reads the finished `.3dsx` and `.cia` with its own code, looks each pack up by name through the hash table the way libctru does, and compares SHA-256 with the pack. For the CIA it also checks the TMD's hash chain down to the content and the NCCH inside.
- `ctrtool_check.sh` (CI, Linux): ctrtool v1.2.0 reports good ExeFS, RomFS and all three IVFC level hashes, accepts the memory mode and service list, and extracts the five packs byte for byte.
- Azahar (an emulator) ran the probe app (`tests/probe/`: mounts the packs through `pak_mount_default()` and checks them against a manifest) from a `.3dsx` and from a `.cxi` made from the same spec (its libretro core can't load a `.cia`), each with full-size synthetic packs (850 MB): `RESULT PASS`.
- **Not tested:** any of this on a real console. Azahar doesn't check a title's service or kernel permissions and reports `envIsHomebrew()` as false, so the Homebrew Launcher's `romfsMountSelf` path, the CIA's permission set and the installed-title RomFS read speed are untested. The real game has not been started from either bundle (the real packs don't exist off lorenzo's PC), only the probe app has. The first real-console run is the test; if it fails, `3ds\PokeRogue.3dsx` in `folder` format is the fallback and nothing else on the card changes.

## Pins (`pins.json`)
| What | Pin | Download |
|---|---|---|
| Python | python-build-standalone `20250918`, CPython 3.12.11, x86-64 | 45 MB |
| numpy, Pillow, lz4, fontTools | 1.26.4, 12.3.0, 4.4.5, 4.66.0 (PyPI wheels, cp312 win_amd64). numpy stays on 1.26 because 2.x calls `crealf`, which Wine 9's ucrtbase lacks, and the Wine test should run exactly what ships | 24 MB |
| ffmpeg + ffprobe | GyanD codexffmpeg `8.1.2` essentials | 110 MB |
| PokeRogue assets | `pokerogue-assets` `87426a7` (codeload zip; 32,045 files, 765 MB unpacked) | about 530 MB |
| PokeRogue locales | `pokerogue-locales` `270ed0a` (1,851 files) | 4 MB |
| Game source | `pokerogue` `8555c08`: `src/audio/audio-manager.ts` only (BGM loop points) | 17 KB |
| FriBiDi | conda-forge `fribidi-1.0.10-h8d14728_0` (`fribidi-0.dll` only, LGPL; downloaded, not shipped) | 65 KB |
| makerom | 3DSGuy/Project_CTR `makerom-v0.18.4` (MIT), win_x86_64 for the user, ubuntu_x86_64 for the Linux tests; only for the `.cia` | 0.24 MB |
| bannertool | carstene1ns/3ds-bannertool `1.2.3` (Linux), run by `make-zip.sh` only | 0.2 MB |
| ctrtool | 3DSGuy/Project_CTR `ctrtool-v1.2.0` (Linux), run by the CI only | 0.8 MB |
| DSP1 | zoogie/DSP1 `v1.0` `DSP1.3dsx`, only when `dspfirm.cdc` is missing | 0.4 MB |

**Assets pin.** The packs were first built from assets `056a1f4`, while the game's `8555c08` pins its assets submodule at `87426a7`. The two differ in one file only, `fonts/pokemon-emerald-pro.ttf` (upstream #128, font adjustments), and `056a1f4` is one commit later. The builder uses `87426a7`, so every upstream input comes from the one game commit, like `run-core`'s tests. The locales pin `270ed0a` is already the game's submodule pin.

To change a pin: update the URL and hash (or the commit, file count and bytes for an upstream archive) in `pins.json`, then run the tests below. `expect` holds each pack's entry count from the reference build; a mismatch is only noted in the log.

## Same packs on every PC
The packs a user builds are byte-identical to the ones built on Linux and tested in Azahar. Two things were needed for that:
- **Font layout.** Pillow's Windows wheel includes Raqm (HarfBuzz layout) but loads FriBiDi at run time, and without it falls back to its basic layout. That layout uses hinted advances: 31,557 of 630,565 glyphs (5%) came out 1 px wider or narrower than the font's own widths, and 9,386 bitmaps shifted. The builder puts `fribidi-0.dll` next to `python.exe`, and `font.py` now refuses to build without Raqm instead of silently falling back.
- **File order.** `build_sd.py` walks folders in sorted order, so the order of entries inside `data.pak` and `gfx.pak` no longer depends on the file system (their contents were already the same).

The workflow checks this on every run (below).

## Making the zip
```sh
GAME_DIR=DIR sd-builder/make-zip.sh [OUT_DIR] [APP.3dsx ...]    # needs mingw-w64, zip, python3, curl
```
`GAME_DIR` holds `pokerogue.3dsx`, a stripped `pokerogue.elf` and `pokerogue.smdh` from a `game/` build (`make`, then `arm-none-eabi-strip --strip-debug`). With it the zip can make the one-file formats; without it the builder only makes the loose files. The banner is drawn and packed by `bannertool` (the pinned Linux build is downloaded and checked; `BANNERTOOL=PATH` uses your own). It cross-compiles `dspadpcm.exe` (statically, 45 KB) and uses Windows line endings for the .bat, .ps1 and .txt files. It refuses to package anything that looks like upstream content. The zip is reproducible: the file dates are fixed.

## Tests
- `tests/e2e.sh ZIP MIRROR [REFERENCE_PACKS] [--wine]` unzips the packaged zip and runs it against a fake SD card, serving every download from a local mirror. It checks four things:
  - the first run: all five packs, DSP1, the bench and nothing else on the card
  - a second run: every pack skipped, both as a build and as a copy, and a card with `dspfirm.cdc` gets no DSP1
  - a build-only run: `SD-CARD-FILES`
  - a pack marked unfinished: only that pack is rebuilt

  `--wine` runs the Windows Python, libraries, ffmpeg and encoder under Wine.
- `tests/bundle_test.py [--reference [ELF SMDH]]` (standard library): the RomFS and `.3dsx` writer looked up the way libctru does it, its refusals, and with `--reference` byte for byte against `mkromfs3ds` and `3dsxtool` from the devkitarm Docker image.
- `tests/check_bundle.py PACKS [--3dsx F] [--cia F]` and `tests/ctrtool_check.sh CTRTOOL CIA PACKS`: read finished files back with code that shares nothing with `bundle.py`. Planted test: one flipped byte in a pack inside either file is reported.
- `tests/e2e_bundle.sh BUILDER MIRROR`: the one-file builds on fake cards, the format menu (Enter, 2, a wrong answer then 3), reuse of unchanged files, and the checks above (`e2e.sh` runs it when the zip has the game).
- `tests/probe/` + `tests/synth_packs.py`: a small 3DS app and synthetic packs of the real sizes, to run a bundle in Azahar (`tests/probe/README.md`).
- `tests/recipe_test.py` (standard library): `recipe()` changes when an extra picture is edited, added or removed, when a pipeline script or the encoder source changes and when a pin changes, and `pending_stages()` lists the packs whose marker is from another recipe. Planted check: with the extra pictures taken out of `recipe()`, it fails.
- `tests/check_pack_keys.py GFX_PAK EXTRA_IMAGES_DIR --min N`: reads a finished `gfx.pak` with code of its own and checks that every PNG in the folder is in it under the key the game asks for (`images/trainer/custom/kira`), through the hash index. Planted check: a pack made without the folder fails. The workflow runs it on the real `gfx.pak` on Windows and Linux.
- `.github/workflows/sd-builder-windows.yml` builds the game and the zip on Ubuntu, then runs the zip on a GitHub Windows runner, the way a user would: the .bat, `setup.ps1`, the real downloads, a full build into a folder whose path has a space and an accent, a second run, and a third run on a second card with `--format both`, whose files `check_bundle.py` reads back. After that the Windows job does what an update does: it extracts the zip again over the folder, runs (nothing downloaded or converted), runs again with a changed app (only the app is copied), then removes `.gfx.done`, which is what a pictures pack made before the JoJo sprites looks like, and runs once more (only the pictures are made again, `check_pack_keys.py` finds the 17 sprites in the new `gfx.pak`, and the card has it). The same zip also builds on an Ubuntu runner (loose files, then both formats, checked with `check_bundle.py` and ctrtool), and a last job fails unless the two sets of packs have the same SHA-256. It keeps only the logs, build reports and hashes. The packs never leave the runners.
- `assets-pipeline/tests/anim_bg_test.py` checks the battle-animation backgrounds (see the pipeline README).

Results are in the PRs that added this folder (#19) and made the packs identical across platforms. What isn't covered: the drive picker on a real card reader, SmartScreen, antivirus reactions to the unsigned `dspadpcm.exe` and `makerom.exe`, and anything on a real console (see **One file instead of loose packs**).

## Known limits
- Windows 10 1803 or later, or Windows 11, on x86-64. Windows on Arm runs it through emulation (untested). No Mac or Linux launcher: lorenzo builds on Windows.
- Unsigned, so SmartScreen may ask to confirm ("More info", then "Run anyway").
- It depends on GitHub's codeload serving a 530 MB archive of `pokerogue-assets`. If that ever fails, the fallback is Remote Control: a Claude session on the user's PC runs `build_sd.py` directly.
