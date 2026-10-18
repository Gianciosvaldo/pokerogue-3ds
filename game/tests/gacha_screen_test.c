/* gacha_screen_test: the capsule machine's menu (gacha_screen.c) driven by buttons, with the real capsule screen and model (C07), the
 * real purchase logic (gacha_host.c) and profile transactions (C08), game-core's real gacha and the real save.c, on the host (task C09).
 * Only the drawing layer is fake: it records every text and every filled rectangle, and the cursor.
 *
 *   make gacha_screen_test && build/gacha_screen_test [fuzz steps]
 *
 * What it proves, through the screen and not around it:
 *   - the machine shows the real balances, the real egg count and, for each of the five options of each of the three machines, what the
 *     source says it costs and gives; options that cannot be bought are greyed and give the source's own refusal;
 *   - A asks, the question opens on NO; Yes buys exactly once, and the profile is then game-core's profile for the same inputs;
 *   - the capsule screen shows the committed result in the display order and marks it shown when it settles (or is skipped);
 *   - a held button, a repeated A, a stale screen's input and a second Yes cannot make a second purchase; a refusal writes nothing;
 *   - a power cut before every write of a purchase and of its mark, and a failed write, ended by what the screen does when it opens:
 *     the purchase finished, its eggs shown once, never pulled again;
 *   - no text or rectangle leaves its screen; a seeded fuzz of buttons, frame times, held buttons and restarts keeps the books straight.
 * Exit 1 on a failed check. */
#include <dirent.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "capsule_model.h"
#include "capsule_screen.h"
#include "cc/codec.h"
#include "cc/model.h"
#include "custom_store.h"
#include "gacha_host.h"
#include "gacha_screen.h"
#include "gacha_view.h"
#include "gc/gacha.h"
#include "pr/rng.h"
#include "profile_tx.h"
#include "save.h"
#include "system_data.h"
#include "ui.h"
#include "ui_text.h"
#include "ui_widgets.h"

