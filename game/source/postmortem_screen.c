/* The post-mortem screen (see postmortem_screen.h). */
#include "postmortem_screen.h"

#include <stdio.h>
#include <string.h>

#include "gm_enum_names.h"
#include "i18n.h"
#include "run_text.h"
#include "ui.h"
#include "ui_text.h"
#include "ui_widgets.h"

#define ROW_MAX (CC_MAX_EVENTS_PER_TURN + 1)
#define ROW_LEN 96
#define VISIBLE 10       /* event rows on the bottom screen */
#define ROW_H 14
#define LIST_Y 74
#define LIST_W 296       /* the list's text is cut to this many px */

enum { TONE_NORMAL, TONE_FAINT, TONE_NOTE };

static struct {
  pm_args a;
  const co_report* rep;
  int page;                        /* index into the report's turns */
  int scroll;                      /* first visible row */
  int n_rows;
  char rows[ROW_MAX][ROW_LEN];
  uint8_t tone[ROW_MAX];
  cc_turn_summary sum;
  bool sum_ok;
  char head[48];
  char notice[2][96];
  char totals[2][96];
} P;

/* ------------------------------------------------------------------ the words */

static const char* side_word(int side) { return side == CC_SIDE_PLAYER ? "Your" : "Foe"; }

void pm_describe(const cc_event* e, char* out, size_t n) {
  const char* name = i18n_species_name(e->target_species);
  const char* side = side_word(e->target_side);
  char hp[24] = "";
  if (e->known & CC_EK_HP_AFTER) snprintf(hp, sizeof hp, " (HP %d)", (int)e->hp_after);
  switch (e->kind) {
    case CC_EV_DAMAGE:
      if (e->known & CC_EK_SHOWN)
        snprintf(out, n, "%s %.40s took %d damage%s", side, name, (int)e->shown, hp);
      else
        snprintf(out, n, "%s %.40s was hit%s", side, name, hp);
      break;
    case CC_EV_HEAL:
      if (e->known & CC_EK_SHOWN)
        snprintf(out, n, "%s %.40s recovered %d HP%s", side, name, (int)e->shown, hp);
      else
        snprintf(out, n, "%s %.40s recovered HP%s", side, name, hp);
      break;
    case CC_EV_HP_LOSS: /* only the engine's measurement; the cause is not in the event */
      if (e->known & CC_EK_HP_DELTA)
        snprintf(out, n, "%s %.40s lost %d HP, no hit shown%s", side, name, (int)(e->hp_delta < 0 ? -(int64_t)e->hp_delta : e->hp_delta), hp);
      else
        snprintf(out, n, "%s %.40s lost HP, no hit shown%s", side, name, hp);
      break;
    case CC_EV_FAINT:
      snprintf(out, n, "%s %.40s fainted", side, name);
      break;
    case CC_EV_SWITCH_OUT:
      snprintf(out, n, "%s %.40s left the field", side, name);
      break;
    case CC_EV_SWITCH_IN:
      snprintf(out, n, "%s %.40s came out", side, name);
      break;
    default:
      snprintf(out, n, "%s %s: event %d", side, name, (int)e->kind);
      break;
  }
}

/* Cuts s so that it fits `w` px in `style`, with ".." at the end. */
static void fit(char* s, text_style style, float w) {
  if (text_width(style, s, NULL) <= w) return;
  size_t len = strlen(s);
  if (len > ROW_LEN - 3) len = ROW_LEN - 3; /* room for the dots */
  while (len > 3) {
    len--;
    while (len > 0 && ((unsigned char)s[len] & 0xC0) == 0x80) len--; /* not in the middle of a UTF-8 character */
    char t[ROW_LEN];
    snprintf(t, sizeof t, "%.*s..", (int)len, s);
    if (text_width(style, t, NULL) <= w) {
      memcpy(s, t, strlen(t) + 1);
      return;
    }
  }
}

