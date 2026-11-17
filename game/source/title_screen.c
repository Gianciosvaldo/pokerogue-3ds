/* The title screen (see title_screen.h). */
#include "title_screen.h"

#include <stdio.h>
#include <string.h>

#include "bgm.h"
#include "events_host.h"
#include "events_view.h"
#include "field_border.h"
#include "gm_enum_names.h"
#include "i18n.h"
#include "ui.h"
#include "ui_text.h"
#include "ui_widgets.h"

static struct {
  title_args args;
  optlist list;
  char labels[8][48];
  const char* label_ptrs[8];
  title_choice choice[8];
  char biome[48];
} T;

static bool ts_input(ui_screen* s, ui_button b) {
  (void)s;
  if (!optlist_input(&T.list, b)) return false;
  if (T.list.chosen >= 0) {
    const title_choice c = T.choice[T.list.chosen];
    T.list.chosen = -1;
    if (T.args.on_choose) T.args.on_choose(c, T.args.arg);
  }
  return true;
}

static bool ts_touch(ui_screen* s, float x, float y) {
  (void)s;
  if (!optlist_touch(&T.list, x, ui_bottom_ui_y(y))) return false;
  if (T.list.chosen >= 0) {
    const title_choice c = T.choice[T.list.chosen];
    T.list.chosen = -1;
    if (T.args.on_choose) T.args.on_choose(c, T.args.arg);
  }
  return true;
}


/* ---- the timed events (custom-core's cc/tevents.h, through events_view.h) ---- */
#define EV_GOLD RR_RGBA(0xe8, 0xe8, 0xa8, 255)
#define EV_GREEN RR_RGBA(0x98, 0xe8, 0x98, 255)
#define EV_BLUE RR_RGBA(0x88, 0xc8, 0xf0, 255)
#define EV_GREY RR_RGBA(0xb0, 0xb0, 0xb0, 255)
#define EV_PANEL_X 4
#define EV_PANEL_W 206
#define EV_INNER_X 12
#define EV_INNER_R 202 /* the right edge of the text, where the times end */

/* Cuts s so that it fits `w` px in `style`, with ".." at the end. */
static void fit_to(char* s, size_t cap, text_style style, float w) {
  if (text_width(style, s, NULL) <= w) return;
  size_t len = strlen(s);
  if (len > cap - 3) len = cap - 3;
  while (len > 3) {
    len--;
    while (len > 0 && ((unsigned char)s[len] & 0xC0) == 0x80) len--; /* not in the middle of a UTF-8 character */
    char t[96];
    snprintf(t, sizeof t, "%.*s..", (int)len, s);
    if (text_width(style, t, NULL) <= w) {
      snprintf(s, cap, "%s", t);
      return;
    }
  }
}

static void ev_text(text_style st, float x, float y, const char* s, rr_color color, float origin_x) {
  text_opts o = text_defaults();
  o.origin_x = origin_x;
  if (color) o.color = color;
  text_draw(st, x, y, s, &o);
}

/* One event: its name on the left, the time on the right, and below them what it changes. */
static void ev_draw_row(const ev_row* r, bool on, float y) {
  char when[40], name[EV_NAME + 8], blurb[EV_BLURB + 8];
  if (on) ev_left(r->secs, when, sizeof when);
  else ev_when(r, when, sizeof when);
  const float tw = text_width(TS_INSTRUCTIONS_TEXT, when, NULL); /* the small font: "tomorrow 00:00" beside a long name */
  snprintf(name, sizeof name, "%s", r->name);
  fit_to(name, sizeof name, TS_TOOLTIP_CONTENT, EV_INNER_R - EV_INNER_X - tw - 6);
  snprintf(blurb, sizeof blurb, "%s", r->blurb);
  fit_to(blurb, sizeof blurb, TS_INSTRUCTIONS_TEXT, EV_INNER_R - EV_INNER_X);
  ev_text(TS_TOOLTIP_CONTENT, EV_INNER_X, y, name, 0, 0);
  ev_text(TS_INSTRUCTIONS_TEXT, EV_INNER_R, y + 2, when, on ? EV_GREEN : EV_BLUE, 1.0f);
  ev_text(TS_INSTRUCTIONS_TEXT, EV_INNER_X, y + 12, blurb, EV_GREY, 0);
}

