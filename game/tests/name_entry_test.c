/* name_entry_test: the text entry screen (source/name_entry.c) on the host, over a fake UI stack and drawing layer (no pixels: the fakes record
 * what is drawn; a character is g_char_w px wide in them).
 *
 *   make name_entry_test && build/name_entry_test
 *
 * Checks, against name_entry.h: the cursor starts on the first letter and moves over the 10 x 5 keys and the two rows under them as the D-pad
 * says (wrapping, and aiming for the same column over a wide key); A types the key under the cursor, Shift (the key, L, R) is off / the next
 * letter / all letters and the accented vowels get their Latin-1 capitals; the text stops at max_chars with the error sound; B deletes
 * a whole UTF-8 character and, with nothing typed, cancels; X types a space, Y deletes; START and the ok button hand over the trimmed text
 * ("" when only spaces), the cancel button hands over NULL; the callback runs once and nothing is taken after it; the screens draw the title,
 * the label, the text (its end when it does not fit), a count and the 55 keys. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "i18n.h"
#include "name_entry.h"
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

static ui_screen* g_screen;
static int n_set, n_select, n_error;
void ui_set(ui_screen* s) {
  g_screen = s;
  n_set++;
}
void ui_play_select(void) { n_select++; }
void ui_play_error(void) { n_error++; }

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

typedef struct {
  float x, y, w, h;
  rr_color c;
} rect_t;
static rect_t g_rects[256];
static int n_rects;
void rr_rect(float x, float y, float w, float h, rr_color c) {
  if (n_rects < 256) g_rects[n_rects++] = (rect_t){x, y, w, h, c};
}

typedef struct {
  float x, y;
  char text[300];
} drawn_t;
static drawn_t g_drawn[128];
static int n_drawn;
static float g_char_w = 6;

text_opts text_defaults(void) {
  text_opts o;
  memset(&o, 0, sizeof o);
  o.chars = TEXT_ALL;
  return o;
}
static int count_chars(const char* s) {
  int n = 0;
  for (; *s; s++)
    if (((unsigned char)*s & 0xc0) != 0x80) n++;
  return n;
}
float text_width(text_style st, const char* s, const text_opts* o) {
  (void)st, (void)o;
  return g_char_w * (float)count_chars(s);
}
float text_draw(text_style st, float x, float y, const char* s, const text_opts* o) {
  (void)st, (void)o;
  if (n_drawn < 128) {
    drawn_t* d = &g_drawn[n_drawn++];
    d->x = x;
    d->y = y;
    snprintf(d->text, sizeof d->text, "%s", s);
  }
  return g_char_w * (float)count_chars(s);
}

const char* i18n_t(char* out, int n, const char* key, ...) {
  const char* t = key;
  if (!strcmp(key, "menu:renameRun")) t = "Rename Run";
  if (!strcmp(key, "menu:runName")) t = "Run Name";
  if (!strcmp(key, "menu:rename")) t = "Rename";
  if (!strcmp(key, "menu:cancel")) t = "Cancel";
  snprintf(out, (size_t)n, "%s", t);
  return out;
}

/* ------------------------------------------------------------------ the owner */

static int n_done;
static bool got_text; /* on_done got text, not NULL */
static char result[200];
static void on_done(const char* text, void* arg) {
  (void)arg;
  n_done++;
  got_text = text != NULL;
  snprintf(result, sizeof result, "%s", text ? text : "<cancelled>");
}

static void open_entry(int max_chars) {
  n_done = 0;
  got_text = false;
  result[0] = '\0';
  const name_entry_args a = {"menu:renameRun", "menu:runName", "menu:rename", "menu:cancel", max_chars, on_done, NULL};
  name_entry_open(&a);
}

