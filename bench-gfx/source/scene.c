/* Rendering: frame times of battle scenes laid out like upstream's, plus ramps that find where
 * sprites, text and full-screen layers stop fitting in a frame. */
#include <citro2d.h>
#include <citro3d.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "bench.h"

/* ------------------------------------------------------------------ sprite sources */

/* Each source is a gfx.pak entry. Without gfx.pak (or when an entry is missing) a stand-in of the
 * same page size, frame size and frame count is built instead, so draw and GPU costs match; only
 * the pixels are made up. Sizes are from gfx.pak built from upstream assets 056a1f4. */
typedef struct {
  const char* key;
  u16 pw, ph, fw, fh, frames;
} spec;

enum {
  S_BG, S_A, S_B,
  S_EN0, S_PL0, S_EN1, S_PL1,
  S_BIG_EN0, S_BIG_EN1, S_BIG_PL0, S_BIG_PL1,
  S_INFO_PL, S_INFO_PL_MINI, S_INFO_EN, S_HP, S_EXP, S_TYPE_PL, S_TYPE_EN, S_STATUS,
  S_TRAY_BALL, S_TRAY_PL, S_TRAY_EN,
  S_MSG_BG, S_WINDOW, S_CURSOR, S_TYPES,
  S_ANIM,
  S_COUNT
};

static const spec SPECS[S_COUNT] = {
    [S_BG] = {"images/arenas/grass_bg", 512, 256, 320, 180, 1},
    [S_A] = {"images/arenas/grass_a", 512, 256, 320, 132, 1},
    [S_B] = {"images/arenas/grass_b", 512, 256, 320, 132, 1},
    [S_EN0] = {"images/pokemon/445", 512, 512, 82, 80, 160},          /* Garchomp */
    [S_PL0] = {"images/pokemon/back/257", 512, 512, 74, 72, 124},     /* Blaziken */
    [S_EN1] = {"images/pokemon/149", 512, 512, 80, 82, 126},          /* Dragonite */
    [S_PL1] = {"images/pokemon/back/6", 512, 1024, 98, 83, 144},      /* Charizard */
    [S_BIG_EN0] = {"images/pokemon/646-white", 1024, 1024, 104, 95, 360},
    [S_BIG_EN1] = {"images/pokemon/642-therian", 1024, 1024, 129, 98, 193},
    [S_BIG_PL0] = {"images/pokemon/back/642-therian", 1024, 1024, 135, 108, 193},
    [S_BIG_PL1] = {"images/pokemon/back/569", 1024, 1024, 102, 83, 136},
    [S_INFO_PL] = {"images/ui/pbinfo_player", 256, 64, 130, 42, 1},
    [S_INFO_PL_MINI] = {"images/ui/pbinfo_player_mini", 256, 64, 130, 42, 1},
    [S_INFO_EN] = {"images/ui/pbinfo_enemy_mini", 256, 32, 130, 31, 1},
    [S_HP] = {"images/ui/overlay_hp", 64, 8, 48, 2, 3},
    [S_EXP] = {"images/ui/overlay_exp", 128, 8, 85, 2, 1},
    [S_TYPE_PL] = {"images/ui/pbinfo_player_type1", 64, 128, 20, 12, 20},
    [S_TYPE_EN] = {"images/ui/pbinfo_enemy_type1", 64, 128, 20, 12, 20},
    [S_STATUS] = {"images/statuses", 64, 32, 22, 8, 8},
    [S_TRAY_BALL] = {"images/ui/pb_tray_ball", 16, 16, 7, 7, 4},
    [S_TRAY_PL] = {"images/ui/pb_tray_overlay_player", 128, 8, 104, 4, 1},
    [S_TRAY_EN] = {"images/ui/pb_tray_overlay_enemy", 128, 8, 104, 4, 1},
    [S_MSG_BG] = {"images/ui/bg", 512, 256, 320, 48, 5},
    [S_WINDOW] = {"images/ui/windows/window_1", 32, 32, 24, 24, 1},
    [S_CURSOR] = {"images/ui/cursor", 8, 16, 6, 10, 1},
    [S_TYPES] = {"images/types", 128, 128, 32, 14, 20},
    [S_ANIM] = {"images/battle_anims/PRAS- Fire", 256, 512, 96, 96, 75}, /* Flamethrower's sheet */
};

typedef struct {
  rr_atlas* a;
  int n;
  bool real;
} src;

static src g_src[S_COUNT];
static rr_atlas* g_shadow;  /* stand-in: upstream draws the shadow inside its sprite shader */
static rr_atlas* g_anim_bg; /* stand-in: gfx.pak cuts anim backgrounds into 96 px cells (see README) */
static rr_atlas* g_glyphs;  /* stand-in glyphs when font.pak is missing */
static rr_font *g_f16, *g_f12;
static int g_quads; /* quads queued this frame (sprites + glyphs, counted here) */