static void add_phrase(char* dst, size_t n, const char* phrase) {
  const size_t k = strlen(dst);
  snprintf(dst + k, n - k, "%s%s", k ? ", " : "", phrase);
}

/* Everything the bottom screen shows for the current page. */
static void build_page(void) {
  const cc_history* h = &P.rep->history;
  P.n_rows = 0;
  P.scroll = 0;
  P.head[0] = P.notice[0][0] = P.notice[1][0] = P.totals[0][0] = P.totals[1][0] = 0;
  P.sum_ok = false;
  if (h->n_turns == 0) return;
  const cc_turn* t = &h->turns[P.page];
  if (t->turn == 0)
    snprintf(P.head, sizeof P.head, "Wave %u, battle start", (unsigned)t->wave);
  else
    snprintf(P.head, sizeof P.head, "Wave %u, turn %u", (unsigned)t->wave, (unsigned)t->turn);

  char why[ROW_LEN] = "";
  if (t->flags & CC_TF_EVICTED) {
    char b[40];
    snprintf(b, sizeof b, "%u earlier events dropped", (unsigned)t->dropped);
    add_phrase(why, sizeof why, b);
  }
  if (t->flags & CC_TF_SOURCE_LOSS) add_phrase(why, sizeof why, "some events were not observed");
  if (t->flags & CC_TF_BEGAN_MID) add_phrase(why, sizeof why, "it began before observation");
  if (why[0]) snprintf(P.notice[0], sizeof P.notice[0], "Incomplete turn: %.70s", why);

  for (int i = 0; i < t->n_events; i++) {
    const cc_event* e = &t->events[i];
    pm_describe(e, P.rows[P.n_rows], ROW_LEN);
    fit(P.rows[P.n_rows], TS_INSTRUCTIONS_TEXT, LIST_W);
    P.tone[P.n_rows] = e->kind == CC_EV_FAINT ? TONE_FAINT : TONE_NORMAL;
    P.n_rows++;
  }
  if (P.n_rows == 0) {
    snprintf(P.rows[0], ROW_LEN, "No events were stored for this turn.");
    P.tone[0] = TONE_NOTE;
    P.n_rows = 1;
  }

  P.sum_ok = cc_turn_summarize(t, &P.sum) == CC_OK;
  if (P.sum_ok) {
    const char* lead = P.sum.exact ? "" : "At least: ";
    const cc_amounts* d = P.sum.damage;
    const cc_amounts* l = P.sum.hp_loss;
    if (d[0].n || d[1].n)
      snprintf(P.totals[0], sizeof P.totals[0], "%sdamage shown: yours %llu, foe %llu", lead, (unsigned long long)d[0].shown,
               (unsigned long long)d[1].shown);
    if (l[0].n || l[1].n)
      snprintf(P.totals[1], sizeof P.totals[1], "%sHP loss (measured): yours %llu, foe %llu", lead,
               (unsigned long long)l[0].measured, (unsigned long long)l[1].measured);
    if (!P.totals[0][0] && !P.totals[1][0]) snprintf(P.totals[0], sizeof P.totals[0], "%sno hits or HP loss stored", lead);
    if (P.totals[0][0] >= 'a' && P.totals[0][0] <= 'z') P.totals[0][0] = (char)(P.totals[0][0] - 32);
    if (P.totals[1][0] >= 'a' && P.totals[1][0] <= 'z') P.totals[1][0] = (char)(P.totals[1][0] - 32);
  }
  /* "N more above" is drawn at x 236 on the notice's line once the list scrolls, so a notice on a turn that can scroll stops short of it */
  fit(P.notice[0], TS_INSTRUCTIONS_TEXT, P.n_rows > VISIBLE ? LIST_W - 72 : LIST_W);
  fit(P.totals[0], TS_INSTRUCTIONS_TEXT, LIST_W);
  fit(P.totals[1], TS_INSTRUCTIONS_TEXT, LIST_W);
}

