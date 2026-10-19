# Pack probe

A tiny 3DS app for trying the single-file builds in the Azahar emulator (or on a console) without the real packs.
It mounts the packs with `pak_mount_default()`, the game's own call, and checks them against a manifest.

```
python3 sd-builder/tests/synth_packs.py PACKS_DIR [SCALE]      # five synthetic packs + probe-manifest.txt
docker run --rm -v "$PWD":/src -w /src/sd-builder/tests/probe devkitpro/devkitarm make
```
`probe.3dsx` and `probe.elf` come out here. Bundle them like the game (`sd-builder/bundle.py`), put
`probe-manifest.txt` in the emulated SD card's `3ds/` folder, run, and read `3ds/probe-result.txt` (the last line is
`RESULT PASS` or `RESULT FAIL`). CI only checks that it builds; running it needs the emulator core (`tools/azahar-libretro/`).