static inline u32 morton8(u32 x, u32 y) {
  return (x & 1) | ((y & 1) << 1) | ((x & 2) << 1) | ((y & 2) << 2) | ((x & 4) << 2) | ((y & 4) << 3);
}

static void put16(u8* p, u16 v) { memcpy(p, &v, 2); }
static void put32(u8* p, u32 v) { memcpy(p, &v, 4); }

rr_atlas* synth_atlas(int pw, int ph, int fw, int fh, int frames, unsigned seed) {
  if (fw > pw) fw = pw;
  if (fh > ph) fh = ph;
  int cols = pw / fw, rows = ph / fh, cells = cols * rows;
  enum { NPAL = 9 };
  size_t head = 20 + NPAL * 4 + 4 + (size_t)frames * 20 + 1;
  size_t tex_off = (head + 3) & ~(size_t)3;
  size_t len = tex_off + (size_t)pw * ph;
  u8* b = calloc(1, len);
  if (!b) return NULL;
  memcpy(b, "RRT1", 4); /* kind 0 (palette indices), gpu 0 (RGBA5551): what Pokemon sheets use */
  put16(b + 6, 1);
  put16(b + 8, frames);
  put16(b + 10, NPAL);
  put16(b + 12, pw);
  put16(b + 14, ph);
  put32(b + 16, 1);
  static const u32 pal[NPAL] = {0,          0x5078d0ff, 0x88b0f0ff, 0xe06040ff, 0xf0a060ff,
                                0x60b060ff, 0xa0e0a0ff, 0xd0d0d0ff, 0x404048ff};
  memcpy(b + 20, pal, sizeof pal);
  u8* p = b + 20 + NPAL * 4;
  put16(p, pw);
  put16(p + 2, ph);
  p += 4;
  for (int i = 0; i < frames; i++, p += 20) {
    int c = i % cells;
    put16(p + 2, (c % cols) * fw);
    put16(p + 4, (c / cols) * fh);
    put16(p + 6, fw);
    put16(p + 8, fh);
    put16(p + 14, fw);
    put16(p + 16, fh);
  }
  u8* tex = b + tex_off;
  float rx = fw / 2.0f, ry = fh / 2.0f;
  for (int y = 0; y < rows * fh; y++) {
    for (int x = 0; x < cols * fw; x++) {
      float dx = (x % fw + 0.5f - rx) / rx, dy = (y % fh + 0.5f - ry) / ry;
      u8 v = dx * dx + dy * dy <= 1.0f ? 1 + (x / 4 + y / 4 + seed) % 8 : 0;
      tex[(((y >> 3) * (pw >> 3) + (x >> 3)) << 6) + morton8(x & 7, y & 7)] = v;
    }
  }
  rr_atlas* a = rr_atlas_load_rrt(b, len, NULL, 0);
  free(b);
  return a;
}

static bool load_src(int i) {
  src* o = &g_src[i];
  if (o->a) return true;
  const spec* s = &SPECS[i];
  o->a = g_pak[P_GFX] ? rr_atlas_load_key(s->key) : NULL;
  o->real = o->a != NULL;
  if (!o->a) o->a = synth_atlas(s->pw, s->ph, s->fw, s->fh, s->frames, i);
  o->n = rr_atlas_count(o->a);
  return o->a != NULL;
}

static void free_src(int i) {
  rr_atlas_free(g_src[i].a);
  memset(&g_src[i], 0, sizeof g_src[i]);
}

static const rr_frame* frame_of(int i, int f) {
  src* s = &g_src[i];
  return s->n ? rr_atlas_frame(s->a, f % s->n) : NULL;
}

static const rr_frame* frame_named(int i, const char* name, int fallback) {
  const rr_frame* f = g_src[i].real ? rr_atlas_find(g_src[i].a, name) : NULL;
  return f ? f : frame_of(i, fallback);
}

/* ------------------------------------------------------------------ drawing */

static const rr_color TXT = RR_RGBA(0xf8, 0xf8, 0xf8, 255);
static const rr_color TXT_SHADOW = RR_RGBA(0x6b, 0x5a, 0x73, 255);

static void spr(int i, int f, float x, float y, const rr_draw_opts* o) {
  const rr_frame* fr = frame_of(i, f);
  if (!fr) return;
  rr_draw(fr, x, y, o);
  g_quads++;
}

