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
#include "gacha_view.h"
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

/* The view (menus/gacha_view.c) is the menus' layer and is not linked here: this stand-in records what the screen hands it. The words and the
 * eggs' places are what the screen decides; how they look is checked on the emulator. One rectangle per egg that is on show stands for its art. */
static void rec(const char* text) {
  if (!text[0] || n_drawn >= DRAWN_MAX) return;
  drawn_t* d = &g_drawn[n_drawn++];
  d->style = TS_WINDOW;
  d->x = d->x0 = 8;
  d->y = 10;
  d->ox = 0;
  d->screen = g_which;
  snprintf(d->text, sizeof d->text, "%s", text);
}
static int g_hatch = -1, g_machine = -1; /* what the last top-screen draw was given */
void gv_draw_reveal_top(const gv_reveal* v) {
  g_hatch = v->hatch;
  g_machine = v->machine;
  rec(v->top_title);
  rec(v->top_sub);
  for (int i = 0; i < v->n; i++)
    if (v->egg[i].phase && n_rects < RECT_MAX) g_rects[n_rects++] = (rect_t){v->egg[i].x - 10, v->egg[i].y - 10, 20, 20, g_which};
}
static char g_sel_name[64];
static int g_n_rows;
void gv_draw_reveal_bottom(const gv_reveal* v) {
  g_sel_name[0] = 0;
  g_n_rows = v->n_rows;
  rec(v->title);
  for (int i = 0; i < v->n_rows; i++) {
    if (v->row[i].selected) snprintf(g_sel_name, sizeof g_sel_name, "%s", v->row[i].name);
    rec(v->row[i].name);
    rec(v->row[i].tier_name);
  }
  rec(v->more_above);
  rec(v->more_below);
  rec(v->big);
  rec(v->small);
  rec(v->note[0]);
  rec(v->note[1]);
  for (int i = 0; i < v->hints.n; i++) rec(v->hints.text[i]);
}
/* The hint a tap is on (menus/gacha_view.c measures the 8 px pixel font; here a character is 6 px): the same walk */
int gv_hint_at(const gv_hints* h, float x) {
  float end = GV_HINT_X;
  for (int i = 0; i < h->n; i++) {
    end += 6.0f * (float)strlen(h->text[i]);
    if (x < end + GV_HINT_GAP / 2.0f) return i;
    end += GV_HINT_GAP;
  }
  return -1;
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
  /* the capsules stay near the screen (the chute is above its top edge, cap_screen_y) */
  if (which == 0)
    for (int i = 0; i < n_rects; i++)
      if (g_rects[i].w <= 60) /* the capsules' parts; the backdrop panels are bigger */
        CHECK(g_rects[i].x >= -30 && g_rects[i].x + g_rects[i].w <= 350 && g_rects[i].y >= -70 && g_rects[i].y <= 230, "a rectangle at (%.0f, %.0f) is off the screen", g_rects[i].x, g_rects[i].y);
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

/* cap_screen_y: the capsules come in from above the top screen and rest in its bottom band, under the machine */
static void test_screen_y(void) {
  for (int n = 1; n <= CAP_MAX; n++) {
    cap_model m;
    cap_fixture f = fx(n);
    CHECK(cap_init(&m, &f), "init %d", n);
    for (int i = 0; i < n; i++) {
      const cap_body* b = &m.body[i];
      const float chute = cap_screen_y(b->y, b->sy), rest = cap_screen_y(b->sy, b->sy);
      CHECK(chute + CAP_RADIUS < CAP_SCREEN_TOP, "capsule %d of %d leaves the chute off the screen (%.1f)", i, n, chute);
      CHECK(fabsf(rest - (b->sy + CAP_SCREEN_BAND)) < 0.01f && rest + CAP_RADIUS <= CAP_SCREEN_BOTTOM, "capsule %d of %d rests one band lower (%.1f)", i, n, rest);
      if (i / CAP_COLS == (n - 1) / CAP_COLS) CHECK(rest + CAP_RADIUS > CAP_FIELD_H, "capsule %d of %d (the bottom row) rests in the bottom band (%.1f)", i, n, rest);
      CHECK(cap_screen_y((b->y + b->sy) / 2, b->sy) > chute && cap_screen_y((b->y + b->sy) / 2, b->sy) < rest, "and falls in between");
      CHECK(cap_screen_y(b->sy - 5, b->sy) < rest, "a bounce stays above its slot");
    }
  }
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

/* The machine's hatch: closed before the pull, opening from the first capsule leaving the chute at 12 frames a second, held open until
 * the last one has left, closing, closed again before the results; never outside 1..4. */
static void test_hatch(void) {
  CHECK(capsule_hatch_frame(NULL) == 1, "no model: closed");
  const int sizes[] = {1, 2, 10, 25};
  for (int si = 0; si < 4; si++) {
    const int n = sizes[si];
    cap_fixture f = fx(n);
    cap_model m;
    cap_init(&m, &f);
    CHECK(capsule_hatch_frame(&m) == 1, "closed before the pull (%d)", n);
    cap_step(&m, 100);
    CHECK(capsule_hatch_frame(&m) == 1, "and while time does nothing (%d)", n);
    cap_pull(&m);
    float last_spawn = 0;
    for (int i = 0; i < n; i++)
      if (m.body[i].spawn_ms > last_spawn) last_spawn = m.body[i].spawn_ms;
    int prev = 1, peak_at = -1, closed_at = -1;
    bool rising = true, saw[5] = {false};
    for (int step = 0; m.stage != CAP_SETTLED && step < 400; step++) {
      cap_step(&m, 8.0f);
      if (m.stage == CAP_SETTLED) break;
      const int h = capsule_hatch_frame(&m);
      CHECK(h >= 1 && h <= 4, "the frame is 1..4 (%d at %.0f ms, %d)", h, m.t, n);
      saw[h] = true;
      if (m.t < CAP_PULL_MS) CHECK(h == 1, "closed until the first capsule leaves (%.0f ms, %d)", m.t, n);
      if (m.t < CAP_PULL_MS + 83.0f) CHECK(h == 1, "the first frame lasts 1/12 s (%.0f ms, %d)", m.t, n);
      if (m.t > CAP_PULL_MS + 3.0f * 83.4f && m.t < last_spawn + 100.0f - 8.0f) CHECK(h == 4, "open while capsules are still leaving (%.0f ms, %d)", m.t, n);
      if (rising && peak_at >= 0 && h < 4) rising = false;
      if (rising) {
        CHECK(h >= prev, "it opens without going back (%d -> %d at %.0f ms, %d)", prev, h, m.t, n);
        if (h == 4 && peak_at < 0) peak_at = (int)m.t;
      }
      if (!rising) CHECK(h <= prev, "it closes without reopening (%d -> %d at %.0f ms, %d)", prev, h, m.t, n);
      if (h == 1 && !rising && closed_at < 0) closed_at = (int)m.t;
      prev = h;
    }
    CHECK(saw[2] && saw[3] && saw[4], "it passes through every frame (%d)", n);
    CHECK(peak_at >= (int)(CAP_PULL_MS + 3.0f * 83.3f) - 1, "fully open no sooner than 3/12 s after the first (%d at %d ms)", n, peak_at);
    CHECK(closed_at > 0 && closed_at < (int)CAP_DEADLINE_MS, "closed again before the deadline (%d at %d ms)", n, closed_at);
    CHECK(closed_at >= (int)last_spawn, "not closed before the last capsule has left (%d: %d ms, last %.0f)", n, closed_at, last_spawn);
    while (m.stage != CAP_SETTLED) cap_step(&m, 16.0f);
    CHECK(capsule_hatch_frame(&m) == 1, "closed once settled (%d)", n);
  }
  cap_fixture f = fx(10);
  cap_model m;
  cap_init(&m, &f);
  cap_pull(&m);
  for (int i = 0; i < 50; i++) cap_step(&m, 8.0f);   /* 400 ms */
  CHECK(capsule_hatch_frame(&m) > 1, "open mid-drop");
  cap_skip(&m);
  CHECK(capsule_hatch_frame(&m) == 1, "a skip closes it");
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

/* the screen hands the view the model's frame and the chosen machine */
static void test_hatch_screen(void) {
  reset_fakes();
  counts c = {0};
  cap_fixture f = fx(10);
  caps_args a = args_for(&f, &c);
  a.machine = 2;
  CHECK(capsule_screen_open(&a), "opens with machine 2");
  draw(0);
  CHECK(g_machine == 2 && g_hatch == 1, "the view gets the machine %d and a closed hatch (%d) before the pull", g_machine, g_hatch);
  press(BTN_ACTION);
  int max_hatch = 0;
  for (int i = 0; i < 80; i++) {
    tick(16.0f);
    draw(0);
    if (g_hatch > max_hatch) max_hatch = g_hatch;
  }
  CHECK(max_hatch == 4, "the view sees the hatch open (%d)", max_hatch);
  run_ms(2000, 16);
  draw(0);
  CHECK(g_hatch == 1 && g_machine == 2, "and closed with the results");
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
    float low = -1000;
    for (int i = 0; i < n_rects; i++)
      if (g_rects[i].w == 20 && g_rects[i].y + 20 > low) low = g_rects[i].y + 20;
    CHECK(low > CAP_FIELD_H && low <= CAP_SCREEN_BOTTOM, "the settled capsules are drawn resting in the screen's bottom band (lowest edge %.0f)", low);
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

/* A tap (physical bottom-screen px; the canvas starts 30 px down). The knob is the canvas square x 120..200, y 72..152; the buttons at the foot
 * start at canvas y 160; with 6 px a character the foot's texts are at x 8..50 (A: pull) and 64..106 (B: back) before the pull, 8..86
 * (UP/DOWN: look) and 100..148 (A: close) once the results show, and 8..194 while it runs; each takes the half gap (7) after it. The list's
 * rows are canvas y 29 + 18 i (18 high, x 4..316), and the notes of more results are canvas y 124..136 (more above x < 160, more below right). */
static bool tap(float x, float y) { return g_top->touch(g_top, x, y); }
#define FOOT 200.0f
#define LIST(i, dy) (30.0f + 29.0f + 18.0f * (float)(i) + (dy))
#define NOTE 160.0f

static void test_touch(void) {
  /* before the pull: the knob and the two buttons */
  reset_fakes();
  counts c = {0};
  cap_fixture f = fx(25);
  caps_args a = args_for(&f, &c);
  CHECK(capsule_screen_open(&a) && g_top->touch != NULL, "opens, and takes taps");
  draw(1);
  CHECK(has_text("Pull the knob") && has_text("A: pull") && has_text("B: back"), "the two buttons are two texts");
  CHECK(!tap(119, 130) && !tap(200, 130) && !tap(160, 30 + 71) && !tap(160, 30 + 152), "a tap just outside the knob's square takes nothing");
  CHECK(!tap(30, 189) && !tap(150, FOOT) && !tap(319, FOOT), "nor above the foot, nor right of B: back");
  CHECK(n_select == 0, "(nothing was pulled)");
  CHECK(tap(160, 130) && n_select == 1, "a tap on the knob pulls");
  CHECK(!tap(160, 130) && !tap(20, FOOT - 100) && n_select == 1, "and is not a second pull: the knob is gone");
  draw(1);
  CHECK(has_text("B or START: skip to the results") && !has_text("A: pull"), "while it runs the foot offers the skip, as one text");
  CHECK(!tap(100, 130) && !tap(250, FOOT) && c.settled == 0, "a tap on the screen's body, or beyond the skip, does not skip");
  CHECK(tap(200, FOOT) && c.settled == 1, "a tap on the skip (x 194 is the half gap after it) shows the results");
  draw(1);
  CHECK(has_text("1. Fixture result 1"), "the results");
  CHECK(!strcmp(g_sel_name, "1. Fixture result 1") && g_n_rows == 5, "the first of five rows is pointed at");

  /* the results: rows, the second tap, the pause */
  n_select = 0;
  CHECK(!tap(3, LIST(0, 5)) && !tap(316, LIST(0, 5)) && !tap(100, LIST(0, -1)) && !tap(100, LIST(5, 0)), "a tap beside the rows takes nothing");
  CHECK(n_select == 0, "(no sound)");
  CHECK(tap(100, LIST(2, 5)) && n_select == 1, "a tap on the third row points at it");
  draw(1);
  CHECK(!strcmp(g_sel_name, "3. Fixture result 3"), "(%s)", g_sel_name);
  CHECK(tap(4, LIST(4, 17)), "the last pixel of the fifth row, its left edge");
  draw(1);
  CHECK(!strcmp(g_sel_name, "5. Fixture result 5"), "(%s)", g_sel_name);
  CHECK(tap(100, LIST(0, 0)), "the first pixel of the first");
  draw(1);
  CHECK(!strcmp(g_sel_name, "1. Fixture result 1"), "(%s)", g_sel_name);
  CHECK(tap(100, LIST(2, 0)) && tap(100, LIST(2, 5)) && c.exits == 0, "a second tap on the pointed row is A: the pause is not over yet, so it does not close");
  run_ms(400, 16.7f);
  CHECK(tap(100, LIST(2, 9)) && c.exits == 1 && c.after == 1, "after the pause it closes");
  CHECK(tap(100, LIST(2, 5)) && tap(20, FOOT) && c.exits == 1, "and a screen that was left takes taps and does nothing");

  /* the notes page the list by five */
  reset_fakes();
  counts d = {0};
  a = args_for(&f, &d);
  capsule_screen_open(&a);
  tap(160, 130);
  tap(100, FOOT);
  draw(1);
  CHECK(d.settled == 1 && has_text("20 more below") && !has_text("more above"), "25 results: five shown, 20 below, none above");
  CHECK(!tap(30, NOTE), "no note above, so a tap there takes nothing");
  CHECK(!tap(200, 30 + 123) && !tap(200, 30 + 136), "nor just above or below the notes' band (canvas y 124..136)");
  CHECK(tap(200, NOTE), "the note below pages down");
  draw(1);
  CHECK(!strcmp(g_sel_name, "6. Fixture result 6") && has_text("19 more below") && has_text("more above"), "the pointer moved five on (%s)", g_sel_name);
  CHECK(tap(100, LIST(2, 5)), "the list now shows results 2..6: its third row is result 4");
  draw(1);
  CHECK(!strcmp(g_sel_name, "4. Fixture result 4"), "a scrolled list's rows are the results they show (%s)", g_sel_name);
  CHECK(tap(100, LIST(0, 0)), "and its first row result 2");
  draw(1);
  CHECK(!strcmp(g_sel_name, "2. Fixture result 2"), "(%s)", g_sel_name);
  CHECK(tap(159, NOTE), "x 159 is the note above's half");
  draw(1);
  CHECK(!strcmp(g_sel_name, "1. Fixture result 1") && !has_text("more above"), "and pages up (%s)", g_sel_name);
  CHECK(tap(160, NOTE), "x 160 is the note below's");
  draw(1);
  CHECK(!strcmp(g_sel_name, "6. Fixture result 6"), "(%s)", g_sel_name);
  static const char* const down[4] = {"11.", "16.", "21.", "25."};
  for (int k = 0; k < 4; k++) {
    CHECK(tap(200, NOTE), "page down");
    draw(1);
    CHECK(!strncmp(g_sel_name, down[k], 3), "to %s (%s)", down[k], g_sel_name);
  }
  CHECK(!has_text("more below") && has_text("more above") && !tap(200, NOTE), "at the end there is no note below, and a tap there takes nothing");
  static const char* const up[5] = {"20.", "15.", "10.", "5.", "1."};
  for (int k = 0; k < 5; k++) {
    CHECK(tap(30, NOTE), "page up");
    draw(1);
    CHECK(!strncmp(g_sel_name, up[k], 2), "to %s (%s)", up[k], g_sel_name);
  }
  CHECK(!has_text("more above") && !tap(30, NOTE), "back at the top: no note above");

  /* the foot: UP/DOWN is only said, A: close is A (after the pause) */
  draw(1);
  CHECK(has_text("UP/DOWN: look") && has_text("A: close"), "the foot's two texts");
  CHECK(!tap(50, FOOT) && !tap(200, FOOT) && !tap(30, 30 + 159), "UP/DOWN: look, beyond A: close and the row above the foot take nothing");
  CHECK(tap(120, FOOT) && d.exits == 0, "A: close in the pause after the results does not close them yet");
  run_ms(400, 16.7f);
  CHECK(tap(120, FOOT) && d.exits == 1 && d.after == 1, "after it, it closes");

  /* a short list: no notes, and no row below the last result */
  reset_fakes();
  counts e = {0};
  cap_fixture g = fx(3);
  a = args_for(&g, &e);
  capsule_screen_open(&a);
  tap(160, 130);
  tap(100, FOOT);
  draw(1);
  CHECK(g_n_rows == 3 && !has_text("more"), "three results, three rows, no notes");
  CHECK(!tap(100, LIST(3, 5)) && !tap(200, NOTE) && !tap(30, NOTE), "no fourth row and no notes to tap");
  CHECK(tap(100, LIST(2, 5)), "the third row");
  draw(1);
  CHECK(!strcmp(g_sel_name, "3. Fixture result 3"), "(%s)", g_sel_name);
  CHECK(tap(315, LIST(1, 5)), "the right edge of a row (x 315 is its last pixel)");
  draw(1);
  CHECK(!strcmp(g_sel_name, "2. Fixture result 2"), "(%s)", g_sel_name);

  /* the knob's four corner pixels pull, and the foot's first row of pixels is the foot */
  static const float corner[4][2] = {{120, 102}, {199, 102}, {120, 181}, {199, 181}};
  for (int k = 0; k < 4; k++) {
    reset_fakes();
    counts m = {0};
    a = args_for(&f, &m);
    capsule_screen_open(&a);
    n_select = 0;
    CHECK(tap(corner[k][0], corner[k][1]) && n_select == 1, "the knob's corner %d pulls", k);
  }
  reset_fakes();
  counts n = {0};
  a = args_for(&f, &n);
  capsule_screen_open(&a);
  n_select = 0;
  CHECK(tap(20, 190) && n_select == 1, "the foot's first pixel row (canvas y 160) is A: pull");

  /* B before the pull, by its button */
  reset_fakes();
  counts h = {0};
  a = args_for(&f, &h);
  capsule_screen_open(&a);
  CHECK(tap(80, FOOT) && h.exits == 1 && h.before == 1, "B: back before the pull leaves, before the reveal");
  CHECK(tap(20, FOOT) && tap(160, 130) && h.exits == 1, "and the screen that was left takes taps and does nothing");

  /* the pull by its button */
  reset_fakes();
  counts k = {0};
  a = args_for(&f, &k);
  capsule_screen_open(&a);
  n_select = 0;
  CHECK(tap(20, FOOT) && n_select == 1, "A: pull pulls");
  draw(1);
  CHECK(has_text("Here they come"), "(it runs)");
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
  test_screen_y();
  test_timeline();
  test_hatch();
  test_units_and_frame_rate();
  test_bad_dt();
  test_skip_at_each_stage();
  test_deadline();
  test_determinism_and_immutability();
  test_no_ambient_random();
  test_screen_refusals();
  test_screen_words_and_pull();
  test_touch();
  test_hatch_screen();
  test_held_buttons();
  test_skip_callbacks_and_dwell();
  test_exit_before_pull_and_leaving();
  test_reopen_from_callback();
  test_list_and_cost();
  fuzz(argc > 1 ? atoi(argv[1]) : 300);
  printf("capsule_test: %d checks, %d failed\n", checks, failed);
  return failed ? 1 : 0;
}
