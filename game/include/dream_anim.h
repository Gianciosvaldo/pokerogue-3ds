/*
 * dream_anim: the timeline of Weird Dream's transformation, as pure functions of time (no drawing, no allocation), so the host tests can
 * check it. Upstream: doPokemonTransformationSequence (encounter-transformation-sequence.ts) with Animation.doSpiralUpward / doArcDownward /
 * doCycle / doCircleInward (animations.ts), scheduled by weird-dream-encounter.ts (centred back to back for up to three Pokemon, otherwise
 * three side by side, 4 s apart, the rest following in the same spots).
 *
 * Coordinates are upstream's 320x180 canvas (the dream container's own: the origin is the canvas' top left). Times are in ms. The Phaser
 * tweens' constants are kept: 2000 ms fade in, a 1000 ms white tint with the spiral, 700 ms and the downward arc, 1000 ms, the cycle of
 * silhouettes (500/c ms a leg, c = 1.5, 2, ... 6), the circle inward, 900 ms, a 1500 ms fade of the new silhouette after 150 ms, 3000 ms of
 * holding, then the new sprite fades out over 2000 ms after 150 ms. Upstream's `easing:` key on the two Sine.easeIn tweens is not a Phaser
 * option (the key is `ease`): they run linear, and so do they here.
 */
#ifndef GM_DREAM_ANIM_H
#define GM_DREAM_ANIM_H

#include <math.h>
#include <stdbool.h>
#include <stddef.h>

#include "tween.h"


#define DA_MAX 6            /* a party */
#define DA_MAX_PARTICLES 160

enum { DA_CENTER, DA_LEFT, DA_RIGHT };   /* TransformationScreenPosition */

/* What each of a sequence's four sprites looks like at local time t (ms since the sequence began). The silhouettes are the sprite filled white. */
typedef struct {
  bool old_vis;   float old_alpha;                          /* the old Pokemon */
  bool otint_vis; float otint_alpha, otint_scale;           /* its white silhouette */
  bool new_vis;   float new_alpha;                          /* the new Pokemon */
  bool ntint_vis; float ntint_alpha, ntint_scale;           /* its white silhouette */
} da_state;

typedef struct {
  float x, y, scale;   /* the evo_sparkle image, origin (0.5, 0.5), canvas coordinates */
} da_particle;

/* The x and y offsets of a screen position (0, 0 centre; -100, -15 left; 100, -15 right). The sprites' centre is (160 + x, 90 + y). */


/* The sparkles of one sequence at its local time t, in creation order; returns how many (at most max). */

/* The moment (ms into a sequence) its promise resolves (the next one may begin), and the moment its last sprite has faded away. */

/* The moments the white silhouettes of the old and the new Pokemon are first and last needed: the old sheet from the sequence's start to
 * da_old_free_ms, the new one from da_new_load_ms to the end. For loading each sheet only while it shows. */

/* The schedule for n Pokemon: when each sequence begins and where. Returns the time every sequence has resolved (upstream's Promise.all),
 * when the background starts to fade away. */

/* The container's alpha: it fades in (3000 ms, Sine.easeInOut) from t = 0, and out the same way from t_hide (< 0 = not yet). */
#define DA_FADE_MS 3000

/* ---- implementation (header only, so the host tests that include battle_view.c need nothing more) ---- */

#define DA_FRAME_MS (1000.0 / 60.0)   /* getFrameMs(1) */
#define DA_PI_128 (3.14159265358979323846 / 128.0)

/* Animation.sin / cos */
static inline double da_a_sin(double index, double amp) { return amp * sin(index * DA_PI_128); }
static inline double da_a_cos(double index, double amp) { return amp * cos(index * DA_PI_128); }

