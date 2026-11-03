/* The run settings screen (see run_settings.h). */
#include "run_settings.h"

#include <stdio.h>
#include <string.h>

#include "i18n.h"
#include "ui.h"
#include "ui_text.h"
#include "ui_widgets.h"

#define ROWS 4
#define ROW_H 16.0f

/* ------------------------------------------------------------------ the screen */

static struct {
  rset_args a;
  rset_values v;
  int cursor; /* 0 Custom, 1 Mode, 2 Game Speed, 3 Start */
} R;

static int available_modes(int out[3]) {
  int n=0;out[n++]=GAME_MODES_CLASSIC;
  if(!R.v.custom&&R.a.endless_unlocked)out[n++]=GAME_MODES_ENDLESS;
  if(!R.v.custom&&R.a.spliced_endless_unlocked)out[n++]=GAME_MODES_SPLICED_ENDLESS;
  return n;
}

static void change(int row, int dir) {
  if (row == 0) {
    R.v.custom = !R.v.custom;
    if(R.v.custom)R.v.mode=GAME_MODES_CLASSIC;
  } else if (row == 1) {
    int modes[3],n=available_modes(modes),current=0;
    for(int i=0;i<n;i++)if(modes[i]==R.v.mode)current=i;
    current=(current+dir+n)%n;R.v.mode=modes[current];
  } else if (row == 2) {
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
      if (R.cursor == 3) return false;
      change(R.cursor, b == BTN_RIGHT ? 1 : -1);
      ui_play_select();
      return true;
    case BTN_ACTION: {
      ui_play_select();
      if (R.cursor < 3) {
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
  char label[ROWS][48], value[3][48];
  snprintf(label[0], sizeof label[0], "Custom"); /* the custom content's own setting: no upstream string */
  snprintf(label[1], sizeof label[1], "Mode");
  i18n_t(label[2], sizeof label[2], "settings:gameSpeed", NULL);
  i18n_t(label[3], sizeof label[3], "settings:confirm", NULL);
  i18n_t(value[0], sizeof value[0], R.v.custom ? "menu:yes" : "menu:no", NULL);
  static const char* const MODE_KEYS[] = {"gameMode:classic","gameMode:endless","gameMode:endlessSpliced"};
  i18n_t(value[1], sizeof value[1], MODE_KEYS[R.v.mode], NULL);
  i18n_t(value[2], sizeof value[2], SPEED_KEYS[R.v.speed - RSET_SPEED_SLOW], NULL);
  const float w = 200, h = 16 + ROWS * ROW_H, x = 160 - w / 2, y = -48 - h - 8;
  ui_window(x, y, w, h, WIN_NORMAL);
  text_opts right = text_defaults();
  right.origin_x = 1;
  for (int i = 0; i < ROWS; i++) {
    const float ry = y + 8 + i * ROW_H;
    text_draw(TS_WINDOW, x + 18, ry, label[i], NULL);
    if (i < 3) text_draw(TS_WINDOW, x + w - 10, ry, value[i], &right);
  }
  ui_cursor(x + 12, y + 8 + R.cursor * ROW_H + 8);
  /* what the highlighted row does, in the message strip */
  ui_window(0, -48, 320, 48, WIN_NORMAL);
  const char* const HELP[ROWS] = {"A Classic run, or one with your custom content.",
    R.v.custom ? "Custom runs use Classic rules." : R.a.endless_unlocked ? "Choose Classic, Endless, or Spliced Endless." : "Endless unlocks after a Classic win.",
    "How fast animations and text play.", "Choose a slot and start the run."};
  text_opts help = text_defaults();
  help.wrap_w = 296;
  text_draw(TS_WINDOW, 12, -39, HELP[R.cursor], &help);
}

/* A tap on a row is the cursor on it and A: the toggles change, the last row starts the run */
static bool rs_touch(ui_screen* s, float x, float y) {
  const float w = 200, h = 16 + ROWS * ROW_H, left = 160 - w / 2, top = -48 - h - 8, uy = ui_bottom_ui_y(y);
  if (x < left || x >= left + w || uy < top + 8 || uy >= top + 8 + ROWS * ROW_H) return false;
  R.cursor = (int)((uy - top - 8) / ROW_H);
  return rs_input(s, BTN_ACTION);
}

static ui_screen g_screen = {"run-settings", UI_OVERLAY, NULL, NULL, NULL, rs_input, NULL, rs_draw, NULL, rs_touch};

void rset_open(const rset_args* a) {
  memset(&R, 0, sizeof R);
  R.a = *a;
  R.v = a->values;
  if(R.v.mode< GAME_MODES_CLASSIC || R.v.mode>GAME_MODES_SPLICED_ENDLESS)R.v.mode=GAME_MODES_CLASSIC;
  if(R.v.mode==GAME_MODES_ENDLESS&&!a->endless_unlocked)R.v.mode=GAME_MODES_CLASSIC;
  if(R.v.mode==GAME_MODES_SPLICED_ENDLESS&&!a->spliced_endless_unlocked)R.v.mode=a->endless_unlocked?GAME_MODES_ENDLESS:GAME_MODES_CLASSIC;
  if (R.v.speed < RSET_SPEED_SLOW || R.v.speed > RSET_SPEED_TURBO) R.v.speed = RSET_SPEED_NORMAL;
  ui_push(&g_screen);
}
