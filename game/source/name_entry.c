/* The text entry screen (see name_entry.h). */
#include "name_entry.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "i18n.h"
#include "ui.h"
#include "ui_text.h"
#include "ui_widgets.h"

#define COLS 10
#define KEY_ROWS 5
#define ROW_SPECIAL 5 /* Shift, Space, Delete */
#define ROW_BUTTONS 6 /* the ok and cancel buttons */
#define ROWS 7

/* geometry on the bottom screen (320 x 240) */
#define GX 20.0f     /* the left of the first column */
#define GY 28.0f     /* the top of the first row */
#define KEY_W 26.0f
#define KEY_H 24.0f
#define COL_PITCH 28.0f
#define ROW_PITCH 26.0f
#define SPECIAL_GAP 6.0f /* the extra space above the Shift row */

/* geometry of the form on the top screen (UI_FIELD: 320 x 180) */
#define FORM_X 50.0f
#define FORM_Y 40.0f
#define FORM_W 220.0f
#define FORM_H 80.0f
#define BOX_X 70.0f /* the text box's offset in the form */
#define BOX_Y 32.0f
#define BOX_W 140.0f
#define BOX_H 16.0f
#define BOX_PAD 5.0f

static const char* const KEYS[KEY_ROWS][COLS] = {
    {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"},
    {"q", "w", "e", "r", "t", "y", "u", "i", "o", "p"},
    {"a", "s", "d", "f", "g", "h", "j", "k", "l", "'"},
    {"z", "x", "c", "v", "b", "n", "m", ",", ".", "-"},
    {"\xc3\xa0", "\xc3\xa8", "\xc3\xa9", "\xc3\xac", "\xc3\xb2", "\xc3\xb9", "!", "?", "(", ")"},
};

typedef enum { K_CHAR, K_SHIFT, K_SPACE, K_DELETE, K_OK, K_CANCEL } key_kind;

typedef struct {
  uint8_t row, c0, c1; /* the columns it covers, c0..c1 */
  uint8_t kind;
  const char* key; /* K_CHAR: the character, lower case */
} cell;

#define N_CELLS (KEY_ROWS * COLS + 5)

static struct {
  name_entry_args a;
  bool done;
  char text[NE_MAX_BYTES];
  int n_chars;
  int shift; /* 0 off, 1 the next letter, 2 every letter */
  cell cells[N_CELLS];
  int cur;   /* the cell under the cursor */
  int want;  /* the column a vertical move aims for, kept while the cursor goes up and down */
  float t;   /* ms, for the caret */
  char title[64], label[64], ok[48], cancel[48];
  char shift_label[2][16];
} E;

/* ------------------------------------------------------------------ text */

/* the upper case of a key: ASCII letters, and the accented Latin-1 ones (U+00E0..U+00FE less the division sign) */
static void capital(const char* key, char out[5]) {
  snprintf(out, 5, "%s", key);
  const unsigned char* k = (const unsigned char*)key;
  if (k[0] >= 'a' && k[0] <= 'z')
    out[0] = (char)(k[0] - 32);
  else if (k[0] == 0xc3 && k[1] >= 0xa0 && k[1] <= 0xbe && k[1] != 0xb7)
    out[1] = (char)(k[1] - 0x20);
}

static bool is_letter(const char* key) {
  char up[5];
  capital(key, up);
  return strcmp(up, key) != 0;
}

/* the character a key types now */
static void typed(const char* key, char out[5]) {
  if (E.shift && is_letter(key))
    capital(key, out);
  else
    snprintf(out, 5, "%s", key);
}

static void put(const char* s) {
  const size_t n = strlen(s), have = strlen(E.text);
  if (E.n_chars >= E.a.max_chars || have + n >= sizeof E.text) {
    ui_play_error();
    return;
  }
  memcpy(E.text + have, s, n + 1);
  E.n_chars++;
  ui_play_select();
}

/* removes the last character; false when there is none */
static bool erase(void) {
  size_t n = strlen(E.text);
  if (!n) return false;
  do n--;
  while (n > 0 && ((unsigned char)E.text[n] & 0xc0) == 0x80);
  E.text[n] = '\0';
  E.n_chars--;
  ui_play_select();
  return true;
}

static void finish(bool ok) {
  E.done = true;
  if (!ok) {
    if (E.a.on_done) E.a.on_done(NULL, E.a.arg);
    return;
  }
  /* upstream's sanitizeInputs: trim */
  char* s = E.text;
  while (*s == ' ') s++;
  size_t n = strlen(s);
  while (n && s[n - 1] == ' ') n--;
  s[n] = '\0';
  if (E.a.on_done) E.a.on_done(s, E.a.arg);
}

/* ------------------------------------------------------------------ cells */

static void build_cells(void) {
  int n = 0;
  for (int r = 0; r < KEY_ROWS; r++)
    for (int c = 0; c < COLS; c++) E.cells[n++] = (cell){(uint8_t)r, (uint8_t)c, (uint8_t)c, K_CHAR, KEYS[r][c]};
  E.cells[n++] = (cell){ROW_SPECIAL, 0, 1, K_SHIFT, NULL};
  E.cells[n++] = (cell){ROW_SPECIAL, 2, 7, K_SPACE, NULL};
  E.cells[n++] = (cell){ROW_SPECIAL, 8, 9, K_DELETE, NULL};
  E.cells[n++] = (cell){ROW_BUTTONS, 0, 4, K_OK, NULL};
  E.cells[n++] = (cell){ROW_BUTTONS, 5, 9, K_CANCEL, NULL};
}

static void cell_rect(const cell* c, float* x, float* y, float* w, float* h) {
  *x = GX + (float)c->c0 * COL_PITCH;
  *w = (float)(c->c1 - c->c0) * COL_PITCH + KEY_W;
  *y = GY + (float)c->row * ROW_PITCH + (c->row >= ROW_SPECIAL ? SPECIAL_GAP : 0);
  *h = KEY_H;
}

static int cell_at(int row, int col) {
  for (int i = 0; i < N_CELLS; i++)
    if (E.cells[i].row == row && E.cells[i].c0 <= col && col <= E.cells[i].c1) return i;
  return 0;
}

static void set_cursor(int i) {
  E.cur = i;
  E.want = (E.cells[i].c0 + E.cells[i].c1) / 2;
}

static void move_side(int dir) {
  const int row = E.cells[E.cur].row;
  int i = E.cur;
  do {
    i = (i + dir + N_CELLS) % N_CELLS;
  } while (E.cells[i].row != row);
  set_cursor(i);
}

static void move_vertical(int dir) {
  const int row = (E.cells[E.cur].row + dir + ROWS) % ROWS;
  E.cur = cell_at(row, E.want);
}

static void cycle_shift(void) {
  E.shift = (E.shift + 1) % 3;
  ui_play_select();
}

static void press(const cell* c) {
  switch (c->kind) {
    case K_CHAR: {
      char s[5];
      typed(c->key, s);
      const int n = E.n_chars;
      put(s);
      if (E.shift == 1 && n != E.n_chars && is_letter(c->key)) E.shift = 0;
      break;
    }
    case K_SHIFT:
      cycle_shift();
      break;
    case K_SPACE:
      put(" ");
      break;
    case K_DELETE:
      if (!erase()) ui_play_error();
      break;
    case K_OK:
      ui_play_select();
      finish(true);
      break;
    case K_CANCEL:
      ui_play_select();
      finish(false);
      break;
  }
}

/* ------------------------------------------------------------------ the screen */

static bool ne_input(ui_screen* s, ui_button b) {
  (void)s;
  if (E.done) return true;
  switch (b) {
    case BTN_LEFT:
      move_side(-1);
      ui_play_select();
      return true;
    case BTN_RIGHT:
      move_side(1);
      ui_play_select();
      return true;
    case BTN_UP:
      move_vertical(-1);
      ui_play_select();
      return true;
    case BTN_DOWN:
      move_vertical(1);
      ui_play_select();
      return true;
    case BTN_ACTION:
      press(&E.cells[E.cur]);
      return true;
    case BTN_CANCEL:
      if (!erase()) {
        ui_play_select();
        finish(false);
      }
      return true;
    case BTN_MENU:
      ui_play_select();
      finish(true);
      return true;
    case BTN_CYCLE_FORM:
    case BTN_CYCLE_SHINY:
      cycle_shift();
      return true;
    case BTN_CYCLE_TERA:
      put(" ");
      return true;
    case BTN_CYCLE_NATURE:
      if (!erase()) ui_play_error();
      return true;
    default:
      return false;
  }
}

static void ne_update(ui_screen* s, float dt) {
  (void)s;
  E.t += dt;
  if (E.t > 1060) E.t -= 1060;
}

#define YELLOW RR_RGBA(0xf8, 0xe8, 0x58, 255)

static void draw_top(void) {
  if (ui_space(UI_PHYS_TOP)) rr_rect(0, 0, 400, 240, RR_RGBA(0x18, 0x20, 0x30, 255));
  if (!ui_space(UI_FIELD)) return;
  ui_window(FORM_X, FORM_Y, FORM_W, FORM_H, WIN_NORMAL);
  text_opts mid = text_defaults();
  mid.origin_x = 0.5f;
  text_draw(TS_WINDOW, FORM_X + FORM_W / 2, FORM_Y + 6, E.title, &mid);
  text_draw(TS_WINDOW, FORM_X + 10, FORM_Y + BOX_Y + 3, E.label, NULL);
  const float bx = FORM_X + FORM_W - 10 - BOX_W, by = FORM_Y + BOX_Y;
  ui_window(bx, by, BOX_W, BOX_H, WIN_XTHIN);
  /* the end of the text, when all of it does not fit */
  const char* shown = E.text;
  while (*shown && text_width(TS_WINDOW, shown, NULL) > BOX_W - 2 * BOX_PAD) {
    shown++;
    while (((unsigned char)*shown & 0xc0) == 0x80) shown++;
  }
  const float w = text_draw(TS_WINDOW, bx + BOX_PAD, by + 3, shown, NULL);
  if (E.t < 530 && !E.done) rr_rect(bx + BOX_PAD + w + 1, by + 3, 1, 10, RR_RGBA(0xf8, 0xf8, 0xf8, 255));
  char count[16];
  snprintf(count, sizeof count, "%d/%d", E.n_chars, E.a.max_chars);
  text_opts right = text_defaults();
  right.origin_x = 1;
  text_draw(TS_WINDOW, FORM_X + FORM_W - 10, FORM_Y + BOX_Y + BOX_H + 8, count, &right);
}

static void draw_cell(int i) {
  const cell* c = &E.cells[i];
  float x, y, w, h;
  cell_rect(c, &x, &y, &w, &h);
  const bool selected = i == E.cur;
  const bool lit = c->kind == K_SHIFT && E.shift;
  rr_rect(x, y, w, h, selected ? RR_RGBA(0x68, 0x70, 0x90, 255) : lit ? RR_RGBA(0x90, 0x80, 0x30, 255) : RR_RGBA(0x38, 0x3c, 0x48, 255));
  if (selected) {
    rr_rect(x - 1, y - 1, w + 2, 2, YELLOW);
    rr_rect(x - 1, y + h - 1, w + 2, 2, YELLOW);
    rr_rect(x - 1, y - 1, 2, h + 2, YELLOW);
    rr_rect(x + w - 1, y - 1, 2, h + 2, YELLOW);
  }
  char buf[8];
  const char* label = buf;
  switch (c->kind) {
    case K_CHAR:
      typed(c->key, buf);
      break;
    case K_SHIFT:
      label = E.shift_label[E.shift == 2];
      break;
    case K_SPACE:
      label = "Space";
      break;
    case K_DELETE:
      label = "Delete";
      break;
    case K_OK:
      label = E.ok;
      break;
    default:
      label = E.cancel;
      break;
  }
  text_opts mid = text_defaults();
  mid.origin_x = mid.origin_y = 0.5f;
  text_draw(TS_WINDOW, x + w / 2, y + h / 2, label, &mid);
}

static void ne_draw(ui_screen* s, rr_screen which) {
  (void)s, (void)which;
  draw_top();
  if (!ui_space(UI_PHYS_BOTTOM)) return;
  rr_rect(0, 0, 320, 240, RR_RGBA(0x18, 0x20, 0x30, 255));
  ui_window(8, 14, 304, 214, WIN_NORMAL);
  for (int i = 0; i < N_CELLS; i++) draw_cell(i);
}

static ui_screen g_screen = {"name-entry", UI_FADE, NULL, NULL, NULL, ne_input, ne_update, ne_draw, NULL};

void name_entry_open(const name_entry_args* a) {
  memset(&E, 0, sizeof E);
  E.a = *a;
  if (E.a.max_chars < 1) E.a.max_chars = 1;
  if (E.a.max_chars > NE_MAX_BYTES / 4 - 1) E.a.max_chars = NE_MAX_BYTES / 4 - 1;
  build_cells();
  set_cursor(cell_at(1, 0)); /* the first letter */
  i18n_t(E.title, sizeof E.title, a->title_key, NULL);
  i18n_t(E.label, sizeof E.label, a->label_key, NULL);
  i18n_t(E.ok, sizeof E.ok, a->ok_key, NULL);
  i18n_t(E.cancel, sizeof E.cancel, a->cancel_key, NULL);
  snprintf(E.shift_label[0], sizeof E.shift_label[0], "Shift");
  snprintf(E.shift_label[1], sizeof E.shift_label[1], "Caps");
  ui_set(&g_screen);
}
