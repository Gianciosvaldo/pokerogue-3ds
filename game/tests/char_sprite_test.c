/* char_sprite_test: source/char_sprite.c (upstream's CharSprite) with a fake drawing layer.
 *
 *   make char_sprite_test && build/char_sprite_test
 *
 * Checks against src/ui/containers/char-sprite.ts: it comes in from x = 352 to 218 over 750 ms (Cubic.easeOut) and goes out over 750 ms
 * (Cubic.easeIn) from wherever it is, its feet at y = 138 with origin (0.5, 1); a new expression fades in over the old one in 250 ms
 * (Sine.easeIn) and becomes the sprite when it ends; showing the same character again only changes the expression; a missing atlas draws
 * nothing and changes nothing else. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "char_sprite.h"
#include "ui_widgets.h"

static int checks, failed;
#define CHECK(cond, ...)                               \
  do {                                                 \
    checks++;                                          \
    if (!(cond)) {                                     \
      failed++;                                        \
      if (failed < 40) {                               \
        printf("FAIL line %d: %s: ", __LINE__, #cond); \
        printf(__VA_ARGS__);                           \
        printf("\n");                                  \
      }                                                \
    }                                                  \
  } while (0)

/* ------------------------------------------------------------------ fakes */

const rr_draw_opts RR_DEFAULT_OPTS = {0, 0, 1, 1, 0, RR_RGBA(255, 255, 255, 255)};
static bool g_has = true;
bool img_has(const char* key) { return g_has && !strncmp(key, "images/character/", 17); }

typedef struct {
  char key[48], frame[32];
  float x, y, ox, oy;
  int alpha;
} drawn;
static drawn D[8];
static int n_drawn;
static void rec(const char* key, const char* frame, float x, float y, float ox, float oy, int alpha) {
  if (n_drawn >= 8) return;
  drawn* d = &D[n_drawn++];
  snprintf(d->key, sizeof d->key, "%s", key);
  snprintf(d->frame, sizeof d->frame, "%s", frame ? frame : "");
  d->x = x, d->y = y, d->ox = ox, d->oy = oy, d->alpha = alpha;
}
void img_draw(const char* key, const char* frame, float x, float y, float ox, float oy) { rec(key, frame, x, y, ox, oy, 255); }
void img_draw_opts(const char* key, const char* frame, float x, float y, const rr_draw_opts* o) { rec(key, frame, x, y, o->origin_x, o->origin_y, (int)(o->tint & 0xFF)); }

static void draw(void) {
  n_drawn = 0;
  chs_draw();
}
static bool near(float a, float b) { return fabsf(a - b) < 0.01f; }
#define ATLAS "images/character/rival_f"

