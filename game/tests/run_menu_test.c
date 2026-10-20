/* run_menu_test: the in-run menu (source/run_menu.c) on the host, over a fake UI stack and drawing layer (no pixels: the fakes record the
 * windows and texts the menu draws, a character is 6 px wide in them).
 *
 *   make run_menu_test && build/run_menu_test
 *
 * Checks, against upstream's MenuUiHandler (src/ui/handlers/menu-ui-handler.ts, MenuOptions.SAVE_AND_QUIT) and ui-inputs.ts buttonMenu:
 * the menu opens once; its window is the canvas height less 2 and ends at the right edge; B and START close it (on_close, no quit);
 * Save and Quit in the battle's first turn quits at once, past it asks the warning (menuUiHandler:losingProgressionWarning) first, shows
 * Yes / No only once the text is typed, Yes quits, No and B on the box go back to the menu with the text gone, B on the typing text closes
 * the menu; the SD card's refusal swaps the warning for its own sentence and asks even in the first turn; on_quit runs once and only
 * after the menu has left the stack; nothing else is taken (every other button leaves it alone). */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "i18n.h"
#include "run_menu.h"
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

#define STACK_MAX 8
static ui_screen* g_stack[STACK_MAX];
static int g_depth;
static int n_push, n_pop, n_select, n_error, n_menu_open_sound;
static char last_sound[64];

void ui_push(ui_screen* s) {
  if (g_depth < STACK_MAX) g_stack[g_depth++] = s;
  n_push++;
}
void ui_pop(void) {
  if (g_depth > 0) g_depth--;
  n_pop++;
}
void ui_play_select(void) { n_select++; }
void ui_play_error(void) { n_error++; }
void ui_play_sound(const char* key) {
  snprintf(last_sound, sizeof last_sound, "%s", key ? key : "");
  if (key && !strcmp(key, "ui/menu_open")) n_menu_open_sound++;
}

static int g_which; /* 0 top, 1 bottom */
bool ui_space(ui_space_id sp) {
  const bool top = sp == UI_FIELD || sp == UI_FIELD_UI || sp == UI_TOP_CANVAS || sp == UI_TOP_CANVAS_UI || sp == UI_PHYS_TOP;
  return top == (g_which == 0);
}

typedef struct {
  float x, y, w, h;
} win_t;
static win_t g_wins[16];
static int n_wins;
void ui_window(float x, float y, float w, float h, win_variant v) {
  (void)v;
  if (n_wins < 16) g_wins[n_wins++] = (win_t){x, y, w, h};
}
static int n_cursors;
static float g_cursor_x, g_cursor_y;
void ui_cursor(float x, float y) {
  n_cursors++;
  g_cursor_x = x;
  g_cursor_y = y;
}

typedef struct {
  float x, y;
  char text[300];
} drawn_t;
static drawn_t g_drawn[32];
static int n_drawn;

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
  (void)st, (void)o;
  if (n_drawn < 32) {
    drawn_t* d = &g_drawn[n_drawn++];
    d->x = x;
    d->y = y;
    snprintf(d->text, sizeof d->text, "%s", s);
  }
  return 6.0f * (float)strlen(s);
}
void text_wrap(text_style st, const char* s, const text_opts* o, char* out, int n) {
  (void)st, (void)o;
  snprintf(out, (size_t)n, "%s", s); /* the length is checked by the test, not the breaking */
}

/* the strings the menu asks for, as upstream's English locales have them (menu-ui-handler.json, menu.json) */
const char* i18n_t(char* out, int n, const char* key, ...) {
  const char* t = key;
  if (!strcmp(key, "menuUiHandler:saveAndQuit")) t = "Save and Quit";
  if (!strcmp(key, "menuUiHandler:losingProgressionWarning")) t = "You will lose any progress since the beginning of the battle. Proceed?";
  if (!strcmp(key, "menu:yes")) t = "Yes";
  if (!strcmp(key, "menu:no")) t = "No";
  snprintf(out, (size_t)n, "%s", t);
  return out;
}

