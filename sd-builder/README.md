# SD card builder (Windows)

One download and one double-click that put the game's packs on a 3DS SD card, with nothing to install. The packs (`gfx`, `snd`, `bgm`, `font` and `data.pak`, 848 MB) are Pokémon and PokeRogue content, so they can't be shared: each person builds their own from PokeRogue's public files. This wraps [`assets-pipeline/build_sd.py`](../assets-pipeline/) so a non-developer can do that.

**The zip holds no Pokémon or PokeRogue content**, only the tool: the launcher, this folder's Python, the pipeline scripts and the Windows build of the audio encoder (183 KB). The user's steps are in [`READ-ME-FIRST.txt`](READ-ME-FIRST.txt), which is also in the zip.

## What it does
1. `BUILD-SD-CARD.bat` runs `setup.ps1`, which downloads a portable Python (python-build-standalone, 45 MB) with `curl.exe`, checks its SHA-256 and unpacks it with `tar.exe`. Both ship with Windows 10 1803 and later.
2. `builder.py` does the rest, using only the standard library:
   - Finds the SD card: drives with a `3ds` folder, or removable ones. It skips the system drive, avoids "insert a disk" pop-ups from empty slots, and asks before using a drive with no `3ds` folder or one that isn't FAT32. Before the long build it writes a test file, so a locked card is caught at the start.
   - Downloads the pipeline's libraries (4 wheels), ffmpeg (GyanD essentials build) and upstream's files at the pins below. Every tool is checked by SHA-256. The upstream archives are checked by file count and total size after unpacking, because GitHub doesn't promise stable archive bytes. Interrupted downloads resume where the server allows it, and are retried 5 times.
   - Runs `build_sd.py` once per pack (data, font, gfx, snd, bgm) at below-normal priority, using all cores but one. It shows the percentage for gfx and bgm and a "still working" line every minute. A finished pack is marked with a hash of the pins and the pipeline code, so a rerun skips it; a pack stopped halfway is rebuilt.
   - Copies the packs to `3ds\pokerogue\` on the card. Each one goes to a `.part` file first, is synced, and is renamed only when complete. Packs already on the card are skipped.
   - If `3ds\dspfirm.cdc` is missing, it puts DSP1 (zoogie, MIT) in `3ds\` and tells the user to run it once on the 3DS.
   - Copies any `.3dsx` shipped in `files\apps\` to `3ds\` (the current zip carries the graphics bench, for the next console check).
   - Without a card (typing SKIP, or `--no-sd`), it puts exactly what goes on the card in `SD-CARD-FILES\`, as hard links, for copying by hand.
3. It never deletes anything on the card. It only writes `3ds\pokerogue\*.pak`, `3ds\DSP1.3dsx` and the shipped `.3dsx` files.

Everything is logged to `build-log.txt` next to the .bat. It stops with a plain explanation for the cases a user can fix: no internet, a locked or full card, low disk space, a damaged download, or the folder being inside OneDrive (which would upload 2 GB). Anything else keeps the traceback in the log.

## Pins (`pins.json`)
| What | Pin | Download |
|---|---|---|
| Python | python-build-standalone `20250918`, CPython 3.12.11, x86-64 | 45 MB |
| numpy, Pillow, lz4, fontTools | 1.26.4, 12.3.0, 4.4.5, 4.66.0 (PyPI wheels, cp312 win_amd64). numpy stays on 1.26 because 2.x calls `crealf`, which Wine 9's ucrtbase lacks, and the Wine test should run exactly what ships | 24 MB |
| ffmpeg + ffprobe | GyanD codexffmpeg `8.1.2` essentials | 110 MB |
| PokeRogue assets | `pokerogue-assets` `87426a7` (codeload zip; 32,045 files, 765 MB unpacked) | about 530 MB |
| PokeRogue locales | `pokerogue-locales` `270ed0a` (1,851 files) | 4 MB |
| Game source | `pokerogue` `8555c08`: `src/audio/audio-manager.ts` only (BGM loop points) | 17 KB |
| DSP1 | zoogie/DSP1 `v1.0` `DSP1.3dsx`, only when `dspfirm.cdc` is missing | 0.4 MB |

**Assets pin.** The packs were first built from assets `056a1f4`, while the game's `8555c08` pins its assets submodule at `87426a7`. The two differ in one file only, `fonts/pokemon-emerald-pro.ttf` (upstream #128, font adjustments), and `056a1f4` is one commit later. The builder uses `87426a7`, so every upstream input comes from the one game commit, like `run-core`'s tests. The locales pin `270ed0a` is already the game's submodule pin.

To change a pin: update the URL and hash (or the commit, file count and bytes for an upstream archive) in `pins.json`, then run the tests below. `expect` holds each pack's entry count from the reference build; a mismatch is only noted in the log.

## Making the zip
```sh
sd-builder/make-zip.sh [OUT_DIR] [APP.3dsx ...]    # needs mingw-w64, zip, python3
```
It cross-compiles `dspadpcm.exe` (statically, 45 KB) and uses Windows line endings for the .bat, .ps1 and .txt files. It refuses to package anything that looks like upstream content. The zip is reproducible: the file dates are fixed.

## Tests
- `tests/e2e.sh ZIP MIRROR [REFERENCE_PACKS] [--wine]` unzips the packaged zip and runs it against a fake SD card, serving every download from a local mirror. It checks four things:
  - the first run: all five packs, DSP1, the bench and nothing else on the card
  - a second run: every pack skipped, both as a build and as a copy, and a card with `dspfirm.cdc` gets no DSP1
  - a build-only run: `SD-CARD-FILES`
  - a pack marked unfinished: only that pack is rebuilt

  `--wine` runs the Windows Python, libraries, ffmpeg and encoder under Wine.
- `.github/workflows/sd-builder-windows.yml` builds the zip on Ubuntu, then runs it on a GitHub Windows runner, the way a user would: the .bat, `setup.ps1`, the real downloads, a full build into a folder whose path has a space and an accent, and a second run. It keeps only the log and the build report as artifacts. The packs never leave the runner.
- `assets-pipeline/tests/anim_bg_test.py` checks the battle-animation backgrounds (see the pipeline README).

Results are in the PR that added this folder. What isn't covered: the drive picker on a real card reader, SmartScreen, and antivirus reactions to the unsigned `dspadpcm.exe`.

## Known limits
- Windows 10 1803 or later, or Windows 11, on x86-64. Windows on Arm runs it through emulation (untested). No Mac or Linux launcher: lorenzo builds on Windows.
- Unsigned, so SmartScreen may ask to confirm ("More info", then "Run anyway").
- It depends on GitHub's codeload serving a 530 MB archive of `pokerogue-assets`. If that ever fails, the fallback is Remote Control: a Claude session on the user's PC runs `build_sd.py` directly.