static int text(rr_font* f, float x, float y, const char* s, rr_color c, rr_color shadow) {
  int glyphs = 0;
  for (const char* p = s; *p; p++) glyphs += *p != ' ' && *p != '\n';
  g_quads += glyphs * ((shadow & 0xFF) ? 2 : 1);
  if (f) return rr_text(f, x, y, s, c, shadow);
  /* stand-in: one quad per glyph from a made-up glyph sheet, 6 px advance */
  rr_draw_opts o = RR_DEFAULT_OPTS;
  float pen = 0, line = 0;
  for (const char* p = s; *p; p++) {
    if (*p == '\n') {
      pen = 0;
      line += 16;
      continue;
    }
    if (*p != ' ') {
      const rr_frame* g = rr_atlas_frame(g_glyphs, (unsigned char)*p % rr_atlas_count(g_glyphs));
      if (shadow & 0xFF) {
        o.tint = shadow;
        rr_draw(g, x + pen + 1, y + line + 1, &o);
      }
      o.tint = c;
      rr_draw(g, x + pen, y + line, &o);
    }
    pen += 6;
  }
  return (int)pen;
}

static void set_additive(bool on) {
  C2D_Flush(); /* blend state applies to the whole pending batch */
  if (on)
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE, GPU_SRC_ALPHA, GPU_ONE);
  else /* citro3d's default */
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA,
                   GPU_ONE_MINUS_SRC_ALPHA);
}

static void field_bg(void) {
  spr(S_BG, 0, 0, 0, NULL);
  spr(S_B, 0, 20, 0, NULL); /* enemy base, as app-src main.c places it */
  spr(S_A, 0, 0, 0, NULL);
}

/* A Pokemon: shadow, then its sprite animated at upstream's 10 fps (pokemon.ts frameRate). */
static void mon(int i, int t, float x, float y) {
  rr_draw_opts o = RR_DEFAULT_OPTS;
  o.origin_x = 0.5f;
  o.origin_y = 0.5f;
  o.tint = RR_RGBA(0, 0, 0, 110);
  rr_draw(rr_atlas_frame(g_shadow, 0), x, y, &o);
  g_quads++;
  o.origin_y = 1.0f;
  o.tint = RR_RGBA(255, 255, 255, 255);
  spr(i, (t * 10) / 60 + i * 7, x, y, &o);
}

static void hp_bar(float x, float y, float frac) {
  const rr_frame* f = frame_named(S_HP, frac > 0.5f ? "high" : frac > 0.25f ? "medium" : "low", 0);
  if (!f) return;
  rr_draw_cropped(f, x, y, (int)ceilf(48 * frac), 6, NULL);
  g_quads++;
}

static void info_enemy(float ex, float ey, const char* name, int t) {
  rr_draw_opts o = RR_DEFAULT_OPTS;
  o.origin_x = 1;
  o.origin_y = 0.5f;
  spr(S_INFO_EN, 0, ex, ey, &o);
  text(g_f12, ex - 124, ey - 11.2f, name, TXT, TXT_SHADOW);
  text(g_f12, ex - 48, ey - 11, "Lv.100", TXT, TXT_SHADOW);
  hp_bar(ex - 71, ey + 4.5f, 0.35f + 0.3f * sinf(t * 0.05f) * sinf(t * 0.05f));
  spr(S_TYPE_EN, 14, ex - 130, ey - 9, NULL);
  spr(S_TYPE_EN, 4, ex - 130, ey + 1, NULL);
  spr(S_STATUS, 2, ex - 100, ey + 2, NULL);
}

static void info_player(float px, float py, const char* name, bool mini, int t) {
  rr_draw_opts o = RR_DEFAULT_OPTS;
  o.origin_x = 1;
  o.origin_y = 0.5f;
  spr(mini ? S_INFO_PL_MINI : S_INFO_PL, 0, px, py, &o);
  text(g_f12, px - 115, py - 15.2f, name, TXT, TXT_SHADOW);
  text(g_f12, px - 39, py - 16, "Lv.100", TXT, TXT_SHADOW);
  hp_bar(px - 61, py - 1, 0.9f);
  spr(S_TYPE_PL, 9, px - 131, py - 13, NULL);
  if (!mini) {
    char hp[16];
    snprintf(hp, sizeof hp, "%d/301", 250 + (t / 4) % 50);
    text(g_f12, px - 45, py + 4, hp, TXT, TXT_SHADOW);
    const rr_frame* f = frame_of(S_EXP, 0);
    if (f) rr_draw_cropped(f, px - 98, py + 18, 20 + (t / 3) % 60, 2, NULL), g_quads++;
  }
}

static void trays(void) {
  spr(S_TRAY_EN, 0, 0, 20, NULL);
  spr(S_TRAY_PL, 0, 216, 112, NULL);
  for (int k = 0; k < 6; k++) {
    spr(S_TRAY_BALL, k % 4, 6 + k * 10, 14, NULL);
    spr(S_TRAY_BALL, k % 4, 250 + k * 10, 106, NULL);
  }
}

static const char* MOVES[4] = {"Dragon Claw", "Earthquake", "Swords Dance", "Stone Edge"};

/* Bottom screen: upstream's message strip and command window (logical y 132..180, as app-src main.c)
 * and, in the free space above it, a move list with PP and types, as a 3DS layout might use it. */
