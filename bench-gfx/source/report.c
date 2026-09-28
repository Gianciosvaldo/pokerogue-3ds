/* Result file, log, progress screen, statistics and memory snapshots. */
#include <malloc.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "bench.h"

static char g_res[64 * 1024];
static size_t g_res_len;
static FILE* g_log;
bool g_stop;

#define SCREEN_LINES 13
static char g_lines[SCREEN_LINES][72];
static int g_nlines;

void report_open(void) {
  mkdir("sdmc:/3ds", 0777);
  g_log = fopen(LOG_PATH, "w");
}

void report_close(void) {
  if (g_log) fclose(g_log);
  g_log = NULL;
}

void logf_(const char* fmt, ...) {
  if (!g_log) return;
  va_list ap;
  va_start(ap, fmt);
  vfprintf(g_log, fmt, ap);
  va_end(ap);
  fflush(g_log);
}

static void screen_line(const char* s, size_t n) {
  if (g_nlines == SCREEN_LINES) {
    memmove(g_lines[0], g_lines[1], sizeof g_lines[0] * (SCREEN_LINES - 1));
    g_nlines--;
  }
  if (n >= sizeof g_lines[0]) n = sizeof g_lines[0] - 1;
  memcpy(g_lines[g_nlines], s, n);
  g_lines[g_nlines][n] = 0;
  g_nlines++;
}

void res(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  char* at = g_res + g_res_len;
  size_t room = sizeof g_res - g_res_len;
  int n = vsnprintf(at, room, fmt, ap);
  va_end(ap);
  if (n < 0) return;
  if ((size_t)n >= room) n = (int)room - 1;
  g_res_len += n;
  if (g_res_len + 1 < sizeof g_res) g_res[g_res_len++] = '\n';
  g_res[g_res_len] = 0;
  logf_("%.*s\n", n, at);
  for (const char *p = at, *e = at + n; p < e;) {
    const char* nl = memchr(p, '\n', e - p);
    size_t len = (nl ? nl : e) - p;
    screen_line(p, len);
    p += len + 1;
  }
}

void res_save(void) {
  FILE* f = fopen(RESULT_PATH, "w");
  if (!f) return;
  fwrite(g_res, 1, g_res_len, f);
  fclose(f);
}

bool keep_running(void) {
  if (!aptMainLoop()) {
    g_stop = true;
    return false;
  }
  hidScanInput();
  if (hidKeysDown() & KEY_START) g_stop = true;
  return !g_stop;
}

bool status(const char* fmt, ...) {
  char step[160];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(step, sizeof step, fmt, ap);
  va_end(ap);
  static char last[160];
  if (strcmp(step, last)) { /* the done screen redraws the same step every frame */
    logf_("-- %s\n", step);
    snprintf(last, sizeof last, "%s", step);
  }
  rr_set_origin(RR_TOP, 0, 0);
  rr_set_origin(RR_BOTTOM, 0, 0);
  rr_frame_begin();
  rr_target(RR_TOP, RR_RGBA(16, 16, 28, 255));
  rr_text_sys(10, 10, 0.6f, "PokeRogue graphics + memory bench", RR_RGBA(240, 240, 240, 255));
  rr_text_sys(10, 40, 0.5f, step, RR_RGBA(255, 220, 120, 255));
  rr_text_sys(10, 200, 0.45f, "About 1.5 minutes without the packs, longer with them.\nSTART stops early (the results so far are kept).",
              RR_RGBA(160, 160, 180, 255));
  rr_target(RR_BOTTOM, RR_RGBA(8, 8, 16, 255));
  for (int i = 0; i < g_nlines; i++) rr_text_sys(4, 4 + i * 18, 0.42f, g_lines[i], RR_RGBA(210, 210, 210, 255));
  rr_frame_end();
  return keep_running();
}

static int cmp_float(const void* a, const void* b) {
  float x = *(const float*)a, y = *(const float*)b;
  return (x > y) - (x < y);
}

dist dist_of(const float* v, int n) {
  dist d = {0};
  d.n = n;
  if (n <= 0) return d;
  float* s = malloc(n * sizeof *s);
  if (!s) return d;
  memcpy(s, v, n * sizeof *s);
  qsort(s, n, sizeof *s, cmp_float);
  double sum = 0;
  for (int i = 0; i < n; i++) sum += s[i];
  d.min = s[0];
  d.med = n % 2 ? s[n / 2] : (s[n / 2 - 1] + s[n / 2]) / 2.0;
  d.p95 = s[(int)((n - 1) * 0.95 + 0.5)];
  d.max = s[n - 1];
  d.mean = sum / n;
  free(s);
  return d;
}

mem_snap mem_now(void) {
  mem_snap m;
  rr_stats s = rr_get_stats();
  struct mallinfo mi = mallinfo();
  m.linear_free = linearSpaceFree();
  m.vram_free = vramSpaceFree();
  m.heap_used = mi.uordblks;
  m.tex_bytes = s.tex_bytes;
  m.tex_count = s.tex_count;
  return m;
}
