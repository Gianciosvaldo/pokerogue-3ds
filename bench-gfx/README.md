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
Only the emulator so far; the console run is still to come. **Azahar doesn't model GPU time, the CPU cache or real SD card timing**, so its GPU column is a constant (0.50 ms whatever is drawn), the cache flush looks free, and its SD and load times aren't the console's. Its CPU times are an approximation: the native battle bench ran 2.35x slower on the real console than in Azahar, and the "console guess" column below is build plus `rr_frame_end` times that factor (a guess, not a measurement).

Azahar, Old 3DS mode, all packs, 60 frames per scene (full files: `results/azahar-old3ds-packs.txt`, `results/azahar-old3ds-no-packs.txt`), ms:

| scene | quads | build (CPU) | `rr_frame_end` | console guess, CPU per frame | missed refreshes |
|---|---:|---:|---:|---:|---:|
| empty | 0 | 0.01 | 0.29 | 0.7 | 0 |
| singles | 327 | 1.52 | 0.31 | 4.3 | 0 |
| singles + move anim | 360 | 1.77 | 0.31 | 4.9 | 0 |
| doubles | 384 | 1.81 | 0.31 | 5.0 | 0 |
| doubles + move anim | 417 | 2.06 | 0.31 | 5.6 | 0 |
| doubles + anim + sound | 417 | 2.07 | 0.31 | 5.6 | 0 |
| doubles + anim, largest sprites | 417 | 2.06 | 0.31 | 5.6 | 0 |

What the emulator run shows:
- **The 1 px offset is Azahar's, not the app's.** Read back from memory, the lines on all four edges of both screens are exactly where they were drawn. In the picture Azahar's libretro core outputs, both screens are shifted down one row and the bottom row wraps round to the top (checked pixel by pixel in the last frame of the run). Whether the real LCD shows all four lines is the question the bench asks on the console.
- **CPU per frame is small.** The busiest scene queues 417 quads in 2.06 ms plus 0.31 ms in `rr_frame_end` (about 5.6 ms on the console by the 2.35x factor, out of 16.7 ms per frame). The sprite ramp costs 2.7 µs per extra sprite and text 7.6 µs per glyph with its shadow, so a glyph costs about three times a Pokemon sprite. The GPU side is unknown until the console runs it.
- **Memory numbers should carry over to the console** (inferred: same 32 MB linear heap and the same libctru allocator; only the app heap differs, 63.7 MB here against 29.6 MB measured on the console by `bench-native`). A singles battle holds 2.78 MB of textures, doubles 1.50 MB more, and even the four largest sheets in the game on top leave 16.0 MB free. 145 Pokemon front sheets (27.9 MB) fit before a load fails with 1.6 MB still free. Sound effects and music still start at that point. Freeing everything gives the memory back (4 KB short, within the sound cache).
- **Loading a Pokemon is CPU-heavy as well as SD-heavy.** A median sheet takes 16.4 ms here, of which LZ4 and the texture build are about half; the largest (back Thundurus-T, 3 MB of texture) takes 95 ms, 56 ms of it CPU, so over 100 ms on the console before counting the SD card. Music playing doesn't slow loading down here.

## Found while building this
- **Upstream's boot-time preload leaves no room for a battle.** Every image upstream's loading scene loads at boot comes to 28.2 MB of textures in this port's format. It does load (183 of 183, in the emulator), but leaves 1.3 MB of the 32 MB linear heap, which also has to hold the Pokemon, the GPU buffers and sound. Loading must be on demand.
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
Copy `pokerogue-gfx-bench.3dsx` to `sd:/3ds/`, start it from the Homebrew Launcher and leave it running (a few minutes; not yet timed on a console). At the end the top screen shows a thin line on each edge and the bottom screen asks what you see: press A if you can see all four, or the d-pad direction of a missing one. Then press START to exit. The results are in `sd:/3ds/pokerogue-gfx-bench-result.txt` (and the full log in `pokerogue-gfx-bench-log.txt`). On a New 3DS the bench keeps the Old 3DS clock.

## Files
- `source/gfxbench.c`: order of the sections, the pack and sound check
- `source/scene.c`: the scenes, stand-in images, frame timing and the ramps
- `source/loads.c`: SD card, pack loading, audio CPU and memory sections
- `source/edge.c`: the screen-edge readback and question
- `source/report.c`: result file, progress screen, statistics
- `source/preload_keys.h`: generated by `tools/gen-preload-keys.py`
- `tools/run-azahar.sh`: headless emulator run
- `results/`: result files (so far the two emulator runs; console runs go here too)
