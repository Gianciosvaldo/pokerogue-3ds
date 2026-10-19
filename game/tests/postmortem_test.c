/* postmortem_test: the post-mortem screen (source/postmortem_screen.c) and its place in the result flow (source/result_screen.c), over the
 * real combat observer and custom-core's reducer, on the host. The drawing layer is faked (no pixels): the fakes record every text drawn
 * on each screen with its position, and a character is 6 px wide in them (wider than the game's font, so a line that fits here fits there).
 *
 *   make postmortem_test && build/postmortem_test
 *
 * Reports are built the way the app builds them: raw WAVE / TURN / BATTLE events through co_observe, then co_finish. Checks the words of
 * every kind of event, the top screen for a defeat, a clear, an unknown last-fallen and an incomplete history, the bottom screen's turn
 * pages, the totals ("at least" when a turn is not whole), the notices, scrolling, the buttons (A / B / LEFT / RIGHT / UP / DOWN), that
 * nothing on either screen names a cause or an attacker, that every line stays on its screen, and the flow around it: the result screen's
 * A opens the post-mortem, its B comes back to the result screen with the line waiting again, its A leaves once, and a result without a
 * report leaves straight away. Exit 1 on a failed check. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "combat_observer.h"
#include "cc/model.h"
#include "gm_enum_names.h"
#include "i18n.h"
#include "postmortem_screen.h"
#include "result_screen.h"
#include "char_sprite.h"
#include "dialogue_view.h"
#include "toast_view.h"
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

/* ------------------------------------------------------------------ fakes */

typedef struct {
  float x, y;
  int screen;     /* 0 top, 1 bottom */
  int space;
  text_style style;
  char text[160];
} drawn_t;
#define DRAWN_MAX 256
static drawn_t g_drawn[DRAWN_MAX];
static int n_drawn;
static int g_which;       /* the physical screen being drawn: 0 top, 1 bottom */
static int g_space;

static ui_screen* g_top;  /* the screen ui_set made current */
static int n_set, n_select;

void ui_set(ui_screen* s) {
  g_top = s;
  n_set++;
}
void ui_play_select(void) { n_select++; }
bool ui_space(ui_space_id sp) {
  const bool top = sp == UI_FIELD || sp == UI_FIELD_UI || sp == UI_TOP_CANVAS || sp == UI_TOP_CANVAS_UI || sp == UI_PHYS_TOP;
  g_space = (int)sp;
  return top == (g_which == 0);
}
void rr_rect(float x, float y, float w, float h, rr_color c) { (void)x, (void)y, (void)w, (void)h, (void)c; }
void ui_window(float x, float y, float w, float h, win_variant v) { (void)x, (void)y, (void)w, (void)h, (void)v; }
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
float text_width(text_style st, const char* s, const text_opts* o) {
  (void)st, (void)o;
  return 6.0f * (float)strlen(s);
}
float text_draw(text_style st, float x, float y, const char* s, const text_opts* o) {
  if (n_drawn < DRAWN_MAX) {
    drawn_t* d = &g_drawn[n_drawn++];
    d->style = st;
    d->x = x;
    d->y = y;
    d->screen = g_which;
    d->space = g_space;
    snprintf(d->text, sizeof d->text, "%s", s);
    /* right-aligned and centred text: remember where its left edge is */
    if (o && o->origin_x > 0) d->x = x - o->origin_x * 6.0f * (float)strlen(s);
  }
  return 6.0f * (float)strlen(s);
}
void text_wrap(text_style st, const char* s, const text_opts* o, char* out, int n) {
  (void)st, (void)o;
  snprintf(out, (size_t)n, "%s", s);
}
const char* i18n_t(char* out, int n, const char* key, ...) {
  snprintf(out, (size_t)n, "[%s]", key);
  return out;
}

static const char* species_name(int id) {
  switch (id) {
    case 25: return "Pikachu";
    case 16: return "Pidgey";
    case 19: return "Rattata";
    case 10: return "Caterpie";
    case 150: return "Mewtwo-with-a-very-very-very-long-name-that-cannot-fit-on-any-line-of-this-screen";
    default: return "?";
  }
}
const char* i18n_species_name(int id) { return species_name(id); }

/* what a win gave and the bars that show it (win_gains.c, menus/toast_view.c): this test has no gains */
bool wg_is_toast(const wg_item* it) { (void)it; return false; }
void wg_toast_of(const wg_item* it, wg_toast* o) { (void)it, (void)o; }
void wg_message_of(const wg_item* it, char* out, int n) { (void)it, (void)n; out[0] = 0; }
void tv_draw(const tv_info* t, float slide) { (void)t, (void)slide; }
void ui_play_sound(const char* key) { (void)key; }

