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
### Real Old 3DS, without the packs (2026-09-28)
lorenzo's Old 3DS, bench `fcea806`, no packs on the SD card, so the scenes used stand-in images of the real sizes and the loading and audio sections were skipped (full file: `results/old3ds-console-no-packs.txt`). Frame times in ms; "no vsync" is the whole frame with no wait for the screen (1000 / max fps):

| scene | quads | build (CPU) | `rr_frame_end` | GPU | no vsync | missed refreshes |
|---|---:|---:|---:|---:|---:|---:|
| empty | 0 | 0.02 | 0.17 | 1.13 | 1.35 | 0 |
| singles | 327 | 2.29 | 0.23 | 2.81 | 5.38 | 0 |
| singles + move anim | 360 | 2.74 | 0.23 | 5.07 | 8.06 | 0 |
| doubles | 384 | 2.71 | 0.23 | 3.06 | 5.99 | 0 |
| doubles + move anim | 417 | 3.14 | 0.23 | 5.30 | 8.70 | 0 |
| doubles + anim + sound | 417 | 3.16 | 0.23 | 5.31 | 8.70 | 0 |
| doubles + anim, largest sprites | 417 | 3.15 | 0.23 | 5.42 | 8.77 | 0 |

What the console run settles:
- **The 1 px offset is Azahar's.** The lines on all four edges of both screens are exactly where they were drawn in the framebuffer, and all four are visible on the real top screen (lorenzo's answer). Azahar's libretro output shifts both screens down one row; the console doesn't.
- **A battle fits in a frame with room to spare.** The busiest scene (doubles with a move animation, the largest sprites in the game) takes 8.8 ms of the 16.7 ms frame, and no scene missed a refresh. CPU and GPU run one after the other here (build + end + GPU adds up to the no-vsync frame), so drawing the next frame while the GPU finishes the last one could win back up to about 3 ms if it's ever needed (inferred, not measured).
- **The move animation is the GPU cost, not the Pokemon.** 29 animation sprites plus a full-screen background add about 2.2 ms of GPU time. An 82x80 Pokemon sprite costs the GPU 0.048 ms and the CPU 5 µs, so about 266 of them fit at 60 fps (the sprite ramp is GPU-bound). Text is the opposite: 8.8 µs of CPU per glyph with its shadow and little GPU, about 1480 glyphs at 60 fps.
- **Fill rate:** one full-screen 400x240 layer costs the GPU 0.57 ms with the texture in FCRAM (where `rr` puts every texture) and 0.48 ms in VRAM. VRAM is 16% faster; not worth managing VRAM for now.
- **The per-frame cache flush is cheap:** flushing the whole 32 MB linear heap takes 0.08 ms (max 0.22).
- **Drawing code runs 1.7x to 1.9x slower on the console than in Azahar** (build times of the same scenes), not the 2.35x of the battle logic. The busiest scene took 3.4 ms of CPU with stand-in glyphs; with the real font expect about 4 ms (in Azahar the real font added 0.35 ms; inferred). The earlier guess of 5.6 ms was too high.
- **SD card (lorenzo's card):** each read costs about 5 ms before any data moves, about 10 ms when it starts somewhere new, then about 11 MB/s: random reads took 10.8 ms for 4 KB, 13.1 ms for 32 KB and 33.8 ms for 256 KB; sequential reads ran 2.4 MB/s in 16 KB chunks and 10.2 MB/s in 1 MB chunks, writes 4.3 MB/s. Reading the same 4 MB twice wasn't faster, so nothing caches. A 64 KB save took 131 ms (max 173 ms), so saves must not happen during an animation. Each image in a pack is one seek and one read, so a typical Pokemon sheet (7 to 40 KB in the pack) should cost 11 to 14 ms of SD time plus its LZ4 and texture build (inferred from these reads; loading from the packs wasn't measured yet).
- **Memory behaves exactly as in Azahar:** the same 474 stand-in sheets (29.6 MB) fit, sound fails once the heap is that full, and freeing everything gives it all back. The app heap is 31.7 MB on the console (63.7 MB in Azahar), the linear heap 32 MB on both. So the emulator's memory numbers with the real sheets below should hold on the console.

Still unmeasured on the console: loading from the packs, the CPU the audio takes, and the memory tests with the real sheets. They need a run with the packs on the SD card.

### Azahar
**Azahar doesn't model GPU time, the CPU cache or real SD card timing**, so its GPU column is a constant (0.50 ms whatever is drawn), the cache flush looks free, and its SD and load times aren't the console's.

Azahar, Old 3DS mode, all packs, 60 frames per scene (full files: `results/azahar-old3ds-packs.txt`, `results/azahar-old3ds-no-packs.txt`), ms:

| scene | quads | build (CPU) | `rr_frame_end` | missed refreshes |
|---|---:|---:|---:|---:|
| empty | 0 | 0.01 | 0.29 | 0 |
| singles | 327 | 1.52 | 0.31 | 0 |
| singles + move anim | 360 | 1.77 | 0.31 | 0 |
| doubles | 384 | 1.81 | 0.31 | 0 |
| doubles + move anim | 417 | 2.06 | 0.31 | 0 |
| doubles + anim + sound | 417 | 2.07 | 0.31 | 0 |
| doubles + anim, largest sprites | 417 | 2.06 | 0.31 | 0 |

What only the emulator run has so far (it had the packs):
- **Memory with the real sheets.** A singles battle holds 2.78 MB of textures, doubles 1.50 MB more, and even the four largest sheets in the game on top leave 16.0 MB free. 145 Pokemon front sheets (27.9 MB) fit before a load fails with 1.6 MB still free. Sound effects and music still start at that point. Freeing everything gives the memory back (4 KB short, within the sound cache).
- **Loading a Pokemon is CPU-heavy as well as SD-heavy.** A median sheet takes 16.4 ms here, of which LZ4 and the texture build are about half; the largest (back Thundurus-T, 3 MB of texture) takes 95 ms, 56 ms of it CPU, so roughly 100 ms of CPU on the console if the drawing code's 1.7-1.9x holds for it, plus its SD read. Music playing doesn't slow loading down here.

## Found while building this
- **Upstream's boot-time preload leaves no room for a battle.** Every image upstream's loading scene loads at boot comes to 28.2 MB of textures in this port's format. It does load (183 of 183, in the emulator), but leaves 1.3 MB of the 32 MB linear heap, which also has to hold the Pokemon, the GPU buffers and sound. Loading must be on demand.
- **Battle-animation backgrounds are cut up in `gfx.pak`.** `gfx.py` slices every `battle_anims/*.png` into 96x96 cells, but upstream loads the 121 animation backgrounds (named in the animations' `AnimTimedAddBgEvent`/`AnimTimedUpdateBgEvent` events) as whole images (`loadImage` in `battle-anims.ts`), so their edges are lost. Two names are used both ways (`PRAS- Psychic BG`, `PRAS- Sketch + Lick`). The bench draws a stand-in background for this reason. Not fixed here.
- **Every frame flushes the whole 32 MB linear heap from the CPU cache.** `rr_frame_end` calls `C3D_FrameEnd(0)`, and citro3d then runs `GSPGPU_FlushDataCache` over the entire linear heap, because citro2d's vertex buffer needs flushing and citro2d doesn't flush it itself. On the console that costs 0.08 ms a frame, so it stays as it is.

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
Copy `pokerogue-gfx-bench.3dsx` to `sd:/3ds/`, start it from the Homebrew Launcher and leave it running (76 s on an Old 3DS without the packs; longer with them). At the end the top screen shows a thin line on each edge and the bottom screen asks what you see: press A if you can see all four, or the d-pad direction of a missing one. Then press START to exit. The results are in `sd:/3ds/pokerogue-gfx-bench-result.txt` (and the full log in `pokerogue-gfx-bench-log.txt`). On a New 3DS the bench keeps the Old 3DS clock.

## Files
- `source/gfxbench.c`: order of the sections, the pack and sound check
- `source/scene.c`: the scenes, stand-in images, frame timing and the ramps
- `source/loads.c`: SD card, pack loading, audio CPU and memory sections
- `source/edge.c`: the screen-edge readback and question
- `source/report.c`: result file, progress screen, statistics
- `source/preload_keys.h`: generated by `tools/gen-preload-keys.py`
- `tools/run-azahar.sh`: headless emulator run
- `results/`: result files from the console and from Azahar
