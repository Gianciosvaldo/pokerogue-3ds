/* capsule_test: the capsule-machine presentation prototype (source/capsule_model.c and source/capsule_screen.c) on the host, with a fake
 * drawing layer (no pixels: the fakes record every text and every filled rectangle with the screen it was drawn on).
 *
 *   make capsule_test && build/capsule_test [fuzz sessions]
 *
 * Model: what makes a fixture usable (1, 10 and 25 items; none, 26, a bad tier, a label without an end or empty are refused); the slots
 * (reading order, inside the field, distinct); the timeline (the pull, the drop, settled within 1.5 s of the pull but not at once, in any
 * frame rate: dt in milliseconds, not seconds), dt of 0, below 0, NaN, infinite and a long pause; a skip at every stage; the pull and the
 * settle reported once; the same fixture always plays the same way and is never written to; the final positions follow the fixture's
 * order, not the landing order; no ambient random number is drawn.
 * Screen: refusals; the words (nothing offers a purchase or an unlock); one pull however often A is pressed or held; a skip shows the
 * supplied results at once; on_settled once whether the time ran out or the player skipped; on_exit once; nothing after leaving; a
 * callback of an earlier opening never reaches a later one; a callback that reopens the screen; the result list in the fixture's order; the
 * number of rectangles per frame. Then a seeded fuzz of random buttons and frame times. Exit 1 on a failed check. */
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "capsule_model.h"
#include "capsule_screen.h"
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
  float x, y, x0, ox;
  int screen;
  text_style style;
  char text[200];
} drawn_t;
#define DRAWN_MAX 256
static drawn_t g_drawn[DRAWN_MAX];
static int n_drawn;
typedef struct {
  float x, y, w, h;
  int screen;
} rect_t;
#define RECT_MAX 1024
static rect_t g_rects[RECT_MAX];
static int n_rects;
static int g_which;
static ui_screen* g_top;
static int n_set, n_select;
static bool g_repeat;

void ui_set(ui_screen* s) {
  if (g_top && g_top->leave) g_top->leave(g_top);
  g_top = s;
  if (s->enter) s->enter(s);
  n_set++;
}
void ui_play_select(void) { n_select++; }
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
    d->x0 = x;
    d->ox = o ? o->origin_x : 0;
    d->x = x - d->ox * 6.0f * (float)strlen(s);
    d->y = y;
    d->screen = g_which;
    snprintf(d->text, sizeof d->text, "%s", s);
  }
  return 6.0f * (float)strlen(s);
}

static bool has_text(const char* needle) {
  for (int i = 0; i < n_drawn; i++)
    if (strstr(g_drawn[i].text, needle)) return true;
  return false;
}
static void draw(int which) {
  n_drawn = 0;
  n_rects = 0;
  g_which = which;
  g_top->draw(g_top, which ? RR_BOTTOM : RR_TOP);
}
static void press(ui_button b) { g_top->input(g_top, b); }
static void press_held(ui_button b) {
  g_repeat = true;
  g_top->input(g_top, b);
  g_repeat = false;
}
static void tick(float ms) { g_top->update(g_top, ms); }
static void run_ms(float ms, float frame) {
  for (float t = 0; t < ms; t += frame) tick(frame);
}

