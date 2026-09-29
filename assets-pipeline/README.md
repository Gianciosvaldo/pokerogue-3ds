# PokeRogue 3DS asset pipeline (step 3, done 2026-09-26)

Turns the upstream PokeRogue assets into 5 pack files the 3DS app reads from the SD card. Upstream pins are game `8555c08` with its own submodule pins, assets `87426a7` and locales `270ed0a`. The first packs were built from assets `056a1f4`, one commit later, which differs only in `fonts/pokemon-emerald-pro.ttf` ([`sd-builder/README.md`](../sd-builder/README.md#pins-pinsjson)). Everything was tested in Azahar's Old 3DS mode, not on a real console.

| Pack | Contents | Size | Build time (4 cores) |
|---|---|---|---|
| `gfx.pak` | all 16,859 images | 126 MB | 2.7 min |
| `snd.pak` | 2,557 sound effects, cries and move sounds | 53 MB | 1.3 min |
| `bgm.pak` | all 178 music tracks (4.8 h) | 649 MB | 2 min |
| `font.pak` | 5 fonts x 13 sizes, all glyphs incl. CJK | 15 MB | 2.6 min |
| `data.pak` | battle animations, 34 languages of text, data JSON, BGM loop points | 5.6 MB | seconds |
| **total** | | **848 MB** | |

## Building the SD card
On Windows, use the one-click [`sd-builder`](../sd-builder/): it downloads everything at these pins and writes the packs to the card. By hand:
```
python3 -m pip install numpy==1.26.4 pillow==12.3.0 lz4==4.4.5 fonttools==4.66.0   # the versions sd-builder pins
# plus ffmpeg on PATH and a C compiler (cc/gcc/clang)
python3 build_sd.py --assets PATH/pokerogue-assets --game PATH/pokerogue --locales PATH/pokerogue-locales --out sd-card
```
Copy `sd-card/` to the SD card root; the packs go to `sd:/3ds/pokerogue/`. `--only gfx,snd,bgm,font,data` rebuilds a subset. `build-report.json` records counts, sizes, the worst audio SNR and any errors. Sound needs `sd:/3ds/dspfirm.cdc` on a real console, dumped once with the DSP1 homebrew.

**The packs contain Pokémon art, music and PokeRogue content. They are for your own console and must never be shared.** The pipeline and app code contain none of it.

## Formats (details in each file's header comment)
- **Pack (`rpak.py`, `pak.c`)**: one file per category with a hash index sorted for binary search, 24 bytes per entry in RAM. Entries are keyed by upstream path (for example `images/pokemon/back/642-therian` or `audio/bgm/title`) and LZ4-compressed, except music, which is stored raw for streaming.
- **Textures (`gfx.py`)**:
  - Images are stored as 8-bit palette indices with the exact 8-bit palette. Every Pokémon sheet has 178 colors or fewer. Upstream shiny variants and fusions recolor by exact color match, and the palette keeps that possible. `rr_atlas_load_key_variant` applies upstream's variant maps with upstream's shader rules: the first 32 pairs apply and the first match wins.
  - Battle-animation images follow upstream's `loadAnimAssets`: an animation's graphic is cut into 96x96 cells named `0`, `1`, ... The 116 backgrounds (the `AnimTimedAddBgEvent` names) stay whole, as one frame, because upstream draws them as tile sprites. The 2 names used both ways (`PRAS- Psychic BG`, `PRAS- Sketch + Lick`) get the cells plus a whole-image frame named `__BASE`. `tests/anim_bg_test.py` checks this against the PNGs. Upstream names 2 backgrounds that have no image (`PRAS- Extrasensory BG`, `PRAS-Dragon Dance BG`); its own load fails for those too.
  - Frames are de-duplicated and repacked into power-of-two pages of at most 1024 px. That uses 30% less texture memory for Pokémon and a third as much for battle animations. Thundurus-Therian now fits.
  - Texels are pre-swizzled to PICA200 tile order.
  - Images with more than 255 colors (battle effects, event banners) are stored as ready-made texels.
- **Audio (`audio.py`, `tools/dspadpcm.c`, `snd.c`)**:
  - DSP-ADPCM, which the 3DS DSP decodes in hardware at no CPU cost.
  - Music is 32728 Hz stereo, streamed by a high-priority thread with about 1.7 s buffered, so slow frames can't starve it.
  - Each track carries its upstream loop point, and `snd_bgm_play_from` can loop from any other point (upstream passes a loop point per biome).
  - Sound effects keep their own rate, are mono when both channels are identical, and are cached, about 1 MB.
  - Encode quality: worst track 27 dB SNR, worst sound effect 20 dB.
- **Fonts (`font.py`)**: 1-bit glyphs at every size upstream's UI uses (canvas px / 6, rounded: 5-16 and 21 px). They are packed into 256x256 pages, and the app loads only the pages a string touches. `rr_font_load_key("fonts/emerald@16")`. Glyphs are placed with Pillow's Raqm (HarfBuzz) layout, as browsers do, and the build stops if Raqm is missing: Pillow's basic layout makes 5% of glyphs 1 px wider or narrower. Pillow's Linux and macOS wheels include it; on Windows it also needs `fribidi-0.dll` next to `python.exe` (sd-builder installs it).
- **Data**: JSON, minified. The battle-logic port decides how to parse it.

## Tested in Azahar (Old 3DS mode)
- All 16,859 textures load with 0 failures and no memory leak. Average 4.7 ms each in emulated time; the slowest was 217 ms, a 2270x1414 banner.
- **Pixel test** (`tests/emu_gfx_test.py`, `gfx-pixel-test.png`): these frames match the upstream PNGs:
  - a Thundurus page-2 frame
  - Garchomp
  - a Fire animation cell
  - a shiny-variant Bulbasaur
  - Blaziken
- **Music**: a test chirp plays at the right speed and loops gaplessly at its loop point, 4 times in a row. Left and right channels stay sample-aligned. A real track (`mystery_encounter_gen_5_gts`, 42 s) played through to its end and continued from its upstream loop point at 8.5 s.
- **Sound effects**: a cry, a stereo move sound and a UI sound play at the requested frames. The cry matches the reference decode.
- **Demo scene**: the skeleton's battle scene loading everything from the packs looks the same as the PNG/TTF version (`demo-pak-vs-png.png`). Its text is pixel-identical. Load time fell from 1,043 ms to 61 ms in emulated time.

## Known gaps
- **Real-console timings are unknown.** Azahar doesn't model SD card speed or GPU time.
- **1 px screen offset.** Screenshots show the top screen 1 px low, with row 0 duplicated and row 239 lost. A 1 px rectangle drawn at y=0 lands on rows 0 and 1. That points to Azahar's display output rather than the app, but I haven't confirmed it on hardware.
- **Pixel-test regressions are only caught with that shift allowed for.** `emu_gfx_test.py` compares without the shift, so it reports that offset.
- **Font sizes from fractional upstream sizes are rounded.** 80 canvas px is 13.3 logical px, so it is drawn at 13.
- **Event banners over 1024 px are kept at full size.** The large one is 7 MB of texture; downscaling them would be a later decision.
- `intro_dark.mp4` and `evo_bg.mp4` (videos) are not converted.

## Files
- `build_sd.py`, `gfx.py`, `audio.py`, `font.py`, `rpak.py`, `tools/dspadpcm.c`: the pipeline.
- `app-src/`: `skeleton/src` extended with the following. The `skeleton/` folder itself is unchanged.
  - `pak.c/h`: pack reader
  - `snd.c/h`: music and sound effects
  - RRT textures, variants and baked fonts in `rr.c/h`
  - `paktest.c`: a test mode that runs when `sd:/3ds/pokerogue/paktest.txt` exists
  - `main.c`: loads from the packs when they are present
  - `lr-run.c`: gained `wav=`, `until=` and `shotlast=`
- `pokerogue-assettest.3dsx`: that build. It contains no Pokémon assets.
