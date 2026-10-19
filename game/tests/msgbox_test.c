/* msgbox_test: the message box (source/ui_widgets.c, upstream's MessageUiHandler.showText) with a fake text and drawing layer: typing,
 * "$" pages, and the @c{variant} / @d{frames} / @s{sound} markers.
 *
 *   make msgbox_test && build/msgbox_test
 *
 * Checks against src/ui/handlers/message-ui-handler.ts: one character per 20 ms; a marker at character k fires when the k-th character is
 * on screen (the first timer tick, with no character yet, for k = 0: a page that opens with @c{shock}); @d pauses for that many frames of
 * 1/60 s; @s plays the sound; @c hands its expression to the owner once; a page keeps only its own markers; the markers are not in the text. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

const rr_draw_opts RR_DEFAULT_OPTS = {0, 0, 1, 1, 0, RR_RGBA(255, 255, 255, 255)};
rr_atlas* rr_atlas_load_key(const char* key) { (void)key; return NULL; }
void rr_atlas_free(rr_atlas* a) { (void)a; }
const rr_frame* rr_atlas_find(const rr_atlas* a, const char* name) { (void)a, (void)name; return NULL; }
const rr_frame* rr_atlas_frame(const rr_atlas* a, int i) { (void)a, (void)i; return NULL; }
int rr_atlas_count(const rr_atlas* a) { (void)a; return 0; }
int rr_atlas_pages(const rr_atlas* a) { (void)a; return 0; }
rr_tex* rr_atlas_page(const rr_atlas* a, int i) { (void)a, (void)i; return NULL; }
size_t rr_tex_bytes(const rr_tex* t) { (void)t; return 0; }
void rr_draw(const rr_frame* f, float x, float y, const rr_draw_opts* o) { (void)f, (void)x, (void)y, (void)o; }
void rr_rect(float x, float y, float w, float h, rr_color c) { (void)x, (void)y, (void)w, (void)h, (void)c; }
void rr_nineslice(const rr_frame* f, float x, float y, float w, float h, int border) { (void)f, (void)x, (void)y, (void)w, (void)h, (void)border; }

text_opts text_defaults(void) {
  text_opts o;
  memset(&o, 0, sizeof o);
  o.chars = -1;
  return o;
}
int text_utf8_len(const char* s) {
  int n = 0;
  for (; *s; s++) n += ((unsigned char)*s & 0xC0) != 0x80;
  return n;
}
float text_width(text_style st, const char* s, const text_opts* o) { (void)st, (void)o; return 6.0f * (float)text_utf8_len(s); }
float text_line_width(text_style st, int font_px, const char* s, int nbytes) { (void)st, (void)font_px; return 6.0f * (float)(nbytes < 0 ? text_utf8_len(s) : nbytes); }
static char g_drawn[512];
float text_draw(text_style st, float x, float y, const char* s, const text_opts* o) {
  (void)st, (void)x, (void)y;
  /* the first o->chars characters of s */
  int n = o->chars < 0 ? text_utf8_len(s) : o->chars, bytes = 0;
  for (; s[bytes] && n > 0; bytes++)
    if (((unsigned char)s[bytes + 1] & 0xC0) != 0x80) n--;
  snprintf(g_drawn, sizeof g_drawn, "%.*s", bytes, s);
  return 0;
}
void text_wrap(text_style st, const char* s, const text_opts* o, char* out, int n) { (void)st, (void)o; snprintf(out, (size_t)n, "%s", s); }
const char* i18n_t(char* out, int n, const char* key, ...) { snprintf(out, (size_t)n, "[%s]", key); return out; }

static char g_sounds[8][24];
static int n_sounds, n_selects;
void ui_play_sound(const char* key) { if (n_sounds < 8) snprintf(g_sounds[n_sounds++], 24, "%s", key); }
void ui_play_select(void) { n_selects++; }
void ui_play_error(void) {}
void ui_push(ui_screen* s) { (void)s; }
void ui_pop(void) {}
bool ui_space(ui_space_id sp) { (void)sp; return true; }

/* ------------------------------------------------------------------ helpers */

static msgbox M;
/* typing: `ms` of time, 1 ms steps, collecting the expressions handed over */
static char g_vars[8][24];
static int n_vars;
static void type(float ms) {
  for (float t = 0; t < ms; t += 1.0f) {
    msgbox_update(&M, 1.0f);
    const char* v = msgbox_take_variant(&M);
    if (v && n_vars < 8) snprintf(g_vars[n_vars++], 24, "%s", v);
  }
}
static const char* shown(void) {
  msgbox_draw(&M, 0, 0);
  return g_drawn;
}
static void start(const char* text, bool prompt) {
  msgbox_init(&M, TS_WINDOW, 300, 3);
  n_vars = 0;
  n_sounds = 0;
  msgbox_show(&M, text, -1, prompt);
}