static bool press(ui_button b) { return g_screen->input(g_screen, b); }
static bool tap(float x, float y) { return g_screen->touch(g_screen, x, y); }
/* the centre of key column c / row r of the layout name_entry.c draws on the bottom screen, and of its wide keys */
#define KX(c) (20.0f + 28.0f * (float)(c) + 13.0f)
#define KY(r) (28.0f + 26.0f * (float)(r) + 12.0f)
#define SPECIAL_Y (28.0f + 26.0f * 5 + 6.0f + 12.0f)
#define BTN_Y (28.0f + 26.0f * 6 + 6.0f + 12.0f)
#define OK_X 89.0f      /* the ok button, columns 0..4: x 20..158 */
#define CANCEL_X 229.0f /* the cancel button, columns 5..9: x 160..298 */

/* the keys as the test knows them (the layout name_entry.h describes) */
static const char* const ROWS5[5][10] = {
    {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"},
    {"q", "w", "e", "r", "t", "y", "u", "i", "o", "p"},
    {"a", "s", "d", "f", "g", "h", "j", "k", "l", "'"},
    {"z", "x", "c", "v", "b", "n", "m", ",", ".", "-"},
    {"\xc3\xa0", "\xc3\xa8", "\xc3\xa9", "\xc3\xac", "\xc3\xb2", "\xc3\xb9", "!", "?", "(", ")"},
};

/* where the cursor is, as the test follows it: row 0..4 keys, 5 the Shift / Space / Delete row, 6 the buttons */
static int cur_row, cur_col;

static void home(void) {
  open_entry(20);
  cur_row = 1; /* the first letter */
  cur_col = 0;
}

static void go(int row, int col) {
  while (cur_row < row) {
    press(BTN_DOWN);
    cur_row++;
  }
  while (cur_row > row) {
    press(BTN_UP);
    cur_row--;
  }
  while (cur_col < col) {
    press(BTN_RIGHT);
    cur_col++;
  }
  while (cur_col > col) {
    press(BTN_LEFT);
    cur_col--;
  }
}

/* presses the key that types `s` (a lower case letter, digit or mark of the grid) */
static bool type_key(const char* s) {
  for (int r = 0; r < 5; r++)
    for (int c = 0; c < 10; c++)
      if (!strcmp(ROWS5[r][c], s)) {
        go(r, c);
        press(BTN_ACTION);
        return true;
      }
  return false;
}

static void type_text(const char* s) {
  char one[5];
  for (const char* p = s; *p;) {
    int len = 1;
    if (((unsigned char)*p & 0xe0) == 0xc0) len = 2;
    memcpy(one, p, (size_t)len);
    one[len] = '\0';
    CHECK(type_key(one), "no key for \"%s\"", one);
    p += len;
  }
}

/* The special cells are reached the way a player does: down from a key above them (the cursor aims for that key's column). The model of
 * the cursor (cur_row, cur_col) only follows moves over the 10 columns of the key rows and vertical moves; the cells that are wider than a key
 * (Shift 0-1, Space 2-7, Delete 8-9 in row 5, ok 0-4 and cancel 5-9 in row 6) are entered and left vertically in these tests, but for the
 * navigation checks at the end. */
static void key_special(int col, int rows_down) {
  go(4, col);
  for (int i = 0; i < rows_down; i++) press(BTN_DOWN);
  cur_row = 4 + rows_down;
  press(BTN_ACTION);
}
static void key_shift(void) { key_special(0, 1); }
static void key_space(void) { key_special(4, 1); }
static void key_delete(void) { key_special(9, 1); }
static void btn_ok(int col) { key_special(col, 2); }      /* col 0..4 */
static void btn_cancel(int col) { key_special(col, 2); }  /* col 5..9 */

static void draw_both(void) {
  n_wins = n_rects = n_drawn = 0;
  g_which = 0;
  g_screen->draw(g_screen, RR_TOP);
}
static void draw_bottom(void) {
  n_wins = n_rects = n_drawn = 0;
  g_which = 1;
  g_screen->draw(g_screen, RR_BOTTOM);
  g_which = 0;
}
static bool drew(const char* s) {
  for (int i = 0; i < n_drawn; i++)
    if (!strcmp(g_drawn[i].text, s)) return true;
  return false;
}

/* ------------------------------------------------------------------ the tests */

int main(void) {
  /* ---- opening */
  open_entry(20);
  CHECK(g_screen && n_set == 1 && !strcmp(g_screen->name, "name-entry") && g_screen->flags == UI_FADE, "the entry is the screen, with a fade");
  CHECK(n_done == 0, "nothing is done at the start");
  /* ---- typing */
  home();
  type_text("hello");
  press(BTN_MENU);
  CHECK(n_done == 1 && got_text && !strcmp(result, "hello"), "typed text: \"%s\"", result);

  home();
  type_text("a'b-1.");
  press(BTN_MENU);
  CHECK(!strcmp(result, "a'b-1."), "marks and digits: \"%s\"", result);

  /* ---- the digits, the accents and every key of the grid types what it shows */
  home();
  for (int r = 0; r < 5; r++)
    for (int c = 0; c < 10; c++) {
      go(r, c);
      press(BTN_ACTION);
    }
  press(BTN_MENU);
  CHECK(!strcmp(result, "1234567890qwertyuiop"), "every key in order, the text stops at 20: \"%s\"", result);

  /* ---- the 20 character limit: the key that does not fit gives the error sound and changes nothing */
  home();
  const int err0 = n_error;
  type_text("abcdefghijklmnopqrst"); /* 20 */
  CHECK(n_error == err0, "20 characters fit without an error");
  type_text("uv");
  CHECK(n_error == err0 + 2, "characters 21 and 22 were refused (%d errors)", n_error - err0);
  press(BTN_MENU);
  CHECK(!strcmp(result, "abcdefghijklmnopqrst"), "the text kept its 20 characters: \"%s\"", result);

  open_entry(3);
  cur_row = 1;
  cur_col = 0;
  type_text("abcd");
  press(BTN_MENU);
  CHECK(!strcmp(result, "abc"), "max_chars 3: \"%s\"", result);

  /* a limit above the buffer is brought down to what the buffer holds: 31 characters of 4 bytes */
  open_entry(500);
  cur_row = 1;
  cur_col = 0;
  for (int i = 0; i < 40; i++) {
    cur_row = 1;
    cur_col = 0;
    go(4, 0); /* an accented key: 2 bytes */
    press(BTN_ACTION);
  }
  press(BTN_MENU);
  CHECK(strlen(result) <= NE_MAX_BYTES - 1 && count_chars(result) <= NE_MAX_BYTES / 4 - 1, "a huge limit is cut to the buffer: %zu bytes", strlen(result));

  /* ---- Shift: off, the next letter, all letters */
  home();
  press(BTN_CYCLE_FORM);
  type_text("ab");
  press(BTN_MENU);
  CHECK(!strcmp(result, "Ab"), "Shift once capitalizes one letter: \"%s\"", result);

  home();
  press(BTN_CYCLE_SHINY);
  press(BTN_CYCLE_SHINY);
  type_text("abc");
  press(BTN_CYCLE_SHINY);
  type_text("d");
  press(BTN_MENU);
  CHECK(!strcmp(result, "ABCd"), "Shift twice keeps capitals, the third press ends them: \"%s\"", result);

  home();
  press(BTN_CYCLE_FORM);
  type_text("1");
  type_text("a");
  press(BTN_MENU);
  CHECK(!strcmp(result, "1A"), "a digit does not use up the Shift: \"%s\"", result);

  home();
  key_shift();
  type_text("\xc3\xa8"); /* e grave */
  type_text("e");
  press(BTN_MENU);
  CHECK(!strcmp(result, "\xc3\x88" "e"), "Shift from its key, an accented capital: \"%s\"", result);

  home();
  key_shift();
  key_shift();
  type_text("\xc3\xa0\xc3\xac\xc3\xb9"); /* a grave, i grave, u grave */
  type_text("!");
  press(BTN_MENU);
  CHECK(!strcmp(result, "\xc3\x80\xc3\x8c\xc3\x99!"), "locked Shift: accented capitals and a mark that stays: \"%s\"", result);

  /* ---- deleting */
  home();
  type_text("abc");
  press(BTN_CANCEL);
  CHECK(n_done == 0, "B with text only deletes");
  press(BTN_MENU);
  CHECK(!strcmp(result, "ab"), "B deleted one character: \"%s\"", result);

  home();
  type_text("a\xc3\xa0");
  press(BTN_CANCEL);
  press(BTN_MENU);
  CHECK(!strcmp(result, "a"), "B deleted a whole 2-byte character: \"%s\"", result);

  home();
  press(BTN_CANCEL);
  CHECK(n_done == 1 && !got_text && !strcmp(result, "<cancelled>"), "B with nothing typed cancels");

  home();
  type_text("xy");
  press(BTN_CYCLE_NATURE);
  press(BTN_CYCLE_NATURE);
  const int err1 = n_error;
  press(BTN_CYCLE_NATURE);
  CHECK(n_error == err1 + 1 && n_done == 0, "Y deletes and, with nothing to delete, gives the error sound");
  type_text("q");
  key_delete();
  const int err2 = n_error;
  key_delete();
  CHECK(n_error == err2 + 1, "the Delete key with nothing to delete gives the error sound");
  press(BTN_MENU);
  CHECK(!strcmp(result, ""), "the Delete key deletes: \"%s\"", result);

  /* ---- space and trimming */
  home();
  press(BTN_CYCLE_TERA);
  type_text("a");
  key_space();
  key_space();
  type_text("b");
  key_space();
  press(BTN_CYCLE_TERA);
  press(BTN_MENU);
  CHECK(n_done == 1 && got_text && !strcmp(result, "a  b"), "spaces inside stay, the ends are trimmed: \"%s\"", result);

  home();
  press(BTN_CYCLE_TERA);
  press(BTN_CYCLE_TERA);
  press(BTN_MENU);
  CHECK(n_done == 1 && got_text && result[0] == '\0', "only spaces: an empty text, not a cancel (\"%s\")", result);

  /* ---- the buttons */
  home();
  type_text("run");
  btn_ok(0);
  CHECK(n_done == 1 && !strcmp(result, "run"), "the ok button: \"%s\"", result);
  home();
  type_text("run");
  btn_ok(4); /* still ok */
  CHECK(n_done == 1 && !strcmp(result, "run"), "the ok button's last column: \"%s\"", result);
  home();
  type_text("run");
  btn_cancel(5);
  CHECK(n_done == 1 && !got_text, "the cancel button hands over NULL");
  home();
  type_text("run");
  btn_cancel(9);
  CHECK(n_done == 1 && !got_text, "the cancel button's last column");

  /* ---- after the answer */
  home();
  type_text("done");
  press(BTN_MENU);
  CHECK(n_done == 1, "answered once");
  const int sel_done = n_select;
  CHECK(press(BTN_ACTION) && press(BTN_MENU) && press(BTN_CANCEL) && press(BTN_UP), "input is taken (and ignored) after the answer");
  CHECK(n_done == 1 && n_select == sel_done, "nothing happened after the answer: %d answers", n_done);

  /* ---- moving: wraps, and the column kept over a wide key */
  home();
  cur_row = 1;
  cur_col = 0;
  press(BTN_LEFT); /* wraps along the row */
  press(BTN_ACTION);
  press(BTN_MENU);
  CHECK(!strcmp(result, "p"), "left of the first key is the last of its row: \"%s\"", result);
  home();
  press(BTN_RIGHT);
  press(BTN_UP); /* 2 */
  press(BTN_ACTION);
  press(BTN_MENU);
  CHECK(!strcmp(result, "2"), "up from the first letter row is the digits: \"%s\"", result);
  home();
  press(BTN_UP);
  press(BTN_UP); /* wraps to the buttons */
  press(BTN_ACTION);
  CHECK(n_done == 1 && got_text, "up from the digits wraps to the buttons (the left one is ok)");
  home();
  go(4, 8);
  press(BTN_DOWN); /* Delete, columns 8..9: the cursor aims for the column 8 it came from */
  const int err3 = n_error;
  press(BTN_ACTION); /* nothing to delete */
  CHECK(n_error == err3 + 1, "A on Delete");
  press(BTN_UP);     /* row 4, column 8 */
  press(BTN_ACTION);
  press(BTN_MENU);
  CHECK(!strcmp(result, "("), "up from Delete came back to column 8: \"%s\"", result);

  home();
  go(4, 0);
  press(BTN_DOWN);  /* Shift */
  press(BTN_RIGHT); /* Space: it now aims for the middle of Space, column 4 */
  press(BTN_DOWN);  /* the buttons: ok (0..4) */
  press(BTN_ACTION);
  CHECK(n_done == 1 && got_text && result[0] == '\0', "right of Shift is Space and down from it the ok button");

  home();
  go(4, 0);
  press(BTN_DOWN);  /* Shift */
  press(BTN_RIGHT); /* Space */
  press(BTN_UP);    /* row 4, column 4 */
  press(BTN_ACTION);
  press(BTN_MENU);
  CHECK(!strcmp(result, "\xc3\xb2"), "up from Space's middle is column 4 (o grave): \"%s\"", result);

  home();
  go(4, 9);
  press(BTN_DOWN);  /* Delete */
  press(BTN_RIGHT); /* wraps to Shift */
  press(BTN_ACTION);
  cur_row = 5; /* the cursor is on Shift, aiming for column 0 */
  cur_col = 0;
  type_text("q");
  press(BTN_MENU);
  CHECK(!strcmp(result, "Q"), "right of Delete wraps to Shift: \"%s\"", result);

  home();
  go(4, 0);
  press(BTN_DOWN);  /* Shift */
  press(BTN_LEFT);  /* wraps to Delete */
  press(BTN_ACTION);
  press(BTN_UP);
  press(BTN_UP);
  press(BTN_UP);
  press(BTN_UP);
  press(BTN_UP);    /* row 0 */
  press(BTN_ACTION);
  press(BTN_MENU);
  CHECK(!strcmp(result, "9"), "left of Shift wraps to Delete; up from Delete aims for its middle: \"%s\"", result);

  home();
  go(4, 7);
  press(BTN_DOWN);
  press(BTN_DOWN);  /* cancel (5..9), aiming for column 7 */
  press(BTN_UP);    /* row 5, column 7: Space */
  press(BTN_ACTION);/* a space */
  press(BTN_UP);    /* row 4, column 7 */
  press(BTN_ACTION);/* '?' */
  press(BTN_MENU);
  CHECK(!strcmp(result, "?"), "up from cancel keeps column 7 (Space) and then reaches '?': \"%s\"", result);

  /* ---- taps on the bottom screen (physical px; the keys are drawn there: the first key's left at 20, top at 28, keys 26 x 24 on a pitch of
   * 28 x 26, the Shift / Space / Delete row and the buttons under it 6 px lower). A tap on a key is the cursor on it and A. */
  CHECK(g_screen->touch != NULL, "the screen takes taps");
  for (int r = 0; r < 5; r++)
    for (int c = 0; c < 10; c++) {
      open_entry(20);
      CHECK(tap(KX(c), KY(r)), "a tap on key %d,%d is taken", r, c);
      CHECK(tap(OK_X, BTN_Y), "the ok button");
      CHECK(n_done == 1 && !strcmp(result, ROWS5[r][c]), "a tap on key %d,%d types \"%s\": \"%s\"", r, c, ROWS5[r][c], result);
    }
  open_entry(20);
  for (int r = 0; r < 2; r++)
    for (int c = 0; c < 10; c++) tap(KX(c), KY(r));
  tap(OK_X, BTN_Y);
  CHECK(!strcmp(result, "1234567890qwertyuiop"), "taps in a row type in order: \"%s\"", result);

  /* Shift from its key (x 20..74): the next letter, all letters, off */
  open_entry(20);
  tap(47, SPECIAL_Y);
  tap(KX(0), KY(1));
  tap(KX(1), KY(1));
  tap(47, SPECIAL_Y);
  tap(47, SPECIAL_Y);
  tap(KX(2), KY(1));
  tap(KX(3), KY(1));
  tap(47, SPECIAL_Y);
  tap(KX(4), KY(1));
  tap(OK_X, BTN_Y);
  CHECK(!strcmp(result, "QwERt"), "Shift once capitalizes one letter, twice all of them, a third tap ends them: \"%s\"", result);

  /* Space and Delete, and the edges of the wide keys (Shift 20..74, Space 76..242, Delete 244..298, the gaps go to the nearer key) */
  open_entry(20);
  tap(KX(0), KY(1));
  tap(75, SPECIAL_Y);       /* Space's left edge, with the half gap before it */
  tap(241.5f, SPECIAL_Y);   /* its right edge */
  tap(KX(1), KY(1));
  tap(OK_X, BTN_Y);
  CHECK(!strcmp(result, "q  w"), "Space's two edges type a space each: \"%s\"", result);
  open_entry(20);
  tap(KX(0), KY(1));
  tap(KX(1), KY(1));
  tap(KX(2), KY(1));
  const int errd = n_error;
  tap(271, SPECIAL_Y);
  tap(244, SPECIAL_Y);
  tap(298.5f, SPECIAL_Y);
  CHECK(n_error == errd, "three taps on Delete delete three letters without an error");
  tap(271, SPECIAL_Y);
  CHECK(n_error == errd + 1, "Delete with nothing to delete gives the error sound");
  tap(KX(3), KY(1));
  tap(OK_X, BTN_Y);
  CHECK(!strcmp(result, "r"), "Delete deleted: \"%s\"", result);

  /* the buttons: ok x 20..158, cancel 160..298, y 190..214 as drawn */
  open_entry(20);
  tap(KX(5), KY(2));
  tap(19, BTN_Y);
  CHECK(n_done == 1 && got_text && !strcmp(result, "h"), "ok's left edge: \"%s\"", result);
  open_entry(20);
  tap(KX(5), KY(2));
  tap(158.5f, BTN_Y);
  CHECK(n_done == 1 && got_text, "ok's right edge, with the half gap");
  open_entry(20);
  tap(KX(5), KY(2));
  tap(CANCEL_X, BTN_Y);
  CHECK(n_done == 1 && !got_text, "a tap on cancel hands over NULL");
  open_entry(20);
  tap(159.5f, BTN_Y);
  CHECK(n_done == 1 && !got_text, "cancel's left edge, with the half gap");
  open_entry(20);
  tap(298.5f, 214.5f);
  CHECK(n_done == 1 && !got_text, "cancel's right and lower edges");
  open_entry(20);
  tap(KX(5), KY(2));
  tap(OK_X, 189.5f);
  CHECK(n_done == 1 && got_text, "the buttons' upper edge");

  /* the gaps between keys go to a key, the margins and the gap above the Shift row to nothing */
  open_entry(20);
  CHECK(tap(46.5f, 40) && tap(47.5f, 40), "taps in the gap between two keys are taken");
  CHECK(tap(KX(0), 52.5f) && tap(KX(0), 53.5f), "taps in the gap between two rows are taken");
  tap(OK_X, BTN_Y);
  CHECK(!strcmp(result, "12" "1" "q"), "the gaps go to the nearer key: \"%s\"", result);
  open_entry(20);
  const int sel_gap = n_select, err_gap = n_error;
  CHECK(!tap(KX(0), 160) && !tap(5, 5) && !tap(310, 100) && !tap(18.5f, KY(1)) && !tap(KX(0), 26.5f) && !tap(OK_X, 232), "taps on nothing take nothing");
  CHECK(n_select == sel_gap && n_error == err_gap, "(no sound)");
  tap(OK_X, BTN_Y);
  CHECK(!strcmp(result, ""), "nothing was typed: \"%s\"", result);

  /* a tap moves the cursor: the highlight is on that key, and the D-pad goes on from it */
  open_entry(20);
  tap(KX(5), KY(3));
  draw_bottom();
  int on_key = 0;
  for (int i = 0; i < n_rects; i++)
    if (g_rects[i].c == RR_RGBA(0xf8, 0xe8, 0x58, 255) && g_rects[i].x == KX(5) - 13 - 1 && g_rects[i].y == KY(3) - 12 - 1) on_key++;
  CHECK(on_key == 2, "the outline moved to the key that was tapped (its top and left sides start at its corner: %d)", on_key);
  press(BTN_RIGHT);
  press(BTN_ACTION);
  tap(OK_X, BTN_Y);
  CHECK(!strcmp(result, "n" "m"), "the D-pad goes on from the tapped key: \"%s\"", result);

  /* the limit, and nothing after the answer */
  open_entry(2);
  const int err_lim = n_error;
  tap(KX(0), KY(1));
  tap(KX(1), KY(1));
  tap(KX(2), KY(1));
  CHECK(n_error == err_lim + 1, "a tap past the limit gives the error sound");
  tap(OK_X, BTN_Y);
  CHECK(!strcmp(result, "qw"), "the limit holds: \"%s\"", result);
  const int sel_end = n_select;
  CHECK(tap(KX(0), KY(1)) && tap(OK_X, BTN_Y) && tap(CANCEL_X, BTN_Y), "taps are taken (and ignored) after the answer");
  CHECK(n_done == 1 && n_select == sel_end, "nothing happened after the answer: %d answers", n_done);

  /* ---- drawing */
  home();
  type_text("ab");
  draw_both();
  CHECK(drew("Rename Run") && drew("Run Name") && drew("ab") && drew("2/20"), "the top screen draws the title, the label, the text and the count");
  CHECK(n_wins >= 2, "the form and the text box are windows (%d)", n_wins);
  CHECK(g_wins[0].w == 220 && g_wins[0].x + g_wins[0].w <= 320, "the form is 220 wide and on the field");
  draw_bottom();
  int n_keys = 0;
  for (int r = 0; r < 5; r++)
    for (int c = 0; c < 10; c++)
      if (drew(ROWS5[r][c])) n_keys++;
  CHECK(n_keys == 50, "all 50 keys are drawn (%d)", n_keys);
  CHECK(drew("Shift") && drew("Space") && drew("Delete") && drew("Rename") && drew("Cancel"), "the Shift, Space, Delete, ok and cancel keys are drawn");
  CHECK(n_drawn == 55, "55 labels (%d)", n_drawn);
  int yellow = 0;
  for (int i = 0; i < n_rects; i++)
    if (g_rects[i].c == RR_RGBA(0xf8, 0xe8, 0x58, 255)) yellow++;
  CHECK(yellow == 4, "the cursor's cell has a 4 sided outline (%d)", yellow);
  /* Shift shows on the keys */
  press(BTN_CYCLE_FORM);
  draw_bottom();
  CHECK(drew("Q") && !drew("q") && drew("Shift"), "Shift once draws capitals");
  press(BTN_CYCLE_FORM);
  draw_bottom();
  CHECK(drew("Q") && drew("Caps") && !drew("Shift"), "Shift twice is Caps");
  press(BTN_CYCLE_FORM);
  draw_bottom();
  CHECK(drew("q") && drew("Shift") && !drew("Caps"), "Shift off again");

  /* a text wider than its box shows its end */
  g_char_w = 8; /* 20 characters are 160 px, the box holds 130 */
  home();
  type_text("abcdefghijklmnopqrst");
  draw_both();
  CHECK(drew("20/20"), "the count at the limit");
  int shown_len = 0;
  for (int i = 0; i < n_drawn; i++)
    if (g_drawn[i].text[0] == 'e' || g_drawn[i].text[0] == 'f') shown_len = (int)strlen(g_drawn[i].text);
  CHECK(shown_len > 0 && shown_len <= 16, "the text shown is its end and fits the box (%d characters)", shown_len);
  CHECK(drew("efghijklmnopqrst") || drew("fghijklmnopqrst") || drew("ghijklmnopqrst"), "it is the last characters");
  g_char_w = 6;

  /* a blinking caret: drawn in the first half of the cycle, not in the second */
  home();
  g_screen->update(g_screen, 100);
  draw_both();
  const int caret_on = n_rects;
  g_screen->update(g_screen, 600);
  draw_both();
  CHECK(n_rects == caret_on - 1, "the caret blinks (%d, then %d rectangles)", caret_on, n_rects);

  printf("%d checks, %d failed\n", checks, failed);
  printf(failed ? "FAILED\n" : "OK\n");
  return failed ? 1 : 0;
}