/* every line on its screen, in a readable style, and none a word the preview must not say */
static void check_screen(int which) {
  const float w = 320, h = which ? 240 : 180;
  for (int i = 0; i < n_drawn; i++) {
    const drawn_t* d = &g_drawn[i];
    const float len = 6.0f * (float)strlen(d->text);
    CHECK(d->x >= 0 && d->y >= 0 && d->y + 12 <= h + 1, "\"%s\" is off the screen at (%.0f, %.0f)", d->text, d->x, d->y);
    CHECK(d->x + len <= w + 1, "\"%s\" runs to x %.0f", d->text, d->x + len);
    CHECK(d->style == TS_WINDOW || d->style == TS_SUMMARY || d->style == TS_INSTRUCTIONS_TEXT, "\"%s\" is in an unreadable style", d->text);
    static const char* const banned[] = {"Buy", "buy ", "Purchase", "purchase", "Price", "price", "oucher", "oin", "urrency", "nlock", "caught", "atch"};
    for (size_t k = 0; k < sizeof banned / sizeof banned[0]; k++) CHECK(!strstr(d->text, banned[k]), "\"%s\" offers or implies \"%s\"", d->text, banned[k]);
  }
  /* the capsules stay near the field (the chute is above its top edge) */
  if (which == 0)
    for (int i = 0; i < n_rects; i++)
      if (g_rects[i].w <= 60) /* the capsules' parts; the backdrop panels are bigger */
        CHECK(g_rects[i].x >= -30 && g_rects[i].x + g_rects[i].w <= 350 && g_rects[i].y >= -40 && g_rects[i].y <= 200, "a rectangle at (%.0f, %.0f) is off the field", g_rects[i].x, g_rects[i].y);
}

static cap_fixture fx(int n) {
  cap_fixture f;
  capsule_debug_fixture(n, &f);
  return f;
}

/* ------------------------------------------------------------------ model tests */

static void test_fixture_validity(void) {
  cap_fixture f = fx(10);
  CHECK(cap_fixture_valid(&f), "10 items");
  CHECK(cap_fixture_valid(&(cap_fixture){0}) == false, "no items is refused");
  f = fx(25);
  CHECK(cap_fixture_valid(&f), "25 items");
  f.n = 26;
  CHECK(!cap_fixture_valid(&f), "26 items are refused");
  f = fx(1);
  CHECK(cap_fixture_valid(&f), "1 item");
  f.item[0].tier = CAP_TIERS;
  CHECK(!cap_fixture_valid(&f), "a tier out of range");
  f = fx(3);
  memset(f.item[1].label, 'x', CAP_LABEL);
  CHECK(!cap_fixture_valid(&f), "a label with no end");
  f = fx(3);
  f.item[2].label[0] = 0;
  CHECK(!cap_fixture_valid(&f), "an empty label");
  CHECK(!cap_fixture_valid(NULL), "NULL");
  cap_model m;
  CHECK(!cap_init(&m, NULL) && !cap_init(NULL, &f), "init refuses NULL");
  f.n = 0;
  CHECK(!cap_init(&m, &f), "init refuses an invalid fixture");
  cap_fixture bad = fx(3);
  bad.item[0].tier = 9;
  memset(&m, 0xAB, sizeof m);
  CHECK(!cap_init(&m, &bad) && m.stage == CAP_IDLE && m.fx.n == 0, "and leaves the model zeroed");
  for (int i = 0; i <= 8; i++) {
    cap_fixture t = fx(5);
    (void)t;
  }
  CHECK(!capsule_debug_fixture(0, &f) && !capsule_debug_fixture(26, &f) && !capsule_debug_fixture(5, NULL), "the debug fixture refuses 0, 26 and NULL");
}

static void test_slots(void) {
  for (int n = 1; n <= CAP_MAX; n++) {
    float px[CAP_MAX], py[CAP_MAX];
    for (int i = 0; i < n; i++) {
      cap_slot(n, i, &px[i], &py[i]);
      CHECK(px[i] - CAP_RADIUS >= 0 && px[i] + CAP_RADIUS <= CAP_FIELD_W && py[i] - CAP_RADIUS >= 0 && py[i] + CAP_RADIUS <= CAP_FIELD_H, "slot %d of %d inside the field (%.0f, %.0f)", i, n, px[i], py[i]);
      for (int k = 0; k < i; k++) CHECK(fabsf(px[i] - px[k]) >= 2 * CAP_RADIUS || fabsf(py[i] - py[k]) >= 2 * CAP_RADIUS, "slots %d and %d of %d overlap", k, i, n);
      if (i > 0 && i / CAP_COLS == (i - 1) / CAP_COLS) CHECK(px[i] > px[i - 1], "reading order, left to right");
      if (i > 0 && i / CAP_COLS != (i - 1) / CAP_COLS) CHECK(py[i] > py[i - 1], "reading order, top to bottom");
    }
  }
  float x, y;
  cap_slot(1, 0, &x, &y);
  CHECK(x == 160.0f, "one item is centred");
  cap_slot(7, 5, &x, &y);
  float x2;
  cap_slot(7, 6, &x2, &y);
  CHECK((x + x2) / 2 == 160.0f, "a short last row is centred on itself");
}

