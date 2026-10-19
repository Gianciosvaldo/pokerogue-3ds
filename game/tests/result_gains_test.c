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

#include "char_sprite.h"
#include "dialogue_view.h"
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
/* the black laid over a whole screen (ui.fadeIn / fadeOut): its alpha, per screen; -1 = none drawn since the last reset */
static int g_fade_top, g_fade_top_n, g_fade_bottom = -1;
#define FADE_TOP (g_fade_top_n >= 2 ? g_fade_top : 0) /* the screen's own black is the first full black rectangle, the fade is the second */
void rr_rect(float x, float y, float w, float h, rr_color c) {
  (void)x, (void)y;
  if (h == 240.0f && (c & 0xFFFFFF00u) == 0) {
    if (w == 400.0f && g_which == 0) {
      g_fade_top = (int)(c & 0xFF);
      g_fade_top_n++;
    }
    if (w == 320.0f && g_which == 1) g_fade_bottom = (int)(c & 0xFF);
  }
}
static float g_box_h; /* the bottom window drawn last: its height */
void ui_window(float x, float y, float w, float h, win_variant v) {
  (void)x, (void)y, (void)w, (void)v;
  if (g_which == 1) g_box_h = h;
}
/* what the top screen drew: the images, in order (keys only) */
static char g_imgs[16][48];
static int n_imgs;
static void note_img(const char* key) {
  if (n_imgs < 16) snprintf(g_imgs[n_imgs++], 48, "%s", key);
}
static bool drew(const char* key) {
  for (int i = 0; i < n_imgs; i++)
    if (!strcmp(g_imgs[i], key)) return true;
  return false;
}
bool img_has(const char* key) { return !strncmp(key, "images/cg/", 10) || !strncmp(key, "images/arenas/", 14); }
void img_draw(const char* key, const char* frame, float x, float y, float ox, float oy) { (void)frame, (void)x, (void)y, (void)ox, (void)oy; note_img(key); }
void img_draw_opts(const char* key, const char* frame, float x, float y, const rr_draw_opts* o) { (void)frame, (void)x, (void)y, (void)o; note_img(key); }
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
/* the ending dialogue is two pages with an expression each; its text can be made missing (the key comes back, like i18next) */
static bool g_no_ending_text;
const char* i18n_t(char* out, int n, const char* key, ...) {
  if (!strcmp(key, "miscDialogue:ending") && !g_no_ending_text)
    snprintf(out, (size_t)n, "@c{shock}You're back?$@c{smile}Let's go home.");
  else if (!strcmp(key, "trainerNames:rivalFemale"))
    snprintf(out, (size_t)n, "Ivy");
  else if (!strcmp(key, "miscDialogue:ending"))
    snprintf(out, (size_t)n, "%s", key);
  else
    snprintf(out, (size_t)n, "[%s]", key);
  return out;
}
const char* i18n_species_name(int id) { (void)id; return "?"; }
void rsum_draw(const rsv_info* in, float x, float y) { (void)in, (void)x, (void)y; }

