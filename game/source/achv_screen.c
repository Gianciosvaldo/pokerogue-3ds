/* The custom Achievements list (see achv_screen.h). */
#include "achv_screen.h"

#include <stdio.h>
#include <string.h>

#include "ui.h"
#include "ui_text.h"
#include "ui_widgets.h"

#define ROW_H 26
#define VISIBLE 7
#define LIST_Y 30
#define HINT_X 10
#define HINT_Y 224
#define TEXT_W 296

static const char* const HINTS[3] = {"B: back", "^/v: choose", "</>: page"};

static struct {
  achv_args args;
  int sel, scroll;
  bool left;
} A;

static void select_to(int i)
{
  if (i < 0) i = 0;
  if (i >= GC_ACHV_COUNT) i = GC_ACHV_COUNT - 1;
  if (i == A.sel) return;
  A.sel = i;
  if (A.sel < A.scroll) A.scroll = A.sel;
  if (A.sel >= A.scroll + VISIBLE) A.scroll = A.sel - VISIBLE + 1;
  ui_play_select();
}

static bool as_input(ui_screen* s, ui_button b)
{
  (void)s;
  if (A.left) return true;
  switch (b) {
    case BTN_UP: select_to(A.sel - 1); return true;
    case BTN_DOWN: select_to(A.sel + 1); return true;
    case BTN_LEFT: select_to(A.sel - VISIBLE); return true;
    case BTN_RIGHT: select_to(A.sel + VISIBLE); return true;
    case BTN_CANCEL:
      ui_play_select();
      A.left = true;
      if (A.args.on_exit) A.args.on_exit(A.args.arg);
      return true;
    default: return false;
  }
}

/* a tap on the bottom screen (physical px): the hint "B: back" leaves, a row points at it */
static bool as_touch(ui_screen* s, float x, float y)
{
  if (A.left) return true;
  if (y >= HINT_Y - 8) return hint_at(TS_INSTRUCTIONS_TEXT, HINT_X, HINTS, 3, x) == 0 ? as_input(s, BTN_CANCEL) : false;
  for (int i = 0; i < VISIBLE && A.scroll + i < GC_ACHV_COUNT; i++) {
    const float top = LIST_Y + 4 + i * ROW_H;
    if (x < 4 || x >= 316 || y < top || y >= top + ROW_H) continue;
    select_to(A.scroll + i);
    return true;
  }
  return false;
}

/* "Done", "3/5" or "" for an achievement */
static void state_text(int id, char* out, int n)
{
  uint32_t have, need;
  if (gc_achv_unlocked(A.args.sys, id))
    snprintf(out, (size_t)n, "Done");
  else if (gc_achv_progress_of(A.args.sys, id, &have, &need))
    snprintf(out, (size_t)n, "%u/%u", (unsigned)have, (unsigned)need);
  else
    out[0] = 0;
}

static int unlocked_count(void)
{
  int c = 0;
  for (int i = 0; i < GC_ACHV_COUNT; i++) c += gc_achv_unlocked(A.args.sys, i);
  return c;
}

static void draw_top(void)
{
  if (ui_space(UI_PHYS_TOP)) rr_rect(0, 0, 400, 240, RR_RGBA(0x10, 0x10, 0x18, 255));
  if (!ui_space(UI_FIELD)) return;
  const gc_achv_def* d = &gc_achv_defs[A.sel];
  char t[160];
  ui_window(8, 8, 304, 164, WIN_NORMAL);
  text_draw(TS_WINDOW, 20, 18, d->name, NULL);
  text_opts o = text_defaults();
  o.wrap_w = TEXT_W - 24;
  char wrapped[320];
  text_wrap(TS_INSTRUCTIONS_TEXT, d->desc, &o, wrapped, sizeof wrapped);
  text_draw(TS_INSTRUCTIONS_TEXT, 20, 44, wrapped, &o);
  snprintf(t, sizeof t, "Reward: %s", gc_achv_reward_name(d));
  text_draw(TS_INSTRUCTIONS_TEXT, 20, 96, t, NULL);
  char st[48];
  state_text(A.sel, st, sizeof st);
  if (gc_achv_unlocked(A.args.sys, A.sel))
    snprintf(t, sizeof t, "Done");
  else if (st[0])
    snprintf(t, sizeof t, "Progress: %s", st);
  else
    snprintf(t, sizeof t, "Not done yet");
  text_draw(TS_INSTRUCTIONS_TEXT, 20, 118, t, NULL);
}

static void draw_row(int id, float y)
{
  const bool sel = id == A.sel, done = gc_achv_unlocked(A.args.sys, id);
  if (sel) {
    rr_rect(4, y - 2, 312, ROW_H - 2, RR_RGBA(255, 255, 255, 30));
    ui_cursor(11, y + 10);
  }
  const gc_achv_def* d = &gc_achv_defs[id];
  text_opts o = text_defaults();
  if (done) o.color = RR_RGBA(0x90, 0x90, 0x90, 255);
  text_draw(TS_WINDOW, 22, y - 1, d->name, &o);
  char t[96];
  snprintf(t, sizeof t, "%s", gc_achv_reward_name(d));
  text_opts r = text_defaults();
  r.color = RR_RGBA(0xa8, 0xa8, 0xa8, 255);
  text_draw(TS_INSTRUCTIONS_TEXT, 22, y + 11, t, &r);
  state_text(id, t, sizeof t);
  text_opts e = text_defaults();
  e.origin_x = 1.0f;
  if (done) e.color = RR_RGBA(0xe8, 0xe8, 0xa8, 255);
  text_draw(TS_WINDOW, 308, y + 4, t, &e);
}

static void draw_bottom(void)
{
  char t[64];
  ui_window(1, 1, 318, 24, WIN_NORMAL);
  text_draw(TS_WINDOW, 10, 5, "Achievements", NULL);
  snprintf(t, sizeof t, "%d/%d", unlocked_count(), GC_ACHV_COUNT);
  text_opts o = text_defaults();
  o.origin_x = 1.0f;
  text_draw(TS_WINDOW, 310, 5, t, &o);
  ui_window(1, LIST_Y, 318, VISIBLE * ROW_H + 10, WIN_NORMAL);
  for (int i = 0; i < VISIBLE && A.scroll + i < GC_ACHV_COUNT; i++) draw_row(A.scroll + i, (float)(LIST_Y + 6 + i * ROW_H));
  for (int i = 0; i < 3; i++) text_draw(TS_INSTRUCTIONS_TEXT, hint_x(TS_INSTRUCTIONS_TEXT, HINT_X, HINTS, i), HINT_Y, HINTS[i], NULL);
}

static void as_draw(ui_screen* s, rr_screen which)
{
  (void)s;
  if (which == RR_TOP) {
    draw_top();
    return;
  }
  if (!ui_space(UI_PHYS_BOTTOM)) return;
  rr_rect(0, 0, 320, 240, RR_RGBA(0x10, 0x10, 0x18, 255));
  draw_bottom();
}

static ui_screen g_screen = {"achievements", UI_FADE, NULL, NULL, NULL, as_input, NULL, as_draw, NULL, as_touch};

void achv_screen_open(const achv_args* a)
{
  memset(&A, 0, sizeof A);
  A.args = *a;
  ui_set(&g_screen);
}