/* the rival's ending (char_sprite.c, menus/dialogue_view.c, the msgbox's @c): this test has none */
void chs_show(const char* atlas, const char* variant) { (void)atlas, (void)variant; }
void chs_set_variant(const char* variant) { (void)variant; }
void chs_hide(void) {}
void chs_reset(void) {}
void chs_update(float dt) { (void)dt; }
void chs_draw(void) {}
bool chs_busy(void) { return false; }
bool chs_shown(void) { return false; }
float chs_x(void) { return 0; }
void dv_namebox(float x, float y, const char* name, int window_style) { (void)x, (void)y, (void)name, (void)window_style; }
int ui_window_style(void) { return 5; }

/* run_summary / run_text, used by the result screen's drawing only */
void rsum_draw(const rsv_info* in, float x, float y) { (void)in, (void)x, (void)y; }

/* a message box that shows its text at once and waits for A or B */
void msgbox_init(msgbox* m, text_style style, float wrap_w, int max_lines) {
  (void)style, (void)wrap_w, (void)max_lines;
  memset(m, 0, sizeof *m);
}
void msgbox_show(msgbox* m, const char* text, float delay, bool prompt) {
  (void)delay;
  snprintf(m->text, sizeof m->text, "%s", text);
  m->prompt = prompt;
  m->waiting = true;
  m->done = false;
}
void msgbox_clear(msgbox* m) { m->text[0] = 0; m->waiting = false; m->done = true; }
const char* msgbox_take_variant(msgbox* m) { (void)m; return NULL; }
void msgbox_update(msgbox* m, float dt) { (void)m, (void)dt; }
bool msgbox_input(msgbox* m, ui_button b) {
  if (b != BTN_ACTION && b != BTN_CANCEL) return false;
  if (m->waiting) {
    m->waiting = false;
    m->done = true;
  }
  return true;
}
void msgbox_draw(const msgbox* m, float x, float y) { (void)m, (void)x, (void)y; }
bool msgbox_done(const msgbox* m) { return m->done; }

/* ------------------------------------------------------------------ helpers */

static void reset_draw(void) { n_drawn = 0; }
static void draw(int which) {
  reset_draw();
  g_which = which;
  g_top->draw(g_top, which ? RR_BOTTOM : RR_TOP);
}
static bool has_text(const char* needle) {
  for (int i = 0; i < n_drawn; i++)
    if (strstr(g_drawn[i].text, needle)) return true;
  return false;
}
static void press(ui_button b) { g_top->input(g_top, b); }

/* every line starts and ends on its screen (320 x 240 bottom, the field's 320 x 180 on top) */
static void check_on_screen(int which) {
  const float w = 320, h = which ? 240 : 180;
  for (int i = 0; i < n_drawn; i++) {
    const drawn_t* d = &g_drawn[i];
    const float right = d->x + 6.0f * (float)strlen(d->text);
    CHECK(d->x >= 0 && d->y >= 0 && d->y + 12 <= h + 1, "\"%s\" is off the screen at (%.0f, %.0f)", d->text, d->x, d->y);
    CHECK(right <= w + 1, "\"%s\" runs to x %.0f", d->text, right);
    /* the 9 and 12 px emerald styles (TS_MOVE_INFO_CONTENT, TS_BATTLE_INFO) were unreadable in Azahar: only the 16 px emerald (TS_WINDOW, TS_SUMMARY) and the 8 px pixel font (TS_INSTRUCTIONS_TEXT) are used */
    CHECK(d->style == TS_WINDOW || d->style == TS_SUMMARY || d->style == TS_INSTRUCTIONS_TEXT, "\"%s\" is drawn in a style that was not readable on screen (%d)", d->text, (int)d->style);
  }
}

