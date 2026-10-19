/* The run settings screen (see run_settings.h). */
#include "run_settings.h"

#include <stdio.h>
#include <string.h>

#include "i18n.h"
#include "ui.h"
#include "ui_text.h"
#include "ui_widgets.h"

#define ROWS 3
#define ROW_H 16.0f

/* ------------------------------------------------------------------ the screen */

static struct {
  rset_args a;
  rset_values v;
  int cursor; /* 0 Custom, 1 Game Speed, 2 Start */
} R;

static void change(int row, int dir) {
  if (row == 0) {
    R.v.custom = !R.v.custom;
  } else if (row == 1) {
    int s = R.v.speed + dir;
    if (s < RSET_SPEED_SLOW) s = RSET_SPEED_TURBO;
    if (s > RSET_SPEED_TURBO) s = RSET_SPEED_SLOW;
    R.v.speed = s;
    ui_set_game_speed(rset_time_scale(s));
  }
}

static bool rs_input(ui_screen* s, ui_button b) {
  (void)s;
  switch (b) {
    case BTN_UP:
      R.cursor = (R.cursor + ROWS - 1) % ROWS;
      ui_play_select();
      return true;
    case BTN_DOWN:
      R.cursor = (R.cursor + 1) % ROWS;
      ui_play_select();
      return true;
    case BTN_LEFT:
    case BTN_RIGHT:
      if (R.cursor == 2) return false;
      change(R.cursor, b == BTN_RIGHT ? 1 : -1);
      ui_play_select();
      return true;
    case BTN_ACTION: {
      ui_play_select();
      if (R.cursor < 2) {
        change(R.cursor, 1);
        return true;
      }
      rset_save(&R.v); /* a card that refuses only loses the remembered defaults: the run starts all the same */
      const rset_args a = R.a;
      const rset_values v = R.v;
      ui_pop();
      if (a.on_start) a.on_start(&v, a.arg);
      return true;
    }
    case BTN_CANCEL: {
      ui_play_select();
      rset_save(&R.v); /* the speed applies already: it is kept like upstream's settings are */
      const rset_args a = R.a;
      ui_pop();
      if (a.on_back) a.on_back(a.arg);
      return true;
    }
    default:
      return false;
  }
}

static void rs_draw(ui_screen* s, rr_screen which) {
  (void)s;
  if (which != RR_BOTTOM) return;
  if (ui_space(UI_PHYS_BOTTOM)) rr_rect(0, 0, 320, 240, RR_RGBA(0x10, 0x10, 0x18, 255));
  if (!ui_space(UI_BOTTOM_UI)) return;
  static const char* const SPEED_KEYS[4] = {"settings:gameSpeedSlow", "settings:gameSpeedNormal", "settings:gameSpeedFast",
                                            "settings:gameSpeedTurbo"};
  char label[ROWS][48], value[2][48];
  snprintf(label[0], sizeof label[0], "Custom"); /* the custom content's own setting: no upstream string */
  i18n_t(label[1], sizeof label[1], "settings:gameSpeed", NULL);
  i18n_t(label[2], sizeof label[2], "settings:confirm", NULL);
  i18n_t(value[0], sizeof value[0], R.v.custom ? "menu:yes" : "menu:no", NULL);
  i18n_t(value[1], sizeof value[1], SPEED_KEYS[R.v.speed - RSET_SPEED_SLOW], NULL);
  const float w = 200, h = 16 + ROWS * ROW_H, x = 160 - w / 2, y = -48 - h - 8;
  ui_window(x, y, w, h, WIN_NORMAL);
  text_opts right = text_defaults();
  right.origin_x = 1;
  for (int i = 0; i < ROWS; i++) {
    const float ry = y + 8 + i * ROW_H;
    text_draw(TS_WINDOW, x + 18, ry, label[i], NULL);
    if (i < 2) text_draw(TS_WINDOW, x + w - 10, ry, value[i], &right);
  }
  ui_cursor(x + 12, y + 8 + R.cursor * ROW_H + 8);
  /* what the highlighted row does, in the message strip */
  ui_window(0, -48, 320, 48, WIN_NORMAL);
  static const char* const HELP[ROWS] = {"A Classic run, or one with your custom content.", "How fast animations and text play.",
                                         "Choose a slot and start the run."};
  text_opts help = text_defaults();
  help.wrap_w = 296;
  text_draw(TS_WINDOW, 12, -39, HELP[R.cursor], &help);
}

static ui_screen g_screen = {"run-settings", UI_OVERLAY, NULL, NULL, NULL, rs_input, NULL, rs_draw, NULL};

void rset_open(const rset_args* a) {
  memset(&R, 0, sizeof R);
  R.a = *a;
  R.v = a->values;
  if (R.v.speed < RSET_SPEED_SLOW || R.v.speed > RSET_SPEED_TURBO) R.v.speed = RSET_SPEED_NORMAL;
  ui_push(&g_screen);
}