static bool bodies_equal(const cap_model* a, const cap_model* b) { return memcmp(a->body, b->body, sizeof a->body) == 0 && a->stage == b->stage && a->t == b->t; }

static void test_timeline(void) {
  const int sizes[] = {1, 10, 25};
  for (int si = 0; si < 3; si++) {
    const int n = sizes[si];
    cap_fixture f = fx(n);
    cap_model m;
    CHECK(cap_init(&m, &f) && m.stage == CAP_IDLE, "init (%d)", n);
    cap_step(&m, 100);
    CHECK(m.stage == CAP_IDLE && m.t == 0, "time does nothing before the pull");
    CHECK(cap_pull(&m) && m.stage == CAP_PULL, "the pull is accepted");
    CHECK(!cap_pull(&m), "and only once");
    float ms = 0;
    bool saw_drop = false, bounced = false;
    int guard = 0;
    while (m.stage != CAP_SETTLED && guard++ < 1000) {
      cap_step(&m, 16.667f);
      ms += 16.667f;
      if (m.stage == CAP_DROP) saw_drop = true;
      for (int i = 0; i < n; i++) {
        const cap_body* b = &m.body[i];
        CHECK(b->y <= b->sy + 0.001f, "a capsule never goes below its slot (%d of %d at %.0f ms)", i, n, ms);
        CHECK(b->open >= 0 && b->open <= 1.0f, "open stays in 0..1");
        if (b->vy < 0) bounced = true;
        CHECK(b->x > -30 && b->x < 350, "a capsule stays near the field");
      }
      if (ms < 400.0f) CHECK(m.stage != CAP_SETTLED, "not settled at %.0f ms", ms);
    }
    CHECK(m.stage == CAP_SETTLED && saw_drop, "settles after a drop (%d)", n);
    CHECK(m.t < CAP_DEADLINE_MS, "settled by the motion itself, before the deadline: t = %.0f (%d)", m.t, n);
    CHECK(fabsf(m.t - (n == 1 ? 1216.0f : 1280.0f)) <= 24.0f, "the timeline is the one measured (1216 ms for one capsule, 1280 for more): %.0f (%d)", m.t, n);
    CHECK(bounced, "the capsules bounced (%d)", n);
    CHECK(ms >= 700.0f && ms <= 1520.0f, "settled between 0.7 s and 1.5 s of frames: %.0f ms (%d)", ms, n);
    for (int i = 0; i < n; i++) {
      CHECK(m.body[i].x == m.body[i].sx && m.body[i].y == m.body[i].sy && m.body[i].open == 1.0f && m.body[i].phase == 2, "item %d rests open at its slot", i);
    }
    CHECK(cap_take_settled(&m) && !cap_take_settled(&m), "the settle is reported once");
    const cap_model before = m;
    cap_step(&m, 50);
    CHECK(memcmp(&before, &m, sizeof m) == 0, "a settled model does not move");
    CHECK(!cap_skip(&m), "nothing to skip when settled");
    CHECK(!cap_pull(&m), "no second pull after the results");
  }
}

