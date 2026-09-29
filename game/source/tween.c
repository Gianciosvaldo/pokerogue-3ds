#include "tween.h"

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

float ease(ease_fn f, float t) {
  if (t <= 0) return 0;
  if (t >= 1) return 1;
  switch (f) {
    case EASE_SINE_IN:
      return 1 - cosf(t * (float)M_PI / 2);
    case EASE_SINE_OUT:
      return sinf(t * (float)M_PI / 2);
    case EASE_SINE_IN_OUT:
      return 0.5f * (1 - cosf((float)M_PI * t));
    case EASE_QUAD_IN:
      return t * t;
    case EASE_QUAD_OUT:
      return t * (2 - t);
    case EASE_QUAD_IN_OUT:
      t *= 2;
      if (t < 1) return 0.5f * t * t;
      t -= 1;
      return -0.5f * (t * (t - 2) - 1);
    case EASE_CUBIC_IN:
      return t * t * t;
    case EASE_CUBIC_OUT:
      t -= 1;
      return t * t * t + 1;
    case EASE_CUBIC_IN_OUT:
      t *= 2;
      if (t < 1) return 0.5f * t * t * t;
      t -= 2;
      return 0.5f * (t * t * t + 2);
    case EASE_BACK_OUT: {
      const float s = 1.70158f;
      t -= 1;
      return t * t * ((s + 1) * t + s) + 1;
    }
    case EASE_EXPO_OUT:
      return 1 - powf(2, -10 * t);
    case EASE_LINEAR:
    default:
      return t;
  }
}

void tween_start_delay(tween* tw, float from, float to, float ms, float delay, ease_fn e) {
  tw->from = from;
  tw->to = to;
  tw->value = from;
  tw->ms = ms;
  tw->delay = delay;
  tw->t = 0;
  tw->ease = e;
  tw->active = true;
  if (ms <= 0 && delay <= 0) {
    tw->value = to;
    tw->active = false;
  }
}

void tween_start(tween* tw, float from, float to, float ms, ease_fn e) { tween_start_delay(tw, from, to, ms, 0, e); }

bool tween_update(tween* tw, float dt) {
  if (!tw->active) return false;
  if (tw->delay > 0) {
    tw->delay -= dt;
    if (tw->delay > 0) return false;
    dt = -tw->delay;
    tw->delay = 0;
  }
  tw->t += dt;
  if (tw->ms <= 0 || tw->t >= tw->ms) {
    tw->value = tw->to;
    tw->active = false;
    return true;
  }
  tw->value = tw->from + (tw->to - tw->from) * ease(tw->ease, tw->t / tw->ms);
  return false;
}

bool tween_done(const tween* tw) { return !tw->active; }
