/* A Mystery Encounter's question and the pick lists (see me_screen.h). */
#include "me_screen.h"

#include <stdio.h>
#include <string.h>

#include "ui.h"
#include "ui_text.h"
#include "ui_widgets.h"
#include "mui.h"

#define ROW_H 24
#define LIST_Y 6
#define TIP_Y 150
#define HINT_X 10
#define HINT_Y 226
#define TIP_W 296

static const char* const HINTS[2] = {"A: choose", "B: back"};

static struct {
  me_args a;
  int scroll;
  bool left;
} M;

/* ------------------------------------------------------------------ BBCode */

static int kind_of(const char* name, size_t len)
{
  static const struct { const char* n; int k; } T[] = {{"TOOLTIP_TITLE", ME_K_TITLE}, {"MONEY", ME_K_MONEY},
                                                        {"SUMMARY_GREEN", ME_K_GREEN}, {"SUMMARY_BLUE", ME_K_BLUE}};
  for (size_t i = 0; i < sizeof T / sizeof *T; i++)
    if (strlen(T[i].n) == len && !strncmp(T[i].n, name, len)) return T[i].k;
  return ME_K_NORMAL;
}

size_t me_markup(const char* src, char* plain, uint8_t* kinds, size_t n)
{
  size_t o = 0;
  if (n == 0) return 0;
  for (const char* p = src; *p && o + 1 < n;) {
    if (p[0] == '@' && p[1] == '[') {
      const char* close = strchr(p + 2, ']');
      if (close && close[1] == '{') {
        const char* end = strchr(close + 2, '}');
        if (end) {
          const int k = kind_of(p + 2, (size_t)(close - (p + 2)));
          for (const char* q = close + 2; q < end && o + 1 < n; q++) {
            plain[o] = *q;
            kinds[o++] = (uint8_t)k;
          }
          p = end + 1;
          continue;
        }
      }
    }
    plain[o] = *p++;
    kinds[o++] = ME_K_NORMAL;
  }
  plain[o] = 0;
  /* upstream: /(\(\+\)[^([]*)/ green and /(\(-\)[^([]*)/ blue, over text no wrapper coloured */
  for (size_t i = 0; i + 2 < o; i++) {
    if (plain[i] != '(' || plain[i + 2] != ')' || (plain[i + 1] != '+' && plain[i + 1] != '-') || kinds[i] != ME_K_NORMAL) continue;
    const uint8_t k = plain[i + 1] == '+' ? ME_K_GREEN : ME_K_BLUE;
    size_t j = i;
    kinds[j++] = k;
    kinds[j++] = k;
    kinds[j++] = k;
    for (; j < o && plain[j] != '(' && plain[j] != '['; j++)
      if (kinds[j] == ME_K_NORMAL) kinds[j] = k;
    i = j - 1;
  }
  return o;
}

static rr_color kind_color(int k, bool shadow)
{
  switch (k) {
    case ME_K_TITLE: return text_color(TS_TOOLTIP_TITLE, shadow);
    case ME_K_MONEY: return text_color(TS_MONEY, shadow);
    case ME_K_GREEN: return text_color(TS_SUMMARY_GREEN, shadow);
    case ME_K_BLUE: return text_color(TS_SUMMARY_BLUE, shadow);
    default: return text_color(TS_TOOLTIP_CONTENT, shadow);
  }
}

