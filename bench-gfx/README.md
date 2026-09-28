# bench-gfx: what a battle costs the console in frames, memory and loading

A console app that measures the parts of a PokeRogue battle that `bench-native` doesn't: drawing, texture memory, loading from the SD card packs, and audio. It uses `assets-pipeline/app-src`'s renderer (`rr`), pack reader, sound and save code unchanged, and draws scenes laid out like upstream's battle screen. It also checks where 1 px lines drawn on the screen edges land, to settle the 1 px top-screen offset seen in Azahar.

`app-src` is read-only here: the Makefile compiles its sources as they are, minus its demo `main.c` and `paktest.c`.

## What it measures
Every section writes to `sd:/3ds/pokerogue-gfx-bench-result.txt` as it finishes, so a crash or an early stop (START) keeps what was measured.

1. **Screen edges.** Red, green, blue and yellow 1 px lines on the four edges of both screens, then the framebuffer the console displays is read back to find which rows and columns they landed on. At the very end the app asks what the top screen shows (A: all four lines visible, or a d-pad direction for a missing one) and records the answer.
2. **SD card.** Write speed (4 MB), sequential reads at 16 KB to 1 MB per read, the same 4 MB read twice (to spot caching), random 4, 32 and 256 KB reads (pack entries are this size), and a 64 KB save through `save_write` (temp file, CRC, `.bak`, rename).
3. **Loading from the packs.** 16 Pokemon sheets (small to the largest, fronts and backs), arena images, two battle-animation sheets, an icon sheet and the item atlas. Each is split into SD read, LZ4 unpack and texture build, loaded a second time, and once through `rr_atlas_load_key` itself. Then a shiny recolour, two biome changes, a font's first page (English, then Japanese), cries, music start and track switch, and the largest sheet loaded while music streams.
4. **Rendering.** Seven scenes, each 250 frames in the app's normal vsync loop and 120 frames with no vsync wait (the no-vsync frame rate is the headroom):
   - empty (screen clears only), for the fixed per-frame cost
   - singles: arena background and bases, the two Pokemon animated at upstream's 10 fps with shadows, both info boxes with HP and EXP bars, type and status icons, party trays, and text. On the bottom screen: the message strip, command window and cursor, plus a move list with PP and types in the space above them.
   - singles + a move animation at its busiest: 29 sprites from Flamethrower's sheet (29 is the most any upstream animation shows in one frame; the 90th percentile is 11), every 5th additive-blended (20% of upstream's animation sprites are), over a full-field background layer
   - doubles (4 Pokemon, 4 info boxes), with and without the animation
   - doubles + animation + sound: music streaming, a cry every 30 frames and two sound effects every 10
   - doubles + animation with the four largest sprite sheets in the game

   For each: the frame period and missed refreshes under vsync, CPU time to queue the draws, time in `rr_frame_end`, and GPU time (drawing plus the copy to the screens). Then the cost of the data-cache flush citro3d runs in every `rr_frame_end` (below), and three ramps with no vsync: animated Pokemon sprites until a frame takes 45 ms, glyphs with drop shadow, and full-screen layers with the texture in FCRAM (where `rr` puts every texture) and in VRAM.
5. **Audio CPU cost.** A busy loop's speed with music streaming, and with music plus 8 sound effects, against silence.
6. **Memory.** Every image upstream's loading scene keeps loaded from boot (`source/preload_keys.h`, 183 images), then from an empty heap as many Pokemon front sheets as fit, whether sound still starts when the heap is that full, and whether freeing everything gives the memory back.

Also reported: app heap, linear heap and VRAM at start and after opening the packs.