/* raw events, as in observer_test.c */
static pr_event ev(int type, int battler, int species, int hp, int max_hp) {
  pr_event e;
  memset(&e, 0, sizeof e);
  e.type = (uint8_t)type;
  e.battler = (int8_t)battler;
  e.target = -1;
  e.field[battler].species = (uint16_t)species;
  e.field[battler].hp = hp;
  e.field[battler].max_hp = max_hp;
  return e;
}
static pr_event dmg(int b, int sp, int hp, int mx, int amount) {
  pr_event e = ev(PR_EV_DAMAGE, b, sp, hp, mx);
  e.id = 3;
  e.value = amount;
  return e;
}
static pr_event heal(int b, int sp, int hp, int mx, int amount) {
  pr_event e = ev(PR_EV_HP, b, sp, hp, mx);
  e.value = amount;
  return e;
}
static gc_event gev(int kind, int a, int b) {
  gc_event g;
  memset(&g, 0, sizeof g);
  g.kind = (uint8_t)kind;
  g.a = a;
  g.b = b;
  return g;
}
static uint32_t dn;
static void turn(int wave_or_0, int t, const pr_event* be, int n, bool battle_overflow) {
  gc_event q[3];
  int nq = 0;
  if (wave_or_0) q[nq++] = gev(GC_EV_WAVE, wave_or_0, 0);
  q[nq++] = gev(GC_EV_TURN, t, 0);
  q[nq++] = gev(GC_EV_BATTLE, 0, n);
  co_observe(++dn, q, nq, be, n, battle_overflow, false);
}
static void begin(bool resumed) {
  co_begin_run(resumed);
  dn = 0;
}

/* A defeat over three turns of wave 12. Turn 1: both sides hit. Turn 2: a heal, poison, a flagged turn. Turn 3: a hit and the faint. */
static const co_report* make_defeat(void) {
  begin(false);
  pr_event t1[] = {ev(PR_EV_SWITCH_IN, 0, 25, 55, 55), ev(PR_EV_SWITCH_IN, 2, 16, 40, 40), dmg(0, 25, 31, 55, 24), dmg(2, 16, 22, 40, 18)};
  turn(12, 1, t1, 4, false);
  pr_event t2[] = {heal(0, 25, 41, 55, 10), heal(0, 25, 33, 55, -8)};
  turn(0, 2, t2, 2, true); /* the game says it lost events in this drain */
  pr_event t3[] = {dmg(0, 25, 0, 55, 33), ev(PR_EV_FAINT, 0, 25, 0, 55)};
  turn(0, 3, t3, 2, false);
  CHECK(co_finish(false, 1, 12), "finish");
  return co_report_get();
}

static const co_report* make_report_with_n_events(int n) {
  begin(false);
  static pr_event big[64];
  for (int i = 0; i < n; i++) big[i] = dmg(0, 25, 1000 - i, 1000, 1);
  turn(3, 1, big, n, false);
  CHECK(co_finish(false, 1, 3), "finish");
  return co_report_get();
}

/* ------------------------------------------------------------------ the words */

static void describe_one(const char* want, int kind, int side, int species, uint16_t known, int shown, int delta, int hp_after) {
  cc_event e;
  memset(&e, 0, sizeof e);
  e.kind = (uint8_t)kind;
  e.target_side = (uint8_t)side;
  e.target_species = (uint16_t)species;
  e.known = known;
  e.shown = shown;
  e.hp_delta = delta;
  e.hp_after = hp_after;
  char out[160];
  pm_describe(&e, out, sizeof out);
  CHECK(!strcmp(out, want), "described \"%s\", wanted \"%s\"", out, want);
}

static void test_describe(void) {
  describe_one("Your Pikachu took 24 damage (HP 31)", CC_EV_DAMAGE, CC_SIDE_PLAYER, 25, CC_EK_SHOWN | CC_EK_HP_AFTER, 24, 0, 31);
  describe_one("Foe Pidgey took 5 damage", CC_EV_DAMAGE, CC_SIDE_ENEMY, 16, CC_EK_SHOWN, 5, 0, 0);
  describe_one("Foe Pidgey was hit", CC_EV_DAMAGE, CC_SIDE_ENEMY, 16, 0, 0, 0, 0);
  describe_one("Your Pikachu recovered 10 HP (HP 41)", CC_EV_HEAL, CC_SIDE_PLAYER, 25, CC_EK_SHOWN | CC_EK_HP_AFTER, 10, 0, 41);
  describe_one("Your Pikachu recovered HP", CC_EV_HEAL, CC_SIDE_PLAYER, 25, 0, 0, 0, 0);
  describe_one("Your Pikachu lost 8 HP, no hit shown (HP 33)", CC_EV_HP_LOSS, CC_SIDE_PLAYER, 25, CC_EK_HP_DELTA | CC_EK_HP_AFTER, 0, -8, 33);
  describe_one("Foe Rattata lost HP, no hit shown", CC_EV_HP_LOSS, CC_SIDE_ENEMY, 19, 0, 0, 0, 0);
  describe_one("Your Pikachu fainted", CC_EV_FAINT, CC_SIDE_PLAYER, 25, 0, 0, 0, 0);
  describe_one("Your Pikachu left the field", CC_EV_SWITCH_OUT, CC_SIDE_PLAYER, 25, 0, 0, 0, 0);
  describe_one("Foe Caterpie came out", CC_EV_SWITCH_IN, CC_SIDE_ENEMY, 10, 0, 0, 0, 0);
}

