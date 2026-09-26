/*
 * Asset-pack test mode. Runs instead of the demo when
 * sdmc:/3ds/pokerogue/paktest.txt exists; one command per line:
 *
 *   loadall PAK                 load every texture in PAK.pak (e.g. gfx) and time it
 *   show KEY FRAME X Y [MAP V]  draw a frame on the top screen at physical X,Y (1:1).
 *                               FRAME: a frame name, #N for index N, "*" for all frames,
 *                               "-" for the "" frame. MAP/V: variant colour map key + variant.
 *   clear R G B                 top-screen clear colour for "show" (default 32 32 32)
 *   bgm KEY                     stream a BGM entry (see snd.h)
 *   sfx KEY                     play a sound effect once
 *   sfxat FRAME KEY             play a sound effect at display frame FRAME
 *   rect X Y W H                white rectangle on the top screen (renderer alignment checks)
 *   frames N                    how many frames to display before finishing (default 120)
 *
 * Keys and frame names cannot contain spaces here: write '|' for a space.
 *
 * Results go to sdmc:/3ds/pokerogue/paktest-log.txt; sdmc:/3ds/pokerogue/paktest-done is
 * written at the end so a headless runner can stop; paktest-shown is written once
 * the "show" items have been on screen for 10 frames (take screenshots after it).
 */
#include <3ds.h>
#include <lz4.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pak.h"
#include "rr.h"
#include "snd.h"

#define DIR "sdmc:/3ds/pokerogue/"
#define MAX_SHOW 32
#define MAX_SFX 16

static FILE* g_log;
static double ms(u64 ticks) { return ticks / (SYSCLOCK_ARM11 / 1000.0); }

