/* The character portrait (see char_sprite.h). */
#include "char_sprite.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ui_widgets.h"

#define HALF_PI 1.5707963f

typedef enum { MOVE_NONE, MOVE_IN, MOVE_OUT } move_kind;

static struct {
  char atlas[64];
  char frame[32];   /* the expression on the sprite */
  char next[32];    /* the expression fading in over it (transitionSprite), "" = none */
  bool shown;
  bool visible;
  move_kind move;
  float move_ms;
  float x, from;   /* where the slide started */
  float fade_ms;
} C;

void chs_reset(void) {
  memset(&C, 0, sizeof C);
  C.x = C.from = CHS_X_OUT;
}

static void finish_fade(void) {
  if (!C.next[0]) return;
  snprintf(C.frame, sizeof C.frame, "%s", C.next);
  C.next[0] = 0;
}

void chs_show(const char* atlas, const char* variant) {
  if (!atlas || !*atlas) return;
  if (!variant || !*variant) variant = "neutral";
  if (C.shown) {
    if (!strcmp(atlas, C.atlas)) {
      chs_set_variant(variant);
      return;
    }
    chs_reset(); /* upstream hides the other character first; with no one else to talk, it simply starts again */
  }
  snprintf(C.atlas, sizeof C.atlas, "%s", atlas);
  snprintf(C.frame, sizeof C.frame, "%s", variant);
  C.next[0] = 0;
  C.shown = true;
  C.visible = img_has(atlas); /* setVisible(texture is not the missing one) */
  C.move = MOVE_IN;
  C.move_ms = 0;
  C.x = C.from = CHS_X_OUT;
}

void chs_set_variant(const char* variant) {
  if (!C.shown || !variant || !*variant) return;
  if (C.next[0]) finish_fade(); /* a second change before the first one ended: the first lands */
  if (!strcmp(variant, C.frame)) return;
  snprintf(C.next, sizeof C.next, "%s", variant);
  C.fade_ms = 0;
}

void chs_hide(void) {
  if (!C.shown) return;
  C.shown = false;
  C.move = MOVE_OUT;
  C.move_ms = 0;
  C.from = C.x;
}

void chs_update(float dt) {
  if (C.move != MOVE_NONE) {
    C.move_ms += dt;
    float t = C.move_ms / CHS_SLIDE_MS;
    if (t >= 1.0f) {
      C.x = C.move == MOVE_IN ? CHS_X_IN : CHS_X_OUT;
      if (C.move == MOVE_OUT) C.visible = false;
      C.move = MOVE_NONE;
    } else if (C.move == MOVE_IN) {
      const float u = 1.0f - t;
      C.x = C.from + (CHS_X_IN - C.from) * (1.0f - u * u * u); /* Cubic.easeOut */
    } else {
      C.x = C.from + (CHS_X_OUT - C.from) * (t * t * t);         /* Cubic.easeIn */
    }
  }
  if (C.next[0]) {
    C.fade_ms += dt;
    if (C.fade_ms >= CHS_FADE_MS) finish_fade();
  }
}

void chs_draw(void) {
  if (!C.visible || !C.atlas[0]) return;
  img_draw(C.atlas, C.frame, C.x, CHS_Y, 0.5f, 1.0f);
  if (C.next[0]) {
    rr_draw_opts o = RR_DEFAULT_OPTS;
    o.origin_x = 0.5f;
    o.origin_y = 1.0f;
    const float a = 1.0f - cosf(C.fade_ms / CHS_FADE_MS * HALF_PI); /* Sine.easeIn */
    o.tint = RR_RGBA(255, 255, 255, (int)(a * 255.0f + 0.5f));
    img_draw_opts(C.atlas, C.next, C.x, CHS_Y, &o);
  }
}

bool chs_busy(void) { return C.move != MOVE_NONE; }
bool chs_shown(void) { return C.shown; }
float chs_x(void) { return C.x; }
