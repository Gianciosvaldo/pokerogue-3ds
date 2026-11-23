/* gamestats_screen_test: the Game Stats screen (source/gamestats_screen.c) on the host, over the real model and a fake drawing layer.
 *
 *   make gamestats_screen_test && build/gamestats_screen_test
 *
 * Checks: the header, the two columns (slot 0 and 1 side by side, nine rows, upstream's x and y), the first page and every scroll position (one row =
 * two stats; refused at both ends, no sound then), the scroll arrows (down at the top, up at the bottom, both between), "???" for a hidden stat that is 0
 * and its label from 1, values and play time as given, taps (upper half up, lower half down, the hint leaves), B leaves once, and the profile is
 * untouched. Exit 1 on a failed check. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gamestats_model.h"
#include "gamestats_screen.h"
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

typedef struct {
  float x, y;
  int screen, style;
  float origin_x;
  char text[200];
} drawn_t;
#define DRAWN_MAX 512
static drawn_t g_drawn[DRAWN_MAX];
static int n_drawn, g_which;
static ui_screen* g_top;
static int n_select, n_exit, n_arrow_up, n_arrow_down;
static float g_now;

void ui_set(ui_screen* s) { g_top = s; }
void ui_play_select(void) { n_select++; }
void ui_play_error(void) {}
float ui_now(void) { return g_now; }
bool ui_space(ui_space_id sp) {
  const bool top = sp == UI_FIELD || sp == UI_FIELD_UI || sp == UI_TOP_CANVAS || sp == UI_TOP_CANVAS_UI || sp == UI_PHYS_TOP;
  return top == (g_which == 0);
}
const rr_draw_opts RR_DEFAULT_OPTS = {0, 0, 1, 1, 0, 0};
void rr_rect(float x, float y, float w, float h, rr_color c) { (void)x, (void)y, (void)w, (void)h, (void)c; }
static rr_frame g_frame;
const rr_frame* img_frame(const char* key, const char* frame) {
  (void)frame;
  return strcmp(key, "images/ui/prompt") == 0 ? &g_frame : NULL;
}
void rr_draw(const rr_frame* f, float x, float y, const rr_draw_opts* o) {
  (void)f, (void)x, (void)y;
  if (g_which == 1) {
    if (o->scale_y < 0) n_arrow_up++;
    else n_arrow_down++;
  }
}
void ui_window(float x, float y, float w, float h, win_variant v) { (void)x, (void)y, (void)w, (void)h, (void)v; }
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
  if (n_drawn < DRAWN_MAX) {
    drawn_t* d = &g_drawn[n_drawn++];
    d->x = x;
    d->y = y;
    d->screen = g_which;
    d->style = (int)st;
    d->origin_x = o ? o->origin_x : 0.0f;
    snprintf(d->text, sizeof d->text, "%s", s);
  }
  return 6.0f * (float)strlen(s);
}
/* the locale: a key shows as itself, except the three the screen interpolates */
const char* i18n_t(char* out, int n, const char* key, ...) {
  if (!strcmp(key, "common:guest")) {
    snprintf(out, (size_t)n, "Guest");
  } else if (!strcmp(key, "gameStatsUiHandler:stats")) {
    va_list ap;
    va_start(ap, key);
    (void)va_arg(ap, const char*); /* "username" */
    const char* name = va_arg(ap, const char*);
    va_end(ap);
    snprintf(out, (size_t)n, "Stats for %s", name);
  } else {
    snprintf(out, (size_t)n, "%s", key);
  }
  return out;
}
const char* i18n_raw(const char* key) {
  if (!strcmp(key, "common:abrThousand")) return "k";
  if (!strcmp(key, "common:abrMillion")) return "M";
  if (!strcmp(key, "common:abrBillion")) return "B";
  return NULL;
}

/* ------------------------------------------------------------------ helpers */

static gc_system S, before;
static gsm_row model[GSM_COUNT];
static uint32_t play_now = 3661;
static uint32_t play_fn(void) { return play_now; }
static void exited(void* arg) { n_exit += *(int*)arg; }

static void draw(int which) {
  n_drawn = 0;
  n_arrow_up = n_arrow_down = 0;
  g_which = which;
  g_top->draw(g_top, which ? RR_BOTTOM : RR_TOP);
}
static const drawn_t* find(const char* text) {
  for (int i = 0; i < n_drawn; i++)
    if (g_drawn[i].screen == 1 && !strcmp(g_drawn[i].text, text)) return &g_drawn[i];
  return NULL;
}
static void press(ui_button b) { g_top->input(g_top, b); }

/* the label upstream shows for a model row (its key here, "???" when hidden) */
static const char* shown_label(int i) { return gsm_label_hidden(&model[i]) ? "???" : model[i].label_key; }

