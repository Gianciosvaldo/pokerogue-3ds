/* The Game Stats screen (see gamestats_screen.h). Geometry is upstream's (a 320 px wide canvas, like the bottom screen). */
#include "gamestats_screen.h"

#include <stdio.h>
#include <string.h>

#include "gamestats_model.h"
#include "i18n.h"
#include "ui.h"
#include "ui_text.h"
#include "ui_widgets.h"

#define HEADER_H 24
#define COL_W 159     /* (320 - 2) / 2: the window of a column */
#define COL_BG_W 157  /* COL_W - 2: where the second column starts */
#define STATS_BG_H 154 /* upstream's canvas is 180 px high: 180 - header - 2 */
#define ENTRY_Y0 28
#define ENTRY_H 16
#define HINT_X 10
#define HINT_Y 224

static const char* const HINTS[2] = {"B: back", "^/v: scroll"};

static struct {
  gamestats_args args;
  int cursor; /* 0..GSM_MAX_CURSOR: the first stat shown is cursor * 2 */
  gsm_row rows[GSM_COUNT];
  char abbr_store[3][8];
  const char* abbr[GSM_ABBR];
  char header[96];
  bool left;
} S;

static void build(void) {
  const uint32_t play = S.args.play_seconds ? S.args.play_seconds() : S.args.sys->gstats.play_time;
  gsm_build(S.args.sys, play, S.abbr, S.rows);
}

static bool set_cursor(int c) {
  /* upstream setCursor: refused outside 0..max and when it does not move */
  if (c < 0 || c > GSM_MAX_CURSOR || c == S.cursor) return false;
  S.cursor = c;
  build();
  return true;
}

static bool gs_input(ui_screen* s, ui_button b) {
  (void)s;
  if (S.left) return true;
  switch (b) {
    case BTN_UP:
    case BTN_DOWN:
      if (set_cursor(S.cursor + (b == BTN_DOWN ? 1 : -1))) ui_play_select();
      return true;
    case BTN_CANCEL:
      ui_play_select();
      S.left = true;
      if (S.args.on_exit) S.args.on_exit(S.args.arg);
      return true;
    default: return false;
  }
}

/* a tap on the bottom screen (physical px): the hint "B: back" leaves, the upper half of the columns scrolls up, the lower half down */
static bool gs_touch(ui_screen* s, float x, float y) {
  if (S.left) return true;
  if (y >= HINT_Y - 8) return hint_at(TS_INSTRUCTIONS_TEXT, HINT_X, HINTS, 2, x) == 0 ? gs_input(s, BTN_CANCEL) : false;
  if (y < HEADER_H || y >= HEADER_H + STATS_BG_H) return false;
  return gs_input(s, y < HEADER_H + STATS_BG_H / 2 ? BTN_UP : BTN_DOWN);
}

/* upstream's scroll arrow: the "prompt" sprite, flipped for the upper one, animated at 6 fps */
static void arrow(float x, float y, bool up) {
  char fr[4];
  snprintf(fr, sizeof fr, "%d", 1 + (int)(ui_now() * 6 / 1000) % 4);
  rr_draw_opts o = RR_DEFAULT_OPTS;
  o.origin_x = o.origin_y = 0.5f;
  if (up) o.scale_y = -1.0f;
  const rr_frame* f = img_frame("images/ui/prompt", fr);
  if (f) rr_draw(f, x, y, &o);
}

static void draw_bottom(void) {
  ui_window(0, 0, 318, HEADER_H, WIN_NORMAL);
  text_draw(TS_HEADER_LABEL, 8, 4, S.header, NULL);
  for (int i = 0; i < GSM_COLUMNS; i++) ui_window((float)(i * COL_BG_W), HEADER_H, COL_W, STATS_BG_H, WIN_NORMAL);
  for (int slot = 0; slot < GSM_PER_PAGE; slot++) {
    const int idx = S.cursor * GSM_COLUMNS + slot;
    if (idx >= GSM_COUNT) break;
    const gsm_row* r = &S.rows[idx];
    const int col = slot & 1, row = slot >> 1;
    const float y = (float)(ENTRY_Y0 + row * ENTRY_H);
    char label[96];
    if (gsm_label_hidden(r))
      snprintf(label, sizeof label, "???");
    else
      i18n_t(label, sizeof label, r->label_key, NULL);
    text_draw(TS_STATS_LABEL, (float)(8 + (col ? COL_BG_W : 0)), y, label, NULL);
    text_opts o = text_defaults();
    o.origin_x = 1.0f;
    text_draw(TS_STATS_VALUE, (float)(col ? COL_BG_W * 2 - 8 : COL_BG_W - 8), y, r->value, &o);
  }
  if (S.cursor > 0) arrow(COL_W, HEADER_H + 3, true);
  if (S.cursor < GSM_MAX_CURSOR) arrow(COL_W, 180 - 5, false);
  for (int i = 0; i < 2; i++) text_draw(TS_INSTRUCTIONS_TEXT, hint_x(TS_INSTRUCTIONS_TEXT, HINT_X, HINTS, i), HINT_Y, HINTS[i], NULL);
}

static void gs_draw(ui_screen* s, rr_screen which) {
  (void)s;
  if (which == RR_TOP) {
    if (ui_space(UI_PHYS_TOP)) rr_rect(0, 0, 400, 240, RR_RGBA(0x10, 0x10, 0x18, 255));
    return;
  }
  if (!ui_space(UI_PHYS_BOTTOM)) return;
  rr_rect(0, 0, 320, 240, RR_RGBA(0x10, 0x10, 0x18, 255));
  draw_bottom();
}

static ui_screen g_screen = {"gamestats", UI_FADE, NULL, NULL, NULL, gs_input, NULL, gs_draw, NULL, gs_touch};

void gamestats_screen_open(const gamestats_args* a) {
  memset(&S, 0, sizeof S);
  S.args = *a;
  /* the abbreviations of the money stat (common:abrThousand...) */
  static const char* const keys[3] = {"common:abrThousand", "common:abrMillion", "common:abrBillion"};
  static const char* const english[3] = {"k", "M", "B"};
  S.abbr[0] = "";
  for (int i = 0; i < 3; i++) {
    const char* raw = i18n_raw(keys[i]);
    snprintf(S.abbr_store[i], sizeof S.abbr_store[i], "%s", raw ? raw : english[i]);
    S.abbr[i + 1] = S.abbr_store[i];
  }
  char guest[48];
  i18n_t(guest, sizeof guest, "common:guest", NULL);
  i18n_t(S.header, sizeof S.header, "gameStatsUiHandler:stats", "username", guest, NULL);
  build();
  ui_set(&g_screen);
}
