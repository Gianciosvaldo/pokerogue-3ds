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
/* the Mode row's name for the port's own mode (run_settings.h's RSET_MODE_BIZARRE) */
#define BIZARRE_NAME "Classic (Bizarre)"

/* ------------------------------------------------------------------ the screen */

static struct {
  rset_args a;
  rset_values v;
  int cursor; /* 0 Mode, 1 Game Speed, 2 Start */
} R;

/* The Mode row's values: Classic, then Classic (Bizarre) beside it, then upstream's Challenge and Endless modes, each once unlocked (upstream's
 * title phase lists Challenge with Endless: both come with Unlockables.ENDLESS_MODE) */
static int available_modes(int out[5]) {
  int n=0;out[n++]=GAME_MODES_CLASSIC;
  if(R.a.bizarre_unlocked)out[n++]=RSET_MODE_BIZARRE;
  if(R.a.endless_unlocked)out[n++]=GAME_MODES_CHALLENGE;
  if(R.a.endless_unlocked)out[n++]=GAME_MODES_ENDLESS;
  if(R.a.spliced_endless_unlocked)out[n++]=GAME_MODES_SPLICED_ENDLESS;
  return n;
}

static void change(int row, int dir) {
  if (row == 0) {
    int modes[5],n=available_modes(modes),current=0;
    for(int i=0;i<n;i++)if(modes[i]==R.v.mode)current=i;
    current=(current+dir+n)%n;R.v.mode=modes[current];
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
  char label[ROWS][48], value[2][48];
  snprintf(label[0], sizeof label[0], "Mode");
  i18n_t(label[1], sizeof label[1], "settings:gameSpeed", NULL);
  i18n_t(label[2], sizeof label[2], "settings:confirm", NULL);
  static const char* const MODE_KEYS[] = {"gameMode:classic","gameMode:endless","gameMode:endlessSpliced","gameMode:dailyRun","gameMode:challenge"};
  if (R.v.mode == RSET_MODE_BIZARRE)
    snprintf(value[0], sizeof value[0], "%s", BIZARRE_NAME); /* the port's own mode: no upstream string */
  else
    i18n_t(value[0], sizeof value[0], MODE_KEYS[R.v.mode], NULL);
  i18n_t(value[1], sizeof value[1], rset_speed_key(R.v.speed), NULL);
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
  const char* const HELP[ROWS] = {
    !R.a.bizarre_unlocked ? "Classic Pokemon with Nemesis rematches. More modes unlock after a Classic win."
    : R.v.mode == RSET_MODE_BIZARRE ? "Classic rules, with JoJo trainers and the Pillar Men on top."
    : R.v.mode == GAME_MODES_CLASSIC ? "Classic rules with Nemesis rematches. Left and Right pick the mode."
    : R.v.mode == GAME_MODES_CHALLENGE ? "Classic with challenge modifiers, upstream's rules. No rematches."
    : "Endless modes have no Nemesis rematches.",
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
  if(R.v.mode< GAME_MODES_CLASSIC || R.v.mode>RSET_MODE_BIZARRE || R.v.mode==GAME_MODES_DAILY)R.v.mode=GAME_MODES_CLASSIC;
  if(R.v.mode==RSET_MODE_BIZARRE&&!a->bizarre_unlocked)R.v.mode=GAME_MODES_CLASSIC;
  if((R.v.mode==GAME_MODES_ENDLESS||R.v.mode==GAME_MODES_CHALLENGE)&&!a->endless_unlocked)R.v.mode=GAME_MODES_CLASSIC;
  if(R.v.mode==GAME_MODES_SPLICED_ENDLESS&&!a->spliced_endless_unlocked)R.v.mode=a->endless_unlocked?GAME_MODES_ENDLESS:GAME_MODES_CLASSIC;
  if (R.v.speed < RSET_SPEED_SLOW || R.v.speed > RSET_SPEED_TURBO) R.v.speed = RSET_SPEED_NORMAL;
  ui_push(&g_screen);
}