/* ---- the timeline of one sequence (ms) ---- */
#define DA_T_FADE_IN 2000.0                  /* the old sprite's alpha, Cubic.easeInOut */
#define DA_T_SPIRAL DA_T_FADE_IN                /* doSpiralUpward and the white tint's 1000 ms start together */
#define DA_T_TINT_MS 1000.0
#define DA_T_OLD_GONE (DA_T_FADE_IN + DA_T_TINT_MS)
#define DA_T_ARC (DA_T_OLD_GONE + 700.0)        /* doArcDownward */
#define DA_T_CYCLE (DA_T_ARC + 1000.0)          /* the new silhouette appears at scale 0.25 and doCycle(1.5, 6) begins */
#define DA_CYCLE_FIRST 1.5
#define DA_CYCLE_LAST 6.0
#define DA_CIRCLE_TO_FADE 900.0
#define DA_FADE_DELAY 150.0
#define DA_NTINT_FADE 1500.0
#define DA_HOLD 3000.0
#define DA_NEW_FADE 2000.0

/* the cycle's duration: every step but the last runs forth and back */
static inline double da_cycle_ms(void)
{
  double total = 0;
  for (double c = DA_CYCLE_FIRST; c < DA_CYCLE_LAST + 0.001; c += 0.5) total += c < DA_CYCLE_LAST - 0.001 ? 2 * 500.0 / c : 500.0 / c;
  return total;
}
static inline double da_t_cycle_end(void) { return DA_T_CYCLE + da_cycle_ms(); }                 /* the new sprite shows, the circle goes inward */
static inline double da_t_ntint_fade(void) { return da_t_cycle_end() + DA_CIRCLE_TO_FADE + DA_FADE_DELAY; }
static inline double da_t_resolve(void) { return da_t_ntint_fade() + DA_NTINT_FADE + DA_HOLD; }
static inline double da_t_new_fade(void) { return da_t_resolve() + DA_FADE_DELAY; }

static inline float da_resolve_ms(void) { return (float)da_t_resolve(); }
static inline float da_end_ms(void) { return (float)(da_t_new_fade() + DA_NEW_FADE); }
static inline float da_old_free_ms(void) { return (float)da_t_cycle_end(); }
static inline float da_new_load_ms(void) { return (float)DA_T_ARC; }

static inline void da_offset(int pos, float* x, float* y)
{
  *x = pos == DA_CENTER ? 0 : pos == DA_RIGHT ? 100 : -100;
  *y = pos == DA_CENTER ? 0 : -15;
}

static inline double da_clamp01(double v) { return v < 0 ? 0 : v > 1 ? 1 : v; }

static inline void da_state_at(float tf, da_state* s)
{
  const double t = tf;
  *s = (da_state){0};
  /* pokemonSprite: alpha 0 -> 1 over 2000, hidden when the tint is complete */
  s->old_vis = t < DA_T_OLD_GONE;
  s->old_alpha = (float)ease(EASE_CUBIC_IN_OUT, (float)da_clamp01(t / DA_T_FADE_IN));
  /* pokemonTintSprite: alpha 0 -> 1 over the next 1000, then the cycle scales it, hidden by the last step */
  const double ce = da_t_cycle_end();
  s->otint_alpha = (float)da_clamp01((t - DA_T_SPIRAL) / DA_T_TINT_MS);
  s->otint_scale = 1;
  s->otint_vis = t < ce;
  /* pokemonEvoTintSprite: visible from the cycle's start at scale 0.25, alpha 1; fades once the circle has come in */
  s->ntint_vis = t >= DA_T_CYCLE && t < da_t_ntint_fade() + DA_NTINT_FADE;
  s->ntint_alpha = 1;
  s->ntint_scale = 0.25f;
  if (t >= da_t_ntint_fade()) s->ntint_alpha = (float)(1 - da_clamp01((t - da_t_ntint_fade()) / DA_NTINT_FADE));
  /* doCycle: step c lasts 500 / c a leg, forth and back except the last */
  if (t >= DA_T_CYCLE) {
    double at = DA_T_CYCLE;
    for (double c = DA_CYCLE_FIRST; c < DA_CYCLE_LAST + 0.001; c += 0.5) {
      const bool last = c >= DA_CYCLE_LAST - 0.001;
      const double d = 500.0 / c, len = last ? d : 2 * d;
      if (t < at + len || last) {
        const double u = t - at;
        double p;
        if (u >= d && last) p = 1;
        else if (u < d) p = ease(EASE_CUBIC_IN_OUT, (float)da_clamp01(u / d));
        else p = ease(EASE_CUBIC_IN_OUT, (float)da_clamp01(1 - (u - d) / d));
        s->otint_scale = (float)(1 + (0.25 - 1) * p);
        s->ntint_scale = (float)(0.25 + (1 - 0.25) * p);
        break;
      }
      at += len;
    }
    if (t >= ce) {
      s->otint_scale = 0.25f;
      s->ntint_scale = 1;
    }
  }
  /* pokemonEvoSprite: shown at the cycle's end, faded out after the hold */
  s->new_vis = t >= ce && t < da_t_new_fade() + DA_NEW_FADE;
  s->new_alpha = t < da_t_new_fade() ? 1 : (float)(1 - da_clamp01((t - da_t_new_fade()) / DA_NEW_FADE));
}