/* every stat of a page is drawn at its place: slot s of the page starting at stat `first` */
static void check_page(int cursor) {
  draw(1);
  const int first = cursor * GSM_COLUMNS;
  for (int slot = 0; slot < GSM_PER_PAGE; slot++) {
    const int idx = first + slot;
    if (idx >= GSM_COUNT) break;
    const int col = slot & 1, row = slot >> 1;
    /* labels may repeat ("???"): find the one at this slot's place */
    const float lx = col ? 165.0f : 8.0f, ly = (float)(28 + row * 16);
    bool label_ok = false, value_ok = false;
    for (int i = 0; i < n_drawn; i++) {
      const drawn_t* d = &g_drawn[i];
      if (d->screen != 1 || d->y != ly) continue;
      if (d->x == lx && d->origin_x == 0.0f && !strcmp(d->text, shown_label(idx))) label_ok = true;
      if (d->x == (col ? 306.0f : 149.0f) && d->origin_x == 1.0f && !strcmp(d->text, model[idx].value)) value_ok = true;
    }
    CHECK(label_ok, "cursor %d slot %d: label %s at %.0f,%.0f", cursor, slot, shown_label(idx), lx, ly);
    CHECK(value_ok, "cursor %d slot %d: value %s", cursor, slot, model[idx].value);
  }
  /* nothing from outside the page: exactly a label and a value per stat on it, in the stats' rows */
  int in_rows = 0;
  for (int i = 0; i < n_drawn; i++)
    if (g_drawn[i].screen == 1 && g_drawn[i].y >= 28 && g_drawn[i].y < 28 + 9 * 16) in_rows++;
  const int on_page = GSM_COUNT - first < GSM_PER_PAGE ? GSM_COUNT - first : GSM_PER_PAGE;
  CHECK(in_rows == 2 * on_page, "cursor %d: %d texts in the stat rows, expected %d", cursor, in_rows, 2 * on_page);
  CHECK(n_arrow_down == (cursor < GSM_MAX_CURSOR ? 1 : 0), "cursor %d: down arrow %d", cursor, n_arrow_down);
  CHECK(n_arrow_up == (cursor > 0 ? 1 : 0), "cursor %d: up arrow %d", cursor, n_arrow_up);
}

int main(void) {
  gc_system_init_defaults(&S);
  S.stats.total_battles = 123;
  S.stats.highest_damage = 456;
  S.gstats.highest_endless_wave = 0;   /* hidden at 0 */
  S.gstats.endless_sessions_played = 2; /* hidden, shown from 1 */
  S.gstats.highest_money = 1234567;
  before = S;
  int one = 1;
  const gamestats_args a = {&S, play_fn, exited, &one};
  gamestats_screen_open(&a);
  CHECK(g_top != NULL, "the screen is up");
  gsm_build(&S, play_now, NULL, model);

  draw(1);
  CHECK(find("Stats for Guest") != NULL, "the header");
  CHECK(find("00:01:01:01") != NULL, "the play time given");
  CHECK(find("123") != NULL, "a value of the profile");
  CHECK(find("1.235M") != NULL, "abbreviated money");
  /* row 12 is Endless Runs (2): its label; row 13 is the highest Endless wave (0): "???" */
  CHECK(strcmp(shown_label(12), "gameStatsUiHandler:endlessRuns") == 0, "a hidden stat shows its label once it is not 0");
  CHECK(strcmp(shown_label(13), "???") == 0, "and \"???\" while it is 0");
  CHECK(find("gameStatsUiHandler:endlessRuns") != NULL && find("???") != NULL, "both are drawn on the first page");

  /* every scroll position */
  for (int c = 0; c <= GSM_MAX_CURSOR; c++) {
    check_page(c);
    if (c < GSM_MAX_CURSOR) {
      const int sel = n_select;
      press(BTN_DOWN);
      CHECK(n_select == sel + 1, "a scroll plays the sound");
    }
  }
  const int sel = n_select;
  press(BTN_DOWN);
  CHECK(n_select == sel, "the last position refuses a further scroll, silently");
  check_page(GSM_MAX_CURSOR);
  for (int c = GSM_MAX_CURSOR; c > 0; c--) press(BTN_UP);
  check_page(0);
  const int at_top = n_select;
  press(BTN_UP);
  CHECK(n_select == at_top, "the first position refuses a scroll up, silently");
  check_page(0);

  /* taps: the upper half of the columns scrolls up, the lower half down */
  g_top->touch(g_top, 100, 24 + 140);
  check_page(1);
  g_top->touch(g_top, 100, 24 + 10);
  check_page(0);
  CHECK(!g_top->touch(g_top, 100, 10), "a tap on the header does nothing");

  /* the top screen only has the background */
  draw(0);
  CHECK(n_drawn == 0, "nothing is written on the top screen");

  CHECK(memcmp(&S, &before, sizeof S) == 0, "the screen changes nothing of the profile");
  CHECK(n_exit == 0, "not left yet");
  press(BTN_CANCEL);
  CHECK(n_exit == 1, "B leaves");
  press(BTN_CANCEL);
  g_top->touch(g_top, 12, 224);
  CHECK(n_exit == 1, "and only once");

  /* the hint leaves too, and a profile's play time is used when no clock is given */
  S.gstats.play_time = 90061; /* 1 day 1 hour 1 minute 1 second */
  const gamestats_args b = {&S, NULL, exited, &one};
  gamestats_screen_open(&b);
  draw(1);
  CHECK(find("01:01:01:01") != NULL, "the profile's own play time");
  CHECK(g_top->touch(g_top, 12, 224), "a tap on the hint");
  CHECK(n_exit == 2, "leaves");

  printf("gamestats_screen_test: %d checks, %d failed\n", checks, failed);
  return failed ? 1 : 0;
}