static void test_units_and_frame_rate(void) {
  cap_fixture f = fx(25);
  cap_model a, b, c;
  cap_init(&a, &f);
  cap_init(&b, &f);
  cap_init(&c, &f);
  cap_pull(&a);
  cap_pull(&b);
  cap_pull(&c);
  for (int i = 0; i < 100; i++) cap_step(&a, 8);      /* 800 ms in 8 ms frames */
  for (int i = 0; i < 800; i++) cap_step(&b, 1);      /* the same in 1 ms frames */
  for (int i = 0; i < 50; i++) cap_step(&c, 16);      /* and in 16 ms frames */
  CHECK(bodies_equal(&a, &b), "8 ms frames and 1 ms frames give the same state");
  CHECK(bodies_equal(&a, &c), "8 ms frames and 16 ms frames give the same state");
  CHECK(a.t == 800.0f && a.stage == CAP_DROP, "800 ms is 800 in dt milliseconds, still dropping (t = %.0f)", a.t);
  /* dt is milliseconds: a model told 1 means 1 ms, so 1000 of them are 1 s and not 1000 s */
  cap_model d;
  cap_init(&d, &f);
  cap_pull(&d);
  for (int i = 0; i < 100; i++) cap_step(&d, 1);
  CHECK(d.t == 96.0f || d.t == 100.0f || (d.t > 90 && d.t <= 100), "100 ms of 1 ms steps is about 100 ms of time (t = %.0f)", d.t);
  CHECK(d.stage == CAP_PULL, "and still turning the knob");
}

static void test_bad_dt(void) {
  cap_fixture f = fx(10);
  cap_model m, ref;
  cap_init(&m, &f);
  cap_pull(&m);
  cap_step(&m, 40);
  ref = m;
  cap_step(&m, 0);
  cap_step(&m, -5);
  cap_step(&m, NAN);
  cap_step(&m, -INFINITY);
  CHECK(memcmp(&ref, &m, sizeof m) == 0, "dt of 0, below 0, NaN and -inf change nothing");
  cap_step(&m, INFINITY);
  CHECK(m.t - ref.t <= 100.0f && m.t > ref.t, "an infinite dt is a clamped 100 ms (advanced %.0f)", m.t - ref.t);
  cap_step(&m, 1e9f);
  CHECK(m.t - ref.t <= 200.0f, "a pause of a million seconds is two clamped steps (advanced %.0f)", m.t - ref.t);
  /* one call integrates at most CAP_MAX_STEPS steps */
  cap_init(&m, &f);
  cap_pull(&m);
  cap_step(&m, 100);
  CHECK(m.t <= CAP_MAX_STEPS * CAP_STEP_MS && m.t >= 96.0f, "one call is at most 16 steps of 8 ms (t = %.0f)", m.t);
}

static void test_skip_at_each_stage(void) {
  cap_fixture f = fx(25);
  cap_model m;
  cap_init(&m, &f);
  CHECK(!cap_skip(&m) && m.stage == CAP_IDLE, "no skip before the pull");
  /* at several moments of the timeline */
  const float at[] = {0, 8, 190, 208, 400, 700, 1000, 1400};
  for (size_t k = 0; k < sizeof at / sizeof at[0]; k++) {
    cap_init(&m, &f);
    cap_pull(&m);
    for (float t = 0; t < at[k]; t += 8) cap_step(&m, 8);
    const cap_stage was = m.stage;
    if (was == CAP_SETTLED) continue;
    CHECK(cap_skip(&m), "skip at %.0f ms (stage %d)", at[k], (int)was);
    CHECK(m.stage == CAP_SETTLED && m.skipped, "settled by the skip");
    for (int i = 0; i < f.n; i++) CHECK(m.body[i].x == m.body[i].sx && m.body[i].y == m.body[i].sy && m.body[i].open == 1.0f, "every capsule is at its slot and open");
    CHECK(cap_take_settled(&m) && !cap_take_settled(&m), "the settle is reported once after a skip");
    CHECK(!cap_skip(&m), "and a second skip does nothing");
  }
}

/* the deadline: a capsule that cannot reach its slot (here one pushed out of reach by hand) is put there at 1.5 s, so the time to the result
 * is bounded whatever the motion does */
static void test_deadline(void) {
  cap_fixture f = fx(10);
  cap_model m;
  cap_init(&m, &f);
  m.body[3].sy = 1.0e9f; /* a slot it will not reach in time */
  cap_pull(&m);
  float ms = 0;
  while (m.stage != CAP_SETTLED && ms < 3000) {
    cap_step(&m, 8);
    ms += 8;
  }
  CHECK(m.stage == CAP_SETTLED && m.t >= CAP_DEADLINE_MS && m.t <= CAP_DEADLINE_MS + CAP_STEP_MS, "settled at the deadline, not later: t = %.0f", m.t);
  CHECK(m.body[3].y == m.body[3].sy && m.body[3].open == 1.0f, "the stray capsule was put at its slot and opened");
  for (int i = 0; i < f.n; i++) CHECK(m.body[i].phase == 2 && m.body[i].open == 1.0f, "every capsule is open at the deadline");
}