/* ---- sparkles ---- */
/* Each particle is a per-frame loop upstream (a counter that repeats every frame): frame f happens f frames after its creation, and the loop
 * destroys the particle at the first frame whose check, made on the position of the frame before, fails. */
#define DA_MAX_FRAMES 400

typedef struct {
  double x, y, scale;
} da_pos2;

static inline da_pos2 da_spiral_at(int trig0, int f, double ix, double yo)
{
  const double amp = 48 - (f / 2);
  const double trig = trig0 + 4.0 * f;
  return (da_pos2){ix + da_a_cos(trig, amp), 88 - ((double)f * f) / 80 + yo + da_a_sin(trig, amp) / 4, 1 - f / 80.0};
}
/* alive at frame f? (for f >= 1, the frame before had y > 8) */
static inline int da_spiral_frames(int trig0, double ix, double yo)
{
  for (int f = 1; f < DA_MAX_FRAMES; f++)
    if (da_spiral_at(trig0, f - 1, ix, yo).y <= 8) return f;
  return DA_MAX_FRAMES;
}

static inline double da_arc_amp(int f) { return f == 0 ? 8 : 8 + da_a_sin((f - 1) * 4.0, 40); }
static inline da_pos2 da_arc_at(int trig0, int f, double ix, double yo)
{
  const double amp = da_arc_amp(f);
  return (da_pos2){ix + da_a_cos(trig0, amp), 8 + ((double)f * f) / 5 + yo + da_a_sin(trig0, amp) / 4, 0.5};
}
static inline int da_arc_frames(int trig0, double ix, double yo)
{
  for (int f = 1; f < DA_MAX_FRAMES; f++)
    if (da_arc_at(trig0, f - 1, ix, yo).y >= 88) return f;
  return DA_MAX_FRAMES;
}

static inline da_pos2 da_circle_at(int trig0, int speed, int f, double cx, double cy)
{
  const double amp = 120 - (double)speed * f, trig = trig0 + 4.0 * f;
  return (da_pos2){cx + da_a_cos(trig, amp), cy + da_a_sin(trig, amp), 1};
}
static inline int da_circle_frames(int speed)
{
  int f = 0;
  while (120 - speed * f > 8) f++;
  return f;
}

static inline int da_emit(da_particle* out, int n, int max, da_pos2 p)
{
  if (n < max) out[n] = (da_particle){(float)p.x, (float)p.y, (float)p.scale};
  return n < max ? n + 1 : n;
}