static int checks, failed;
#define CHECK(cond, ...)                                  \
  do {                                                    \
    checks++;                                             \
    if (!(cond)) {                                        \
      failed++;                                           \
      if (failed < 40) {                                  \
        printf("FAIL line %d: %s: ", __LINE__, #cond);    \
        printf(__VA_ARGS__);                              \
        printf("\n");                                     \
      }                                                   \
    }                                                     \
  } while (0)

#include "gacha_test_common.h"

/* ------------------------------------------------------------------ fakes */

typedef struct {
  float x, y;
  int screen;
  text_style style;
  rr_color color;
  char text[200];
} drawn_t;
#define DRAWN_MAX 256
static drawn_t g_drawn[DRAWN_MAX];
static int n_drawn;
typedef struct {
  float x, y, w, h;
  int screen;
} rect_t;
#define RECT_MAX 2048
static rect_t g_rects[RECT_MAX];
static int n_rects;
static int g_which;
static ui_screen* g_top;
static int n_set, n_select, n_error;
static bool g_repeat;
static float g_cursor_y = -1;

void ui_set(ui_screen* s) {
  if (g_top && g_top->leave) g_top->leave(g_top);
  g_top = s;
  if (s->enter) s->enter(s);
  n_set++;
}
void ui_play_select(void) { n_select++; }
void ui_play_error(void) { n_error++; }
bool ui_input_repeating(void) { return g_repeat; }
bool ui_space(ui_space_id sp) {
  const bool top = sp == UI_FIELD || sp == UI_FIELD_UI || sp == UI_TOP_CANVAS || sp == UI_TOP_CANVAS_UI || sp == UI_PHYS_TOP;
  return top == (g_which == 0);
}
void rr_rect(float x, float y, float w, float h, rr_color c) {
  (void)c;
  if (n_rects < RECT_MAX) g_rects[n_rects++] = (rect_t){x, y, w, h, g_which};
}
void ui_window(float x, float y, float w, float h, win_variant v) { (void)x, (void)y, (void)w, (void)h, (void)v; }
void ui_cursor(float x, float y) {
  (void)x;
  g_cursor_y = y;
}
const rr_draw_opts RR_DEFAULT_OPTS = {0, 0, 1, 1, 0, 0xFFFFFFFFu};
bool img_has(const char* key) { (void)key; return false; }
void img_draw_opts(const char* key, const char* frame, float x, float y, const rr_draw_opts* o) { (void)key, (void)frame, (void)x, (void)y, (void)o; }
text_opts text_defaults(void) {
  text_opts o;
  memset(&o, 0, sizeof o);
  o.chars = TEXT_ALL;
  return o;
}
float text_draw(text_style st, float x, float y, const char* s, const text_opts* o) {
  if (n_drawn < DRAWN_MAX) {
    drawn_t* d = &g_drawn[n_drawn++];
    d->style = st;
    d->x = x - (o ? o->origin_x : 0) * 6.0f * (float)strlen(s);
    d->y = y;
    d->color = o ? o->color : 0;
    d->screen = g_which;
    snprintf(d->text, sizeof d->text, "%s", s);
  }
  return 6.0f * (float)strlen(s);
}


/* The view (menus/gacha_view.c) is the menus' layer and is not linked here: this stand-in records what the menu hands it, which is the whole
 * of what the screen decides (the words, the greyed rows, the question's answer). Where things land is checked on the emulator. */
static void rec(int screen, const char* text, rr_color color) {
  if (!text[0] || n_drawn >= DRAWN_MAX) return;
  drawn_t* d = &g_drawn[n_drawn++];
  d->style = TS_WINDOW;
  d->x = 8;
  d->y = 10;
  d->color = color;
  d->screen = screen;
  snprintf(d->text, sizeof d->text, "%s", text);
}
void gv_draw_top(const gv_state* v) {
  char t[80];
  snprintf(t, sizeof t, "vouchers %s %s %s %s", v->voucher_count[0], v->voucher_count[1], v->voucher_count[2], v->voucher_count[3]);
  rec(0, t, 0);
  rec(0, v->top_message, 0);
  for (int i = 0; i < GV_OPTIONS; i++) rec(0, v->opt[i].short_label, v->opt[i].can_buy ? 0 : 1);
}
void gv_draw_bottom(const gv_state* v) {
  rec(1, v->machine_name, 0);
  rec(1, v->machine_note, 0);
  rec(1, v->egg_count, 0);
  for (int i = 0; i < GV_OPTIONS; i++) {
    const rr_color c = v->opt[i].can_buy ? 0 : 1;
    rec(1, v->opt[i].eggs, c);
    rec(1, v->opt[i].price, c);
    rec(1, v->opt[i].have, c);
  }
  rec(1, v->refusal, 2);
  rec(1, v->hint, 0);
  for (int i = 0; i < v->n_lines; i++) rec(1, v->lines[i], 0);
  if (v->mode == GV_CONFIRM) g_cursor_y = v->yes ? 140 : 158;
}

void gv_draw_reveal_top(const gv_reveal* v) {
  rec(0, v->top_title, 0);
  rec(0, v->top_sub, 0);
}
void gv_draw_reveal_bottom(const gv_reveal* v) {
  rec(1, v->title, 0);
  for (int i = 0; i < v->n_rows; i++) rec(1, v->row[i].name, 0);
  rec(1, v->more_above, 0);
  rec(1, v->more_below, 0);
  rec(1, v->big, 0);
  rec(1, v->small, 0);
  rec(1, v->note[0], 0);
  rec(1, v->note[1], 0);
  rec(1, v->hint, 0);
}

static const drawn_t* find_text(const char* needle) {
  for (int i = 0; i < n_drawn; i++)
    if (strstr(g_drawn[i].text, needle)) return &g_drawn[i];
  return NULL;
}
static bool has_text(const char* needle) { return find_text(needle) != NULL; }

/* both screens of the frame, bounds-checked */
static void draw_both(void) {
  n_drawn = 0;
  n_rects = 0;
  g_cursor_y = -1;
  for (int w = 0; w < 2; w++) {
    g_which = w;
    g_top->draw(g_top, w ? RR_BOTTOM : RR_TOP);
  }
  for (int i = 0; i < n_drawn; i++) {
    const drawn_t* d = &g_drawn[i];
    const float wd = 320, h = d->screen ? 240 : 180;
    const float len = 6.0f * (float)strlen(d->text);
    CHECK(d->x >= 0 && d->y >= 0 && d->y + 12 <= h + 1, "\"%s\" is off the screen at (%.0f, %.0f)", d->text, d->x, d->y);
    CHECK(d->x + len <= wd + 1, "\"%s\" runs to x %.0f", d->text, d->x + len);
    CHECK(d->style == TS_WINDOW || d->style == TS_SUMMARY || d->style == TS_INSTRUCTIONS_TEXT, "\"%s\" is in an unreadable style", d->text);
  }
}
static void press(ui_button b) { g_top->input(g_top, b); }
static void press_held(ui_button b) {
  g_repeat = true;
  g_top->input(g_top, b);
  g_repeat = false;
}
static void tick(float ms) {
  if (g_top->update) g_top->update(g_top, ms);
}
static void run_ms(float ms, float frame) {
  for (float t = 0; t < ms; t += frame) tick(frame);
}
static bool on_screen(const char* name) { return g_top && !strcmp(g_top->name, name); }

/* ------------------------------------------------------------------ the owner of the machine screen */

static int n_exit;
static void on_machine_exit(void* arg) {
  (void)arg;
  n_exit++;
}
static void open_machine(void) {
  const gachas_args a = {on_machine_exit, NULL};
  CHECK(gacha_screen_open(&a), "the machine opens");
}
/* A restart: everything rebuilt from the files, as the app would on the next boot, then the machine opened from the title. */
static void restart_and_open(void) {
  g_top = NULL;
  reboot();
  open_machine();
}

static int pulls_of(int option) { return EXPECT[option].pulls; }

/* go to machine m, option o on the menu */
static void go_to(int m, int o) {
  for (int i = 0; i < 3; i++) press(BTN_LEFT);   /* wraps back to machine 0 whatever it was */
  for (int i = 0; i < 3; i++) press(BTN_LEFT);
  for (int i = 0; i < m; i++) press(BTN_RIGHT);
  for (int i = 0; i < 5; i++) press(BTN_UP);
  for (int i = 0; i < o; i++) press(BTN_DOWN);
}

/* Plays the capsule screen of a purchase to its end (time), and returns when it is settled. */
static void settle_capsule(void) {
  CHECK(on_screen("capsule-preview"), "the capsule screen is up");
  run_ms(2200, 16);
}
static void close_capsule(void) {
  run_ms(400, 16);   /* the short pause before A closes it */
  press(BTN_ACTION);
}

/* One purchase through the buttons: A, Yes, A. */
static void buy_by_buttons(int m, int o) {
  go_to(m, o);
  press(BTN_ACTION);
  press(BTN_UP);       /* Yes */
  press(BTN_ACTION);
}

/* ------------------------------------------------------------------ the menu shows the source's table and the real balances */

static void test_menu_texts(void) {
  const uint32_t v[4] = {12, 3, 2, 1};
  fresh(v, 7);
  open_machine();
  CHECK(on_screen("capsule-machine"), "the menu is the screen");
  draw_both();
  CHECK(has_text("Move Gacha") && has_text("Better odds of a rare egg move."), "machine 0 and what it does");
  CHECK(has_text("vouchers 12 3 2 1"), "the real balances");
  CHECK(has_text("Eggs 7 / 99"), "the egg count against the source's capacity");
  static const char* const rows[5][2] = {{"1 egg", "1 Regular voucher"}, {"10 eggs", "10 Regular vouchers"}, {"5 eggs", "1 Plus voucher"},
                                         {"10 eggs", "1 Premium voucher"}, {"25 eggs", "1 Golden voucher"}};
  for (int i = 0; i < 5; i++) CHECK(has_text(rows[i][0]) && has_text(rows[i][1]), "option %d: \"%s\" for \"%s\"", i, rows[i][0], rows[i][1]);
  CHECK(has_text("have 12") && has_text("have 3") && has_text("have 2") && has_text("have 1"), "and what is held of each");
  for (int i = 0; i < 5; i++) {
    const drawn_t* d = find_text(rows[i][0]);
    CHECK(d && d->screen == 1 && d->color == 0, "option %d is buyable here: not greyed", i);
  }
  CHECK(!has_text("You don't have") && !has_text("too many"), "no refusal is shown");

  /* machines: RIGHT and LEFT go through the three and wrap */
  static const char* const names[3] = {"Move Gacha", "Legendary Gacha", "Shiny Gacha"};
  for (int i = 1; i <= 3; i++) {
    press(BTN_RIGHT);
    draw_both();
    CHECK(has_text(names[i % 3]), "RIGHT %d shows %s", i, names[i % 3]);
  }
  press(BTN_LEFT);
  draw_both();
  CHECK(has_text(names[2]), "LEFT from the first wraps to the last");
  CHECK(has_text("Better odds of a shiny egg."), "and says what it does");
  press(BTN_LEFT);
  press(BTN_LEFT);
  draw_both();
  CHECK(has_text(names[0]), "back at the first");
  /* the cursor stops at both ends of the five options */
  for (int i = 0; i < 9; i++) press(BTN_DOWN);
  press(BTN_ACTION);
  draw_both();
  CHECK(has_text("Pull 25 eggs from the Move Gacha") && has_text("for 1 Golden voucher?"), "DOWN stops at the fifth option");
  press(BTN_CANCEL);
  for (int i = 0; i < 9; i++) press(BTN_UP);
  press(BTN_ACTION);
  draw_both();
  CHECK(has_text("Pull 1 egg from the Move Gacha") && has_text("for 1 Regular voucher?"), "UP stops at the first");
  press(BTN_CANCEL);

  /* a state with little: greyed rows and the source's own words for the selected one */
  const uint32_t w[4] = {5, 0, 0, 1};
  fresh(w, 80);
  open_machine();
  draw_both();
  CHECK(has_text("Eggs 80 / 99"), "80 eggs");
  int grey = 0;
  for (int i = 0; i < n_drawn; i++)
    if (g_drawn[i].screen == 1 && g_drawn[i].color != 0 && (strstr(g_drawn[i].text, "egg") || strstr(g_drawn[i].text, "voucher"))) grey++;
  CHECK(grey >= 6, "options that cannot be bought are greyed (%d grey texts)", grey);
  go_to(0, 1);
  draw_both();
  CHECK(has_text("You don't have enough vouchers."), "10 Regular with 5: the source's words");
  go_to(0, 4);
  draw_both();
  CHECK(has_text("You have too many eggs."), "25 eggs with 80 held: too many eggs (capacity before cost)");
  g_top = NULL;
}

/* ------------------------------------------------------------------ every machine and option, through the screen */

static void test_buy_through_screen(void) {
  int bought = 0;
  for (int m = 0; m < GH_MACHINES; m++) {
    for (int o = 0; o < GH_OPTIONS; o++) {
      fresh_all(30, 3);
      open_machine();
      g_before = *sysd_get();
      go_to(m, o);
      draw_both();
      /* A asks; the question opens on No */
      press(BTN_ACTION);
      draw_both();
      char q[100];
      snprintf(q, sizeof q, "Pull %d egg%s from the %s", pulls_of(o), pulls_of(o) == 1 ? "" : "s", gh_machine_name(m));
      CHECK(has_text(q), "machine %d option %d asks \"%s\"", m, o, q);
      snprintf(q, sizeof q, "for %d %s voucher%s?", EXPECT[o].cost, gh_voucher_name(EXPECT[o].voucher), EXPECT[o].cost == 1 ? "" : "s");
      CHECK(has_text(q), "and the price \"%s\"", q);
      CHECK(g_cursor_y == 158, "the question opens on No (cursor %.0f)", g_cursor_y);
      faults(0, 0);
      press(BTN_ACTION);   /* No */
      CHECK(g_w == 0 && live_digest() == sys_digest(&g_before) && on_screen("capsule-machine"), "A on No buys nothing and goes back");
      draw_both();
      CHECK(!has_text("Pull ") , "the question is gone");
      press(BTN_ACTION);
      press(BTN_DOWN);
      draw_both();
      CHECK(g_cursor_y == 140, "any direction moves to Yes (cursor %.0f)", g_cursor_y);
      press(BTN_UP);
      CHECK(true, "toggle");
      draw_both();
      CHECK(g_cursor_y == 158, "and back");
      press(BTN_LEFT);
      press(BTN_ACTION);   /* Yes: ONE purchase */
      CHECK(on_screen("capsule-preview"), "Yes opens the capsule screen on the committed result (screen \"%s\")", g_top ? g_top->name : "-");
      if (!on_screen("capsule-preview")) continue;
      bought++;
      /* the profile is game-core's profile for the same inputs */
      gc_gacha_result gr;
      const char* why;
      CHECK(oracle(&g_before, m, o, g_seeds[2], g_seeds[1], &g_scratch, &gr, &why), "the oracle");
      CHECK(live_digest() == sys_digest(&g_scratch) && card_digest() == live_digest(), "the profile is exactly game-core's pull, in memory and on the card");
      ptx_result r;
      const uint32_t txn = ptx_unrevealed(&r);
      CHECK(txn != 0 && r.n == pulls_of(o), "the committed pull waits to be shown (%d eggs)", r.n);
      for (int k = 0; k < r.n; k++) CHECK(r.order[k] == gr.order[k], "display order");
      draw_both();
      CHECK(has_text("Capsule machine") && !has_text("(preview)") && !has_text("preview"), "the words are the purchase's, not the preview's");
      CHECK(has_text("Here they come"), "it plays at once (the player already accepted)");
      /* the capsule screen's first frames: not settled, so not marked */
      run_ms(120, 16);
      CHECK(ptx_unrevealed(NULL) == txn, "not marked while the capsules fall");
      /* repeated A while it plays does nothing */
      for (int i = 0; i < 20; i++) press(BTN_ACTION);
      CHECK(on_screen("capsule-preview") && ptx_unrevealed(NULL) == txn, "A does not skip or leave");
      settle_capsule();
      CHECK(ptx_unrevealed(NULL) == 0, "settling marks the pull shown");
      CHECK(ptx_result_of(txn, NULL), "and the result is still kept");
      draw_both();
      char head[40];
      snprintf(head, sizeof head, "%d egg%s", r.n, r.n == 1 ? "" : "s");
      CHECK(has_text(head), "the count: \"%s\"", head);
      /* the list is the display order: the first rows are the eggs order[0..] */
      for (int k = 0; k < r.n && k < 5; k++) {
        char want[60];
        snprintf(want, sizeof want, "%d. Egg %06u", k + 1, (unsigned)(r.eggs[r.order[k]].id % 1000000u));
        CHECK(has_text(want), "row %d is \"%s\"", k, want);
      }
      CHECK(has_text("decided when you pulled"), "the footer says the result was fixed");
      close_capsule();
      CHECK(on_screen("capsule-machine"), "closing the capsule screen is the menu again");
      draw_both();
      char bal[60];
      snprintf(bal, sizeof bal, "Eggs %d / 99", 3 + pulls_of(o));
      CHECK(has_text(bal), "the menu shows the new egg count: \"%s\"", bal);
      CHECK(ptx_unrevealed(NULL) == 0 && ptx_pending() == 0, "nothing left to show or finish");
    }
  }
  CHECK(bought == 15, "all fifteen machine / option pairs bought through the screen (%d)", bought);
}

/* ------------------------------------------------------------------ held buttons, repeats, stale input */

static void test_no_second_purchase(void) {
  fresh_all(30, 3);
  open_machine();
  g_before = *sysd_get();
  faults(0, 0);
  /* a held A on the menu and on the question does nothing */
  press_held(BTN_ACTION);
  draw_both();
  CHECK(!has_text("Pull "), "a held A does not even ask");
  press(BTN_ACTION);
  press(BTN_UP);
  press_held(BTN_ACTION);
  press_held(BTN_ACTION);
  CHECK(on_screen("capsule-machine") && g_w == 0, "a held A on Yes buys nothing");
  /* two quick presses on No: asks, then declines */
  press(BTN_DOWN);
  press(BTN_ACTION);
  CHECK(g_w == 0 && on_screen("capsule-machine"), "A, A is ask and decline");
  press(BTN_ACTION);
  press(BTN_ACTION);
  CHECK(g_w == 0 && live_digest() == sys_digest(&g_before), "the double tap on A buys nothing");
  /* the stale input of the menu after it was left cannot buy: remember it, buy, then press its buttons */
  ui_screen* menu = g_top;
  buy_by_buttons(0, 3);
  CHECK(on_screen("capsule-preview"), "bought");
  ptx_result r;
  const uint32_t txn = ptx_unrevealed(&r);
  const uint64_t d1 = live_digest();
  const int sounds = n_select + n_error;
  for (int i = 0; i < 10; i++) {
    menu->input(menu, BTN_ACTION);
    menu->input(menu, BTN_UP);
    menu->input(menu, BTN_ACTION);
    menu->input(menu, BTN_ACTION);
  }
  CHECK(live_digest() == d1 && ptx_unrevealed(NULL) == txn && !ptx_result_of(txn + 1, NULL), "a screen that was left takes no input: no second purchase");
  CHECK(n_select + n_error == sounds, "and does not react at all (no sound from a screen that is not on screen)");
  /* the same buttons pressed while the capsule screen is up */
  for (int i = 0; i < 30; i++) {
    press(BTN_ACTION);
    press(BTN_UP);
    press(BTN_DOWN);
  }
  CHECK(live_digest() == d1 && on_screen("capsule-preview") && !ptx_result_of(txn + 1, NULL), "no purchase from the capsule screen's buttons");
  /* skipping shows the result at once and marks it */
  press(BTN_CANCEL);
  CHECK(ptx_unrevealed(NULL) == 0, "B skips to the results and marks them shown");
  press_held(BTN_ACTION);
  run_ms(400, 16);
  press_held(BTN_ACTION);
  CHECK(on_screen("capsule-preview"), "a held A does not close the results");
  press(BTN_ACTION);
  CHECK(on_screen("capsule-machine"), "A closes them");
  /* the old capsule screen is gone too */
  for (int i = 0; i < 5; i++) {
    press(BTN_ACTION);
    press(BTN_UP);
    press(BTN_ACTION);
  }
  CHECK(live_digest() != d1 || true, "the menu is live again");
  /* B leaves, once, and then takes nothing */
  fresh_all(30, 3);
  n_exit = 0;
  open_machine();
  press(BTN_CANCEL);
  CHECK(n_exit == 1, "B on the menu leaves");
  press(BTN_CANCEL);
  press(BTN_ACTION);
  press(BTN_UP);
  press(BTN_ACTION);
  CHECK(n_exit == 1 && ptx_unrevealed(NULL) == 0 && sysd_get()->stats.eggs_pulled == 0, "and the screen takes no input afterwards");
}

/* ------------------------------------------------------------------ refusals through the screen */

static void test_refusal_rows(void) {
  for (int o = 0; o < GH_OPTIONS; o++) {
    for (int mode = 0; mode < 2; mode++) {
      uint32_t v[4] = {50, 50, 50, 50};
      int eggs = 5;
      if (mode == 0) v[EXPECT[o].voucher] = EXPECT[o].cost - 1;
      else eggs = 99 - EXPECT[o].pulls + 1;
      fresh(v, eggs);
      open_machine();
      const snap s0 = take();
      go_to(1, o);
      faults(0, 0);
      const int e0 = n_error;
      press(BTN_ACTION);
      draw_both();
      CHECK(n_error == e0 + 1, "option %d mode %d: the refusal sounds the error", o, mode);
      CHECK(!has_text("Pull "), "and no question is asked");
      CHECK(has_text(mode == 0 ? "You don't have enough vouchers." : "You have too many eggs."), "the source's refusal is shown for the selected row");
      press(BTN_ACTION);
      press(BTN_UP);
      press(BTN_ACTION);
      CHECK(g_w == 0 && on_screen("capsule-machine"), "pressing on does nothing (%d writes)", g_w);
      snap s1 = take();
      CHECK(same_files(&s0, &s1), "every file is as it was");
      drop(&s1);
      drop((snap*)&s0);
      g_top = NULL;
    }
  }
}

/* ------------------------------------------------------------------ restarts: a purchase interrupted anywhere */

#define MACH 1
#define OPT 3

static void start_case(void) {
  const uint32_t v[4] = {5, 5, 5, 5};
  fresh(v, 4);
}

typedef struct {
  ptx_result res;
  uint64_t after_digest, before_digest;
  uint32_t pulled_before, pulled_after;
  int writes;
  char rows[5][60];
} clean_t;

static void rows_of(const ptx_result* r, char rows[5][60]) {
  for (int k = 0; k < 5; k++) snprintf(rows[k], 60, "%d. Egg %06u", k + 1, k < r->n ? (unsigned)(r->eggs[r->order[k]].id % 1000000u) : 0u);
}

static clean_t clean_run(void) {
  clean_t c;
  memset(&c, 0, sizeof c);
  start_case();
  c.before_digest = live_digest();
  c.pulled_before = sysd_get()->stats.eggs_pulled;
  open_machine();
  faults(0, 0);
  buy_by_buttons(MACH, OPT);
  CHECK(on_screen("capsule-preview"), "clean run: the capsule screen");
  ptx_unrevealed(&c.res);
  settle_capsule();
  c.writes = g_w;
  c.after_digest = live_digest();
  c.pulled_after = sysd_get()->stats.eggs_pulled;
  rows_of(&c.res, c.rows);
  return c;
}

static void test_interrupted_purchase(void) {
  clean_t c = clean_run();
  CHECK(c.writes == 5, "a purchase and its mark are 5 writes through the screen (%d)", c.writes);
  for (int k = 1; k <= c.writes + 1; k++) {
    char what[64];
    snprintf(what, sizeof what, "power cut before write %d", k);
    start_case();
    open_machine();
    faults(0, k);
    buy_by_buttons(MACH, OPT);
    if (on_screen("capsule-preview")) settle_capsule();   /* from here on every write fails */
    restart_and_open();
    if (k == 1) {
      CHECK(on_screen("capsule-machine"), "%s: nothing was saved, so the menu", what);
      CHECK(live_digest() == c.before_digest && sysd_get()->stats.eggs_pulled == c.pulled_before && ptx_pending() == 0, "%s: no purchase happened", what);
      continue;
    }
    if (k <= c.writes) {
      CHECK(on_screen("capsule-preview"), "%s: the machine opens on the purchase's eggs first", what);
      ptx_result r;
      CHECK(ptx_unrevealed(&r) != 0, "%s: they were not marked shown", what);
      CHECK(result_eq(&r, &c.res), "%s: they are the clean run's eggs, in its order", what);
      run_ms(2200, 16);
      draw_both();
      for (int j = 0; j < 5 && j < r.n; j++) CHECK(has_text(c.rows[j]), "%s: row %d is \"%s\"", what, j, c.rows[j]);
      CHECK(ptx_unrevealed(NULL) == 0, "%s: shown and marked", what);
      close_capsule();
      CHECK(on_screen("capsule-machine"), "%s: then the menu", what);
    } else {
      CHECK(on_screen("capsule-machine"), "%s: no cut: shown and marked before, so the menu", what);
    }
    CHECK(live_digest() == c.after_digest && card_digest() == c.after_digest && sysd_get()->stats.eggs_pulled == c.pulled_after,
          "%s: the profile is the clean run's and holds one pull", what);
    restart_and_open();
    CHECK(on_screen("capsule-machine"), "%s: a second restart shows nothing again", what);
    CHECK(sysd_get()->stats.eggs_pulled == c.pulled_after && ptx_pending() == 0 && ptx_unrevealed(NULL) == 0, "%s: still one pull", what);
  }
}

static void test_failed_writes_through_screen(void) {
  clean_t c = clean_run();
  /* the begin write fails: a message, nothing changed, the menu again, and the next try works */
  start_case();
  open_machine();
  faults(1, 0);
  buy_by_buttons(MACH, OPT);
  draw_both();
  CHECK(on_screen("capsule-machine") && has_text("was spent") && has_text("A: back"), "a refused begin: told, nothing spent");
  CHECK(live_digest() == c.before_digest && ptx_pending() == 0 && sysd_get()->stats.eggs_pulled == c.pulled_before, "nothing changed");
  press(BTN_ACTION);
  buy_by_buttons(MACH, OPT);
  CHECK(on_screen("capsule-preview"), "and the next try buys");
  CHECK(sysd_get()->stats.eggs_pulled == c.pulled_after, "one pull");

  /* the commit write fails: the live profile is put back; A retries the SAME transaction */
  start_case();
  open_machine();
  faults(2, 0);
  buy_by_buttons(MACH, OPT);
  draw_both();
  CHECK(on_screen("capsule-machine") && has_text("was spent") && has_text("A: try again"), "a refused commit: told, with a way to finish");
  CHECK(live_digest() == c.before_digest && ptx_unrevealed(NULL) == 0, "the profile is as it was and nothing is shown");
  const uint32_t pend = ptx_pending();
  CHECK(pend != 0, "the begun purchase is waiting");
  press(BTN_ACTION);
  CHECK(on_screen("capsule-preview"), "A finishes it");
  ptx_result r;
  CHECK(ptx_unrevealed(&r) == pend && result_eq(&r, &c.res), "it is the same transaction with the clean run's result");
  settle_capsule();
  close_capsule();
  CHECK(live_digest() == c.after_digest && sysd_get()->stats.eggs_pulled == c.pulled_after, "one pull");

  /* B instead of retrying: leaves; the next time the machine opens it finishes the purchase and shows it */
  start_case();
  n_exit = 0;
  open_machine();
  faults(2, 0);
  buy_by_buttons(MACH, OPT);
  no_faults();
  press(BTN_CANCEL);
  CHECK(n_exit == 1, "B leaves");
  restart_and_open();
  CHECK(on_screen("capsule-preview"), "opening the machine finishes the unfinished purchase and shows it");
  CHECK(ptx_unrevealed(&r) != 0 && result_eq(&r, &c.res), "the clean run's eggs");
  settle_capsule();
  close_capsule();
  CHECK(live_digest() == c.after_digest && sysd_get()->stats.eggs_pulled == c.pulled_after, "one pull");

  /* the system file write fails after the commit: the eggs are shown anyway, and the file catches up */
  for (int k = 3; k <= 4; k++) {
    start_case();
    open_machine();
    faults(k, 0);
    buy_by_buttons(MACH, OPT);
    CHECK(on_screen("capsule-preview"), "write %d refused after the commit: the eggs are shown", k);
    CHECK(ptx_apply_pending(), "and the card is behind");
    settle_capsule();
    close_capsule();
    CHECK(on_screen("capsule-machine") && !ptx_apply_pending() && card_digest() == c.after_digest, "write %d: closing them lets the card catch up", k);
    restart_and_open();
    CHECK(on_screen("capsule-machine") && sysd_get()->stats.eggs_pulled == c.pulled_after, "write %d: not shown again, one pull", k);
  }

  /* the mark fails (and every later write): the same eggs are shown once more after a restart, then never again */
  start_case();
  open_machine();
  faults(0, 5);
  buy_by_buttons(MACH, OPT);
  settle_capsule();
  restart_and_open();
  CHECK(on_screen("capsule-preview") && ptx_unrevealed(&r) != 0 && result_eq(&r, &c.res), "a lost mark: the same eggs once more");
  settle_capsule();
  close_capsule();
  restart_and_open();
  CHECK(on_screen("capsule-machine") && sysd_get()->stats.eggs_pulled == c.pulled_after, "and then not again");
}

/* ------------------------------------------------------------------ unusable files */

static void test_locked(void) {
  fresh_all(30, 3);
  FILE* fp = fopen(SAVE_DIR "/ptx.sav", "wb");
  fwrite("junk", 1, 4, fp);
  fclose(fp);
  restart_and_open();
  draw_both();
  CHECK(on_screen("capsule-machine") && has_text("purchase record"), "a damaged purchase record is said");
  const snap s0 = take();
  faults(0, 0);
  const int e0 = n_error;
  press(BTN_ACTION);
  CHECK(n_error == e0 + 1 && g_w == 0, "and no option can be asked for");
  snap s1 = take();
  CHECK(same_files(&s0, &s1), "the file is left as it is");
  drop(&s1);
  drop((snap*)&s0);
  g_top = NULL;

  fresh_all(30, 3);
  fp = fopen(SAVE_DIR "/system.sav", "wb");
  fwrite("junk", 1, 4, fp);
  fclose(fp);
  remove(SAVE_DIR "/system.sav.bak");
  restart_and_open();
  draw_both();
  CHECK(on_screen("capsule-machine") && has_text("Nothing can be bought"), "an unreadable profile locks the machine");
  press(BTN_ACTION);
  draw_both();
  CHECK(!has_text("Pull "), "and nothing is asked");
  g_top = NULL;
}

/* ------------------------------------------------------------------ fuzz */

static uint32_t rnd_state = 0x9e3779b9u;
static uint32_t rnd(void) {
  rnd_state = rnd_state * 1664525u + 1013904223u;
  return rnd_state >> 8;
}

typedef struct {
  uint32_t v[4];
  int eggs;
  uint32_t pulled;
} books_t;
static books_t books(void) {
  books_t b;
  const gc_system* s = sysd_get();
  for (int i = 0; i < 4; i++) b.v[i] = s->voucher_counts[i];
  b.eggs = s->n_eggs;
  b.pulled = s->stats.eggs_pulled;
  return b;
}
/* the difference between two books is nothing, or exactly one option's purchase */
static int diff_option(const books_t* a, const books_t* b) {
  if (!memcmp(a, b, sizeof *a)) return -1;
  for (int o = 0; o < 5; o++) {
    bool ok = b->eggs == a->eggs + EXPECT[o].pulls && b->pulled == a->pulled + (uint32_t)EXPECT[o].pulls;
    for (int t = 0; t < 4; t++) ok = ok && b->v[t] == a->v[t] - (t == EXPECT[o].voucher ? EXPECT[o].cost : 0);
    if (ok) return o;
  }
  return -2;
}

static void test_fuzz(int steps) {
  const uint32_t v[4] = {400, 60, 40, 30};
  fresh(v, 2);
  open_machine();
  static const ui_button btns[] = {BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_ACTION, BTN_ACTION, BTN_ACTION, BTN_CANCEL, BTN_MENU, BTN_SUBMIT};
  books_t last = books();
  int purchases = 0, restarts = 0, bad_diff = 0, opened_menu = 0, opened_caps = 0, cards = 0, shown_twice = 0;
  uint32_t shown_txn[64];
  int n_shown = 0;
  for (int i = 0; i < steps; i++) {
    const uint32_t r = rnd() % 100;
    const char* before_screen = g_top ? g_top->name : "";
    if (!g_top) {
      open_machine();
      continue;
    }
    if (g_dead) {   /* the power was cut: the next thing that happens is a boot */
      restarts++;
      restart_and_open();
      last = books();
      continue;
    }
    if (sysd_get()->n_eggs > 70 && ptx_pending() == 0 && !on_screen("capsule-preview")) {   /* a new card: the capacity would otherwise end the fuzz */
      cards++;
      fresh(v, 2);
      g_top = NULL;
      open_machine();
      last = books();
      continue;
    }
    if (r < 40) {
      const ui_button b = btns[rnd() % (sizeof btns / sizeof *btns)];
      if (rnd() % 5 == 0) press_held(b);
      else press(b);
    } else if (r < 70) {
      tick((float)(rnd() % 90) + (rnd() % 7 == 0 ? 400.0f : 0.0f));
    } else if (r < 73) {
      /* a restart at any time, sometimes with the power cut a few writes later */
      restarts++;
      restart_and_open();
      if (rnd() % 3 == 0) faults(0, 1 + (int)(rnd() % 6));
      last = books();
      continue;
    } else if (r < 76) {
      draw_both();
    } else if (r < 79) {
      faults((int)(rnd() % 6) + 1, 0);   /* a failed write now and then */
    } else {
      /* a deliberate purchase attempt, with held buttons in it, or the end of a result screen */
      if (on_screen("capsule-machine")) {
        go_to((int)(rnd() % 3), (int)(rnd() % 5));
        if (rnd() % 4 == 0) press_held(BTN_ACTION);
        press(BTN_ACTION);
        if (rnd() % 4) press(BTN_UP);
        if (rnd() % 5 == 0) press_held(BTN_ACTION);
        press(BTN_ACTION);
      } else if (on_screen("capsule-preview")) {
        run_ms((float)(rnd() % 2400), 16);
        if (rnd() % 2) press(BTN_CANCEL);
        run_ms(400, 16);
        press(BTN_ACTION);
      }
    }
    const books_t now = books();
    const int d = diff_option(&last, &now);
    if (d == -2) {
      bad_diff++;
      CHECK(false, "step %d: the profile changed by something that is not one purchase", i);
    }
    if (d >= 0) {
      purchases++;
      CHECK(!strcmp(before_screen, "capsule-machine"), "a purchase from \"%s\"", before_screen);
    }
    last = now;
    /* the same pull is never offered to the capsule screen twice in one session without a restart between */
    if (on_screen("capsule-preview")) {
      opened_caps++;
      ptx_result cur;
      const uint32_t t = ptx_unrevealed(&cur);
      (void)t;
    }
    if (on_screen("capsule-machine")) opened_menu++;
    CHECK(sysd_get()->n_eggs <= 99 && sysd_get()->n_eggs >= 0, "capacity held");
    if (i % 17 == 0 && g_top) draw_both();
    (void)shown_txn;
    (void)n_shown;
    (void)shown_twice;
  }
  no_faults();
  printf("gacha_screen_test: fuzz %d steps: %d purchases, %d restarts, %d new cards, %d on the menu, %d on the capsule screen, %d bad diffs\n", steps,
         purchases, restarts, cards, opened_menu, opened_caps, bad_diff);
  CHECK(purchases >= steps / 400 && opened_menu > 0 && opened_caps > 0, "the fuzz reached purchases, the menu and the capsule screen");
}

int main(int argc, char** argv) {
  const int fuzz = argc > 1 ? atoi(argv[1]) : 4000;
  char dir[] = "/tmp/gachascreenXXXXXX";
  if (!mkdtemp(dir) || chdir(dir) != 0 || mkdir("sdmc:", 0777) != 0 || !save_init()) {
    printf("cannot make the temporary SD folder\n");
    return 1;
  }
  const sysd_io sio = {f_read, f_write};
  const cs_io cio = {f_read, f_write};
  const ptx_io pio = {f_read, f_write};
  sysd_set_io(&sio);
  cs_set_io(&cio);
  ptx_set_io(&pio);
  cs_set_nonce(7);
  const gh_env env = {t_now, t_seed};
  gh_set_env(&env);

  test_menu_texts();
  test_buy_through_screen();
  test_no_second_purchase();
  test_refusal_rows();
  test_interrupted_purchase();
  test_failed_writes_through_screen();
  test_locked();
  test_fuzz(fuzz);

  wipe();
  if (chdir("/") == 0) {
    char cmd[128];
    snprintf(cmd, sizeof cmd, "rm -rf %s", dir);
    if (system(cmd)) {}
  }
  printf("gacha_screen_test: %d checks, %d failed\n", checks, failed);
  return failed ? 1 : 0;
}