## What the SD card needs
- **Without any packs** it still measures rendering (with stand-in images of the same page and frame sizes, so draw and GPU costs match; only the pixels are made up), memory with stand-in sheets, SD card speed (it writes and reads a 24 MB temporary file, then deletes it) and the screen edges.
- **For the loading, sound and real-image numbers** it needs `gfx.pak`, `snd.pak`, `bgm.pak` and `font.pak` in `sd:/3ds/pokerogue/` (843 MB, built with `../assets-pipeline/build_sd.py`; `data.pak` isn't used), and `sd:/3ds/dspfirm.cdc` for sound. Without `dspfirm.cdc` the sound tests are skipped and the cries are only read.

## Results so far
Only the emulator so far. **Azahar doesn't model GPU time or real SD card timing**, so its GPU column is a constant and its SD and load numbers aren't the console's. Its CPU times are an approximation (the native battle bench ran 2.35x slower on the real console than in Azahar).

Azahar, Old 3DS mode, all packs (bench `EMU_REV`), frame times in ms:

EMU_TABLE

Things the emulator run does show:
EMU_FINDINGS

## Found while building this
- **Upstream's boot-time preload does not fit.** The images upstream's loading scene loads at boot come to 28.2 MB of textures in this port's format, against a 32 MB linear heap that also holds the GPU buffers and sound. Loading must be on demand (computed from `gfx-entries.json`; the memory section measures it on the console).
- **Battle-animation backgrounds are cut up in `gfx.pak`.** `gfx.py` slices every `battle_anims/*.png` into 96x96 cells, but upstream loads the 121 animation backgrounds (named in the animations' `AnimTimedAddBgEvent`/`AnimTimedUpdateBgEvent` events) as whole images (`loadImage` in `battle-anims.ts`), so their edges are lost. Two names are used both ways (`PRAS- Psychic BG`, `PRAS- Sketch + Lick`). The bench draws a stand-in background for this reason. Not fixed here.
- **Every frame flushes the whole 32 MB linear heap from the CPU cache.** `rr_frame_end` calls `C3D_FrameEnd(0)`, and citro3d then runs `GSPGPU_FlushDataCache` over the entire linear heap, because citro2d's vertex buffer needs flushing and citro2d doesn't flush it itself. What that costs on the console is one of the numbers this bench reports; if it's large, the fix is to flush only the buffers that changed.

## Building
```sh
# the .3dsx, with devkitARM in docker (from the repo root)
docker run --rm -v "$PWD":/src -w /src/bench-gfx devkitpro/devkitarm make BENCH_REV=$(git rev-parse --short HEAD)
# optional: regenerate the preload list from an upstream checkout (and the assets checkout for the icon sheets)
tools/gen-preload-keys.py PATH/pokerogue PATH/pokerogue-assets
# in the emulator (see ../tools/azahar-libretro/README.md for lr-run and the core):
tools/run-azahar.sh path/to/lr-run path/to/azahar_libretro.so path/to/sd-card out-dir
```
The emulator run marks the emulated SD card as headless: the final question is skipped, and the sprite and text ramps stop at 400 because Azahar's software renderer takes minutes per step beyond that.

## Running on the console
Copy `pokerogue-gfx-bench.3dsx` to `sd:/3ds/`, start it from the Homebrew Launcher and leave it running for about 3 minutes. At the end the top screen shows a thin line on each edge and the bottom screen asks what you see: press A if you can see all four, or the d-pad direction of a missing one. Then press START to exit. The results are in `sd:/3ds/pokerogue-gfx-bench-result.txt` (and the full log in `pokerogue-gfx-bench-log.txt`). On a New 3DS the bench keeps the Old 3DS clock.

## Files
- `source/gfxbench.c`: order of the sections, the pack and sound check
- `source/scene.c`: the scenes, stand-in images, frame timing and the ramps
- `source/loads.c`: SD card, pack loading, audio CPU and memory sections
- `source/edge.c`: the screen-edge readback and question
- `source/report.c`: result file, progress screen, statistics
- `source/preload_keys.h`: generated by `tools/gen-preload-keys.py`
- `tools/run-azahar.sh`: headless emulator run
- `results/`: result files from real runs