static void test_determinism_and_immutability(void) {
  cap_fixture f = fx(25);
  const cap_fixture copy = f;
  cap_model a, b;
  cap_init(&a, &f);
  cap_init(&b, &f);
  cap_pull(&a);
  cap_pull(&b);
  for (int i = 0; i < 70; i++) {
    cap_step(&a, 16.667f);
    cap_step(&b, 16.667f);
  }
  CHECK(bodies_equal(&a, &b), "the same fixture plays the same way");
  CHECK(memcmp(&f, &copy, sizeof f) == 0, "the fixture was not written to");
  const cap_fixture inside = a.fx;
  f.item[3].tier = 0;
  snprintf(f.item[3].label, CAP_LABEL, "changed");
  CHECK(memcmp(&a.fx, &inside, sizeof inside) == 0, "the model holds its own copy: changing the owner's fixture changes nothing");
  cap_skip(&a);
  CHECK(memcmp(&a.fx, &inside, sizeof inside) == 0, "and a skip does not touch the results");
  /* the final positions follow the fixture's order, not the landing order: item i is always at slot i */
  for (int i = 0; i < a.fx.n; i++) {
    float sx, sy;
    cap_slot(a.fx.n, i, &sx, &sy);
    CHECK(a.body[i].x == sx && a.body[i].y == sy, "item %d is at slot %d", i, i);
  }
  /* but the first capsule to land is the last row, not item 0 */
  cap_model c;
  cap_init(&c, &f);
  cap_pull(&c);
  int landed_at[CAP_MAX];
  for (int k = 0; k < c.fx.n; k++) landed_at[k] = -1;
  for (int i = 0; i < 200; i++) {
    cap_step(&c, 8);
    for (int k = 0; k < c.fx.n; k++)
      if (c.body[k].phase == 2 && landed_at[k] < 0) landed_at[k] = i;
  }
  bool out_of_order = false;
  for (int k = 1; k < c.fx.n; k++)
    if (landed_at[k] < landed_at[k - 1]) out_of_order = true;
  CHECK(out_of_order, "the capsules do not land in the fixture's order, yet the results are shown in it");
}

static void test_no_ambient_random(void) {
  cap_fixture f = fx(25);
  srand(1234);
  const int r1 = rand();
  srand(1234);
  cap_model m;
  cap_init(&m, &f);
  cap_pull(&m);
  for (int i = 0; i < 200; i++) cap_step(&m, 16.667f);
  cap_skip(&m);
  CHECK(rand() == r1, "the model draws no random number");
}

/* ------------------------------------------------------------------ screen tests */

typedef struct {
  int settled, exits, before, after;
  int exit_when_settled;   /* settled had fired when on_exit came */
  bool reopen;             /* on_exit opens the screen again */
  int reopened;
} counts;
static caps_args args_for(const cap_fixture* f, counts* c);
static void on_settled(void* arg) { ((counts*)arg)->settled++; }
static void on_exit_cb(caps_exit how, void* arg) {
  counts* c = arg;
  c->exits++;
  if (how == CAPS_EXIT_BEFORE_REVEAL) c->before++;
  else c->after++;
  if (how == CAPS_EXIT_AFTER_REVEAL && c->settled == 1) c->exit_when_settled++;
  if (c->reopen) {
    static cap_fixture again;
    again = fx(3);
    caps_args a = args_for(&again, c);
    c->reopen = false;
    if (capsule_screen_open(&a)) c->reopened++;
  }
}
static caps_args args_for(const cap_fixture* f, counts* c) {
  caps_args a;
  memset(&a, 0, sizeof a);
  a.fixture = f;
  a.on_settled = on_settled;
  a.on_exit = on_exit_cb;
  a.arg = c;
  return a;
}
static void reset_fakes(void) {
  g_top = NULL;
  n_set = n_select = 0;
  g_repeat = false;
  n_drawn = n_rects = 0;
}