static void bottom_ui(int t) {
  rr_draw_opts o = RR_DEFAULT_OPTS;
  o.origin_y = 1;
  const rr_frame* bg = frame_named(S_MSG_BG, "1", 0);
  if (bg) rr_draw(bg, 0, UI_Y(0), &o), g_quads++;
  text(g_f16, 12, UI_Y(-39), "What will\nBlaziken do?", TXT, TXT_SHADOW);
  const rr_frame* w = frame_of(S_WINDOW, 0);
  if (w) rr_nineslice(w, 202, UI_Y(-48), 118, 48, 8), g_quads += 9;
  static const char* CMD[4] = {"FIGHT", "BALL", "POKEMON", "RUN"};
  for (int c = 0; c < 4; c++)
    text(g_f16, 217 + (c % 2 ? 55.8f : 0), UI_Y(-38.7f) + (c < 2 ? 0 : 16), CMD[c], TXT, TXT_SHADOW);
  o.origin_x = o.origin_y = 0.5f;
  int cur = (t / 40) % 4;
  spr(S_CURSOR, 0, 212 + (cur % 2 ? 56 : 0), UI_Y(-30.7f) + (cur >= 2 ? 16 : 0), &o);
  for (int m = 0; m < 4; m++) {
    float y = -54 + m * 30;
    spr(S_TYPES, m * 5, 8, y + 2, NULL);
    text(g_f16, 46, y, MOVES[m], TXT, TXT_SHADOW);
    char pp[16];
    snprintf(pp, sizeof pp, "PP %d/16", 16 - m);
    text(g_f12, 250, y + 2, pp, TXT, TXT_SHADOW);
  }
}

/* A move animation at its busiest: 29 sprites from the move's sheet (the most any upstream
 * animation shows in one frame; p90 is 11), every 5th additive (20% of upstream's anim sprites
 * use additive blending), over a full-field background layer. */
#define ANIM_SPRITES 29
static void anim(int t, float cx, float cy) {
  rr_draw_opts o = RR_DEFAULT_OPTS;
  o.tint = RR_RGBA(255, 255, 255, 200);
  const rr_frame* bg = rr_atlas_frame(g_anim_bg, 0);
  for (int ty = 0; ty < 2; ty++)
    for (int tx = 0; tx < 2; tx++) {
      rr_draw_cropped(bg, tx * 256 - (t % 64), ty * 144, tx ? 64 + (t % 64) : 256, ty ? 36 : 144, &o);
      g_quads++;
    }
  o = RR_DEFAULT_OPTS;
  o.origin_x = o.origin_y = 0.5f;
  bool add = false;
  for (int k = 0; k < ANIM_SPRITES; k++) {
    bool want = k % 5 == 4;
    if (want != add) set_additive(add = want);
    float ang = k * 0.9f + t * 0.05f, r = 10 + (k * 7) % 40;
    spr(S_ANIM, t / 2 + k, cx + cosf(ang) * r, cy + sinf(ang) * r * 0.6f, &o);
  }
  if (add) set_additive(false);
}

/* ------------------------------------------------------------------ scenes */

typedef struct {
  const char* name;
  int nmon; /* per side; 0 = nothing drawn (clears only) */
  int en[2], pl[2];
  bool anim, audio;
} scene_def;

static const scene_def SCENES[] = {
    {"empty (clear both screens)", 0, {0}, {0}, false, false},
    {"singles", 1, {S_EN0}, {S_PL0}, false, false},
    {"singles + move anim", 1, {S_EN0}, {S_PL0}, true, false},
    {"doubles", 2, {S_EN0, S_EN1}, {S_PL0, S_PL1}, false, false},
    {"doubles + move anim", 2, {S_EN0, S_EN1}, {S_PL0, S_PL1}, true, false},
    {"doubles + anim + sound", 2, {S_EN0, S_EN1}, {S_PL0, S_PL1}, true, true},
    {"doubles + anim, largest sprites", 2, {S_BIG_EN0, S_BIG_EN1}, {S_BIG_PL0, S_BIG_PL1}, true, false},
};
#define N_SCENES ((int)(sizeof SCENES / sizeof SCENES[0]))

static const char* EN_NAMES[2] = {"Garchomp", "Dragonite"};
static const char* PL_NAMES[2] = {"Blaziken", "Charizard"};