static void ev_draw_label(const char* s, rr_color color, float y) {
  ev_text(TS_INSTRUCTIONS_TEXT, EV_INNER_X, y, s, color, 0);
}

#define EV_PAGE_SECONDS 5 /* each page of the panel stays this long */

/* The EVENTS window of the bottom screen (UI_BOTTOM_UI: y runs -240 at the top of the screen to 0 at its bottom), beside the option list.
 * What does not fit under the header goes on further pages that turn by the clock; "1/2" says which one this is. */
static void ev_draw_panel(const ev_view* v, int64_t now) {
  const float top = -236, h = 232, bottom = top + h - 8;
  ui_window(EV_PANEL_X, top, EV_PANEL_W, h, WIN_NORMAL);
  float y = top + 8;
  ev_text(TS_SUMMARY_GOLD, EV_INNER_X, y, "EVENTS", 0, 0);
  y += 18;
  if (!v->ok) {
    ev_text(TS_INSTRUCTIONS_TEXT, EV_INNER_X, y + 4, "The console clock cannot be read.", EV_GREY, 0);
    return;
  }
  if (!v->n_on && !v->n_next) {
    ev_text(TS_INSTRUCTIONS_TEXT, EV_INNER_X, y + 4, "No event is on, and none opens", EV_GREY, 0);
    ev_text(TS_INSTRUCTIONS_TEXT, EV_INNER_X, y + 16, "within the next day.", EV_GREY, 0);
    return;
  }
  int start[EV_PAGES_MAX];
  const int pages = ev_layout(v, (int)(bottom - y), start, EV_PAGES_MAX);
  if (pages <= 0) return;
  const int page = (int)((now / EV_PAGE_SECONDS) % pages);
  const int total = v->n_on + v->n_next;
  const int end = page + 1 < pages ? start[page + 1] : total;
  if (pages > 1) {
    char t[16];
    snprintf(t, sizeof t, "%d/%d", page + 1, pages);
    ev_text(TS_INSTRUCTIONS_TEXT, EV_INNER_R, top + 12, t, EV_GREY, 1.0f);
  }
  int group = -1;
  for (int i = start[page]; i < end; i++) {
    bool on = false;
    const ev_row* r = ev_item(v, i, &on);
    if (!r) break;
    if ((on ? 0 : 1) != group) {
      group = on ? 0 : 1;
      ev_draw_label(on ? "ON NOW" : "COMING UP", on ? EV_GREEN : EV_BLUE, y);
      y += EV_LABEL_H;
    }
    ev_draw_row(r, on, y);
    y += EV_ROW_H;
  }
}

/* The banner at the bottom of the top screen's field: the event that closes first, or the next one to open. */
static void ev_draw_banner(const ev_view* v) {
  char title[96], sub[96];
  ev_headline(v, title, sizeof title, sub, sizeof sub);
  const float x = 16, y = 128, w = 288, h = 40;
  ui_window(x, y, w, h, WIN_NORMAL);
  fit_to(title, sizeof title, TS_WINDOW, w - 20);
  fit_to(sub, sizeof sub, TS_INSTRUCTIONS_TEXT, w - 20);
  ev_text(TS_WINDOW, x + 10, y + 5, title, v->ok && v->n_on ? EV_GOLD : v->ok && v->n_next ? EV_BLUE : EV_GREY, 0);
  ev_text(TS_INSTRUCTIONS_TEXT, x + 10, y + 25, sub, EV_GREY, 0);
}