/* ------------------------------------------------------------------ input */

static bool pm_input(ui_screen* s, ui_button b) {
  (void)s;
  const int turns = P.rep->history.n_turns;
  switch (b) {
    case BTN_LEFT:
      if (P.page > 0) {
        P.page--;
        build_page();
        ui_play_select();
      }
      return true;
    case BTN_RIGHT:
      if (P.page < turns - 1) {
        P.page++;
        build_page();
        ui_play_select();
      }
      return true;
    case BTN_UP:
      if (P.scroll > 0) P.scroll--;
      return true;
    case BTN_DOWN:
      if (P.scroll < P.n_rows - VISIBLE) P.scroll++;
      return true;
    case BTN_ACTION:
      ui_play_select();
      if (P.a.on_exit) P.a.on_exit(PM_CONTINUE, P.a.arg);
      return true;
    case BTN_CANCEL:
      ui_play_select();
      if (P.a.on_exit) P.a.on_exit(PM_BACK, P.a.arg);
      return true;
    default:
      return false;
  }
}

/* ------------------------------------------------------------------ drawing */

static void line(float x, float y, const char* s, text_style st, rr_color color) {
  text_opts o = text_defaults();
  if (color) o.color = color;
  text_draw(st, x, y, s, &o);
}

static void draw_top(void) {
  const co_report* r = P.rep;
  if (ui_space(UI_PHYS_TOP)) rr_rect(0, 0, 400, 240, RR_RGBA(0, 0, 0, 255));
  if (!ui_space(UI_FIELD)) return;
  char name[48], key[96];
  gm_enum_lower(GM_E_BIOME_ID, r->biome, name, sizeof name);
  if (name[0]) {
    snprintf(key, sizeof key, "images/arenas/%s_bg", name);
    if (img_has(key)) img_draw(key, NULL, 0, 0, 0, 0);
  }
  rr_rect(0, 0, 320, 180, RR_RGBA(0, 0, 0, 190));
  text_opts o = text_defaults();
  o.origin_x = 0.5f;
  text_draw(TS_SUMMARY, 160, 14, "Run report", &o);

  char t[96];
  float y = 52;
  if (r->outcome == CC_OUT_CLEAR)
    snprintf(t, sizeof t, "Cleared at wave %u", (unsigned)r->wave);
  else
    snprintf(t, sizeof t, "Defeated at wave %u", (unsigned)r->wave);
  text_draw(TS_WINDOW, 160, y, t, &o);
  y += 22;
  if (r->outcome != CC_OUT_CLEAR) {
    if (r->fallen_known)
      snprintf(t, sizeof t, "Last to fall: %s", i18n_species_name(r->fallen_species));
    else
      snprintf(t, sizeof t, "Last to fall: not recorded");
    fit(t, TS_WINDOW, 300);
    text_draw(TS_WINDOW, 160, y, t, &o);
    y += 20;
    text_draw(TS_WINDOW, 160, y, "Final blow: source not recorded", &o);
    y += 20;
  }
  const bool complete = !r->untracked_loss && !r->began_mid && r->history.n_turns > 0;
  bool turns_complete = true;
  for (int i = 0; i < r->history.n_turns; i++) turns_complete &= r->history.turns[i].flags == 0;
  if (r->history.n_turns == 0)
    snprintf(t, sizeof t, "No battle events recorded");
  else if (complete && turns_complete)
    snprintf(t, sizeof t, "Last %d turns, nothing missing", r->history.n_turns);
  else
    snprintf(t, sizeof t, "Last %d turns, history incomplete", r->history.n_turns);
  text_draw(TS_WINDOW, 160, y, t, &o);
  o.color = RR_RGBA(0xa0, 0xa0, 0xa0, 255);
  text_draw(TS_INSTRUCTIONS_TEXT, 160, 156, "Read from the battle log. It names no cause.", &o);
}