static void draw_scene(const scene_def* s, int t) {
  g_quads = 0;
  rr_target(RR_TOP, RR_RGBA(0, 0, 0, 255));
  if (s->nmon) {
    field_bg();
    if (s->nmon == 1) {
      mon(s->en[0], t, 236, 84);
      if (s->anim) anim(t, 236, 60);
      mon(s->pl[0], t, 106, 148);
      info_enemy(140, UI_Y(-141), EN_NAMES[0], t);
      info_player(310, UI_Y(-72), PL_NAMES[0], false, t);
    } else {
      /* upstream getFieldPositionOffset: left -32,-8, right +32,0 */
      mon(s->en[0], t, 236 - 32, 84 - 8);
      mon(s->en[1], t, 236 + 32, 84);
      if (s->anim) anim(t, 236 + 32, 60);
      mon(s->pl[0], t, 106 - 32, 148 - 8);
      mon(s->pl[1], t, 106 + 32, 148);
      info_enemy(140, UI_Y(-149), EN_NAMES[0], t);
      info_enemy(150, UI_Y(-121), EN_NAMES[1], t);
      info_player(310, UI_Y(-84), PL_NAMES[0], true, t);
      info_player(320, UI_Y(-56), PL_NAMES[1], true, t);
    }
    trays();
  }
  rr_target(RR_BOTTOM, RR_RGBA(0x18, 0x18, 0x20, 255));
  if (s->nmon) bottom_ui(t);
}

/* ------------------------------------------------------------------ frame runner */

#define WARMUP 10
#define MAX_FRAMES 400
#define VSYNC_MS (1000.0 / 59.83) /* the 3DS LCD refresh */

typedef struct {
  float period, build, end, gpu;
} fsample;
static fsample g_fs[MAX_FRAMES];
static float g_tmp[MAX_FRAMES];

typedef struct {
  dist period, build, end, gpu;
  int missed, n, quads;
} run_stats;

typedef void (*draw_fn)(int t, void* arg);

/* Runs `frames` recorded frames. locked: citro3d's vsync-locked frame (what the app uses);
 * otherwise frames start as soon as the GPU is free, which gives the real cost per frame.
 * gpu = citro3d's GPU time (render + display transfers) of that frame. */
static run_stats run(draw_fn draw, void* arg, int frames, bool locked, void (*per_frame)(int)) {
  run_stats r;
  memset(&r, 0, sizeof r);
  if (frames > MAX_FRAMES) frames = MAX_FRAMES;
  u64 prev = 0;
  int n = 0;
  for (int i = 0; i <= frames; i++) {
    if (!keep_running()) break;
    if (locked)
      rr_frame_begin();
    else
      C3D_FrameBegin(0);
    u64 t1 = now_ticks();
    if (i > 0) {
      g_fs[i - 1].gpu = C3D_GetDrawingTime();
      g_fs[i - 1].period = (float)ticks_ms(t1 - prev);
      n = i;
    }
    prev = t1;
    if (i == frames) { /* closing frame: only there to read the last frame's GPU time */
      rr_target(RR_TOP, RR_RGBA(0, 0, 0, 255));
      rr_target(RR_BOTTOM, RR_RGBA(0, 0, 0, 255));
      rr_frame_end();
      break;
    }
    if (per_frame) per_frame(i);
    draw(i, arg);
    u64 t2 = now_ticks();
    rr_frame_end();
    u64 t3 = now_ticks();
    g_fs[i].build = (float)ticks_ms(t2 - t1);
    g_fs[i].end = (float)ticks_ms(t3 - t2);
  }
  r.quads = g_quads;
  int from = n > WARMUP * 2 ? WARMUP : 0;
  r.n = n - from;
#define DIST(field)                                              \
  do {                                                           \
    for (int k = from; k < n; k++) g_tmp[k - from] = g_fs[k].field; \
    r.field = dist_of(g_tmp, n - from);                          \
  } while (0)
  DIST(period);
  DIST(build);
  DIST(end);
  DIST(gpu);
#undef DIST
  for (int k = from; k < n; k++) r.missed += g_fs[k].period > VSYNC_MS * 1.5;
  return r;
}

static void draw_scene_fn(int t, void* arg) { draw_scene(arg, t); }

static void scene_audio(int i) {
  if (!g_sound) return;
  if (i % 30 == 0) snd_sfx_play("audio/cry/445");
  if (i % 10 == 5) snd_sfx_play("audio/battle_anims/PRSFX- Flamethrower");
  if (i % 10 == 0) snd_sfx_play("audio/se/hit");
  snd_update();
}

/* ------------------------------------------------------------------ ramps */

typedef struct {
  int count;
} ramp_arg;

static void draw_sprite_ramp(int t, void* arg) {
  int k = ((ramp_arg*)arg)->count;
  g_quads = 0;
  rr_target(RR_TOP, RR_RGBA(0, 0, 0, 255));
  field_bg();
  rr_draw_opts o = RR_DEFAULT_OPTS;
  o.origin_x = 0.5f;
  o.origin_y = 1.0f;
  for (int i = 0; i < k; i++) spr(S_EN0, (t * 10) / 60 + i * 3, 20 + (i * 37) % 280, 80 + (i * 23) % 100, &o);
  rr_target(RR_BOTTOM, RR_RGBA(0x18, 0x18, 0x20, 255));
}