/* The wrapped, coloured text at (x, y). `wrapped` has '\n' where text_wrap broke (in place of a space, or added). */
static void draw_marked(text_style st, float x, float y, const char* src, float wrap_w)
{
  char plain[ME_TEXT_LEN], wrapped[ME_TEXT_LEN + 32];
  uint8_t kinds[ME_TEXT_LEN];
  me_markup(src, plain, kinds, sizeof plain);
  text_opts o = text_defaults();
  o.wrap_w = wrap_w;
  text_wrap(st, plain, &o, wrapped, sizeof wrapped);
  const float pitch = text_line_pitch(st, &o);
  size_t pi = 0;
  float ly = y;
  const char* line = wrapped;
  while (*line) {
    const char* nl = strchr(line, '\n');
    const size_t len = nl ? (size_t)(nl - line) : strlen(line);
    float lx = x;
    for (size_t s = 0; s < len;) {
      size_t e = s + 1;
      const int k = pi + s < sizeof kinds ? kinds[pi + s] : ME_K_NORMAL;
      while (e < len && (pi + e < sizeof kinds ? kinds[pi + e] : ME_K_NORMAL) == k) e++;
      char seg[ME_TEXT_LEN];
      memcpy(seg, line + s, e - s);
      seg[e - s] = 0;
      text_opts so = text_defaults();
      so.color = kind_color(k, false);
      so.shadow = kind_color(k, true);
      text_draw(st, lx, ly, seg, &so);
      lx += text_line_width(st, 0, seg, (int)(e - s));
      s = e;
    }
    pi += len;
    if (!nl) break;
    /* the break replaced a space, or was added by the wrap, or the text had its own newline */
    if (pi < strlen(plain) && (plain[pi] == ' ' || plain[pi] == '\n')) pi++;
    line = nl + 1;
    ly += pitch;
  }
}

/* ------------------------------------------------------------------ the screen */

static void move_to(int i)
{
  if (i < 0 || i >= M.a.n || i == M.a.cursor) return;
  M.a.cursor = i;
  const int rows = (TIP_Y - LIST_Y) / ROW_H;
  if (M.a.cursor < M.scroll) M.scroll = M.a.cursor;
  if (M.a.cursor >= M.scroll + rows) M.scroll = M.a.cursor - rows + 1;
  ui_play_select();
}

static void finish_pick(void)
{
  const int i = M.a.cursor;
  void (*cb)(int, void*) = M.a.on_pick;
  void* arg = M.a.arg;
  M.left = true;
  ui_pop();
  if (cb) cb(i, arg);
}

static void finish_back(void)
{
  void (*cb)(void*) = M.a.on_back;
  void* arg = M.a.arg;
  M.left = true;
  ui_pop();
  if (cb) cb(arg);
}

static bool ms_input(ui_screen* s, ui_button b)
{
  (void)s;
  if (M.left) return true;
  switch (b) {
    case BTN_UP: move_to(M.a.cursor - 1); return true;
    case BTN_DOWN: move_to(M.a.cursor + 1); return true;
    case BTN_ACTION:
    case BTN_SUBMIT:
      if (!M.a.options[M.a.cursor].ok) {
        ui_play_error();
        return true;
      }
      ui_play_select();
      finish_pick();
      return true;
    case BTN_CANCEL:
      if (!M.a.can_back) return true;
      ui_play_select();
      finish_back();
      return true;
    default: return false;
  }
}

static bool ms_touch(ui_screen* s, float x, float y)
{
  if (M.left) return true;
  if (M.a.can_back && y >= HINT_Y - 8) return hint_at(TS_INSTRUCTIONS_TEXT, HINT_X, HINTS, 2, x) == 1 ? ms_input(s, BTN_CANCEL) : false;
  const int rows = (TIP_Y - LIST_Y) / ROW_H;
  for (int i = 0; i < rows && M.scroll + i < M.a.n; i++) {
    const float top = LIST_Y + i * ROW_H;
    if (x < 4 || x >= 316 || y < top || y >= top + ROW_H) continue;
    M.a.cursor = M.scroll + i;
    return ms_input(s, BTN_ACTION);
  }
  return false;
}