/* ------------------------------------------------------------------ the screen */

static const char* const FORBIDDEN[] = {"killed", "caused", "because", "due to", "by a ", "by the ", "attack", "poison", "burn", "weather", "recoil", "move", "fatal"};

static void check_no_cause(void) {
  for (size_t i = 0; i < sizeof FORBIDDEN / sizeof *FORBIDDEN; i++) CHECK(!has_text(FORBIDDEN[i]), "the screen says \"%s\"", FORBIDDEN[i]);
}

static int g_exit_how, g_exit_calls;
static void on_exit_cb(pm_exit how, void* arg) {
  (void)arg;
  g_exit_how = how;
  g_exit_calls++;
}

static void test_screen(void) {
  const co_report* r = make_defeat();
  CHECK(r && r->history.n_turns == 3, "report with 3 turns");
  if (!r) return;
  g_exit_calls = 0;
  const pm_args a = {r, on_exit_cb, NULL};
  CHECK(postmortem_open(&a) && g_top && !strcmp(g_top->name, "postmortem") && (g_top->flags & UI_FADE), "opens as the screen");

  /* top screen: a defeat */
  draw(0);
  CHECK(has_text("Defeated at wave 12") && !has_text("Cleared"), "top: outcome");
  CHECK(has_text("Last to fall: Pikachu"), "top: last to fall");
  CHECK(has_text("Final blow: source not recorded"), "top: no source claimed");
  CHECK(has_text("history incomplete") && has_text("Last 3 turns"), "top: turn 2 was flagged, so the history is incomplete");
  CHECK(has_text("It names no cause"), "top: the footnote");
  check_on_screen(0);
  check_no_cause();

  /* bottom: the newest turn first */
  draw(1);
  CHECK(has_text("Wave 12, turn 3") && has_text("3/3") && has_text("< "), "bottom: header, page, left arrow");
  CHECK(has_text("Your Pikachu took 33 damage (HP 0)") && has_text("Your Pikachu fainted"), "bottom: turn 3 rows");
  CHECK(has_text("Damage shown: yours 33, foe 0") && !has_text("At least"), "bottom: totals of a whole turn");
  CHECK(!has_text("Incomplete"), "bottom: no notice on a whole turn");
  CHECK(has_text("A: continue") && has_text("B: back"), "bottom: the buttons");
  check_on_screen(1);
  check_no_cause();

  /* LEFT: turn 2, which lost events */
  press(BTN_LEFT);
  draw(1);
  CHECK(has_text("Wave 12, turn 2") && has_text("2/3"), "turn 2");
  CHECK(has_text("Incomplete turn: some events were not observed"), "turn 2 says it is incomplete");
  CHECK(has_text("Your Pikachu recovered 10 HP (HP 41)") && has_text("Your Pikachu lost 8 HP, no hit shown (HP 33)"), "turn 2 rows");
  CHECK(has_text("At least: damage shown") == false, "no hits in turn 2: no damage line");
  CHECK(has_text("At least: HP loss (measured): yours 8, foe 0"), "turn 2: a loss is measured, never shown, and only a lower bound");
  check_on_screen(1);
  check_no_cause();

  /* LEFT again: turn 1 (wave 12's first); at the oldest page LEFT does nothing */
  press(BTN_LEFT);
  draw(1);
  CHECK(has_text("Wave 12, turn 1") && has_text("1/3") && !has_text("< 1"), "turn 1, no arrow to the left");
  CHECK(has_text("Your Pikachu came out") && has_text("Foe Pidgey came out"), "turn 1: switch-ins");
  CHECK(has_text("Damage shown: yours 24, foe 18"), "turn 1 totals");
  const int sel = n_select;
  press(BTN_LEFT);
  CHECK(n_select == sel, "LEFT at the oldest turn does nothing");
  draw(1);
  CHECK(has_text("1/3"), "still turn 1");
  press(BTN_RIGHT);
  press(BTN_RIGHT);
  press(BTN_RIGHT);
  draw(1);
  CHECK(has_text("3/3") && !has_text(" >"), "RIGHT stops at the newest turn");

  /* the buttons that leave */
  press(BTN_ACTION);
  CHECK(g_exit_calls == 1 && g_exit_how == PM_CONTINUE, "A continues");
  press(BTN_CANCEL);
  CHECK(g_exit_calls == 2 && g_exit_how == PM_BACK, "B goes back");
  CHECK(!g_top->input(g_top, BTN_MENU), "other buttons are not used");
}

