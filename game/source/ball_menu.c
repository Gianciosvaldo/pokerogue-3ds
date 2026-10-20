/* The Poke Ball list (see ball_menu.h), upstream's BallUiHandler. */
#include "ball_menu.h"

#include <stdio.h>
#include <string.h>

#include "i18n.h"
#include "ui.h"
#include "ui_text.h"
#include "ui_widgets.h"

static struct {
  int counts[BALL_TYPES];
  char names[256];  /* optionsText: the ball names and Cancel, one per line */
  char counts_text[64];
  float x, y;       /* pokeballSelectContainer */
  float w;          /* the window's width */
  int cursor;       /* kept between openings, as upstream's handler */
  void (*on_pick)(int, void*);
  void* arg;
} B;

/* getPokeballName */
static const char* const BALL_KEYS[BALL_TYPES] = {"pokeball:pokeBall", "pokeball:greatBall", "pokeball:ultraBall",
                                                  "pokeball:rogueBall", "pokeball:masterBall"};

static void close_with(int ball) {
  void (*fn)(int, void*) = B.on_pick;
  void* arg = B.arg;
  ui_pop();
  if (fn) fn(ball, arg);
}

static bool bm_input(ui_screen* s, ui_button b) {
  (void)s;
  bool success = false;
  if (b == BTN_ACTION || b == BTN_CANCEL) {
    success = true;
    if (b == BTN_ACTION && B.cursor < BALL_TYPES) {
      if (B.counts[B.cursor]) {
        /* commandPhase.handleCommand(Command.BALL, cursor): the caller decides */
        ui_play_select();
        close_with(B.cursor);
        return true;
      }
      ui_play_error(); /* (success stays true, so upstream plays the select sound too) */
    } else {
      ui_play_select();
      close_with(-1);
      return true;
    }
  } else if (b == BTN_UP) {
    B.cursor = B.cursor ? B.cursor - 1 : BALL_TYPES;
    success = true;
  } else if (b == BTN_DOWN) {
    B.cursor = B.cursor < BALL_TYPES ? B.cursor + 1 : 0;
    success = true;
  }
  if (success) ui_play_select();
  return success;
}

static void bm_draw(ui_screen* s, rr_screen which) {
  (void)s, (void)which;
  if (!ui_space(UI_BOTTOM_UI)) return;
  /* pokeballSelectBg: addWindow(0, 0, 50 + max(64, w), 32 + 480 * scale), origin (0, 1) */
  float h = 32 + 480 / 6.f, top = B.y - h;
  ui_window(B.x, top, B.w, h, WIN_NORMAL);
  text_opts o = text_defaults();
  o.line_spacing = 12; /* scale * 72 */
  o.align = 2;
  o.max_lines = 6;
  text_draw(TS_WINDOW, B.x + 42, top + 9, B.names, &o);
  o.align = 0;
  o.max_lines = 5;
  text_draw(TS_WINDOW, B.x + 18, top + 9, B.counts_text, &o);
  ui_cursor(B.x + 12, top + 15 + (6 + B.cursor * 96) / 6.f);
}

/* A tap on a row of the window picks it, as the cursor on it and A do (the last row, Cancel, closes) */
static bool bm_touch(ui_screen* s, float x, float y) {
  const float uy = ui_bottom_ui_y(y), h = 32 + 480 / 6.f, top = B.y - h;
  if (x < B.x || x >= B.x + B.w || uy < top || uy >= B.y) return false;
  const int row = (int)((uy - top - 8) / 16);
  if (row < 0 || row > BALL_TYPES) return false;
  B.cursor = row;
  return bm_input(s, BTN_ACTION);
}

static ui_screen g_screen = {"ball", UI_OVERLAY, NULL, NULL, NULL, bm_input, NULL, bm_draw, NULL, bm_touch};

void ball_menu_open(const int counts[BALL_TYPES], void (*on_pick)(int, void*), void* arg) {
  memcpy(B.counts, counts, sizeof B.counts);
  B.on_pick = on_pick;
  B.arg = arg;
  /* setup(): the names are laid out once, as upstream (the language can't change mid-battle) */
  int n = 0, c = 0;
  B.names[0] = B.counts_text[0] = 0;
  for (int i = 0; i < BALL_TYPES; i++) {
    const char* name = i18n_raw(BALL_KEYS[i]);
    n += snprintf(B.names + n, sizeof B.names - n, "%s\n", name ? name : BALL_KEYS[i]);
    if (n >= (int)sizeof B.names) n = sizeof B.names - 1;
    /* updateCounts(): "×N" per ball */
    c += snprintf(B.counts_text + c, sizeof B.counts_text - c, i ? "\n\xC3\x97%d" : "\xC3\x97%d", counts[i]);
    if (c >= (int)sizeof B.counts_text) c = sizeof B.counts_text - 1;
  }
  const char* cancel = i18n_raw("commandUiHandler:ballCancel");
  snprintf(B.names + n, sizeof B.names - n, "%s", cancel ? cancel : "Cancel");
  text_opts o = text_defaults();
  o.line_spacing = 12;
  o.align = 2;
  o.max_lines = 6;
  float tw = text_width(TS_WINDOW, B.names, &o);
  float inner = tw > 64 ? tw : 64;
  /* container at (scaledCanvas.width - 51 - max(64, w), -49) */
  B.x = 320 - 51 - inner;
  B.y = -49;
  B.w = 50 + inner;
  ui_push(&g_screen);
}