int main(void) {
  /* plain typing: the timer's first tick (20 ms) shows nothing, then one character per 20 ms; the prompt comes with the last one */
  start("Hello", true);
  type(39);
  CHECK(!strcmp(shown(), ""), "nothing before the second tick ('%s')", shown());
  type(1);
  CHECK(!strcmp(shown(), "H"), "one character at 40 ms ('%s')", shown());
  type(80);
  CHECK(!strcmp(shown(), "Hello") && !M.waiting, "all of it at 120 ms ('%s')", shown());
  type(1);
  CHECK(M.waiting, "and the prompt on the next frame");
  CHECK(!msgbox_done(&M), "not done before A");
  CHECK(msgbox_input(&M, BTN_ACTION) && msgbox_done(&M), "A answers it");
  CHECK(!msgbox_input(&M, BTN_ACTION), "and a second A is not used");

  /* @c at the very start: fires on the first tick, with no character yet */
  start("@c{shock}You're back?$@c{smile}Let's go home.", true);
  CHECK(!strcmp(M.text, "You're back?"), "the marker is not in the text ('%s')", M.text);
  type(19);
  CHECK(n_vars == 0, "not before the first tick");
  type(1);
  CHECK(n_vars == 1 && !strcmp(g_vars[0], "shock") && !strcmp(shown(), ""), "at the first tick: shock, before any character (%d, '%s')", n_vars, shown());
  type(2000);
  CHECK(n_vars == 1, "once");
  CHECK(msgbox_take_variant(&M) == NULL, "and handed over once");
  CHECK(M.waiting, "the page waits");
  CHECK(msgbox_input(&M, BTN_ACTION) && !msgbox_done(&M) && !strcmp(M.text, "Let's go home."), "A brings the next page ('%s')", M.text);
  type(20);
  CHECK(n_vars == 2 && !strcmp(g_vars[1], "smile"), "its own marker fires on its first tick (%d)", n_vars);
  type(2000);
  CHECK(msgbox_input(&M, BTN_ACTION) && msgbox_done(&M), "A on the last page ends it");

  /* a marker in the middle fires when its character is on screen */
  start("abc@c{angry}def", true);
  type(20 * 4 - 1);
  CHECK(n_vars == 0 && !strcmp(shown(), "ab"), "'ab' on screen: not yet ('%s')", shown());
  type(1);
  CHECK(n_vars == 1 && !strcmp(g_vars[0], "angry") && !strcmp(shown(), "abc"), "'abc' on screen: the marker fires (%d, '%s')", n_vars, shown());
  type(20);
  CHECK(!strcmp(shown(), "abcd"), "and the typing goes on ('%s')", shown());

  /* a long name like upstream's: smile_wave_wink, serious_smile_fists */
  start("@c{serious_smile_fists}x", true);
  type(20);
  CHECK(n_vars == 1 && !strcmp(g_vars[0], "serious_smile_fists"), "a long expression name (%s)", g_vars[0]);

  /* @d: pause for that many 1/60 s frames once the character before it is on screen */
  start("ab@d{60}cd", true);
  type(60);
  CHECK(!strcmp(shown(), "ab"), "'ab' on screen at 60 ms ('%s')", shown());
  type(999);
  CHECK(!strcmp(shown(), "ab"), "a second of pause (60 frames) has not passed ('%s')", shown());
  type(2);
  type(20);
  CHECK(!strcmp(shown(), "abc"), "then it types on ('%s')", shown());
  type(100);
  CHECK(!strcmp(shown(), "abcd"), "to the end ('%s')", shown());

  /* @d at the very start: the pause comes on the first tick, before the first character */
  start("@d{30}go", true);
  type(499 + 20);
  CHECK(!strcmp(shown(), ""), "30 frames (500 ms) before the first character ('%s')", shown());
  type(2);
  type(40);
  CHECK(shown()[0] == 'g', "then it types ('%s')", shown());

  /* @s: the sound plays when its character is on screen, at the start on the first tick */
  start("@s{se/pb_rel}hi@s{se/hit}!", true);
  type(20);
  CHECK(n_sounds == 1 && !strcmp(g_sounds[0], "se/pb_rel"), "a sound at the start (%d)", n_sounds);
  type(39);
  CHECK(n_sounds == 1, "'hi' is not complete yet (%d)", n_sounds);
  type(1);
  CHECK(n_sounds == 2 && !strcmp(g_sounds[1], "se/hit"), "the second when 'hi' is on screen (%d)", n_sounds);

  /* no typing delay: everything at once, the markers do not fire (upstream: delay 0 skips the timer) */
  msgbox_init(&M, TS_WINDOW, 300, 3);
  n_sounds = 0;
  n_vars = 0;
  msgbox_show(&M, "@c{shock}hi", 0, false);
  type(100);
  CHECK(n_vars == 0 && !strcmp(shown(), "hi"), "delay 0: all at once, no expression change ('%s', %d)", shown(), n_vars);

  /* a new page forgets the old page's markers; the variant of an old page is not handed over late */
  start("@c{a}x$y", true);
  type(20);
  CHECK(!msgbox_input(&M, BTN_ACTION), "(typing is not skipped)");
  type(2000);
  CHECK(msgbox_input(&M, BTN_ACTION), "next page");
  type(2000);
  CHECK(n_vars == 1, "only the first page's marker fired (%d)", n_vars);

  /* eight markers at most per page, the rest are dropped without harm */
  start("@c{a}1@c{b}2@c{c}3@c{d}4@c{e}5@c{f}6@c{g}7@c{h}8@c{i}9@c{j}0", true);
  type(5000);
  CHECK(n_vars == 8 && !strcmp(shown(), "1234567890"), "eight markers, all the text (%d, '%s')", n_vars, shown());

  printf("msgbox_test: %d checks, %d failed\n", checks, failed);
  return failed ? 1 : 0;
}