static void test_screen_refusals(void) {
  reset_fakes();
  counts c = {0};
  CHECK(!capsule_screen_open(NULL), "NULL is refused");
  caps_args a = args_for(NULL, &c);
  CHECK(!capsule_screen_open(&a), "no fixture is refused");
  cap_fixture f = fx(3);
  f.n = 0;
  a = args_for(&f, &c);
  CHECK(!capsule_screen_open(&a), "an empty preview is refused");
  f = fx(3);
  f.n = 26;
  CHECK(!capsule_screen_open(&a), "26 results are refused");
  f = fx(3);
  f.item[1].tier = 7;
  CHECK(!capsule_screen_open(&a), "a bad tier is refused");
  CHECK(n_set == 0 && !g_top, "and nothing was opened for any of them");
}

static void test_screen_words_and_pull(void) {
  reset_fakes();
  counts c = {0};
  cap_fixture f = fx(10);
  caps_args a = args_for(&f, &c);
  CHECK(capsule_screen_open(&a) && n_set == 1, "opens");
  draw(0);
  check_screen(0);
  CHECK(has_text("Nothing is bought"), "the top screen says it is a preview and buys nothing");
  draw(1);
  check_screen(1);
  CHECK(has_text("Pull the knob") && has_text("A: pull"), "the bottom screen offers the pull and its button");
  press(BTN_RIGHT);
  press(BTN_UP);
  CHECK(n_select == 0, "other buttons do nothing before the pull");
  press(BTN_ACTION);
  CHECK(n_select == 1, "A pulls");
  press(BTN_ACTION);
  press(BTN_SUBMIT);
  CHECK(n_select == 1, "A or SUBMIT again is not a second pull");
  tick(16.7f);
  draw(1);
  check_screen(1);
  CHECK(has_text("START: skip") || has_text("B or START"), "while it runs the bottom screen offers the skip");
  CHECK(c.settled == 0 && c.exits == 0, "no callback yet");
}

static void test_held_buttons(void) {
  reset_fakes();
  counts c = {0};
  cap_fixture f = fx(10);
  caps_args a = args_for(&f, &c);
  capsule_screen_open(&a);
  press_held(BTN_ACTION);
  CHECK(n_select == 0, "the auto-repeat of a held A is not a pull");
  press(BTN_ACTION);
  CHECK(n_select == 1, "the first press is");
  for (int i = 0; i < 20; i++) press_held(BTN_ACTION);
  press_held(BTN_CANCEL);
  press_held(BTN_MENU);
  tick(16.7f);
  CHECK(c.settled == 0, "a held A, B or START never skips");
  run_ms(2000, 16.7f);
  CHECK(c.settled == 1, "it settles by itself");
  run_ms(1000, 16.7f);
  for (int i = 0; i < 10; i++) press_held(BTN_ACTION);
  press_held(BTN_CANCEL);
  CHECK(c.exits == 0, "a held button does not close the results");
  press(BTN_ACTION);
  CHECK(c.exits == 1 && c.after == 1, "a real press does");
}

