/* challenge_screen_test: Challenge Mode's challenge screen (source/challenge_screen.c + challenge_model.c) on the host, over a fake UI stack and
 * drawing layer that record the texts drawn. Buttons and touches, what each reaches, what is drawn. Exit 1 on a failed check. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "challenge_select.h"
#include "i18n.h"
#include "pr_enums.h"
#include "ui.h"
#include "ui_text.h"
#include "ui_widgets.h"

static int checks, failed;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { failed++; printf("FAIL line %d: %s: ", __LINE__, #cond); printf(__VA_ARGS__); printf("\n"); } } while (0)

static ui_screen* g_screen;
static int n_push, n_pop, n_select, n_error;
void ui_push(ui_screen* s) { g_screen = s; n_push++; }
void ui_pop(void) { n_pop++; }
void ui_play_select(void) { n_select++; }
void ui_play_error(void) { n_error++; }
bool ui_space(ui_space_id sp) { (void)sp; return true; }
void ui_window(float x, float y, float w, float h, win_variant v) { (void)x, (void)y, (void)w, (void)h, (void)v; }
void rr_rect(float x, float y, float w, float h, rr_color c) { (void)x, (void)y, (void)w, (void)h, (void)c; }
static float cursor_y;
void ui_cursor(float x, float y) { (void)x; cursor_y = y; }
text_opts text_defaults(void) { text_opts o; memset(&o, 0, sizeof o); return o; }
static char drawn[4096];
static float desc_y;   /* where the description was drawn, and the clip around it */
static int n_clip, n_clip_off;
static float clip_y, clip_h;
void clip_rect(float x, float y, float w, float h) { (void)x, (void)w; clip_y = y; clip_h = h; n_clip++; }
void clip_off(void) { n_clip_off++; }
/* the fake measures every text as one 14 px line, Limited Support's "Both" description as six */
float text_height(text_style st, const char* s, const text_opts* o) { (void)st, (void)o; return strstr(s, "limitedSupport.desc.3") ? 84 : 14; }
float text_draw(text_style st, float x, float y, const char* s, const text_opts* o) {
  (void)st, (void)x, (void)o;
  if (strstr(s, ".desc")) desc_y = y;
  strncat(drawn, s, sizeof drawn - strlen(drawn) - 2);
  strcat(drawn, "|");
  return 0;
}
/* the locale has desc.<value> for Fresh Start and Passives, a single desc for Inverse Battle and Flip Stat */
bool i18n_exists(const char* key) { return !strstr(key, "inverseBattle.desc.") && !strstr(key, "flipStat.desc."); }
const char* i18n_t(char* out, int n, const char* key, ...) {
  snprintf(out, (size_t)n, "[%s]", key);
  return out;
}

static int n_start, n_back;
static gc_challenge_set started;
static void on_start(const gc_challenge_set* set, void* arg) { started = *set; n_start++; (void)arg; }
static void on_back(void* arg) { (void)arg; n_back++; }
static void draw(void) {
  drawn[0] = 0;
  g_screen->draw(g_screen, RR_TOP);
  CHECK(!drawn[0], "nothing on the top screen");
  g_screen->draw(g_screen, RR_BOTTOM);
}
static bool seen(const char* t) { return strstr(drawn, t) != NULL; }
static void open_with(const gc_challenge_set* v) {
  chsel_args a = {.on_start = on_start, .on_back = on_back};
  if (v) a.values = *v;
  chsel_open(&a);
}

