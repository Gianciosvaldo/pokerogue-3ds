/* Challenge Mode's challenge screen (see challenge_select.h). Laid out like the run settings screen: a window of rows on the bottom screen,
 * the highlighted challenge's description in the message strip. */
#include <stdio.h>
#include <string.h>

#include "challenge_select.h"
#include "i18n.h"
#include "ui.h"
#include "ui_text.h"
#include "ui_widgets.h"

#define ROW_H 16.0f

static struct {
  chsel_args a;
  chm_model m;
} C;

static bool cs_input(ui_screen* s, ui_button b) {
  (void)s;
  switch (b) {
    case BTN_UP:
    case BTN_DOWN:
      chm_move(&C.m, b == BTN_DOWN ? 1 : -1);
      ui_play_select();
      return true;
    case BTN_LEFT:
    case BTN_RIGHT:
      if (chm_change(&C.m, b == BTN_RIGHT ? 1 : -1)) ui_play_select();
      return true;
    case BTN_ACTION: {
      if (!chm_can_start(&C.m)) {   /* "None Selected" */
        ui_play_error();
        return true;
      }
      ui_play_select();
      const chsel_args a = C.a;
      const gc_challenge_set v = C.m.v;
      ui_pop();
      if (a.on_start) a.on_start(&v, a.arg);
      return true;
    }
    case BTN_CANCEL: {
      ui_play_select();
      const chsel_args a = C.a;
      ui_pop();
      if (a.on_back) a.on_back(a.arg);
      return true;
    }
    default:
      return false;
  }
}

static void cs_draw(ui_screen* s, rr_screen which) {
  (void)s;
  if (which != RR_BOTTOM) return;
  if (ui_space(UI_PHYS_BOTTOM)) rr_rect(0, 0, 320, 240, RR_RGBA(0x10, 0x10, 0x18, 255));
  if (!ui_space(UI_BOTTOM_UI)) return;
  const int rows = C.m.n + 2;   /* the title, the challenges, Start */
  const float w = 240, h = 16 + rows * ROW_H, x = 160 - w / 2, y = -48 - h - 8;
  ui_window(x, y, w, h, WIN_NORMAL);
  text_opts right = text_defaults();
  right.origin_x = 1;
  char text[160], key[64];
  i18n_t(text, sizeof text, "challenges:title", NULL);
  text_draw(TS_WINDOW, x + 18, y + 8, text, NULL);
  for (int i = 0; i < C.m.n; i++) {
    const int id = C.m.rows[i];
    const float ry = y + 8 + (i + 1) * ROW_H;
    snprintf(key, sizeof key, "challenges:%s.name", chm_key(id));
    i18n_t(text, sizeof text, key, NULL);
    text_draw(TS_WINDOW, x + 18, ry, text, NULL);
    snprintf(key, sizeof key, "challenges:%s.value.%d", chm_key(id), (int)C.m.v.value[id]);
    i18n_t(text, sizeof text, key, NULL);
    text_draw(TS_WINDOW, x + w - 10, ry, text, &right);
  }
  /* Start, or None Selected (dim) until a challenge has a value */
  i18n_t(text, sizeof text, chm_can_start(&C.m) ? "common:start" : "challenges:noneSelected", NULL);
  text_draw(TS_WINDOW, x + 18, y + 8 + (C.m.n + 1) * ROW_H, text, NULL);
  ui_cursor(x + 12, y + 8 + (C.m.cursor + 1) * ROW_H + 8);   /* on the challenges: the Start label is not a row of the cursor */
  /* what the highlighted challenge does at its value */
  ui_window(0, -48, 320, 48, WIN_NORMAL);
  const int id = chm_current(&C.m);
  if (id >= 0) {
    snprintf(key, sizeof key, "challenges:%s.desc.%d", chm_key(id), (int)C.m.v.value[id]);
    i18n_t(text, sizeof text, key, NULL);
  } else {
    text[0] = 0;
  }
  text_opts help = text_defaults();
  help.wrap_w = 296;
  text_draw(TS_WINDOW, 12, -39, text, &help);
}

/* A tap on a challenge's row puts the cursor on it and steps its value; a tap on Start starts */
static bool cs_touch(ui_screen* s, float x, float y) {
  const int rows = C.m.n + 2;
  const float w = 240, h = 16 + rows * ROW_H, left = 160 - w / 2, top = -48 - h - 8, uy = ui_bottom_ui_y(y);
  if (x < left || x >= left + w || uy < top + 8 + ROW_H || uy >= top + 8 + rows * ROW_H) return false;
  const int row = (int)((uy - top - 8) / ROW_H) - 1;
  if (row >= C.m.n) return cs_input(s, BTN_ACTION);
  C.m.cursor = row;
  chm_step(&C.m);
  ui_play_select();
  return true;
}

static ui_screen g_screen = {"challenge-select", UI_OVERLAY, NULL, NULL, NULL, cs_input, NULL, cs_draw, NULL, cs_touch};

void chsel_open(const chsel_args* a) {
  memset(&C, 0, sizeof C);
  C.a = *a;
  chm_init(&C.m, &a->values);
  ui_push(&g_screen);
}