static void test_clear_and_unknown(void) {
  /* a clear: no fallen Pokemon, no final blow line */
  begin(false);
  pr_event e[] = {ev(PR_EV_FAINT, 2, 16, 0, 40)};
  turn(200, 1, e, 1, false);
  co_finish(true, 3, 200);
  const co_report* r = co_report_get();
  const pm_args a = {r, on_exit_cb, NULL};
  CHECK(postmortem_open(&a), "open");
  draw(0);
  CHECK(has_text("Cleared at wave 200") && !has_text("Defeated") && !has_text("Last to fall") && !has_text("Final blow"), "a clear's top screen");
  check_on_screen(0);

  /* a defeat whose last fall is not in the history */
  begin(false);
  pr_event f[] = {ev(PR_EV_SWITCH_IN, 0, 25, 55, 55)};
  turn(5, 1, f, 1, false);
  co_finish(false, 3, 5);
  r = co_report_get();
  const pm_args b = {r, on_exit_cb, NULL};
  CHECK(postmortem_open(&b), "open");
  draw(0);
  CHECK(has_text("Last to fall: not recorded") && has_text("Final blow: source not recorded") && has_text("nothing missing"), "an unknown last-fallen is said to be unknown");

  /* a resumed run's history is incomplete by the report's own flag even when its turns are whole */
  begin(true);
  turn(5, 1, f, 1, false);
  co_finish(false, 3, 5);
  r = co_report_get();
  const pm_args c = {r, on_exit_cb, NULL};
  postmortem_open(&c);
  draw(0);
  CHECK(has_text("history incomplete"), "continued from a save: the history says so");

  /* nothing observed at all */
  begin(false);
  co_finish(false, 3, 5);
  r = co_report_get();
  const pm_args d = {r, on_exit_cb, NULL};
  CHECK(postmortem_open(&d), "open with an empty history");
  draw(0);
  CHECK(has_text("No battle events recorded"), "top: empty");
  draw(1);
  CHECK(has_text("No battle events were recorded for this run."), "bottom: empty");
  check_on_screen(1);
  press(BTN_LEFT);
  press(BTN_RIGHT);
  press(BTN_UP);
  press(BTN_DOWN);
  draw(1);
  CHECK(has_text("No battle events"), "the pager does nothing on an empty history");
  g_exit_calls = 0;
  press(BTN_ACTION);
  CHECK(g_exit_calls == 1, "A still leaves");

  /* no report: nothing opens */
  const pm_args none = {NULL, on_exit_cb, NULL};
  const int sets = n_set;
  CHECK(!postmortem_open(&none) && n_set == sets, "no report, no screen");
  co_report fake;
  memset(&fake, 0, sizeof fake);
  const pm_args invalid = {&fake, on_exit_cb, NULL};
  CHECK(!postmortem_open(&invalid) && n_set == sets, "an invalid report, no screen");
  CHECK(!postmortem_open(NULL), "NULL");
}

