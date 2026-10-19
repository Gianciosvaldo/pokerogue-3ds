/*
 * Pack probe: mounts the packs the way the game does (pak_mount_default: the app's RomFS first, then the SD card)
 * and checks them against sdmc:/3ds/probe-manifest.txt (made by tests/synth_packs.py). Writes
 * sdmc:/3ds/probe-result.txt, then sdmc:/3ds/probe-done, and shows the same on screen.
 *
 *   pack NAME.pak SIZE                   the pack's file size, read through pak_fopen (the way the music streams)
 *   entry KEY SIZE FNV1A64-HEX           an entry read with pak_read_entry matches
 *   3dsx PATH 0|1                        pak_3dsx_has_romfs(PATH) gives that answer (the guard a console launch uses)
 */
#include <3ds.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pak.h"

static FILE* g_out;

static void say(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
static void say(const char* fmt, ...) {
  char line[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(line, sizeof line, fmt, ap);
  va_end(ap);
  puts(line);
  fprintf(g_out, "%s\n", line);
  fflush(g_out);
}

static uint64_t fnv1a64(const unsigned char* p, size_t n) {
  uint64_t h = 0xCBF29CE484222325ull;
  while (n--) h = (h ^ *p++) * 0x100000001B3ull;
  return h;
}

int main(int argc, char** argv) {
  gfxInitDefault();
  consoleInit(GFX_TOP, NULL);
  g_out = fopen("sdmc:/3ds/probe-result.txt", "w");
  if (!g_out) return 1;
  int mounted = pak_mount_default();
  say("homebrew=%d argc=%d argv0=%s", envIsHomebrew(), argc, argc > 0 ? argv[0] : "-");
  say("mounted=%d", mounted);
  int ok = 0, bad = 0;
  FILE* m = fopen("sdmc:/3ds/probe-manifest.txt", "r");
  char line[512];
  while (m && fgets(line, sizeof line, m)) {
    char kind[16], key[256];
    unsigned long long size = 0, want = 0;
    if (sscanf(line, "%15s %255s %llu", kind, key, &size) < 3) continue;
    pak_entry e;
    if (!strcmp(kind, "pack")) {
      /* a pack is found through any of its entries; the manifest's probe/NAME/head0 lives in NAME.pak */
      char first[256];
      snprintf(first, sizeof first, "probe/%.*s/head0", (int)(strchr(key, '.') - key), key);
      if (!pak_find(NULL, first, &e)) {
        say("FAIL %s: not mounted", key);
        bad++;
        continue;
      }
      FILE* f = pak_fopen(e.pak);
      long end = f && fseek(f, 0, SEEK_END) == 0 ? ftell(f) : -1;
      if (f) fclose(f);
      if (end == (long)size) {
        say("ok   %s %ld bytes from %s", key, end, pak_path(e.pak));
        ok++;
      } else {
        say("FAIL %s: %ld bytes, expected %llu", key, end, size);
        bad++;
      }
    } else if (!strcmp(kind, "3dsx")) {
      bool has = pak_3dsx_has_romfs(key);
      if (has == (size != 0)) {
        say("ok   %s %s", key, has ? "has a RomFS" : "has no RomFS");
        ok++;
      } else {
        say("FAIL %s: pak_3dsx_has_romfs says %d", key, has);
        bad++;
      }
    } else if (!strcmp(kind, "entry")) {
      char* sp = strrchr(line, ' ');
      want = sp ? strtoull(sp + 1, NULL, 16) : 0;
      size_t len = 0;
      void* d = pak_find(NULL, key, &e) ? pak_read_entry(&e, &len) : NULL;
      if (d && len == size && fnv1a64(d, len) == want) {
        ok++;
      } else {
        say("FAIL entry %s (%s, %zu bytes)", key, d ? "wrong data" : "not read", len);
        bad++;
      }
      free(d);
    }
  }
  if (m) fclose(m);
  say("checked ok=%d fail=%d", ok, bad);
  say("RESULT %s", bad == 0 && ok > 0 && mounted == 5 ? "PASS" : "FAIL");
  fclose(g_out);
  FILE* d = fopen("sdmc:/3ds/probe-done", "w");
  if (d) fclose(d);
  while (aptMainLoop()) {
    hidScanInput();
    if (hidKeysDown() & KEY_START) break;
    gspWaitForVBlank();
    gfxSwapBuffers();
  }
  gfxExit();
  return 0;
}