static void test_skip_callbacks_and_dwell(void) {
  /* B during the drop */
  for (int variant = 0; variant < 2; variant++) {
    reset_fakes();
    counts c = {0};
    cap_fixture f = fx(25);
    caps_args a = args_for(&f, &c);
    capsule_screen_open(&a);
    press(BTN_ACTION);
    run_ms(100, 16.7f);
    press(variant ? BTN_MENU : BTN_CANCEL);
    CHECK(c.settled == 1, "a skip shows the supplied results at once: on_settled once");
    draw(0);
    check_screen(0);
    draw(1);
    check_screen(1);
    CHECK(has_text("1. Fixture result 1"), "the list shows the fixture in its order");
    press(BTN_CANCEL);
    press(BTN_ACTION);
    CHECK(c.exits == 0, "a button right after the results does not close them (the pause)");
    run_ms(400, 16.7f);
    CHECK(c.settled == 1, "the clock running on does not report the settle again");
    press(BTN_CANCEL);
    CHECK(c.exits == 1 && c.after == 1 && c.before == 0 && c.exit_when_settled == 1, "after the pause a press closes: on_exit once, after the reveal");
    for (int i = 0; i < 5; i++) {
      press(BTN_ACTION);
      press(BTN_CANCEL);
      press(BTN_MENU);
      tick(16.7f);
    }
    CHECK(c.exits == 1 && c.settled == 1, "nothing after on_exit");
  }
  /* skip at several moments, results the same */
  const float moments[] = {0, 150, 300, 800, 1300};
  for (size_t k = 0; k < sizeof moments / sizeof moments[0]; k++) {
    reset_fakes();
    counts c = {0};
    cap_fixture f = fx(10);
    caps_args a = args_for(&f, &c);
    capsule_screen_open(&a);
    press(BTN_ACTION);
    run_ms(moments[k], 16.7f);
    if (c.settled == 0) press(BTN_CANCEL);
    CHECK(c.settled == 1, "settled once at moment %.0f", moments[k]);
    run_ms(2000, 33.0f);
    CHECK(c.settled == 1, "and once only");
  }
}

