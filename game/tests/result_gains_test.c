/* result_gains_test: the result screen's way through what a Classic win gave (source/result_screen.c with win_gains.h's items), over a fake
 * UI stack and drawing layer: the bars on the top screen, one after the other, and the messages in the box, one per A.
 *
 *   make result_gains_test && build/result_gains_test
 *
 * Checks, against upstream's GameOverPhase queue (the end card's line first, then each UnlockPhase / RibbonModifierRewardPhase /
 * GameOverModifierRewardPhase, then the exit) and AchvBar (500 ms in, in place until 10 s after it began, 500 ms out, the next one then,
 * the sound at each): the order of the pages, the fanfare with each message and the bar's sound with each bar, the bar's easing, a box that
 * grows to three lines only when there is something to tell, the exit after the last message, the post-mortem's B coming back without the
 * gains, and a screen that keeps its own copy of what it was given. */
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "i18n.h"
#include "postmortem_screen.h"
#include "result_screen.h"
#include "toast_view.h"
#include "ui.h"
#include "ui_text.h"
#include "ui_widgets.h"

static int checks, failed;
#define CHECK(cond, ...)                               \
  do {                                                 \
    checks++;                                          \
    if (!(cond)) {                                     \
      failed++;                                        \
      if (failed < 40) {                               \
        printf("FAIL line %d: %s: ", __LINE__, #cond); \
        printf(__VA_ARGS__);                           \
        printf("\n");                                  \
      }                                                \
    }                                                  \
  } while (0)

/* ------------------------------------------------------------------ fakes */

static ui_screen* g_top;
static int n_set;
static int g_which; /* 0 top, 1 bottom */
void ui_set(ui_screen* s) {
  g_top = s;
  n_set++;
}
void ui_play_select(void) {}
static char g_sounds[32][32];
static int n_sounds;
void ui_play_sound(const char* key) {
  if (n_sounds < 32) snprintf(g_sounds[n_sounds++], 32, "%s", key ? key : "(null)");
}
bool ui_space(ui_space_id sp) {
  const bool top = sp == UI_FIELD || sp == UI_FIELD_UI || sp == UI_TOP_CANVAS || sp == UI_TOP_CANVAS_UI || sp == UI_PHYS_TOP;
  return top == (g_which == 0);
}
void rr_rect(float x, float y, float w, float h, rr_color c) { (void)x, (void)y, (void)w, (void)h, (void)c; }
static float g_box_h; /* the bottom window drawn last: its height */
void ui_window(float x, float y, float w, float h, win_variant v) {
  (void)x, (void)y, (void)w, (void)v;
  if (g_which == 1) g_box_h = h;
}
bool img_has(const char* key) { (void)key; return false; }
void img_draw(const char* key, const char* frame, float x, float y, float ox, float oy) { (void)key, (void)frame, (void)x, (void)y, (void)ox, (void)oy; }
void img_draw_opts(const char* key, const char* frame, float x, float y, const rr_draw_opts* o) { (void)key, (void)frame, (void)x, (void)y, (void)o; }
const rr_draw_opts RR_DEFAULT_OPTS = {0, 0, 1, 1, 0, RR_RGBA(255, 255, 255, 255)};
text_opts text_defaults(void) {
  text_opts o;
  memset(&o, 0, sizeof o);
  o.chars = TEXT_ALL;
  return o;
}
float text_width(text_style st, const char* s, const text_opts* o) { (void)st, (void)o; return 6.0f * (float)strlen(s); }
float text_draw(text_style st, float x, float y, const char* s, const text_opts* o) { (void)st, (void)x, (void)y, (void)o; return 6.0f * (float)strlen(s); }
void text_wrap(text_style st, const char* s, const text_opts* o, char* out, int n) { (void)st, (void)o; snprintf(out, (size_t)n, "%s", s); }
const char* i18n_t(char* out, int n, const char* key, ...) { snprintf(out, (size_t)n, "[%s]", key); return out; }
const char* i18n_species_name(int id) { (void)id; return "?"; }
void rsum_draw(const rsv_info* in, float x, float y) { (void)in, (void)x, (void)y; }

/* a message box that shows its text at once and waits for A or B */
static int g_max_lines; /* the box's lines, as msgbox_init was asked */
void msgbox_init(msgbox* m, text_style style, float wrap_w, int max_lines) {
  (void)style, (void)wrap_w;
  memset(m, 0, sizeof *m);
  g_max_lines = max_lines;
}
void msgbox_show(msgbox* m, const char* text, float delay, bool prompt) {
  (void)delay;
  snprintf(m->text, sizeof m->text, "%s", text);
  m->prompt = prompt;
  m->waiting = true;
  m->done = false;
}
void msgbox_update(msgbox* m, float dt) { (void)m, (void)dt; }
bool msgbox_input(msgbox* m, ui_button b) {
  if (b != BTN_ACTION && b != BTN_CANCEL) return false;
  if (m->waiting) {
    m->waiting = false;
    m->done = true;
  }
  return true;
}
static char g_box_text[512]; /* the text in the box when it was last drawn */
void msgbox_draw(const msgbox* m, float x, float y) {
  (void)x, (void)y;
  snprintf(g_box_text, sizeof g_box_text, "%s", m->text);
}
bool msgbox_done(const msgbox* m) { return m->done; }

/* the post-mortem: opens when there is a report, and keeps its arguments so the test can press its buttons */
static pm_args g_pm;
static int n_pm;
bool postmortem_open(const pm_args* a) {
  if (!a->report) return false;
  g_pm = *a;
  n_pm++;
  return true;
}

/* the items: kinds below WG_UNLOCK are bars; their words are made of the kind and the id */
bool wg_is_toast(const wg_item* it) { return it->kind == WG_ACHV || it->kind == WG_VOUCHER_TOAST; }
void wg_toast_of(const wg_item* it, wg_toast* o) {
  memset(o, 0, sizeof *o);
  snprintf(o->title, sizeof o->title, "bar%d:%d", it->kind, it->id);
  snprintf(o->desc, sizeof o->desc, "desc%d", it->id);
  snprintf(o->score, sizeof o->score, "+%dpt", it->id);
  o->icon = "icon";
  o->tier = it->id % 5;
}
void wg_message_of(const wg_item* it, char* out, int n) { snprintf(out, (size_t)n, "msg%d:%d", it->kind, it->id); }

static char g_bar_title[8][32];
static float g_bar_slide[8];
static int n_bars;
void tv_draw(const tv_info* t, float slide) {
  if (n_bars < 8) {
    snprintf(g_bar_title[n_bars], 32, "%s", t->title);
    g_bar_slide[n_bars] = slide;
  }
  n_bars++;
}

/* ------------------------------------------------------------------ helpers */

#define LINE "You cleared the run at wave 200!"
static int n_done;
static void on_done(void* arg) { (void)arg; n_done++; }

static bool press(ui_button b) { return g_top->input(g_top, b); }
static void draw_top(void) {
  g_which = 0;
  n_bars = 0;
  g_top->draw(g_top, RR_TOP);
}
static void draw_bottom(void) {
  g_which = 1;
  g_top->draw(g_top, RR_BOTTOM);
}
static void tick(float ms) { g_top->update(g_top, ms); }
static rsv_info run_info(int wave) {
  rsv_info r;
  memset(&r, 0, sizeof r);
  r.wave = wave;
  return r;
}

static result_args args(bool victory, const wg_gains* g, const co_report* rep) {
  result_args a;
  memset(&a, 0, sizeof a);
  a.victory = victory;
  a.run = run_info(victory ? 200 : 12);
  a.report = rep;
  a.gains = g;
  a.on_done = on_done;
  return a;
}

static wg_gains gains_of(const int (*items)[2], int n) {
  wg_gains g;
  memset(&g, 0, sizeof g);
  for (int i = 0; i < n; i++) {
    g.item[i].kind = (uint8_t)items[i][0];
    g.item[i].id = (uint16_t)items[i][1];
  }
  g.n = n;
  return g;
}



int main(void) {
  /* ---- a win with nothing to tell, a defeat: the old behaviour (one line, A leaves, no bars, a two-line box) */
  {
    result_args a = args(true, NULL, NULL);
    result_screen_open(&a);
    CHECK(g_top && n_set == 1, "the result is the screen");
    draw_top();
    CHECK(n_bars == 0 && n_sounds == 0, "no bars and no sounds without gains (%d, %d)", n_bars, n_sounds);
    draw_bottom();
    CHECK(g_box_h == 42.0f && g_max_lines == 2, "the plain box is 42 px tall and two lines (%.0f, %d)", g_box_h, g_max_lines);
    CHECK(!strcmp(g_box_text, LINE), "the line of the end card (%s)", g_box_text);
    CHECK(press(BTN_ACTION), "A answers the line");
    CHECK(n_done == 1, "and leaves");
  }
  {
    wg_gains empty;
    memset(&empty, 0, sizeof empty);
    result_args a = args(true, &empty, NULL);
    result_screen_open(&a);
    draw_top();
    CHECK(n_bars == 0, "an empty list of gains is no gains");
    press(BTN_ACTION);
    CHECK(n_done == 2, "A leaves");
  }

  /* ---- the gains: two bars, three messages */
  n_done = 0;
  n_sounds = 0;
  int list[][2] = {{WG_ACHV, 192}, {WG_VOUCHER_TOAST, 3}, {WG_UNLOCK, 0}, {WG_FIRST_RIBBON, 4}, {WG_PREMIUM, 0}};
  wg_gains g = gains_of(list, 5);
  {
    result_args a = args(true, &g, NULL);
    result_screen_open(&a);
    memset(&g, 0, sizeof g); /* the screen has its own copy */
    CHECK(n_sounds == 1 && !strcmp(g_sounds[0], "se/achv"), "the first bar comes with the achievement sound (%d, %s)", n_sounds, g_sounds[0]);
    draw_bottom();
    CHECK(g_box_h == 58.0f && g_max_lines == 3, "the box has three lines when there are messages (%.0f, %d)", g_box_h, g_max_lines);
    CHECK(!strcmp(g_box_text, LINE), "and the end card's line first (%s)", g_box_text);

    /* AchvBar: 500 ms in (Sine.easeOut), in place until 10 s, 500 ms out (Sine.easeIn) */
    draw_top();
    CHECK(n_bars == 1 && !strcmp(g_bar_title[0], "bar0:192") && g_bar_slide[0] == 0.0f, "at the start the first bar is past the edge (%d, %s, %.3f)", n_bars,
          g_bar_title[0], g_bar_slide[0]);
    tick(250);
    draw_top();
    CHECK(fabsf(g_bar_slide[0] - sinf(0.5f * 1.5707963f)) < 1e-4f, "250 ms in: %.4f", g_bar_slide[0]);
    tick(250);
    draw_top();
    CHECK(g_bar_slide[0] == 1.0f, "500 ms in: in place (%.4f)", g_bar_slide[0]);
    tick(9000);
    draw_top();
    CHECK(g_bar_slide[0] == 1.0f && !strcmp(g_bar_title[0], "bar0:192"), "9.5 s in: still in place");
    tick(500);
    draw_top();
    CHECK(g_bar_slide[0] == 1.0f, "10 s: the bar starts to leave (%.4f)", g_bar_slide[0]);
    tick(250);
    draw_top();
    CHECK(fabsf(g_bar_slide[0] - cosf(0.5f * 1.5707963f)) < 1e-4f, "250 ms out: %.4f", g_bar_slide[0]);
    CHECK(n_sounds == 1, "no second sound yet");
    tick(250);
    CHECK(n_sounds == 2 && !strcmp(g_sounds[1], "se/achv"), "the voucher bar's sound at 10.5 s (%d)", n_sounds);
    draw_top();
    CHECK(!strcmp(g_bar_title[0], "bar1:3") && g_bar_slide[0] == 0.0f, "the second bar starts past the edge (%s, %.3f)", g_bar_title[0], g_bar_slide[0]);
    tick(500);
    draw_top();
    CHECK(g_bar_slide[0] == 1.0f, "and comes in");
    tick(10000);
    CHECK(n_sounds == 2, "after its 10.5 s there is no third bar");
    draw_top();
    CHECK(n_bars == 0, "no bar left to draw");

    /* the messages: the line of the end card first, then one A each, a fanfare with each */
    CHECK(press(BTN_ACTION), "A answers the first line");
    CHECK(n_done == 0, "the screen goes on with the gains");
    draw_bottom();
    CHECK(!strcmp(g_box_text, "msg2:0"), "the unlock is the first message (%s)", g_box_text);
    CHECK(n_sounds == 3 && !strcmp(g_sounds[2], "se/level_up_fanfare"), "the fanfare comes with the unlock (%d, %s)", n_sounds, g_sounds[2]);
    CHECK(press(BTN_ACTION), "A answers the unlock");
    draw_bottom();
    CHECK(!strcmp(g_box_text, "msg3:4"), "the first ribbon is the second (%s)", g_box_text);
    CHECK(n_sounds == 4 && !strcmp(g_sounds[3], "se/level_up_fanfare"), "the fanfare comes with the ribbon");
    CHECK(press(BTN_ACTION), "A answers the ribbon");
    draw_bottom();
    CHECK(!strcmp(g_box_text, "msg4:0"), "the premium voucher is the last (%s)", g_box_text);
    CHECK(n_sounds == 5 && n_done == 0, "the premium voucher comes with its fanfare, and the screen is still up");
    CHECK(press(BTN_ACTION), "A answers the premium voucher");
    CHECK(n_done == 1 && n_sounds == 5, "then the screen leaves, once");
  }

  /* ---- the order of the items does not matter to the bars: bars first or last in the list, the messages keep their own order */
  {
    n_sounds = 0;
    n_done = 0;
    int mixed[][2] = {{WG_PREMIUM, 0}, {WG_ACHV, 193}, {WG_FIRST_RIBBON, 1}, {WG_VOUCHER_TOAST, 0}, {WG_UNLOCK, 3}};
    wg_gains g2 = gains_of(mixed, 5);
    result_args a = args(true, &g2, NULL);
    result_screen_open(&a);
    press(BTN_ACTION); /* the line */
    draw_bottom();
    CHECK(!strcmp(g_box_text, "msg4:0"), "the messages follow the list: premium first (%s)", g_box_text);
    press(BTN_ACTION);
    draw_bottom();
    CHECK(!strcmp(g_box_text, "msg3:1"), "then the ribbon (%s)", g_box_text);
    press(BTN_ACTION);
    draw_bottom();
    CHECK(!strcmp(g_box_text, "msg2:3"), "then the unlock (%s)", g_box_text);
    CHECK(n_sounds == 4, "one bar sound and three fanfares (%d)", n_sounds);
    draw_top();
    CHECK(n_bars == 1 && !strcmp(g_bar_title[0], "bar0:193"), "the bar on the top screen is the first of the bars (%s)", g_bar_title[0]);
    press(BTN_ACTION);
    CHECK(n_done == 1, "and the last A leaves");
  }

  /* ---- only bars: the box stays two lines, A leaves at once, the bars run their time while the line waits */
  {
    n_sounds = 0;
    n_done = 0;
    int bars[][2] = {{WG_ACHV, 192}};
    wg_gains g5 = gains_of(bars, 1);
    result_args a = args(true, &g5, NULL);
    result_screen_open(&a);
    draw_bottom();
    CHECK(g_box_h == 42.0f && g_max_lines == 2, "a win that only gave a bar keeps the plain box (%.0f, %d)", g_box_h, g_max_lines);
    draw_top();
    CHECK(n_bars == 1, "its bar is drawn");
    press(BTN_ACTION);
    CHECK(n_done == 1 && n_sounds == 1, "A leaves and no fanfare was played (%d, %d)", n_done, n_sounds);
  }

  /* ---- the post-mortem: A on the last line opens it, its B comes back to the screen without the gains */
  {
    n_sounds = 0;
    n_done = 0;
    static co_report rep;
    wg_gains g3 = gains_of(list, 5);
    result_args a = args(true, &g3, &rep);
    result_screen_open(&a);
    for (int i = 0; i < 3; i++) press(BTN_ACTION); /* the line, then the first two messages */
    CHECK(n_pm == 0 && n_done == 0, "the post-mortem is not opened before the last message");
    press(BTN_ACTION);
    CHECK(n_pm == 1, "after the last message A opens the post-mortem");
    const int before_sounds = n_sounds;
    g_pm.on_exit(PM_BACK, g_pm.arg);
    CHECK(n_sounds == before_sounds, "B from the post-mortem comes back without the bars or the fanfares again (%d)", n_sounds - before_sounds);
    draw_top();
    CHECK(n_bars == 0, "no bars on the second time");
    draw_bottom();
    CHECK(g_box_h == 42.0f, "and the plain box");
    press(BTN_ACTION);
    CHECK(n_pm == 2, "the line opens the post-mortem again");
    g_pm.on_exit(PM_CONTINUE, g_pm.arg);
    CHECK(n_done == 1, "its A leaves");
  }

  /* ---- a defeat never has gains, whatever it is given */
  {
    n_sounds = 0;
    n_done = 0;
    wg_gains g4 = gains_of(list, 5);
    result_args a = args(false, &g4, NULL);
    result_screen_open(&a);
    draw_top();
    draw_bottom();
    CHECK(n_bars == 0 && n_sounds == 0 && g_box_h == 42.0f, "a defeat ignores the gains it is given (%d bars, %d sounds, %.0f)", n_bars, n_sounds, g_box_h);
    CHECK(!strcmp(g_box_text, "You were defeated at wave 12."), "and says its own line (%s)", g_box_text);
    press(BTN_ACTION);
    CHECK(n_done == 1, "A leaves");
  }

  printf("result_gains_test: %d checks, %d failed\n", checks, failed);
  return failed ? 1 : 0;
}