/* a message box that shows its page at once and waits for A or B; the pages of a "$" text come one per answer, as the real one's do */
static int g_max_lines; /* the box's lines, as msgbox_init was asked */
static char g_rest[1024];
void msgbox_init(msgbox* m, text_style style, float wrap_w, int max_lines) {
  (void)style, (void)wrap_w;
  memset(m, 0, sizeof *m);
  g_max_lines = max_lines;
  g_rest[0] = 0;
}
static void take_page(msgbox* m) {
  const char* d = strchr(g_rest, '$');
  size_t len = d ? (size_t)(d - g_rest) : strlen(g_rest);
  snprintf(m->text, sizeof m->text, "%.*s", (int)len, g_rest);
  memmove(g_rest, d ? d + 1 : g_rest + len, strlen(d ? d + 1 : g_rest + len) + 1);
}
void msgbox_show(msgbox* m, const char* text, float delay, bool prompt) {
  (void)delay;
  snprintf(g_rest, sizeof g_rest, "%s", text);
  take_page(m);
  m->prompt = prompt;
  m->waiting = true;
  m->done = false;
}
void msgbox_clear(msgbox* m) {
  m->text[0] = 0;
  g_rest[0] = 0;
  m->waiting = false;
  m->done = true;
}
static const char* g_pending_variant; /* what the typing "passed": handed over once */
const char* msgbox_take_variant(msgbox* m) {
  (void)m;
  const char* v = g_pending_variant;
  g_pending_variant = NULL;
  return v;
}
void msgbox_update(msgbox* m, float dt) { (void)m, (void)dt; }
bool msgbox_input(msgbox* m, ui_button b) {
  if (b != BTN_ACTION && b != BTN_CANCEL) return false;
  if (m->waiting) {
    if (g_rest[0]) {
      take_page(m);
    } else {
      m->waiting = false;
      m->done = true;
    }
  }
  return true;
}
static char g_box_text[512]; /* the text in the box when it was last drawn */
void msgbox_draw(const msgbox* m, float x, float y) {
  (void)x, (void)y;
  snprintf(g_box_text, sizeof g_box_text, "%s", m->text);
}
bool msgbox_done(const msgbox* m) { return m->done; }