int main(void) {
  open_with(NULL);
  CHECK(n_push == 1 && g_screen, "the screen is pushed");
  draw();
  CHECK(seen("[challenges:title]") && seen("[challenges:freshStart.name]") && seen("[challenges:freshStart.value.0]"), "title, row and its value are drawn");
  CHECK(seen("[challenges:noneSelected]") && !seen("[common:start]"), "None Selected until a challenge has a value");
  CHECK(seen("[challenges:freshStart.desc.0]"), "the description of the highlighted value");

  /* the battle rules' rows: Inverse Battle, Flip Stat, Passives after Fresh Start (upstream's order); a challenge with one description text
   * shows it at every value (getDescription's fallback) */
  CHECK(seen("[challenges:inverseBattle.name]") && seen("[challenges:flipStat.name]") && seen("[challenges:passives.name]"), "the battle rule rows");
  CHECK(strstr(drawn, "freshStart.name") < strstr(drawn, "inverseBattle.name") && strstr(drawn, "inverseBattle.name") < strstr(drawn, "flipStat.name") &&
        strstr(drawn, "flipStat.name") < strstr(drawn, "passives.name"), "in upstream's order");
  g_screen->input(g_screen, BTN_DOWN);
  draw();
  CHECK(seen("[challenges:inverseBattle.desc]") && !seen("inverseBattle.desc."), "Inverse Battle's single description");
  g_screen->input(g_screen, BTN_RIGHT);
  draw();
  CHECK(seen("[challenges:inverseBattle.value.1]") && seen("[challenges:inverseBattle.desc]") && seen("[common:start]"), "Inverse Battle on, the same description");
  g_screen->input(g_screen, BTN_RIGHT);
  draw();
  CHECK(seen("[challenges:inverseBattle.value.1]"), "Inverse Battle stops at On");
  g_screen->input(g_screen, BTN_LEFT);
  for (int i = 0; i < 5; i++) g_screen->input(g_screen, BTN_DOWN);   /* Flip Stat, Limited Catch, Limited Support, Hardcore, Passives */
  draw();
  CHECK(seen("[challenges:passives.desc.0]"), "Passives has a description per value");
  g_screen->input(g_screen, BTN_RIGHT);
  g_screen->input(g_screen, BTN_RIGHT);
  draw();
  CHECK(seen("[challenges:passives.value.2]") && seen("[challenges:passives.desc.2]"), "Passives goes to All");
  g_screen->input(g_screen, BTN_LEFT);
  g_screen->input(g_screen, BTN_LEFT);
  g_screen->input(g_screen, BTN_DOWN);   /* back to Fresh Start (wraps) */
  draw();
  CHECK(seen("[challenges:freshStart.desc.0]") && seen("[challenges:noneSelected]"), "back on Fresh Start, nothing chosen");

  /* A with nothing chosen is the error sound, and nothing starts */
  g_screen->input(g_screen, BTN_ACTION);
  CHECK(n_error == 1 && n_start == 0 && n_pop == 0, "A on None Selected: the error sound");

  /* Right turns Fresh Start on; Start is drawn; Left back; Right stops at the maximum */
  CHECK(g_screen->input(g_screen, BTN_RIGHT), "Right is taken");
  draw();
  CHECK(seen("[challenges:freshStart.value.1]") && seen("[common:start]") && !seen("[challenges:noneSelected]") && seen("[challenges:freshStart.desc.1]"), "Fresh Start on");
  g_screen->input(g_screen, BTN_LEFT);
  draw();
  CHECK(seen("[challenges:freshStart.value.0]"), "Left is off again");
  const int sel = n_select;
  g_screen->input(g_screen, BTN_LEFT);
  CHECK(n_select == sel, "Left at 0 makes no sound");
  g_screen->input(g_screen, BTN_UP);
  g_screen->input(g_screen, BTN_DOWN);
  CHECK(n_select == sel + 2, "Up and Down sound");

  /* A starts with the values; the screen pops first */
  g_screen->input(g_screen, BTN_RIGHT);
  g_screen->input(g_screen, BTN_ACTION);
  CHECK(n_start == 1 && n_pop == 1 && started.value[CHALLENGES_FRESH_START] == 1, "A starts with Fresh Start on");

  /* B goes back */
  g_screen->input(g_screen, BTN_CANCEL);
  CHECK(n_back == 1 && n_pop == 2 && n_start == 1, "B goes back");

  /* coming back with the values as they were */
  gc_challenge_set prev = {{0}};
  prev.value[CHALLENGES_FRESH_START] = 2;
  open_with(&prev);
  draw();
  CHECK(seen("[challenges:freshStart.value.2]") && seen("[common:start]"), "the last choice is shown again");

  /* touch: a tap on the row steps the value (wraps to 0 after the last), a tap on Start starts, a tap outside does nothing */
  CHECK(g_screen->touch(g_screen, 160, 56), "a tap on the row");
  draw();
  CHECK(seen("[challenges:freshStart.value.0]") && seen("[challenges:noneSelected]"), "after the last value a tap turns it off");
  CHECK(g_screen->touch(g_screen, 160, 56), "tap again");
  draw();
  CHECK(seen("[challenges:freshStart.value.1]"), "and on");
  const int starts = n_start;
  CHECK(!g_screen->touch(g_screen, 5, 150) && !g_screen->touch(g_screen, 160, 20), "taps outside the window are not taken");
  CHECK(g_screen->touch(g_screen, 160, 168) && n_start == starts + 1 && started.value[CHALLENGES_FRESH_START] == 1, "a tap on Start starts");
  open_with(NULL);
  const int errs = n_error;
  CHECK(g_screen->touch(g_screen, 160, 168) && n_error == errs + 1 && n_start == starts + 1, "a tap on None Selected is the error sound");

  /* Limited Catch, Limited Support and Hardcore (PR 3): after Flip Stat and before Passives, upstream's order; Limited Support's four values */
  open_with(NULL);
  draw();
  CHECK(seen("[challenges:limitedCatch.name]") && seen("[challenges:limitedSupport.name]") && seen("[challenges:hardcore.name]"), "the run rule rows");
  CHECK(strstr(drawn, "flipStat.name") < strstr(drawn, "limitedCatch.name") && strstr(drawn, "limitedCatch.name") < strstr(drawn, "limitedSupport.name") &&
        strstr(drawn, "limitedSupport.name") < strstr(drawn, "hardcore.name") && strstr(drawn, "hardcore.name") < strstr(drawn, "passives.name"),
        "in upstream's order");
  for (int i = 0; i < 4; i++) g_screen->input(g_screen, BTN_DOWN);   /* Fresh Start, Inverse, Flip, Limited Catch, Limited Support */
  for (int v = 0; v <= 3; v++) {
    char want[64];
    draw();
    snprintf(want, sizeof want, "[challenges:limitedSupport.value.%d]", v);
    CHECK(seen(want), "Limited Support value %d", v);
    snprintf(want, sizeof want, "[challenges:limitedSupport.desc.%d]", v);
    CHECK(seen(want), "its description %d", v);
    CHECK(n_clip > 0 && n_clip == n_clip_off && clip_y == 196 && clip_h == 40, "the description is clipped to its strip");
    CHECK(desc_y == -39, "a description that fits does not move");
    if (v < 3) g_screen->input(g_screen, BTN_RIGHT);
  }
  CHECK(!g_screen->input(g_screen, BTN_RIGHT) || seen("[challenges:limitedSupport.value.3]"), "3 is the last value");
  /* "Both" is taller than the strip: still for 3.2 s, then up 49 px (84 - 35) over 3.2 s, held 5 s, back, 3.2 s at rest, again */
  g_screen->update(g_screen, 3000);
  draw();
  CHECK(desc_y == -39, "still during the delay");
  g_screen->update(g_screen, 1800);   /* 1.6 s into the 3.2 s scroll: halfway */
  draw();
  CHECK(desc_y > -39 - 24.6f && desc_y < -39 - 24.4f, "halfway up (%g)", desc_y);
  g_screen->update(g_screen, 1600 + 4000);
  draw();
  CHECK(desc_y == -39 - 49, "held at the top (%g)", desc_y);
  g_screen->update(g_screen, 1000 + 3200 + 1000);
  draw();
  CHECK(desc_y == -39, "back at rest (%g)", desc_y);
  g_screen->input(g_screen, BTN_LEFT);   /* a new description starts from the top */
  draw();
  g_screen->input(g_screen, BTN_RIGHT);
  draw();
  CHECK(desc_y == -39, "a new description starts at rest");
  CHECK(chm_desc_scroll(49, 3200 + 3200 + 5000 + 3200 + 3200 + 1600) > 24.4f && chm_desc_scroll(0, 9000) == 0 && chm_desc_scroll(-3, 9000) == 0,
        "the tween repeats; no overflow, no scroll");
  CHECK(chm_desc_scroll(200, 3200 + 4400) == 100, "a long overflow scrolls at 44 ms a pixel");

  printf("%d checks, %d failed\n", checks, failed);
  if (failed) return 1;
  puts("OK");
  return 0;
}