/* the lifetimes depend only on the vertical offset (the position) and the particle: worked out once */
static inline int da_cached_spiral(int pos, int cycle, int i, double ix, double yo)
{
  static short life[3][32];
  short* l = &life[pos][cycle * 4 + i];
  if (!*l) *l = (short)da_spiral_frames(16 * cycle + 64 * i, ix, yo);
  return *l;
}
static inline int da_cached_arc(int pos, int i, double ix, double yo)
{
  static short life[3][9];
  short* l = &life[pos][i];
  if (!*l) *l = (short)da_arc_frames(i * 16, ix, yo);
  return *l;
}

static inline int da_particles(float tf, int pos, da_particle* out, int max)
{
  if (pos < 0 || pos > 2) pos = 0;
  float xo, yo;
  da_offset(pos, &xo, &yo);
  const double ix = 160 + xo, cy = 90 + yo, t = tf;
  int n = 0;
  /* doSpiralUpward: a counter with repeat 8 and getFrameMs(8) a lap; each repeat sends four particles */
  for (int cycle = 0; cycle < 8; cycle++) {
    const double born = DA_T_SPIRAL + (cycle + 1) * 8 * DA_FRAME_MS;
    if (t < born) continue;
    for (int i = 0; i < 4; i++) {
      const int trig0 = 16 * cycle + 64 * i;
      const int f = (int)floor((t - born) / DA_FRAME_MS);
      if (f < da_cached_spiral(pos, cycle, i, ix, yo)) n = da_emit(out, n, max, da_spiral_at(trig0, f, ix, yo));
    }
  }
  /* doArcDownward: repeat 6, a frame a lap; nine particles each */
  for (int rep = 0; rep < 6; rep++) {
    const double born = DA_T_ARC + (rep + 1) * DA_FRAME_MS;
    if (t < born) continue;
    for (int i = 0; i < 9; i++) {
      const int f = (int)floor((t - born) / DA_FRAME_MS);
      if (f < da_cached_arc(pos, i, ix, yo)) n = da_emit(out, n, max, da_arc_at(i * 16, f, ix, yo));
    }
  }
  /* doCircleInward: sixteen at once, sixteen more 32 frames later */
  const double ce = da_t_cycle_end();
  for (int batch = 0; batch < 2; batch++) {
    const double born = ce + batch * 32 * DA_FRAME_MS;
    const int speed = batch ? 8 : 4;
    if (t < born) continue;
    const int f = (int)floor((t - born) / DA_FRAME_MS);
    if (f >= da_circle_frames(speed)) continue;
    for (int i = 0; i < 16; i++) n = da_emit(out, n, max, da_circle_at(i * 16, speed, f, ix, cy));
  }
  return n;
}

/* ---- the whole dream ---- */
static inline float da_schedule(int n, float start_ms[DA_MAX], int pos[DA_MAX])
{
  if (n > DA_MAX) n = DA_MAX;
  const float r = da_resolve_ms();
  float all = 0;
  if (n <= 3) {
    for (int i = 0; i < n; i++) {
      start_ms[i] = i * r;
      pos[i] = DA_CENTER;
    }
  } else {
    /* doSideBySideTransformations: Pokemon i begins at 4000 i in spot i (centre, left, right) and the one three places on follows it there */
    for (int i = 0; i < n; i++) {
      pos[i] = i % 3;
      start_ms[i] = i < 3 ? i * 4000.0f : start_ms[i - 3] + r;
    }
  }
  for (int i = 0; i < n; i++) {
    const float done = start_ms[i] + r;
    if (done > all) all = done;
  }
  return all;
}

static inline float da_container_alpha(float t, float t_hide)
{
  const float in = ease(EASE_SINE_IN_OUT, (float)da_clamp01(t / DA_FADE_MS));
  if (t_hide < 0 || t < t_hide) return in;
  const float out = 1 - ease(EASE_SINE_IN_OUT, (float)da_clamp01((t - t_hide) / DA_FADE_MS));
  return in < out ? in : out;
}

#endif