static void draw_top(void)
{
  if (ui_space(UI_PHYS_TOP)) rr_rect(0, 0, 400, 240, RR_RGBA(0x10, 0x10, 0x18, 255));
  if (!ui_space(UI_FIELD)) return;
  ui_window(8, 8, 304, 164, WIN_NORMAL);
  text_opts t = text_defaults();
  t.color = text_color(TS_TOOLTIP_TITLE, false);
  t.shadow = text_color(TS_TOOLTIP_TITLE, true);
  text_draw(TS_WINDOW, 20, 16, M.a.title, &t);
  text_opts o = text_defaults();
  o.wrap_w = 280;
  char wrapped[ME_TEXT_LEN + 32];
  text_wrap(TS_INSTRUCTIONS_TEXT, M.a.description, &o, wrapped, sizeof wrapped);
  text_draw(TS_INSTRUCTIONS_TEXT, 20, 40, wrapped, &o);
  text_wrap(TS_WINDOW, M.a.query, &o, wrapped, sizeof wrapped);
  int n_sprites = 0;
  while (n_sprites < ME_MAX_SPRITES && M.a.sprites[n_sprites][0]) n_sprites++;
  text_draw(TS_WINDOW, 20, n_sprites || M.a.n_intro ? 70 : 130, wrapped, &o);
  for (int i = 0; i < n_sprites; i++) { /* black silhouettes standing on the window's floor, evenly spread */
    const rr_frame* f = img_anim_frame(M.a.sprites[i], 0);
    if (!f) continue;
    rr_draw_opts d = RR_DEFAULT_OPTS;
    d.origin_x = 0.5f;
    d.origin_y = 1;
    d.tint = RR_RGBA(0, 0, 0, 255);
    rr_draw(f, 8 + 304 * (2 * i + 1) / (2 * n_sprites), 166, &d);
  }
  for (int i = 0; i < M.a.n_intro; i++) { /* the encounter's own sprites, in colour (shaded toward black where upstream tints them) */
    const me_intro_sprite* s = &M.a.intro[i];
    const rr_frame* f = s->frame[0] ? mui_frame(s->key, s->frame) : img_anim_frame(s->key, 0);
    if (!f) continue;
    rr_draw_opts d = RR_DEFAULT_OPTS;
    d.origin_x = 0.5f;
    d.origin_y = 1;
    d.scale_x = d.scale_y = s->scale;
    const unsigned v = (unsigned)(255.0f * (1.0f - s->dark));
    d.tint = RR_RGBA(v, v, v, 255);
    rr_draw(f, s->x, s->y, &d);
  }
}

static void draw_row(int i, float y)
{
  const me_option* op = &M.a.options[i];
  if (i == M.a.cursor) {
    rr_rect(4, y, 312, ROW_H - 2, RR_RGBA(255, 255, 255, 30));
    ui_cursor(11, y + 11);
  }
  text_opts o = text_defaults();
  if (!op->ok) o.color = RR_RGBA(0x80, 0x80, 0x80, 255);
  text_draw(TS_ME_OPTION_DEFAULT, 22, y + 3, op->label, &o);
}

static void draw_bottom(void)
{
  ui_window(1, 1, 318, TIP_Y - 2, WIN_NORMAL);
  const int rows = (TIP_Y - LIST_Y) / ROW_H;
  for (int i = 0; i < rows && M.scroll + i < M.a.n; i++) draw_row(M.scroll + i, (float)(LIST_Y + i * ROW_H));
  ui_window(1, TIP_Y, 318, 76, WIN_NORMAL);
  const me_option* op = &M.a.options[M.a.cursor];
  if (op->tooltip[0]) draw_marked(TS_TOOLTIP_CONTENT, 10, TIP_Y + 6, op->tooltip, TIP_W);
  if (M.a.can_back)
    for (int i = 0; i < 2; i++) text_draw(TS_INSTRUCTIONS_TEXT, hint_x(TS_INSTRUCTIONS_TEXT, HINT_X, HINTS, i), HINT_Y, HINTS[i], NULL);
}

static void ms_draw(ui_screen* s, rr_screen which)
{
  (void)s;
  if (which == RR_TOP) {
    draw_top();
    return;
  }
  if (!ui_space(UI_PHYS_BOTTOM)) return;
  rr_rect(0, 0, 320, 240, RR_RGBA(0x10, 0x10, 0x18, 255));
  draw_bottom();
}

static ui_screen g_screen = {"mystery-encounter", 0, NULL, NULL, NULL, ms_input, NULL, ms_draw, NULL, ms_touch};

bool me_screen_open(const me_args* a)
{
  if (!a || a->n < 1 || a->n > ME_MAX_OPTIONS) return false;
  memset(&M, 0, sizeof M);
  M.a = *a;
  if (M.a.cursor < 0 || M.a.cursor >= M.a.n) M.a.cursor = 0;
  /* start on the first row that can be taken (at or after the asked one), as upstream's cursor does not rest on a disabled option */
  for (int i = M.a.cursor; i < M.a.n; i++)
    if (M.a.options[i].ok) {
      M.a.cursor = i;
      break;
    }
  ui_push(&g_screen);
  return true;
}