/* a message box that types one character a millisecond and is done right after the last one */
void msgbox_init(msgbox* m, text_style style, float wrap_w, int max_lines) {
  (void)style, (void)max_lines;
  memset(m, 0, sizeof *m);
  m->opts.wrap_w = wrap_w;
}
void msgbox_show(msgbox* m, const char* text, float delay, bool prompt) {
  m->delay = delay < 0 ? 20 : delay;
  m->prompt = prompt;
  snprintf(m->text, sizeof m->text, "%s", text);
  m->shown = 0;
  m->total = (int)strlen(text);
  m->done = false;
  m->acc = 0;
}
void msgbox_clear(msgbox* m) { memset(m->text, 0, sizeof m->text), m->shown = m->total = 0, m->done = false; }
void msgbox_update(msgbox* m, float dt) {
  if (m->done || !m->text[0]) return;
  m->acc += dt;
  while (m->acc >= m->delay && m->shown < m->total) {
    m->acc -= m->delay;
    m->shown++;
  }
  if (m->shown >= m->total) m->done = true;
}
bool msgbox_input(msgbox* m, ui_button b) { (void)m, (void)b; return false; }
void msgbox_draw(const msgbox* m, float x, float y) {
  if (!m->text[0]) return;
  text_draw(TS_WINDOW, x, y, m->text, NULL);
}
bool msgbox_done(const msgbox* m) { return m->done; }
bool msgbox_typing(const msgbox* m) { return !m->done && m->shown < m->total; }

/* the option list, as ui_widgets.c has it for a short list (rows of 16 px, UP / DOWN wrap, B answers -2) */
void optlist_init(optlist* l, const char* const* labels, int n) {
  memset(l, 0, sizeof *l);
  l->n = n;
  for (int i = 0; i < n; i++) l->labels[i] = labels[i];
  l->max_rows = 10;
  l->x = 319;
  l->chosen = -1;
  float w = 0;
  for (int i = 0; i < n; i++) {
    const float lw = text_width(TS_WINDOW, labels[i], NULL);
    if (lw > w) w = lw;
  }
  l->w = w + 23;
}
bool optlist_input(optlist* l, ui_button b) {
  if (b == BTN_CANCEL) {
    l->chosen = -2;
    ui_play_select();
    return true;
  }
  if (b == BTN_ACTION) {
    l->chosen = l->cursor;
    ui_play_select();
    return true;
  }
  if (b == BTN_UP) {
    l->cursor = l->cursor ? l->cursor - 1 : l->n - 1;
    ui_play_select();
    return true;
  }
  if (b == BTN_DOWN) {
    l->cursor = l->cursor < l->n - 1 ? l->cursor + 1 : 0;
    ui_play_select();
    return true;
  }
  return false;
}
static int n_yn_draws;
static float g_yn_x, g_yn_y;
static char g_yn_labels[2][16];
bool optlist_touch(optlist* l, float x, float y) { /* ui_widgets.c's: a row picks itself (rows of 16 px from 7 px under the window's top) */
  const float h = (l->n + 1) * 16.0f - 2, left = l->x - l->w, top = l->y - h;
  if (x < left || x >= l->x || y < top + 7 || y >= top + 7 + l->n * 16.0f) return false;
  l->cursor = (int)((y - top - 7) / 16);
  return optlist_input(l, BTN_ACTION);
}
void optlist_draw(const optlist* l) {
  n_yn_draws++;
  g_yn_x = l->x;
  g_yn_y = l->y;
  for (int i = 0; i < l->n && i < 2; i++) snprintf(g_yn_labels[i], sizeof g_yn_labels[i], "%s", l->labels[i]);
}

/* ------------------------------------------------------------------ helpers */

static int n_quit, n_close, depth_at_quit = -1, depth_at_close = -1;
static void on_quit(void* arg) {
  (void)arg;
  n_quit++;
  depth_at_quit = g_depth;
}
static void on_close(void* arg) {
  (void)arg;
  n_close++;
  depth_at_close = g_depth;
}

static void reset_counts(void) {
  n_push = n_pop = n_select = n_error = n_menu_open_sound = n_quit = n_close = 0;
  last_sound[0] = 0;
  g_depth = 0;
}

static bool open_menu(int turn, bool unsaved) {
  reset_counts();
  const runmenu_args a = {.turn = turn, .unsaved = unsaved, .on_quit = on_quit, .on_close = on_close};
  return run_menu_open(&a);
}

