/*
 * tween: Phaser's easing functions and a tiny value tween for screen animations.
 *
 * Screens own their tweens (plain structs, no allocation) and advance them in update().
 */
#ifndef GM_TWEEN_H
#define GM_TWEEN_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Phaser's ease names (Phaser.Math.Easing), the ones upstream uses. */
typedef enum {
  EASE_LINEAR,
  EASE_SINE_IN,
  EASE_SINE_OUT,
  EASE_SINE_IN_OUT,
  EASE_QUAD_IN,
  EASE_QUAD_OUT,
  EASE_QUAD_IN_OUT,
  EASE_CUBIC_IN,
  EASE_CUBIC_OUT,
  EASE_CUBIC_IN_OUT,
  EASE_BACK_OUT,
  EASE_EXPO_OUT,
} ease_fn;

float ease(ease_fn f, float t); /* t in 0..1 */

/* A value moving from `from` to `to` over `ms` (after `delay` ms). */
typedef struct {
  float from, to, value;
  float ms, delay, t;
  ease_fn ease;
  bool active;
} tween;

void tween_start(tween* tw, float from, float to, float ms, ease_fn e);
void tween_start_delay(tween* tw, float from, float to, float ms, float delay, ease_fn e);
/* Advances by dt ms; returns true on the frame it finishes. value holds the current value. */
bool tween_update(tween* tw, float dt);
bool tween_done(const tween* tw); /* not running (finished or never started) */

#ifdef __cplusplus
}
#endif
#endif