static void draw_text_ramp(int t, void* arg) {
  int k = ((ramp_arg*)arg)->count;
  g_quads = 0;
  rr_target(RR_TOP, RR_RGBA(0, 0, 0, 255));
  rr_target(RR_BOTTOM, RR_RGBA(0x18, 0x18, 0x20, 255));
  static const char* LINE = "Garchomp used Dragon Claw! It's super eff";
  char buf[48];
  int line = 0;
  while (k > 0) {
    int n = k < 35 ? k : 35; /* LINE has 35 glyphs besides its spaces */
    int take = 0, glyphs = 0;
    while (LINE[take] && glyphs < n) glyphs += LINE[take++] != ' ';
    memcpy(buf, LINE, take);
    buf[take] = 0;
    text(line % 2 ? g_f12 : g_f16, 2 + (t + line) % 4, -58 + (line * 14) % 236, buf, TXT, TXT_SHADOW);
    k -= glyphs;
    line++;
  }
}

/* Full-screen layers straight through citro2d from a texture the bench owns, in linear
 * memory (FCRAM, where rr puts every texture) or in VRAM, to see what texture reads cost the GPU. */
typedef struct {
  int count;
  C3D_Tex* tex;
} fill_arg;
static Tex3DS_SubTexture g_fill_sub = {400, 240, 0.0f, 1.0f, 400 / 512.0f, 1.0f - 240 / 256.0f};

static void draw_fill(int t, void* arg) {
  fill_arg* f = arg;
  g_quads = f->count;
  rr_target(RR_TOP, RR_RGBA(0, 0, 0, 255));
  C2D_Image img = {f->tex, &g_fill_sub};
  for (int i = 0; i < f->count; i++) C2D_DrawImageAt(img, 0, 0, 0.0f, NULL, 1.0f, 1.0f);
  rr_target(RR_BOTTOM, RR_RGBA(0x18, 0x18, 0x20, 255));
}

static void res_run(const char* name, run_stats* lk, run_stats* un) {
  res("%-31s %4d | %5.1f %5.1f %5.1f %4d | %5.2f %5.2f %5.2f | %5.2f %5.2f | %5.2f %5.2f | %5.0f", name, lk->quads,
      lk->period.med, lk->period.p95, lk->period.max, lk->missed, lk->build.med, lk->build.p95, lk->build.max,
      lk->end.med, lk->end.max, lk->gpu.med, lk->gpu.p95, un->period.med > 0 ? 1000.0 / un->period.med : 0);
}

/* Largest count whose frame fits in `budget` ms, interpolated between the measured steps. */
static double fit(const int* count, const double* ms, int n, double budget) {
  for (int i = 1; i < n; i++)
    if (ms[i] > budget) {
      if (ms[i - 1] > budget) return 0;
      return count[i - 1] + (count[i] - count[i - 1]) * (budget - ms[i - 1]) / (ms[i] - ms[i - 1]);
    }
  return -1; /* never over budget */
}

static void ramp_report(const char* what, const int* count, const double* ms, int n) {
  double a = fit(count, ms, n, 1000.0 / 60), b = fit(count, ms, n, 1000.0 / 30);
  char s60[32], s30[32];
  if (a < 0) snprintf(s60, sizeof s60, "over %d", count[n - 1]);
  else snprintf(s60, sizeof s60, "about %.0f", a);
  if (b < 0) snprintf(s30, sizeof s30, "over %d", count[n - 1]);
  else snprintf(s30, sizeof s30, "about %.0f", b);
  res("  => %s per frame: %s at 60 fps, %s at 30 fps (whole frame cost, nothing else running)", what, s60, s30);
}

/* ------------------------------------------------------------------ section */

extern u32 __ctru_linear_heap, __ctru_linear_heap_size;

