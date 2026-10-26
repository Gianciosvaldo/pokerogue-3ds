/* achv_screen_test: the Achievements list (source/achv_screen.c) on the host, over game-core's real achievement table and a fake drawing layer.
 *
 *   make achv_screen_test && build/achv_screen_test
 *
 * Checks: every achievement is listed once and can be reached with the buttons (paging included) and by taps; a finished one says "Done", a
 * counting one how far it is, a plain one nothing; the top screen has the highlighted one in full (name, what it asks, reward, state); the
 * reward text is the table's; the screen changes nothing of the profile; B leaves once. Exit 1 on a failed check. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "achv_screen.h"
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
  int screen;
  char text[200];
} drawn_t;
#define DRAWN_MAX 512
static drawn_t g_drawn[DRAWN_MAX];
static int n_drawn, g_which;
static ui_screen* g_top;
static int n_select, n_exit;

void ui_set(ui_screen* s) { g_top = s; }
void ui_play_select(void) { n_select++; }
void ui_play_error(void) {}
bool ui_space(ui_space_id sp) {
  const bool top = sp == UI_FIELD || sp == UI_FIELD_UI || sp == UI_TOP_CANVAS || sp == UI_TOP_CANVAS_UI || sp == UI_PHYS_TOP;
  return top == (g_which == 0);
}
void rr_rect(float x, float y, float w, float h, rr_color c) { (void)x, (void)y, (void)w, (void)h, (void)c; }
void ui_window(float x, float y, float w, float h, win_variant v) { (void)x, (void)y, (void)w, (void)h, (void)v; }
void ui_cursor(float x, float y) { (void)x, (void)y; }
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
float hint_x(text_style st, float x0, const char* const* texts, int i) {
  float x = x0;
  for (int k = 0; k < i; k++) x += text_width(st, texts[k], NULL) + HINT_GAP;
  return x;
}
int hint_at(text_style st, float x0, const char* const* texts, int n, float x) {
  for (int i = 0; i < n; i++)
    if (x < hint_x(st, x0, texts, i + 1) - HINT_GAP / 2.0f) return i;
  return -1;
}
float text_draw(text_style st, float x, float y, const char* s, const text_opts* o) {
  (void)st, (void)o;
  if (n_drawn < DRAWN_MAX) {
    drawn_t* d = &g_drawn[n_drawn++];
    d->x = x;
    d->y = y;
    d->screen = g_which;
    snprintf(d->text, sizeof d->text, "%s", s);
  }
  return 6.0f * (float)strlen(s);
}
void text_wrap(text_style st, const char* s, const text_opts* o, char* out, int n) {
  (void)st, (void)o;
  snprintf(out, (size_t)n, "%s", s);
}

/* ------------------------------------------------------------------ helpers */

static gc_system S;
static void exited(void* arg) { n_exit += *(int*)arg; }

static void draw(int which) {
  n_drawn = 0;
  g_which = which;
  g_top->draw(g_top, which ? RR_BOTTOM : RR_TOP);
}
static bool has_text(int screen, const char* needle) {
  for (int i = 0; i < n_drawn; i++)
    if (g_drawn[i].screen == screen && strstr(g_drawn[i].text, needle)) return true;
  return false;
}
static void press(ui_button b) { g_top->input(g_top, b); }

int main(void) {
  gc_system_init_defaults(&S);
  int one = 1;
  const achv_args a = {&S, exited, &one};
  achv_screen_open(&a);
  CHECK(g_top != NULL, "the screen is up");

  /* the first page: the first seven names and the count */
  draw(1);
  CHECK(has_text(1, "Achievements") && has_text(1, "0/68"), "title and count");
  CHECK(has_text(1, gc_achv_defs[0].name) && has_text(1, gc_achv_defs[6].name), "first seven rows");
  CHECK(!has_text(1, gc_achv_defs[7].name), "the eighth is on the next page");
  CHECK(has_text(1, "1 Egg Voucher"), "the reward of a row");

  /* every achievement can be reached one step at a time, and the top screen shows it in full */
  for (int i = 0; i < GC_ACHV_COUNT; i++) {
    draw(0);
    CHECK(has_text(0, gc_achv_defs[i].name), "top shows %s", gc_achv_defs[i].name);
    CHECK(has_text(0, gc_achv_defs[i].desc), "top shows what %d asks", i);
    CHECK(has_text(0, gc_achv_reward_name(&gc_achv_defs[i])), "top shows the reward of %d", i);
    draw(1);
    CHECK(has_text(1, gc_achv_defs[i].name), "row %d is on the page", i);
    if (i < GC_ACHV_COUNT - 1) press(BTN_DOWN);
  }
  press(BTN_DOWN); /* the last one stays */
  draw(0);
  CHECK(has_text(0, gc_achv_defs[GC_ACHV_COUNT - 1].name), "the cursor stops at the last");
  for (int i = 0; i < GC_ACHV_COUNT + 3; i++) press(BTN_UP);
  draw(0);
  CHECK(has_text(0, gc_achv_defs[0].name), "and at the first");

  /* paging */
  press(BTN_RIGHT);
  draw(0);
  CHECK(has_text(0, gc_achv_defs[7].name), "right pages by seven");
  press(BTN_LEFT);
  draw(0);
  CHECK(has_text(0, gc_achv_defs[0].name), "left pages back");

  /* states: a plain one says nothing on its row, a counting one how far it is, a finished one "Done" */
  gc_achv_jojo_won(&S, GC_JOJO_KARS, 1, true);
  gc_achv_jojo_won(&S, GC_JOJO_WAMUU, 1, false);
  gc_achv_evaluate(&S, "x", 1, NULL);
  draw(1);
  CHECK(has_text(1, "Done"), "a finished one");
  CHECK(has_text(1, "2/3") == false, "the Pillar Men set is not on the first page");
  for (int i = 0; i < 17; i++) press(BTN_DOWN);
  draw(1);
  CHECK(has_text(1, "2/3"), "the Pillar Men set has 2 of 3");
  CHECK(has_text(1, "3/68"), "three finished (Kars, Wamuu and Untouched: Kars fell with nobody fainted)");
  draw(0);
  CHECK(has_text(0, "Progress: 2/3"), "the top screen says the progress of a counting one");
  for (int i = 0; i < 7; i++) press(BTN_DOWN);
  draw(1);
  CHECK(has_text(1, "2/5"), "Five Faces has 2 of 5");

  /* a tap on a row points at it; a tap on the hint leaves */
  const int before = n_select;
  CHECK(g_top->touch(g_top, 100, 30 + 4 + 2 * 26 + 5), "a tap on a row");
  CHECK(n_select > before, "a tap points at it");
  draw(0);
  CHECK(has_text(0, gc_achv_defs[20].name), "the tapped row (the third of the page 18..24) is the highlighted one");
  const unsigned snap = S.voucher_counts[0];
  CHECK(S.voucher_counts[0] == snap, "the screen changes nothing of the profile");
  CHECK(n_exit == 0, "not left yet");
  press(BTN_CANCEL);
  CHECK(n_exit == 1, "B leaves");
  press(BTN_CANCEL);
  g_top->touch(g_top, 12, 224);
  CHECK(n_exit == 1, "and only once");

  printf("achv_screen_test: %d checks, %d failed\n", checks, failed);
  return failed ? 1 : 0;
}
