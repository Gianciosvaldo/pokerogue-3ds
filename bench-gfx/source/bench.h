/* bench-gfx: shared helpers. See ../README.md for what each section measures. */
#ifndef BENCH_H
#define BENCH_H

#include <3ds.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "pak.h"
#include "rr.h"
#include "snd.h"

#ifndef BENCH_REV
#define BENCH_REV "unknown"
#endif

#define RESULT_PATH "sdmc:/3ds/pokerogue-gfx-bench-result.txt"
#define LOG_PATH "sdmc:/3ds/pokerogue-gfx-bench-log.txt"
/* Emulator runs only (tools/run-azahar.sh): when HEADLESS_FLAG exists, the final question is
 * skipped and DONE_PATH is written at the end so the runner can stop. */
#define HEADLESS_FLAG "sdmc:/3ds/pokerogue-gfx-bench-headless"
#define DONE_PATH "sdmc:/3ds/pokerogue-gfx-bench-done"
extern bool g_headless;

/* upstream ui/fieldUI containers sit at the canvas bottom (y = 180), like app-src main.c */
#define UI_Y(y) (180.0f + (y))

static inline u64 now_ticks(void) { return svcGetSystemTick(); }
static inline double ticks_ms(u64 t) { return (double)t / (SYSCLOCK_ARM11 / 1000.0); }
static inline double ms_since(u64 t0) { return ticks_ms(now_ticks() - t0); }

/* ---- report.c ---- */
void report_open(void);
void report_close(void);
/* A line of the result file (also logged and shown on the bottom screen). */
void res(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
/* The log file only. */
void logf_(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
/* Rewrites the result file with everything so far (called after every section). */
void res_save(void);
/* One frame showing the current step; returns false when START was pressed or the app must quit. */
bool status(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
bool keep_running(void);
extern bool g_stop;

typedef struct {
  double min, med, p95, max, mean;
  int n;
} dist;
dist dist_of(const float* v, int n);

typedef struct {
  u32 linear_free, vram_free;
  size_t heap_used, tex_bytes;
  int tex_count;
} mem_snap;
mem_snap mem_now(void);
static inline double mb(double bytes) { return bytes / 1048576.0; }

/* ---- what is on the SD card (gfxbench.c) ---- */
enum { P_GFX, P_SND, P_BGM, P_FONT, P_COUNT };
extern bool g_pak[P_COUNT];
extern bool g_sound; /* ndsp started (needs sdmc:/3ds/dspfirm.cdc on a console) */

/* ---- sections ---- */
void sd_tests(void);       /* loads.c: raw SD speed and save writes */
void load_tests(void);     /* loads.c: sprites, cries, music, fonts from the packs */
void audio_cpu_test(void); /* loads.c: CPU the audio takes */
void render_tests(void);   /* scene.c: frame times */
void memory_tests(void);   /* loads.c: upstream-style preload and filling the heap */
void edge_readback(void);  /* edge.c: where 1 px lines at the screen edges land in the framebuffer */
void edge_ask(void);       /* edge.c: asks what the screen shows, adds the answer to the results */

/* ---- scene.c helpers shared with loads.c ---- */
/* Synthetic atlas of the given page and frame size (made-up pixels), through the same RRT loader. */
rr_atlas* synth_atlas(int page_w, int page_h, int fw, int fh, int frames, unsigned seed);

#endif