/* the portrait (char_sprite.c) and the name box (menus/dialogue_view.c): what was asked of them, and the time a slide takes */
static char g_cs_log[16][64];
static int n_cs_log;
static float g_cs_busy_ms;
static int n_cs_draw;
static void cs_note(const char* a, const char* b) {
  if (n_cs_log < 16) snprintf(g_cs_log[n_cs_log++], 64, "%s%s%s", a, b ? ":" : "", b ? b : "");
}
void chs_show(const char* atlas, const char* variant) { cs_note(atlas, variant); g_cs_busy_ms = CHS_SLIDE_MS; }
void chs_set_variant(const char* variant) { cs_note("variant", variant); }
void chs_hide(void) { cs_note("hide", NULL); g_cs_busy_ms = CHS_SLIDE_MS; }
void chs_reset(void) { cs_note("reset", NULL); g_cs_busy_ms = 0; }
void chs_update(float dt) { if (g_cs_busy_ms > 0) g_cs_busy_ms -= dt; }
void chs_draw(void) { n_cs_draw++; }
bool chs_busy(void) { return g_cs_busy_ms > 0; }
bool chs_shown(void) { return false; }
float chs_x(void) { return 0; }
static char g_name[48];
static int g_name_style;
void dv_namebox(float x, float y, const char* name, int window_style) {
  (void)x, (void)y;
  snprintf(g_name, sizeof g_name, "%s", name);
  g_name_style = window_style;
}
int ui_window_style(void) { return 5; }

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
  n_imgs = 0;
  n_cs_draw = 0;
  g_fade_top = 0;
  g_fade_top_n = 0;
  g_top->draw(g_top, RR_TOP);
}
static void draw_bottom(void) {
  g_which = 1;
  g_fade_bottom = -1;
  g_name[0] = 0;
  g_top->draw(g_top, RR_BOTTOM);
}
static void tick(float ms) { g_top->update(g_top, ms); }
/* time passing a frame of 10 ms at a time (a stage ends on the frame that reaches its time; the next one is looked at on the next frame) */
static void run(float ms) {
  for (float t = 0; t < ms; t += 10) tick(10);
}
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


  /* ---- the rival's ending before the end card of a Classic win (GameOverPhase): fadeIn(500), she slides in, one page per A with her expression
   * changing, fadeOut(500), she is hidden under the black, EndCardPhase's fadeIn(1000) and its prompt */
  {
    n_done = 0;
    n_sounds = 0;
    n_cs_log = 0;
    int gift[][2] = {{WG_UNLOCK, 0}};
    wg_gains gg = gains_of(gift, 1);
    result_args a = args(true, &gg, NULL);
    a.ending = true;
    result_screen_open(&a);
    CHECK(n_cs_log == 1 && !strcmp(g_cs_log[0], "reset"), "a new screen forgets the portrait (%d: %s)", n_cs_log, n_cs_log ? g_cs_log[0] : "");
    draw_top();
    draw_bottom();
    CHECK(FADE_TOP == 255 && g_fade_bottom == 255, "it opens in the black (%d, %d)", FADE_TOP, g_fade_bottom);
    CHECK(!drew("images/cg/end_m") && drew("images/arenas/plains_bg") == drew("images/arenas/plains_bg"), "the end card is not up yet");
    CHECK(g_max_lines == 3 && g_box_text[0] == 0, "an empty three line box (%d, '%s')", g_max_lines, g_box_text);
    tick(250);
    draw_top();
    draw_bottom();
    CHECK(FADE_TOP == 128 && g_fade_bottom == 128, "250 ms into fadeIn(500): half black (%d, %d)", FADE_TOP, g_fade_bottom);
    CHECK(press(BTN_ACTION), "A is swallowed while it fades");
    CHECK(n_cs_log == 1, "and starts nothing");
    tick(250);
    CHECK(n_cs_log == 2 && !strcmp(g_cs_log[1], "images/character/rival_f:shock"), "she comes in with the first expression of her text (%s)", n_cs_log > 1 ? g_cs_log[1] : "");
    draw_top();
    draw_bottom();
    CHECK(FADE_TOP == 0 && g_fade_bottom == -1, "the fade is over (%d, %d)", FADE_TOP, g_fade_bottom);
    CHECK(n_cs_draw == 1, "she is drawn on the top screen (%d)", n_cs_draw);
    CHECK(g_box_text[0] == 0 && g_name[0] == 0, "no text and no name box while she slides (%s, %s)", g_box_text, g_name);
    CHECK(press(BTN_ACTION) && n_done == 0, "A is swallowed while she slides");
    tick(500);
    draw_bottom();
    CHECK(g_box_text[0] == 0, "750 ms have not passed");
    tick(250);
    draw_bottom();
    CHECK(!strcmp(g_box_text, "@c{shock}You're back?"), "her first page (%s)", g_box_text);
    CHECK(!strcmp(g_name, "Ivy") && g_name_style == 5, "the name box says Ivy in the window style (%s, %d)", g_name, g_name_style);
    CHECK(g_box_h == 58.0f, "the box has three lines (%.0f)", g_box_h);
    g_pending_variant = "smile";
    tick(16);
    CHECK(n_cs_log == 3 && !strcmp(g_cs_log[2], "variant:smile"), "the typing passed @c{smile}: her expression changes (%d, %s)", n_cs_log, n_cs_log > 2 ? g_cs_log[2] : "");
    tick(16);
    CHECK(n_cs_log == 3, "once");
    CHECK(press(BTN_ACTION), "A answers the page");
    draw_bottom();
    CHECK(!strcmp(g_box_text, "@c{smile}Let's go home.") && n_cs_log == 3, "the second page comes, the screen stays on her (%s)", g_box_text);
    CHECK(!strcmp(g_name, "Ivy"), "and her name");
    CHECK(press(BTN_ACTION), "A answers the last page");
    draw_top();
    draw_bottom();
    CHECK(n_done == 0 && n_sounds == 0, "no gains yet, no sounds (%d)", n_sounds);
    tick(250);
    draw_top();
    draw_bottom();
    CHECK(FADE_TOP == 128 && g_fade_bottom == 128 && n_cs_draw == 1, "fadeOut(500): half black over her (%d, %d, %d draws)", FADE_TOP, g_fade_bottom, n_cs_draw);
    CHECK(press(BTN_ACTION), "A is swallowed in the fade");
    tick(250);
    CHECK(n_cs_log == 4 && !strcmp(g_cs_log[3], "hide"), "black: she goes out (%s)", n_cs_log > 3 ? g_cs_log[3] : "");
    draw_top();
    draw_bottom();
    CHECK(FADE_TOP == 255 && g_fade_bottom == 255 && g_box_text[0] == 0 && g_name[0] == 0, "under the black, with nothing in the box (%d, '%s')", FADE_TOP, g_box_text);
    CHECK(press(BTN_ACTION) && n_done == 0, "A is swallowed while she goes out");
    tick(749);
    CHECK(n_cs_log == 4, "750 ms");
    tick(1);
    CHECK(n_cs_log == 5 && !strcmp(g_cs_log[4], "reset"), "then the portrait is forgotten (%s)", n_cs_log > 4 ? g_cs_log[4] : "");
    draw_top();
    draw_bottom();
    CHECK(drew("images/cg/end_m") && n_cs_draw == 0, "the end card is up, fading in over 1 s (alpha %d)", FADE_TOP);
    CHECK(FADE_TOP == 255 && g_box_text[0] == 0, "starting black, no line yet");
    tick(500);
    draw_top();
    CHECK(FADE_TOP == 128, "half way (%d)", FADE_TOP);
    CHECK(press(BTN_ACTION), "A is swallowed in the fade");
    tick(500);
    draw_bottom();
    CHECK(!strcmp(g_box_text, LINE), "then the end card's line (%s)", g_box_text);
    CHECK(press(BTN_ACTION), "A answers it");
    draw_bottom();
    CHECK(!strcmp(g_box_text, "msg2:0"), "and the unlock message follows (%s)", g_box_text);
    press(BTN_ACTION);
    CHECK(n_done == 1, "then the screen leaves");
  }

  /* ---- no text for the ending, a defeat, and the post-mortem's B: no dialogue, the end card at once */
  {
    n_cs_log = 0;
    n_done = 0;
    g_no_ending_text = true;
    result_args a = args(true, NULL, NULL);
    a.ending = true;
    result_screen_open(&a);
    draw_top();
    draw_bottom();
    CHECK(n_cs_log == 1 && drew("images/cg/end_m") && FADE_TOP == 0 && g_fade_bottom == -1, "without the text there is no dialogue and no fade");
    CHECK(!strcmp(g_box_text, LINE), "the line is there at once (%s)", g_box_text);
    g_no_ending_text = false;
    n_cs_log = 0;
    result_args d = args(false, NULL, NULL);
    d.ending = true;
    result_screen_open(&d);
    run(2000);
    draw_bottom();
    CHECK(n_cs_log == 1 && !strcmp(g_box_text, "You were defeated at wave 12."), "a defeat has no ending (%d, %s)", n_cs_log, g_box_text);
    CHECK(press(BTN_ACTION) && n_done == 1, "and A leaves");

    static co_report rep;
    n_pm = 0;
    n_cs_log = 0;
    result_args w = args(true, NULL, &rep);
    w.ending = true;
    result_screen_open(&w);
    run(500);
    run(760);
    CHECK(n_cs_log == 2 && !strcmp(g_cs_log[1], "images/character/rival_f:shock"), "(the win with a report starts with her)");
    press(BTN_ACTION);
    press(BTN_ACTION);
    run(500);
    run(760);
    run(1000);
    CHECK(press(BTN_ACTION) && n_pm == 1, "after her and the end card's line, A opens the post-mortem");
    n_cs_log = 0;
    g_pm.on_exit(PM_BACK, g_pm.arg);
    run(2000);
    draw_bottom();
    CHECK(n_cs_log == 1 && !strcmp(g_cs_log[0], "reset") && !strcmp(g_box_text, LINE), "its B comes back to the line, without her again (%d, %s)", n_cs_log, g_box_text);
  }

  printf("result_gains_test: %d checks, %d failed\n", checks, failed);
  return failed ? 1 : 0;
}