static void test_scroll_and_fit(void) {
  /* 64 events on one turn: ten rows show, the rest are counted */
  const co_report* r = make_report_with_n_events(64);
  const pm_args a = {r, on_exit_cb, NULL};
  postmortem_open(&a);
  draw(1);
  CHECK(has_text("54 more below") && !has_text("more above"), "64 events, 10 visible, 54 below");
  int rows = 0;
  for (int i = 0; i < n_drawn; i++) rows += strstr(g_drawn[i].text, "took 1 damage") != NULL;
  CHECK(rows == 10, "ten rows drawn, %d", rows);
  press(BTN_UP);
  draw(1);
  CHECK(has_text("54 more below"), "UP at the top does nothing");
  for (int i = 0; i < 100; i++) press(BTN_DOWN);
  draw(1);
  CHECK(has_text("54 more above") && !has_text("more below"), "scrolled to the end: nothing below");
  rows = 0;
  for (int i = 0; i < n_drawn; i++) rows += strstr(g_drawn[i].text, "took 1 damage") != NULL;
  CHECK(rows == 10, "still ten rows, %d", rows);
  check_on_screen(1);
  press(BTN_UP);
  draw(1);
  CHECK(has_text("53 more above") && has_text("1 more below"), "one up");
  /* a long notice on a turn that scrolls stops before the "N more above" label (x 236) */
  begin(true); /* began mid-run: that notice is added to the lost-events one */
  {
    static pr_event big[14];
    for (int i = 0; i < 14; i++) big[i] = dmg(0, 25, 1000 - i, 1000, 1);
    turn(3, 1, big, 14, true);
    co_finish(false, 1, 3);
    const co_report* rl = co_report_get();
    const pm_args al = {rl, on_exit_cb, NULL};
    postmortem_open(&al);
    for (int i = 0; i < 3; i++) press(BTN_DOWN);
    draw(1);
    bool seen = false;
    for (int i = 0; i < n_drawn; i++)
      if (!strncmp(g_drawn[i].text, "Incomplete turn", 15)) {
        seen = true;
        CHECK(g_drawn[i].x + 6.0f * (float)strlen(g_drawn[i].text) <= 236.0f, "the notice \"%s\" runs into the \"more above\" label", g_drawn[i].text);
      }
    CHECK(seen && has_text("more above"), "a notice and a scrolled list");
  }
  /* a turn change starts at the top */
  const co_report* r2 = make_defeat();
  const pm_args b = {r2, on_exit_cb, NULL};
  postmortem_open(&b);
  draw(1);
  CHECK(!has_text("more above"), "a new report starts at the top");

  /* a species name too long for the line is cut, not run off the screen */
  begin(false);
  pr_event big[] = {dmg(0, 150, 10, 20, 5)};
  turn(2, 1, big, 1, false);
  co_finish(false, 1, 2);
  const pm_args c = {co_report_get(), on_exit_cb, NULL};
  postmortem_open(&c);
  draw(1);
  CHECK(has_text("Your Mewtwo-with") && has_text(".."), "a long line is cut with ..");
  check_on_screen(1);
  draw(0);
  CHECK(has_text("Last to fall: not recorded"), "top, no faint in it");
}

/* ------------------------------------------------------------------ the result flow */

static int g_done_calls;
static void on_done_cb(void* arg) {
  (void)arg;
  g_done_calls++;
}

static void test_result_flow(void) {
  const co_report* r = make_defeat();
  result_args a;
  memset(&a, 0, sizeof a);
  a.victory = false;
  a.report = r;
  a.run.status = RSV_OK;
  a.run.wave = 12;
  a.on_done = on_done_cb;
  g_done_calls = 0;
  g_exit_calls = 0;
  result_screen_open(&a);
  CHECK(g_top && !strcmp(g_top->name, "run-result"), "the result screen is up");
  press(BTN_ACTION); /* the line */
  CHECK(!strcmp(g_top->name, "postmortem") && g_done_calls == 0, "A on the line opens the post-mortem, nothing has left yet");
  press(BTN_CANCEL); /* back */
  CHECK(!strcmp(g_top->name, "run-result") && g_done_calls == 0, "B comes back to the result screen");
  press(BTN_ACTION);
  CHECK(!strcmp(g_top->name, "postmortem"), "and A opens it again, the line was waiting");
  draw(1);
  CHECK(has_text("3/3"), "from the newest turn again");
  press(BTN_ACTION); /* continue */
  CHECK(g_done_calls == 1, "A in the post-mortem leaves, once");

  /* no report: the result screen leaves on A as it did before */
  a.report = NULL;
  g_done_calls = 0;
  result_screen_open(&a);
  press(BTN_ACTION);
  CHECK(g_done_calls == 1 && !strcmp(g_top->name, "run-result"), "without a report A leaves at once");
  press(BTN_ACTION);
  CHECK(g_done_calls == 1, "and only once");

  /* an invalid report is no report */
  co_report bad;
  memset(&bad, 0, sizeof bad);
  a.report = &bad;
  g_done_calls = 0;
  result_screen_open(&a);
  press(BTN_ACTION);
  CHECK(g_done_calls == 1, "an invalid report leaves at once");
}

int main(void) {
  test_describe();
  test_screen();
  test_clear_and_unknown();
  test_scroll_and_fit();
  test_result_flow();
  printf("postmortem_test: %d checks, %d failed\n", checks, failed);
  return failed ? 1 : 0;
}