static ui_screen* top(void) { return g_depth ? g_stack[g_depth - 1] : NULL; }
static bool press(ui_button b) { return top() && top()->input(top(), b); }
/* a tap on the bottom screen at physical (x, y) */
static bool tap(float x, float y) { return top() && top()->touch && top()->touch(top(), x, y); }
static void tick(float ms) {
  if (top() && top()->update) top()->update(top(), ms);
}
static void draw(int which) {
  n_wins = n_drawn = n_cursors = n_yn_draws = 0;
  g_which = which;
  if (top()) top()->draw(top(), which ? RR_BOTTOM : RR_TOP);
}
static bool has_text(const char* needle) {
  for (int i = 0; i < n_drawn; i++)
    if (strstr(g_drawn[i].text, needle)) return true;
  return false;
}
/* types the message out */
static void type_out(void) {
  n_yn_draws = 0;
  for (int i = 0; i < 400 && top() && !(n_yn_draws > 0); i++) {
    tick(20);
    draw(1);
  }
}

static const char* WARNING = "You will lose any progress since the beginning of the battle. Proceed?";
static const char* UNSAVED = "The last save did not reach the SD card";

int main(void) {
  /* ---- opening: once, over the screen below, with upstream's sound */
  CHECK(open_menu(1, false), "the menu opens");
  CHECK(n_push == 1 && g_depth == 1 && run_menu_is_open(), "it is pushed once and is open");
  CHECK(n_menu_open_sound == 1, "ui/menu_open plays (MenuUiHandler.show), %d times", n_menu_open_sound);
  CHECK(!run_menu_open(&(runmenu_args){.turn = 1}), "a second open while it is open is refused");
  CHECK(n_push == 1, "and pushes nothing");

  /* ---- drawing: the window along the right edge, the full canvas height less 2; the bottom screen only */
  draw(0);
  CHECK(n_wins == 0 && n_drawn == 0, "nothing is drawn on the top screen (the battle shows through)");
  draw(1);
  CHECK(n_wins == 1, "the menu is one window: %d", n_wins);
  if (n_wins == 1) {
    CHECK(g_wins[0].h == 178, "the window is 178 high (the canvas less 2): %.1f", g_wins[0].h);
    CHECK(g_wins[0].x + g_wins[0].w == 319, "it ends at x 319: %.1f", g_wins[0].x + g_wins[0].w);
    CHECK(g_wins[0].y == -179, "it starts 179 above the canvas bottom (UI_BOTTOM_UI): %.1f", g_wins[0].y);
    CHECK(g_wins[0].w == 6.0f * 13 + 23, "its width is the label and 23 px of padding: %.1f", g_wins[0].w);
  }
  CHECK(has_text("Save and Quit"), "the entry reads Save and Quit");
  CHECK(n_cursors == 1, "the cursor is drawn");
  CHECK(n_yn_draws == 0 && !has_text("Proceed"), "no message box and no Yes / No yet");

  /* ---- keys the menu does not use */
  {
    const ui_button other[] = {BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_STATS, BTN_SUBMIT, BTN_CYCLE_SHINY, BTN_CYCLE_FORM};
    const int before = n_select + n_quit + n_close;
    for (size_t i = 0; i < sizeof other / sizeof other[0]; i++) CHECK(!press(other[i]), "button %d is not taken", (int)other[i]);
    CHECK(n_select + n_quit + n_close == before && g_depth == 1, "they change nothing");
  }

  /* ---- B closes it without quitting */
  CHECK(press(BTN_CANCEL), "B is taken");
  CHECK(!run_menu_is_open() && g_depth == 0 && n_close == 1 && n_quit == 0, "B closes the menu: close %d quit %d depth %d", n_close, n_quit, g_depth);

  /* ---- START closes it too (buttonMenu: UiMode.MENU -> revertMode) */
  CHECK(open_menu(5, false), "reopens");
  CHECK(press(BTN_MENU), "START is taken");
  CHECK(!run_menu_is_open() && n_close == 1 && n_quit == 0, "START closes the menu");

  /* ---- the battle's first turn: Save and Quit goes straight through */
  CHECK(open_menu(1, false), "opens in turn 1");
  CHECK(press(BTN_ACTION), "A is taken");
  CHECK(n_quit == 1 && n_close == 0 && g_depth == 0 && !run_menu_is_open(), "turn 1 quits at once: quit %d close %d depth %d", n_quit, n_close, g_depth);

  /* ---- past the first turn: the warning, typed out, then Yes / No */
  CHECK(open_menu(2, false), "opens in turn 2");
  CHECK(press(BTN_ACTION), "A is taken");
  CHECK(n_quit == 0 && g_depth == 1, "the warning comes first, nothing quits yet");
  draw(1);
  CHECK(n_wins == 2, "the menu window and the message box: %d windows", n_wins);
  if (n_wins == 2) {
    CHECK(g_wins[1].x == 2 && g_wins[1].y == -48 && g_wins[1].h == 48, "the message box is the 48 bottom rows at x 2: (%.0f, %.0f) h %.0f", g_wins[1].x, g_wins[1].y, g_wins[1].h);
    CHECK(g_wins[1].x + g_wins[1].w <= g_wins[0].x + 1, "and stops at the menu window, 1 px of overlap as upstream's (%.0f vs %.0f)", g_wins[1].x + g_wins[1].w, g_wins[0].x);
    CHECK(g_wins[1].w == 320 - g_wins[0].w - 2, "its width is the canvas less the window less 2: %.0f", g_wins[1].w);
  }
  CHECK(n_yn_draws == 0, "no Yes / No while the text is still typing");
  CHECK(!press(BTN_ACTION), "A does not skip the typing or answer");
  tick(20);
  draw(1);
  CHECK(n_yn_draws == 0, "after one tick the text is not done");
  type_out();
  CHECK(has_text(WARNING), "the warning is menuUiHandler:losingProgressionWarning, as upstream has it");
  CHECK(n_yn_draws > 0, "Yes / No shows once the text is done");
  CHECK(!strcmp(g_yn_labels[0], "Yes") && !strcmp(g_yn_labels[1], "No"), "Yes first, No second: %s / %s", g_yn_labels[0], g_yn_labels[1]);
  CHECK(g_yn_x == 319 - g_wins[0].w, "the box sits at the menu window's left edge: %.0f vs %.0f", g_yn_x, 319 - g_wins[0].w);
  CHECK(g_yn_y == -48, "and 48 up (ConfirmUiHandler yOffset): %.0f", g_yn_y);
  CHECK(n_quit == 0, "still nothing quit");

  /* No: back to the menu, the text gone */
  CHECK(press(BTN_DOWN) && press(BTN_ACTION), "DOWN, A on No");
  CHECK(n_quit == 0 && g_depth == 1 && run_menu_is_open(), "No keeps the menu open and does not quit");
  draw(1);
  CHECK(n_wins == 1 && !has_text("Proceed") && n_yn_draws == 0, "the message and the box are gone: %d windows", n_wins);

  /* again, and B on the Yes / No box is its No */
  CHECK(press(BTN_ACTION), "A on Save and Quit again");
  type_out();
  CHECK(n_yn_draws > 0, "the box is back");
  CHECK(press(BTN_CANCEL), "B on the box");
  CHECK(n_quit == 0 && n_close == 0 && run_menu_is_open(), "B on the box is No: the menu stays");
  draw(1);
  CHECK(n_wins == 1 && n_yn_draws == 0, "box and message gone");

  /* Yes */
  CHECK(press(BTN_ACTION), "A on Save and Quit a third time");
  type_out();
  CHECK(press(BTN_ACTION), "A on Yes (the cursor opens on Yes, as ui_push_confirm's)");
  CHECK(n_quit == 1 && n_close == 0 && g_depth == 0 && !run_menu_is_open(), "Yes quits: quit %d close %d depth %d", n_quit, n_close, g_depth);
  CHECK(!press(BTN_ACTION), "a closed menu takes nothing");
  CHECK(n_quit == 1, "and quits once");

  /* B while the warning is still typing closes the menu (MenuUiHandler.processInput CANCEL) */
  CHECK(open_menu(3, false), "opens in turn 3");
  CHECK(press(BTN_ACTION), "A");
  tick(20);
  CHECK(press(BTN_CANCEL), "B while typing");
  CHECK(!run_menu_is_open() && n_close == 1 && n_quit == 0, "closes without quitting");
  /* and START in the same place */
  CHECK(open_menu(3, false), "opens");
  CHECK(press(BTN_ACTION), "A");
  CHECK(press(BTN_MENU), "START while typing");
  CHECK(!run_menu_is_open() && n_close == 1 && n_quit == 0, "closes without quitting");
  /* and START on the Yes / No box closes the whole menu */
  CHECK(open_menu(3, false), "opens");
  CHECK(press(BTN_ACTION), "A");
  type_out();
  CHECK(press(BTN_MENU), "START on the box");
  CHECK(!run_menu_is_open() && n_close == 1 && n_quit == 0, "closes without quitting");

  /* ---- the SD card refused the save: its own sentence, even in turn 1 */
  CHECK(open_menu(1, true), "opens, unsaved, turn 1");
  CHECK(press(BTN_ACTION), "A");
  CHECK(n_quit == 0 && run_menu_is_open(), "it asks before quitting");
  type_out();
  CHECK(has_text(UNSAVED) && !has_text(WARNING), "the sentence is the SD card's, not the battle warning");
  CHECK(press(BTN_ACTION), "Yes");
  CHECK(n_quit == 1, "Yes quits");
  /* turn 4 and unsaved: still that sentence (the battle's warning would understate the loss) */
  CHECK(open_menu(4, true), "opens, unsaved, turn 4");
  CHECK(press(BTN_ACTION), "A");
  type_out();
  CHECK(has_text(UNSAVED) && !has_text(WARNING), "the SD card's sentence again");
  CHECK(press(BTN_CANCEL) && press(BTN_MENU), "B backs out of the box, START closes");
  CHECK(!run_menu_is_open() && n_quit == 0 && n_close == 1, "nothing quit: close %d quit %d", n_close, n_quit);

  /* ---- on_quit and on_close run after the menu has left the stack, so they may push their own screens */
  CHECK(open_menu(1, false) && press(BTN_ACTION), "opens in turn 1 and quits");
  CHECK(n_quit == 1 && depth_at_quit == 0, "on_quit saw the stack without the menu (depth %d)", depth_at_quit);
  CHECK(open_menu(1, false) && press(BTN_CANCEL), "opens and closes");
  CHECK(n_close == 1 && depth_at_close == 0, "on_close saw the stack without the menu (depth %d)", depth_at_close);

  /* ---- taps: the window is x 218..319 and physical y 61..92 (the one row, 7 px of padding above it and 8 below) */
  CHECK(open_menu(1, false), "opens for taps");
  CHECK(!tap(100, 70) && !tap(250, 40) && !tap(250, 100), "a tap beside, above or below the entry is not taken");
  CHECK(n_quit == 0 && g_depth == 1, "and nothing happens");
  CHECK(tap(250, 70), "a tap on the entry is taken");
  CHECK(n_quit == 1 && g_depth == 0, "Save and Quit in the first turn quits, as A does: quit %d depth %d", n_quit, g_depth);
  CHECK(open_menu(2, false) && tap(250, 70), "turn 2: the tap starts the warning");
  CHECK(n_quit == 0 && g_depth == 1, "which asks before quitting");
  CHECK(!tap(250, 70), "a second tap on the entry while it types is not taken");
  type_out();
  CHECK(n_yn_draws > 0, "the Yes / No box is up");
  CHECK(!tap(100, 160) && !tap(200, 150) && !tap(300, 160), "taps beside and above the Yes / No box (x 177..218) are not taken");
  CHECK(tap(200, 178) && n_quit == 0 && run_menu_is_open(), "a tap on No (physical y 169..185) goes back to the menu");
  draw(1);
  CHECK(n_wins == 1 && n_yn_draws == 0, "box and message gone");
  CHECK(tap(250, 70), "the entry again");
  type_out();
  CHECK(tap(200, 160), "a tap on Yes (physical y 153..169)");
  CHECK(n_quit == 1 && g_depth == 0 && !run_menu_is_open(), "quits: quit %d depth %d", n_quit, g_depth);

  printf("%d checks, %d failed\n", checks, failed);
  return failed ? 1 : 0;
}