void render_tests(void) {
  res("\n== rendering ==");
  status("rendering: loading the scene");
  mem_snap m0 = mem_now();
  if (g_pak[P_FONT]) {
    g_f16 = rr_font_load_key("fonts/emerald@16");
    g_f12 = rr_font_load_key("fonts/emerald@12");
  }
  if (!g_f16 || !g_f12) g_glyphs = synth_atlas(128, 128, 8, 14, 64, 99);
  g_shadow = synth_atlas(64, 16, 56, 12, 1, 3);
  g_anim_bg = synth_atlas(256, 256, 256, 144, 1, 5);
  static const int singles_set[] = {S_BG, S_A, S_B, S_EN0, S_PL0, S_INFO_PL, S_INFO_PL_MINI, S_INFO_EN, S_HP,
                                    S_EXP, S_TYPE_PL, S_TYPE_EN, S_STATUS, S_TRAY_BALL, S_TRAY_PL, S_TRAY_EN,
                                    S_MSG_BG, S_WINDOW, S_CURSOR, S_TYPES, S_ANIM};
  int stand_ins = 0;
  for (size_t i = 0; i < sizeof singles_set / sizeof singles_set[0]; i++) load_src(singles_set[i]);
  /* touch every glyph page the scene uses, so first-use page loads stay out of the frame times */
  const char* warm = "What will Blaziken do? FIGHT BALL POKEMON RUN Dragon Claw Earthquake Swords Dance Stone "
                     "Edge PP 0123456789/ Garchomp Dragonite Charizard Lv.100 It's super eff";
  if (g_f16) rr_text_width(g_f16, warm);
  if (g_f12) rr_text_width(g_f12, warm);
  mem_snap m1 = mem_now();
  load_src(S_EN1);
  load_src(S_PL1);
  mem_snap m2 = mem_now();
  load_src(S_BIG_EN0);
  load_src(S_BIG_EN1);
  load_src(S_BIG_PL0);
  load_src(S_BIG_PL1);
  mem_snap m3 = mem_now();
  for (int i = 0; i < S_COUNT; i++) stand_ins += g_src[i].a && !g_src[i].real;
  if (stand_ins)
    res("NOTE: %d of %d images are stand-ins (same sizes, made-up pixels): %s", stand_ins, S_COUNT,
        g_pak[P_GFX] ? "missing from gfx.pak" : "gfx.pak not on the SD card");
  res("text: %s", g_f16 && g_f12 ? "font.pak (emerald 16 and 12 px)" : "stand-in glyphs (font.pak missing)");
  res("always stand-ins: the Pokemon shadows (upstream draws them in its shader) and the anim background");
  res("texture memory: singles scene %.2f MB, + doubles Pokemon %.2f MB, + the 4 largest sheets %.2f MB",
      mb(m1.tex_bytes - m0.tex_bytes), mb(m2.tex_bytes - m1.tex_bytes), mb(m3.tex_bytes - m2.tex_bytes));
  res("linear heap free with all of it loaded: %.1f MB", mb(m3.linear_free));
  res("%s", "");
  res("frame times in ms. vsync: the app's normal 60 fps loop (%.1f ms per frame); missed = frames that", VSYNC_MS);
  res("took 2+ refreshes. build = CPU time queueing draws; end = rr_frame_end (citro3d submits and flushes");
  res("the data cache); GPU = drawing + copying to the screens. max fps = the same scene with no vsync wait.");
  res("%-31s %4s | %-23s | %-17s | %-11s | %-11s | %5s", "scene", "quad", "vsync period med p95 max miss",
      "build med p95 max", "end med max", "GPU med p95", "max");
  res("%-31s %4s | %-23s | %-17s | %-11s | %-11s | %5s", "", "s", "", "", "", "", "fps");
  for (int s = 0; s < N_SCENES && !g_stop; s++) {
    const scene_def* sd = &SCENES[s];
    if (!status("rendering: %s", sd->name)) break;
    if (sd->audio && g_sound) snd_bgm_play("audio/bgm/battle_wild", true);
    rr_set_origin(RR_TOP, 40, 30);
    rr_set_origin(RR_BOTTOM, 0, 60);
    /* fewer frames in the emulator: its software renderer is slow and its GPU times mean nothing */
    run_stats lk = run(draw_scene_fn, (void*)sd, g_headless ? 60 : 250, true, sd->audio ? scene_audio : NULL);
    run_stats un = run(draw_scene_fn, (void*)sd, g_headless ? 40 : 120, false, sd->audio ? scene_audio : NULL);
    if (sd->audio && g_sound) snd_bgm_stop();
    char name[48];
    snprintf(name, sizeof name, "%s%s", sd->name, sd->audio && !g_sound ? " (OFF)" : "");
    res_run(name, &lk, &un);
  }
  res_save();

  /* what the per-frame cache flush costs by itself */
  if (!g_stop) {
    float fl[20];
    for (int i = 0; i < 20; i++) {
      u64 t0 = now_ticks();
      GSPGPU_FlushDataCache((void*)__ctru_linear_heap, __ctru_linear_heap_size);
      fl[i] = (float)ms_since(t0);
    }
    dist d = dist_of(fl, 20);
    res("citro3d flushes the whole %.0f MB linear heap from the CPU cache in every rr_frame_end;", mb(__ctru_linear_heap_size));
    res("  that flush alone: median %.2f ms, max %.2f ms", d.med, d.max);
  }

  /* sprite ramp */
  if (!g_stop) {
    res("\nsprite ramp: N animated %dx%d sprites (Garchomp's sheet) over the arena, no vsync", SPECS[S_EN0].fw,
        SPECS[S_EN0].fh);
    res("  %6s | %6s %6s %6s %6s", "N", "frame", "build", "end", "GPU");
    static const int K[] = {0, 25, 50, 100, 200, 300, 400, 600, 800, 1200, 1600, 2400, 3200};
    int cnt[16];
    double ms[16];
    int n = 0;
    for (size_t i = 0; i < sizeof K / sizeof K[0] && !g_stop; i++) {
      if (g_headless && K[i] > 400) break; /* Azahar's software renderer crawls here; its GPU times mean nothing */
      if (!status("rendering: sprite ramp, %d sprites", K[i])) break;
      rr_set_origin(RR_TOP, 40, 30);
      rr_set_origin(RR_BOTTOM, 0, 60);
      ramp_arg a = {K[i]};
      run_stats r = run(draw_sprite_ramp, &a, 40, false, NULL);
      res("  %6d | %6.2f %6.2f %6.2f %6.2f", K[i], r.period.med, r.build.med, r.end.med, r.gpu.med);
      cnt[n] = K[i];
      ms[n++] = r.period.med;
      if (r.period.med > 45) break;
    }
    ramp_report("sprites", cnt, ms, n);
  }

  /* text ramp */
  if (!g_stop) {
    res("\ntext ramp: N glyphs with drop shadow (2 quads each) on the bottom screen, no vsync");
    res("  %6s | %6s %6s %6s %6s", "N", "frame", "build", "end", "GPU");
    static const int K[] = {0, 100, 200, 400, 800, 1200, 1600};
    int cnt[8];
    double ms[8];
    int n = 0;
    for (size_t i = 0; i < sizeof K / sizeof K[0] && !g_stop; i++) {
      if (g_headless && K[i] > 400) break;
      if (!status("rendering: text ramp, %d glyphs", K[i])) break;
      rr_set_origin(RR_TOP, 40, 30);
      rr_set_origin(RR_BOTTOM, 0, 60);
      ramp_arg a = {K[i]};
      run_stats r = run(draw_text_ramp, &a, 40, false, NULL);
      res("  %6d | %6.2f %6.2f %6.2f %6.2f", K[i], r.period.med, r.build.med, r.end.med, r.gpu.med);
      cnt[n] = K[i];
      ms[n++] = r.period.med;
    }
    ramp_report("glyphs", cnt, ms, n);
  }

  /* fill ramp, FCRAM vs VRAM textures */
  if (!g_stop) {
    C3D_Tex lin, vram;
    bool have_lin = C3D_TexInit(&lin, 512, 256, GPU_RGBA5551);
    bool have_vram = have_lin && C3D_TexInitVRAM(&vram, 512, 256, GPU_RGBA5551);
    if (have_lin) {
      u16* px = lin.data;
      for (int i = 0; i < 512 * 256; i++) px[i] = (u16)(((i * 7) & 0xF7FE) | 1);
      C3D_TexSetFilter(&lin, GPU_NEAREST, GPU_NEAREST);
      C3D_TexFlush(&lin);
    }
    if (have_vram) {
      C3D_TexSetFilter(&vram, GPU_NEAREST, GPU_NEAREST);
      GSPGPU_FlushDataCache(lin.data, lin.size);
      C3D_TexLoadImage(&vram, lin.data, GPU_TEXFACE_2D, 0);
    }
    res("\nfill: N full-screen 400x240 layers on the top screen (RGBA5551, alpha-blended), no vsync, GPU ms");
    res("  %6s | %13s | %13s", "N", "texture FCRAM", "texture VRAM");
    static const int K[] = {1, 2, 4, 8, 16, 32};
    double g_lin[6] = {0}, g_vram[6] = {0};
    for (int i = 0; i < 6 && !g_stop && have_lin; i++) {
      if (!status("rendering: fill test, %d layers", K[i])) break;
      rr_set_origin(RR_TOP, 40, 30);
      fill_arg a = {K[i], &lin};
      g_lin[i] = run(draw_fill, &a, 30, false, NULL).gpu.med;
      if (have_vram) {
        a.tex = &vram;
        g_vram[i] = run(draw_fill, &a, 30, false, NULL).gpu.med;
      }
      res("  %6d | %13.2f | %13.2f", K[i], g_lin[i], g_vram[i]);
    }
    if (!g_stop && have_lin && g_lin[5] > g_lin[0]) {
      double per_lin = (g_lin[5] - g_lin[0]) / 31, per_vram = have_vram ? (g_vram[5] - g_vram[0]) / 31 : 0;
      res("  => one full-screen layer costs the GPU %.3f ms from FCRAM (%.0f Mpixel/s), %.3f ms from VRAM (%.0f Mpixel/s)",
          per_lin, 96 / per_lin, per_vram, per_vram > 0 ? 96 / per_vram : 0);
    }
    if (have_vram) C3D_TexDelete(&vram);
    if (have_lin) C3D_TexDelete(&lin);
  }

  for (int i = 0; i < S_COUNT; i++) free_src(i);
  rr_atlas_free(g_shadow);
  rr_atlas_free(g_anim_bg);
  rr_atlas_free(g_glyphs);
  rr_font_free(g_f16);
  rr_font_free(g_f12);
  g_shadow = g_anim_bg = g_glyphs = NULL;
  g_f16 = g_f12 = NULL;
  res_save();
}