static void ts_draw(ui_screen* s, rr_screen which) {
  (void)s;
  static ev_view ev; /* ~3.5 KB: not on the 3DS's small stack */
  const int64_t now = eh_now(); /* the console's clock, read at each draw: a window that opens or closes while the title is up is seen */
  ev_build(now, &ev);
  if (which == RR_TOP) {
    if (ui_space(UI_PHYS_TOP)) rr_rect(0, 0, 400, 240, RR_RGBA(0, 0, 0, 255));
    if (ui_space(UI_FIELD)) {
      char k[96];
      snprintf(k, sizeof k, "images/arenas/%s_bg", T.biome);
      if (img_has(k))
        img_draw(k, NULL, 0, 0, 0, 0);
      else
        rr_rect(0, 0, 320, 180, RR_RGBA(0x88, 0xc8, 0xf0, 255));
      /* addImage(width / 2, 8, "logo") with origin (0.5, 0) */
      if (img_has("images/logo")) {
        img_draw("images/logo", NULL, 160, 8, 0.5f, 0);
      } else {
        /* no logo in the packs: the name in the game's own font */
        text_opts o = text_defaults();
        o.origin_x = 0.5f;
        o.font_px = 192;
        text_draw(TS_SUMMARY, 160, 24, "PokeRogue", &o);
      }
      ev_draw_banner(&ev);
    }
    /* the field is 320x180 on a 400x240 screen: the biome's background widened over the bands around it */
    if (ui_space(UI_PHYS_TOP)) {
      char k[96];
      snprintf(k, sizeof k, "images/arenas/%s_bg", T.biome);
      if (img_has(k)) field_border_draw(k);
    }
  } else {
    if (ui_space(UI_PHYS_BOTTOM)) rr_rect(0, 0, 320, 240, RR_RGBA(0x10, 0x10, 0x18, 255));
    if (ui_space(UI_BOTTOM_UI)) {
      ev_draw_panel(&ev, now);
      optlist_draw(&T.list);
    }
  }
}

/* TitlePhase.start: the title music (the slot list keeps it playing) */
static void ts_enter(ui_screen* s) {
  (void)s;
  bgm_on_title();
}

static ui_screen g_screen = {"title", UI_FADE, ts_enter, NULL, NULL, ts_input, NULL, ts_draw, NULL, ts_touch};

void title_screen_open(const title_args* a) {
  memset(&T, 0, sizeof T);
  T.args = *a;
  int n = 0;
  if (a->can_continue) {
    i18n_t(T.labels[n], sizeof T.labels[n], "menu:continue", NULL);
    T.choice[n++] = TITLE_CONTINUE;
  }
  i18n_t(T.labels[n], sizeof T.labels[n], "menu:newGame", NULL);
  T.choice[n++] = TITLE_NEW_GAME;
  i18n_t(T.labels[n], sizeof T.labels[n], "menu:loadGame", NULL);
  T.choice[n++] = TITLE_LOAD_GAME;
  i18n_t(T.labels[n], sizeof T.labels[n], "menu:runHistory", NULL);
  T.choice[n++] = TITLE_HISTORY;
  snprintf(T.labels[n], sizeof T.labels[n], "Nemesis Radar"); /* the custom content's own entry: no upstream string */
  T.choice[n++] = TITLE_RADAR;
  snprintf(T.labels[n], sizeof T.labels[n], "Capsule Machine"); /* the custom content's own entry: no upstream string */
  T.choice[n++] = TITLE_CAPSULE;
  snprintf(T.labels[n], sizeof T.labels[n], "Achievements"); /* the custom content's own entry: no upstream string */
  T.choice[n++] = TITLE_ACHIEVEMENTS;
  snprintf(T.labels[n], sizeof T.labels[n], "Football Playoff");
  T.choice[n++] = TITLE_FOOTBALL;
  for (int i = 0; i < n; i++) T.label_ptrs[i] = T.labels[i];
  optlist_init(&T.list, T.label_ptrs, n);
  T.list.no_cancel = true; /* blockCancelButton: true */
  gm_enum_lower(GM_E_BIOME_ID, a->biome, T.biome, sizeof T.biome); /* "" for -1 or an unknown biome */
  if (!T.biome[0]) snprintf(T.biome, sizeof T.biome, "plains");
  ui_set(&g_screen);
}