int main(void) {
  /* nothing shown: nothing drawn, nothing busy */
  chs_reset();
  draw();
  CHECK(n_drawn == 0 && !chs_busy() && !chs_shown(), "nothing at the start");
  chs_set_variant("smile");
  chs_hide();
  CHECK(!chs_busy() && !chs_shown(), "variant and hide of nobody do nothing");

  /* coming in */
  chs_show(ATLAS, "shock");
  CHECK(chs_busy() && chs_shown() && near(chs_x(), CHS_X_OUT), "it starts past the right edge (%.2f)", chs_x());
  draw();
  CHECK(n_drawn == 1 && !strcmp(D[0].key, ATLAS) && !strcmp(D[0].frame, "shock") && near(D[0].x, 352) && near(D[0].y, 138) && near(D[0].ox, 0.5f) && near(D[0].oy, 1.0f),
        "drawn with its feet at 138, origin (0.5, 1) (%d: %s %s %.1f %.1f)", n_drawn, D[0].key, D[0].frame, D[0].x, D[0].y);
  chs_update(375);
  /* Cubic.easeOut at 0.5 = 1 - 0.5^3 = 0.875 */
  CHECK(near(chs_x(), 352 + (218 - 352) * 0.875f) && chs_busy(), "halfway in: %.3f", chs_x());
  chs_update(374);
  CHECK(chs_busy(), "1 ms short");
  chs_update(1);
  CHECK(!chs_busy() && near(chs_x(), 218), "in place at 750 ms (%.2f)", chs_x());
  chs_update(5000);
  CHECK(near(chs_x(), 218) && !chs_busy(), "and it stays");

  /* a new expression: fades in over the old one, then it is the sprite */
  chs_set_variant("smile");
  draw();
  CHECK(n_drawn == 2 && !strcmp(D[0].frame, "shock") && D[0].alpha == 255 && !strcmp(D[1].frame, "smile") && D[1].alpha == 0, "the new one starts invisible over the old (%d: %s %d, %s %d)",
        n_drawn, D[0].frame, D[0].alpha, D[1].frame, D[1].alpha);
  CHECK(!chs_busy(), "an expression change is not a slide");
  chs_update(125);
  draw();
  /* Sine.easeIn at 0.5 = 1 - cos(pi/4) = 0.2929 */
  CHECK(n_drawn == 2 && abs(D[1].alpha - (int)(0.29289f * 255 + 0.5f)) <= 1, "half way: alpha %d", D[1].alpha);
  chs_update(125);
  draw();
  CHECK(n_drawn == 1 && !strcmp(D[0].frame, "smile") && D[0].alpha == 255, "after 250 ms it is the sprite (%d: %s)", n_drawn, D[0].frame);
  chs_set_variant("smile");
  draw();
  CHECK(n_drawn == 1, "the same expression again changes nothing");
  chs_show(ATLAS, "neutral");
  draw();
  CHECK(n_drawn == 2 && !strcmp(D[1].frame, "neutral") && !chs_busy() && near(chs_x(), 218), "showing her again only changes the expression");
  chs_update(250);

  /* a second change before the first one ended: the first lands */
  chs_set_variant("angry");
  chs_update(100);
  chs_set_variant("shock");
  draw();
  CHECK(n_drawn == 2 && !strcmp(D[0].frame, "angry") && !strcmp(D[1].frame, "shock") && D[1].alpha == 0, "a change in the middle of a change: the first lands, the second starts (%s, %s)", D[0].frame, D[1].frame);
  chs_update(250);

  /* going out from where it is: out in 750 ms from the middle of the way in */
  chs_hide();
  CHECK(chs_busy() && !chs_shown(), "hide: busy, no longer shown");
  chs_update(375);
  /* Cubic.easeIn at 0.5 = 0.125 */
  CHECK(near(chs_x(), 218 + (352 - 218) * 0.125f), "halfway out: %.3f", chs_x());
  draw();
  CHECK(n_drawn >= 1, "still drawn on its way out");
  chs_update(375);
  CHECK(!chs_busy() && near(chs_x(), 352), "gone at 750 ms (%.2f)", chs_x());
  draw();
  CHECK(n_drawn == 0, "and not drawn");

  /* hiding in the middle of the way in leaves from there */
  chs_show(ATLAS, "smile");
  chs_update(375);
  const float mid = chs_x();
  chs_hide();
  chs_update(1);
  CHECK(chs_x() <= mid + 0.5f && chs_x() >= mid - 0.5f, "it leaves from where it was (%.2f -> %.2f)", mid, chs_x());
  chs_update(749);
  CHECK(!chs_busy() && near(chs_x(), 352), "and is gone after 750 ms");

  /* a missing atlas: nothing drawn, the timeline still runs */
  g_has = false;
  chs_reset();
  chs_show(ATLAS, "smile");
  draw();
  CHECK(n_drawn == 0 && chs_busy(), "no atlas: nothing drawn, still a slide to wait for");
  chs_update(750);
  CHECK(!chs_busy(), "which ends");
  g_has = true;

  /* no expression given: neutral */
  chs_reset();
  chs_show(ATLAS, NULL);
  draw();
  CHECK(n_drawn == 1 && !strcmp(D[0].frame, "neutral"), "no variant: neutral");

  printf("char_sprite_test: %d checks, %d failed\n", checks, failed);
  return failed ? 1 : 0;
}