static void test_exit_before_pull_and_leaving(void) {
  reset_fakes();
  counts c = {0};
  cap_fixture f = fx(5);
  caps_args a = args_for(&f, &c);
  capsule_screen_open(&a);
  press(BTN_CANCEL);
  CHECK(c.exits == 1 && c.before == 1 && c.settled == 0, "B before the pull: on_exit before the reveal, no settle");
  press(BTN_CANCEL);
  press(BTN_ACTION);
  CHECK(c.exits == 1, "and only once");
  ui_screen* old = g_top;

  /* the screen is taken away mid-animation by someone else */
  reset_fakes();
  counts d = {0};
  a = args_for(&f, &d);
  capsule_screen_open(&a);
  press(BTN_ACTION);
  run_ms(300, 16.7f);
  old = g_top;
  ui_screen other = {"other", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
  ui_set(&other);
  CHECK(d.settled == 0 && d.exits == 0, "leaving mid-animation calls nothing");
  old->update(old, 16.7f);
  for (int i = 0; i < 40; i++) old->update(old, 100); /* far past the deadline */
  old->input(old, BTN_CANCEL);
  old->input(old, BTN_ACTION);
  CHECK(d.settled == 0 && d.exits == 0, "and a stale screen never calls back, whatever it is given");

  /* a second opening has its own callbacks: the first one's never reach it */
  counts e = {0};
  a = args_for(&f, &e);
  reset_fakes();
  capsule_screen_open(&a);
  press(BTN_ACTION);
  press(BTN_CANCEL);
  CHECK(e.settled == 1 && d.settled == 0 && d.exits == 0, "a later opening calls its own callbacks only");
}

static void test_reopen_from_callback(void) {
  reset_fakes();
  counts c = {0};
  cap_fixture f = fx(10);
  caps_args a = args_for(&f, &c);
  capsule_screen_open(&a);
  press(BTN_ACTION);
  press(BTN_CANCEL);
  run_ms(400, 16.7f);
  c.reopen = true;
  press(BTN_ACTION);
  CHECK(c.exits == 1 && c.reopened == 1, "on_exit may open the screen again");
  draw(0);
  draw(1);
  CHECK(has_text("Pull the knob") && c.settled == 1, "the new opening starts from the beginning");
  press(BTN_ACTION);
  run_ms(2000, 16.7f);
  CHECK(c.settled == 2 && c.exits == 1, "and its settle is reported on its own (settled %d, exits %d)", c.settled, c.exits);
}

static void test_list_and_cost(void) {
  reset_fakes();
  counts c = {0};
  cap_fixture f = fx(25);
  caps_args a = args_for(&f, &c);
  capsule_screen_open(&a);
  press(BTN_ACTION);
  int max_rects = 0;
  for (int i = 0; i < 100; i++) {
    tick(16.667f);
    draw(0);
    check_screen(0);
    if (n_rects > max_rects) max_rects = n_rects;
    draw(1);
    check_screen(1);
  }
  CHECK(c.settled == 1, "settled");
  draw(0);
  if (n_rects > max_rects) max_rects = n_rects;
  CHECK(max_rects <= 400, "at most 400 rectangles a frame on the top screen with 25 capsules (%d)", max_rects);
  printf("capsule_test: most rectangles on the top screen in a frame with 25 capsules: %d\n", max_rects);
  draw(1);
  CHECK(has_text("1. Fixture result 1") && has_text("5. Fixture result 5") && !has_text("6. Fixture result 6"), "five results are listed from the first");
  CHECK(has_text("20 more below"), "and the rest is announced");
  for (int i = 0; i < 40; i++) press(BTN_DOWN);
  draw(1);
  check_screen(1);
  CHECK(has_text("25. Fixture result 25") && !has_text("more below") && has_text("more above"), "the list scrolls to its last result and stops");
  for (int i = 0; i < 40; i++) press(BTN_UP);
  draw(1);
  CHECK(has_text("1. Fixture result 1") && !has_text("more above"), "and back to the first");
  CHECK(c.settled == 1 && c.exits == 0, "scrolling calls nothing");
}

/* ------------------------------------------------------------------ fuzz */

static uint32_t rng_state = 1;
static uint32_t rnd(void) {
  rng_state = rng_state * 1664525u + 1013904223u;
  return rng_state >> 8;
}

static void fuzz(int sessions) {
  static const ui_button btns[] = {BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_ACTION, BTN_CANCEL, BTN_MENU, BTN_SUBMIT, BTN_STATS};
  for (int s = 0; s < sessions; s++) {
    reset_fakes();
    counts c = {0};
    cap_fixture f = fx(1 + (int)(rnd() % 25));
    caps_args a = args_for(&f, &c);
    CHECK(capsule_screen_open(&a), "fuzz opens");
    ui_screen* mine = g_top;
    bool prev_settled = false;
    for (int step = 0; step < 300 && !c.exits; step++) {
      switch (rnd() % 4) {
        case 0: tick((float)(rnd() % 60)); break;
        case 1: tick((float)(rnd() % 3) == 0 ? 5000.0f : 16.7f); break;
        case 2: press(btns[rnd() % (sizeof btns / sizeof btns[0])]); break;
        default: press_held(btns[rnd() % (sizeof btns / sizeof btns[0])]); break;
      }
      if (rnd() % 7 == 0) {
        draw((int)(rnd() & 1));
        check_screen(g_which);
      }
      CHECK(c.settled <= 1 && c.exits <= 1, "at most one settle and one exit");
      CHECK(!prev_settled || c.settled == 1, "a settle is never taken back");
      prev_settled = c.settled == 1;
    }
    CHECK(c.after == 0 || c.settled == 1, "an exit after the reveal means the reveal was reported");
    if (rnd() % 2) {
      ui_screen other = {"other", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
      ui_set(&other);
      const int se = c.settled, ex = c.exits;
      mine->update(mine, 3000);
      mine->input(mine, BTN_ACTION);
      CHECK(c.settled == se && c.exits == ex, "nothing after the screen was taken away");
    }
  }
}

int main(int argc, char** argv) {
  test_fixture_validity();
  test_slots();
  test_timeline();
  test_units_and_frame_rate();
  test_bad_dt();
  test_skip_at_each_stage();
  test_deadline();
  test_determinism_and_immutability();
  test_no_ambient_random();
  test_screen_refusals();
  test_screen_words_and_pull();
  test_held_buttons();
  test_skip_callbacks_and_dwell();
  test_exit_before_pull_and_leaving();
  test_reopen_from_callback();
  test_list_and_cost();
  fuzz(argc > 1 ? atoi(argv[1]) : 300);
  printf("capsule_test: %d checks, %d failed\n", checks, failed);
  return failed ? 1 : 0;
}
