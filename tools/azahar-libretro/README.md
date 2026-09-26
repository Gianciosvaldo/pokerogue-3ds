# Azahar libretro test rig

Headless emulator used to test builds without a console. The binaries are not in git (they are large and prebuilt).
Put them here locally:

- `azahar_libretro.so`: Azahar (azahar-emu/azahar `ce0486d`) built as a libretro core, software renderer only:
  `-DENABLE_LIBRETRO=ON -DENABLE_QT=OFF -DENABLE_SDL2=OFF -DENABLE_OPENGL=OFF -DENABLE_VULKAN=OFF`
  (plus tests, room, web service, cubeb, OpenAL, libusb and scripting off). The build takes about 15 minutes.
- `lr-run`: the headless frontend, built from `assets-pipeline/app-src/tools/lr-run.c` (newer than the copy in `skeleton/src/tools/`):
  `gcc -O2 -o lr-run assets-pipeline/app-src/tools/lr-run.c -I<azahar>/externals/libretro-common/libretro-common/include -ldl -lpng`

Usage and the emulated SD card layout are in `skeleton/README.md`.