static void logf_(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
static void logf_(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  if (g_log) {
    vfprintf(g_log, fmt, ap);
    fflush(g_log);
  }
  va_end(ap);
}

static void loadall(const char* name) {
  char path[128];
  snprintf(path, sizeof path, DIR "%s.pak", name);
  rr_pak* p = pak_open(path);
  if (!p) {
    logf_("loadall %s: cannot open\n", path);
    return;
  }
  size_t nlen;
  char* names = pak_load_names(p, &nlen);
  FILE* f = pak_fopen(p);
  int n = pak_count(p), tex = 0, fail = 0;
  u64 t_read = 0, t_lz4 = 0, t_build = 0, worst = 0;
  const char* worst_name = "";
  size_t bytes_stored = 0, bytes_raw = 0, max_tex = 0;
  const char* nm = names;
  u64 start = svcGetSystemTick();
  for (int i = 0; i < n; i++, nm += strlen(nm) + 1) {
    pak_entry e;
    pak_entry_at(p, i, &e);
    if (e.type != PAK_TEX) continue;
    tex++;
    u64 t0 = svcGetSystemTick();
    char* st = malloc(e.stored);
    char* raw = e.codec ? malloc(e.size) : st;
    bool ok = st && raw && !fseek(f, e.offset, SEEK_SET) && fread(st, 1, e.stored, f) == e.stored;
    u64 t1 = svcGetSystemTick();
    if (ok && e.codec) ok = LZ4_decompress_safe(st, raw, e.stored, e.size) == (int)e.size;
    u64 t2 = svcGetSystemTick();
    rr_atlas* a = ok ? rr_atlas_load_rrt(raw, e.size, NULL, 0) : NULL;
    u64 t3 = svcGetSystemTick();
    if (!a) {
      fail++;
      logf_("FAIL %s (stored %lu size %lu)\n", nm, (unsigned long)e.stored, (unsigned long)e.size);
    } else {
      size_t tb = 0;
      for (int k = 0; k < rr_atlas_pages(a); k++) tb += rr_tex_bytes(rr_atlas_page(a, k));
      if (tb > max_tex) max_tex = tb;
    }
    rr_atlas_free(a);
    if (e.codec) free(raw);
    free(st);
    t_read += t1 - t0;
    t_lz4 += t2 - t1;
    t_build += t3 - t2;
    if (t3 - t0 > worst) {
      worst = t3 - t0;
      worst_name = nm;
    }
    bytes_stored += e.stored;
    bytes_raw += e.size;
    if (tex % 1000 == 0) logf_("  ... %d textures, %.1f s\n", tex, ms(svcGetSystemTick() - start) / 1000);
  }
  logf_("loadall %s: %d entries, %d textures, %d failed\n", name, n, tex, fail);
  logf_("  stored %.1f MB, unpacked %.1f MB, largest texture set %.2f MB\n", bytes_stored / 1e6,
        bytes_raw / 1e6, max_tex / 1e6);
  logf_("  time: read %.1f s, lz4 %.1f s, build textures %.1f s (total %.1f s, avg %.2f ms)\n",
        ms(t_read) / 1000, ms(t_lz4) / 1000, ms(t_build) / 1000, ms(t_read + t_lz4 + t_build) / 1000,
        ms(t_read + t_lz4 + t_build) / (tex ? tex : 1));
  logf_("  slowest: %s %.1f ms\n", worst_name, ms(worst));
  logf_("  linear heap free after: %.2f MB\n", linearSpaceFree() / 1e6);
  fclose(f);
  free(names);
  pak_close(p);
}

static void unescape(char* s) {
  for (; *s; s++)
    if (*s == '|') *s = ' ';
}

typedef struct {
  float x, y, w, h;
} rect_item;

typedef struct {
  rr_atlas* a;
  int frame; /* -1 = all */
  float x, y;
} show_item;

int paktest_run(void) {
  FILE* cmd = fopen(DIR "paktest.txt", "r");
  if (!cmd) return 0;
  remove(DIR "paktest-done");
  remove(DIR "paktest-shown");
  g_log = fopen(DIR "paktest-log.txt", "w");
  logf_("paktest start, linear free %.2f MB\n", linearSpaceFree() / 1e6);
  int mounted = pak_mount_default();
  logf_("mounted %d paks\n", mounted);
  snd_init();

  show_item items[MAX_SHOW];
  int nitems = 0, frames = 120, nrects = 0;
  rect_item rects[8];
  struct { int at; char key[128]; } sfx[MAX_SFX];
  int nsfx = 0;
  rr_color clear = RR_RGBA(32, 32, 32, 255);
  char line[512];
  while (fgets(line, sizeof line, cmd)) {
    char a[160] = {0}, b[160] = {0}, m[160] = {0};
    float x = 0, y = 0;
    int v = 0, r, g, bl;
    line[strcspn(line, "\r\n")] = 0;
    char* args = line;
    (void)args;
    if (!line[0] || line[0] == '#') continue;
    if (sscanf(line, "loadall %159s", a) == 1) {
      loadall(a);
    } else if (sscanf(line, "frames %d", &frames) == 1) {
    } else if (nrects < 8 && sscanf(line, "rect %f %f %f %f", &rects[nrects].x, &rects[nrects].y, &rects[nrects].w,
                                     &rects[nrects].h) == 4) {
      nrects++;
    } else if (sscanf(line, "clear %d %d %d", &r, &g, &bl) == 3) {
      clear = RR_RGBA(r, g, bl, 255);
    } else if (sscanf(line, "bgm %159s", a) == 1) {
      unescape(a);
      u64 t0 = svcGetSystemTick();
      bool ok = snd_bgm_play(a, true);
      logf_("bgm %s: %s (%.1f ms to start)\n", a, ok ? "playing" : "FAILED", ms(svcGetSystemTick() - t0));
    } else if (sscanf(line, "sfxat %d %159s", &v, a) == 2 && nsfx < MAX_SFX) {
      unescape(a);
      sfx[nsfx].at = v;
      snprintf(sfx[nsfx++].key, sizeof sfx[0].key, "%s", a);
    } else if (sscanf(line, "sfx %159s", a) == 1) {
      unescape(a);
      u64 t0 = svcGetSystemTick();
      bool ok = snd_sfx_play(a);
      logf_("sfx %s: %s (%.1f ms)\n", a, ok ? "ok" : "FAILED", ms(svcGetSystemTick() - t0));
    } else if (sscanf(line, "show %159s %159s %f %f %159s %d", a, b, &x, &y, m, &v) >= 4 && nitems < MAX_SHOW) {
      unescape(a);
      unescape(b);
      unescape(m);
      u64 t0 = svcGetSystemTick();
      rr_atlas* at = m[0] ? rr_atlas_load_key_variant(a, m, v) : rr_atlas_load_key(a);
      double t = ms(svcGetSystemTick() - t0);
      int fi = -2;
      if (at) {
        if (!strcmp(b, "*"))
          fi = -1;
        else if (b[0] == '#')
          fi = atoi(b + 1);
        else {
          const rr_frame* f = rr_atlas_find(at, !strcmp(b, "-") ? "" : b);
          for (int k = 0; f && k < rr_atlas_count(at); k++)
            if (rr_atlas_frame(at, k) == f) fi = k;
        }
      }
      logf_("show %s %s: %s, %d frames, %d pages, %.1f ms\n", a, b, at ? "ok" : "FAILED", rr_atlas_count(at),
            rr_atlas_pages(at), t);
      if (at && fi >= -1) items[nitems++] = (show_item){at, fi, x, y};
    }
  }
  fclose(cmd);

  rr_set_origin(RR_TOP, 0, 0);
  for (int fr = 0; fr < frames && aptMainLoop(); fr++) {
    hidScanInput();
    for (int k = 0; k < nsfx; k++)
      if (sfx[k].at == fr) logf_("sfx %s at frame %d: %s\n", sfx[k].key, fr, snd_sfx_play(sfx[k].key) ? "ok" : "FAILED");
    snd_update();
    rr_frame_begin();
    rr_target(RR_TOP, clear);
    for (int i = 0; i < nitems; i++) {
      if (items[i].frame < 0)
        rr_draw_all(items[i].a, items[i].x, items[i].y, NULL);
      else
        rr_draw(rr_atlas_frame(items[i].a, items[i].frame), items[i].x, items[i].y, NULL);
    }
    for (int i = 0; i < nrects; i++) rr_rect(rects[i].x, rects[i].y, rects[i].w, rects[i].h, RR_RGBA(255, 255, 255, 255));
    rr_target(RR_BOTTOM, RR_RGBA(0, 0, 0, 255));
    rr_frame_end();
    if (fr == 10) {
      FILE* m = fopen(DIR "paktest-shown", "w");
      if (m) fclose(m);
    }
  }
  rr_stats s = rr_get_stats();
  logf_("display done: %d textures, %.2f MB, linear free %.2f MB\n", s.tex_count, s.tex_bytes / 1e6,
        s.linear_free / 1e6);
  snd_exit();
  for (int i = 0; i < nitems; i++) rr_atlas_free(items[i].a);
  pak_unmount_all();
  if (g_log) fclose(g_log);
  FILE* d = fopen(DIR "paktest-done", "w");
  if (d) fclose(d);
  return 1;
}