static void draw_bottom(void) {
  if (!ui_space(UI_PHYS_BOTTOM)) return;
  rr_rect(0, 0, 320, 240, RR_RGBA(0x10, 0x10, 0x18, 255));
  const co_report* r = P.rep;
  ui_window(1, 1, 318, 24, WIN_NORMAL);
  if (r->history.n_turns == 0) {
    line(10, 5, "Last turns", TS_WINDOW, 0);
    line(10, 40, "No battle events were recorded for this run.", TS_WINDOW, 0);
    if (r->began_mid) line(10, 62, "The run was continued from a save.", TS_INSTRUCTIONS_TEXT, 0);
    line(10, 222, "A: continue    B: back", TS_INSTRUCTIONS_TEXT, 0);
    return;
  }
  line(10, 5, P.head, TS_WINDOW, 0);
  char pg[24];
  snprintf(pg, sizeof pg, "%s%d/%d%s", P.page > 0 ? "< " : "", P.page + 1, (int)r->history.n_turns, P.page < r->history.n_turns - 1 ? " >" : "");
  text_opts o = text_defaults();
  o.origin_x = 1.0f;
  text_draw(TS_WINDOW, 310, 5, pg, &o);
  if (P.notice[0][0]) line(10, 30, P.notice[0], TS_INSTRUCTIONS_TEXT, RR_RGBA(0xe8, 0xe8, 0xa8, 255));
  if (P.totals[0][0]) line(10, 44, P.totals[0], TS_INSTRUCTIONS_TEXT, 0);
  if (P.totals[1][0]) line(10, 58, P.totals[1], TS_INSTRUCTIONS_TEXT, 0);

  ui_window(1, LIST_Y - 4, 318, VISIBLE * ROW_H + 8, WIN_NORMAL);
  for (int i = 0; i < VISIBLE && P.scroll + i < P.n_rows; i++) {
    const int k = P.scroll + i;
    rr_color c = 0;
    if (P.tone[k] == TONE_FAINT) c = RR_RGBA(0xf8, 0x58, 0x58, 255);
    if (P.tone[k] == TONE_NOTE) c = RR_RGBA(0xe8, 0xe8, 0xa8, 255);
    line(12, LIST_Y + i * ROW_H, P.rows[k], TS_INSTRUCTIONS_TEXT, c);
  }
  char more[40];
  if (P.scroll > 0) {
    snprintf(more, sizeof more, "%d more above", P.scroll);
    line(236, 30, more, TS_INSTRUCTIONS_TEXT, RR_RGBA(0xa0, 0xa0, 0xa0, 255));
  }
  if (P.scroll + VISIBLE < P.n_rows) {
    snprintf(more, sizeof more, "%d more below", P.n_rows - P.scroll - VISIBLE);
    line(236, LIST_Y + VISIBLE * ROW_H + 6, more, TS_INSTRUCTIONS_TEXT, RR_RGBA(0xa0, 0xa0, 0xa0, 255));
  }
  line(10, 222, "A: continue   B: back   </>: turn   ^/v: scroll", TS_INSTRUCTIONS_TEXT, 0);
}

static void pm_draw(ui_screen* s, rr_screen which) {
  (void)s;
  if (which == RR_TOP)
    draw_top();
  else
    draw_bottom();
}

static ui_screen g_screen = {"postmortem", UI_FADE, NULL, NULL, NULL, pm_input, NULL, pm_draw, NULL};

bool postmortem_open(const pm_args* a) {
  if (!a || !a->report || !a->report->valid) return false;
  memset(&P, 0, sizeof P);
  P.a = *a;
  P.rep = a->report;
  P.page = P.rep->history.n_turns > 0 ? P.rep->history.n_turns - 1 : 0; /* the newest turn first: it is where the run ended */
  build_page();
  ui_set(&g_screen);
  return true;
}
