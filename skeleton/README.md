# PokeRogue 3DS skeleton (2026-09-26)

A .3dsx for an Old 3DS that boots, draws a PokeRogue battle scene on both screens from upstream art loaded off the SD card, and saves to the SD card. Tested in Azahar's libretro core (software renderer, Old 3DS mode), not on a real console.

## What is here
- `pokerogue-skeleton.3dsx`: the app. It holds no Pokemon art, music or logos; those load from `sdmc:/3ds/pokerogue/assets/`.
- `src/include/rr.h` + `src/source/rr.c`: the renderer, a plain C API over citro2d. It loads PNGs and upstream TexturePacker atlases, draws sprites with Phaser-style origin, trim, flip, rotate and tint, draws 9-slice windows, cropped bars, and TTF text through FreeType. It works in upstream's 320x180 logical coordinates, so upstream numbers can be passed straight in (for example, player Pokemon at 106,148).
- `src/include/save.h` + `src/source/save.c`: local saves replacing the server API. Blobs go to `sdmc:/3ds/pokerogue/save/<name>.sav` with a CRC32 header, written to a temp file and rotated so a `.bak` of the last good save always exists.
- `src/source/main.c`: the demo scene (Garchomp vs Blaziken on the grass arena, info boxes, HP/EXP bars, message box, FIGHT/BALL/POKeMON/RUN menu).
- `src/tools/fetch-assets.py`: downloads the 17 upstream files the demo uses (5.2 MB, mostly the font) into an SD card folder.
- `src/tools/lr-run.c`: headless libretro frontend used for testing (scripted buttons, PNG screenshots).
- `screenshots/`: emulator captures (400x480, top screen above bottom).
- `../tools/azahar-libretro/` (not in git, see its README): the built Azahar libretro core (azahar-emu/azahar `ce0486d`, software renderer only) and `lr-run`, so later threads can skip the ~15 minute emulator build.

## Screen layout (a default, one call to change)
- Top screen (400x240): the full 320x180 battle field at 1:1, centred with 40 px side bars and 30 px top/bottom bars.
- Bottom screen (320x240): upstream's bottom UI strip (message box and command window, logical y 132-180) at the bottom, at 1:1. The 320 px width matches exactly. The space above it is free (debug text for now).
Because the message box moves off the top screen, the whole field is visible, including the lower part of the player's Pokemon that upstream hides behind the box.

## Controls
D-pad and A drive the menu. FIGHT removes 20-40% of the enemy HP; a KO advances the wave and autosaves. RUN saves. X toggles a 41-sprite stress test, SELECT hides the debug text, START saves and quits.

## Verified in the emulator
- Boots in Old 3DS mode, 60 fps shown, loads 14 textures (2.30 MB of texture memory) in about 1.0-1.1 s from the emulated SD card.
- Battle scene renders on both screens with upstream art and the upstream font (screenshots 01-03).
- Wave-end autosave and save-on-quit write `session_0.sav`, and the next boot loads it.
- Corrupting the main save makes the next boot load `.bak`, and the next write drops the corrupt file instead of overwriting the good backup.
- With an empty SD card the app still runs, drawing placeholder boxes and the system font and reporting the missing file (screenshot 05).

## Not verified / known gaps
- **Real-console speed.** Azahar doesn't model GPU timing (it always reports 0.50 ms), and its CPU timing is optimistic. The CPU ms shown (about 1.8 ms for the scene, 2.1 ms with 41 extra sprites) suggests plenty of headroom. I don't know the real numbers.
- Layout is upstream's positions for the arena, bases, Pokemon, info boxes, message text and command menu. The level label, HP numbers and prompt arrow are approximations: upstream draws them with digit and label sprites that aren't ported yet.
- The system-font fallback text is not laid out properly (screenshot 05); it only has to be readable.
- The glyph cache is a fixed 512x256 A8 texture with no eviction, which is fine for Latin text but not for CJK.
- There is no audio.

## Memory numbers that matter for the route decision
Measured from all 1,451 upstream front and back atlas JSONs, padded to power-of-two RGBA5551 textures: median 0.12 MB per sheet, p90 0.5 MB, max 2 MB (front) and 8 MB (one back sheet).
The 3DS GPU limit is 1024 px per side, and `pokemon/back/642-therian` is 1175x1175, so it fails to load as-is. The asset pipeline must split or repack sheets like that.
A typical double battle (4 Pokemon, arena, UI, 2 font caches) should land around 3-5 MB of textures, which fits the ~14 MB left beside the QuickJS heap. That is an estimate; only the single-battle scene was measured (2.30 MB).
The font file is 5.2 MB, so it is streamed from the SD card by FreeType rather than held in RAM. The asset pipeline should pre-bake it into a bitmap font.

## Rebuild
```
docker run --rm -v "$PWD/src":/src -w /src devkitpro/devkitarm make
```
Libraries used: citro2d, citro3d, libctru, and portlibs FreeType, libpng, zlib, bzip2 and jansson, all in the devkitarm image.

Emulator test (Linux host):
```
gcc -O2 -o lr-run src/tools/lr-run.c -I<azahar>/externals/libretro-common/libretro-common/include -ldl -lpng
python3 src/tools/fetch-assets.py data/Azahar/sdmc      # emulated SD card = data/Azahar/sdmc
./lr-run ../tools/azahar-libretro/azahar_libretro.so pokerogue-skeleton.3dsx $PWD/data \
    frames=300 press=200:a shot=299:out.png
```
Azahar core build flags: `-DENABLE_LIBRETRO=ON -DENABLE_QT=OFF -DENABLE_SDL2=OFF -DENABLE_OPENGL=OFF -DENABLE_VULKAN=OFF` (plus tests, room, web service, cubeb, OpenAL, libusb and scripting off). The app takes about 100-150 emulated frames to boot.

## On a real 3DS (optional, nothing depends on it yet)
1. Run `python3 src/tools/fetch-assets.py` on a PC and copy the resulting `sd-card/` contents to the SD card root.
2. Copy `pokerogue-skeleton.3dsx` to `sd:/3ds/`.
3. Start it from the Homebrew Launcher. Personal build: don't share the SD assets.
