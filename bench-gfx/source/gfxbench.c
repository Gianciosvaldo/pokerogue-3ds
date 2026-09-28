/* bench-gfx: what a PokeRogue battle costs the console in frame time, memory, SD card loading
 * and audio, and where the screen edges land. Writes sdmc:/3ds/pokerogue-gfx-bench-result.txt.
 * Uses app-src's renderer, pack reader and sound code unchanged. See ../README.md. */
#include <malloc.h>
#include <string.h>

#include "bench.h"

u32 __stacksize__ = 256 * 1024;

bool g_pak[P_COUNT];
bool g_sound;
bool g_headless;

extern u32 __ctru_heap_size, __ctru_linear_heap_size;

static void done_screen(void) {
  g_stop = false;
  while (status("Done. Results: sd:/3ds/pokerogue-gfx-bench-result.txt\nPress START to exit.")) {
  }
}

int main(void) {
  osSetSpeedupEnable(false); /* a New 3DS stays at the Old 3DS clock */
  u64 t_start = now_ticks();
  if (!rr_init()) return 1;
  report_open();
  FILE* hf = fopen(HEADLESS_FLAG, "r");
  g_headless = hf != NULL;
  if (hf) fclose(hf);
  remove(DONE_PATH);
  bool n3ds = false;
  APT_CheckNew3DS(&n3ds);
  res("PokeRogue graphics + memory bench");
  res("build: %s, machine: %s, ARM11 at 268 MHz (the Old 3DS clock)", BENCH_REV, n3ds ? "New 3DS" : "Old 3DS");
  mem_snap m = mem_now();
  res("memory at start: app heap %.1f MB (%.1f MB in use), linear heap %.1f MB (%.1f MB free),",
      mb(__ctru_heap_size), mb(m.heap_used), mb(__ctru_linear_heap_size), mb(m.linear_free));
  res("  VRAM %.2f MB free of 6 (the two screen render targets use the rest)", mb(m.vram_free));
  res("  textures, sound buffers and the GPU command and vertex buffers all come out of the linear heap");
  status("starting");

  edge_readback();

  static const char* NAMES[P_COUNT] = {"gfx", "snd", "bgm", "font"};
  char line[200];
  int len = snprintf(line, sizeof line, "packs in sd:/3ds/pokerogue/:");
  for (int i = 0; i < P_COUNT; i++) {
    char path[64];
    snprintf(path, sizeof path, PAK_DIR "%s.pak", NAMES[i]);
    FILE* f = fopen(path, "rb");
    g_pak[i] = f != NULL;
    if (f) {
      fseek(f, 0, SEEK_END);
      len += snprintf(line + len, sizeof line - len, " %s.pak %.0f MB,", NAMES[i], mb(ftell(f)));
      fclose(f);
    } else {
      len += snprintf(line + len, sizeof line - len, " %s.pak MISSING,", NAMES[i]);
    }
  }
  line[len - 1] = 0;
  res("\n%s (data.pak is not used)", line);
  pak_mount_default();
  g_sound = snd_init();
  res("sound: %s", g_sound ? "on" : "OFF (ndsp did not start: sdmc:/3ds/dspfirm.cdc missing?); audio tests skipped");
  mem_snap m2 = mem_now();
  res("after opening the packs and sound: app heap %.1f MB in use (pack indexes), linear heap %.1f MB free",
      mb(m2.heap_used), mb(m2.linear_free));
  res_save();

  if (!g_stop) sd_tests();
  if (!g_stop) load_tests();
  if (!g_stop) render_tests();
  if (!g_stop) audio_cpu_test();
  if (!g_stop) memory_tests();
  res("\n%s after %.0f s", g_stop ? "STOPPED EARLY" : "finished", ms_since(t_start) / 1000);
  res_save();

  g_stop = false;
  edge_ask();
  if (!g_headless) done_screen();

  report_close();
  snd_exit();
  pak_unmount_all();
  rr_exit();
  return 0;
}
