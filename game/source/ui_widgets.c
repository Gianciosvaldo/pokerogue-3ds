/* Images, windows, the message box and option menus (see ui_widgets.h). */
#include "ui_widgets.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "i18n.h"

/* ------------------------------------------------------------------ images */

#define IMG_MAX 384
typedef struct {
  char key[96];
  rr_atlas* a;
  bool missing;
  short* order; /* animation frame order, NULL = atlas order */
  int n_order;
} img_entry;
static img_entry g_img[IMG_MAX];
static int g_nimg;

static void build_order(img_entry* e) {
  int n = rr_atlas_count(e->a);
  if (n <= 1 || n > 1024) return;
  /* frame names are not exposed by rr; look each numbered name up */
  e->order = malloc(sizeof(short) * n);
  if (!e->order) return;
  int k = 0;
  char nm[16];
  for (int i = 1; i <= 999 && k < n; i++) {
    snprintf(nm, sizeof nm, "%04d.png", i);
    const rr_frame* f = rr_atlas_find(e->a, nm);
    if (!f) {
      if (k) break;
      continue;
    }
    for (int j = 0; j < n; j++) {
      if (rr_atlas_frame(e->a, j) == f) {
        e->order[k++] = (short)j;
        break;
      }
    }
  }
  if (k == 0) {
    free(e->order);
    e->order = NULL;
  } else {
    e->n_order = k;
  }
}

static img_entry* img_entry_get(const char* key) {
  for (int i = 0; i < g_nimg; i++)
    if (!strcmp(g_img[i].key, key)) return &g_img[i];
  if (g_nimg >= IMG_MAX) return NULL;
  img_entry* e = &g_img[g_nimg++];
  memset(e, 0, sizeof *e);
  snprintf(e->key, sizeof e->key, "%s", key);
  e->a = rr_atlas_load_key(key);
  e->missing = !e->a;
  if (e->a) build_order(e);
  return e;
}

rr_atlas* img_get(const char* key) {
  img_entry* e = img_entry_get(key);
  return e ? e->a : NULL;
}

bool img_has(const char* key) { return img_get(key) != NULL; }

static void entry_free(img_entry* e) {
  if (e->a) rr_atlas_free(e->a);
  free(e->order);
}

void img_drop(const char* key) {
  for (int i = 0; i < g_nimg; i++) {
    if (!strcmp(g_img[i].key, key)) {
      entry_free(&g_img[i]);
      g_img[i] = g_img[--g_nimg];
      return;
    }
  }
}

void img_drop_prefix(const char* prefix) {
  size_t n = strlen(prefix);
  for (int i = 0; i < g_nimg;) {
    if (!strncmp(g_img[i].key, prefix, n)) {
      entry_free(&g_img[i]);
      g_img[i] = g_img[--g_nimg];
    } else {
      i++;
    }
  }
}

const rr_frame* img_frame(const char* key, const char* frame) {
  rr_atlas* a = img_get(key);
  if (!a) return NULL;
  if (!frame || !*frame) return rr_atlas_frame(a, 0);
  const rr_frame* f = rr_atlas_find(a, frame);
  return f;
}

int img_frame_count(const char* key) {
  img_entry* e = img_entry_get(key);
  if (!e || !e->a) return 0;
  return e->order ? e->n_order : rr_atlas_count(e->a);
}

const rr_frame* img_anim_frame(const char* key, int i) {
  img_entry* e = img_entry_get(key);
  if (!e || !e->a) return NULL;
  int n = e->order ? e->n_order : rr_atlas_count(e->a);
  if (n <= 0) return NULL;
  i %= n;
  if (i < 0) i += n;
  return rr_atlas_frame(e->a, e->order ? e->order[i] : i);
}

void img_draw_opts(const char* key, const char* frame, float x, float y, const rr_draw_opts* o) {
  const rr_frame* f = img_frame(key, frame);
  if (f) rr_draw(f, x, y, o);
}

void img_draw(const char* key, const char* frame, float x, float y, float ox, float oy) {
  rr_draw_opts o = RR_DEFAULT_OPTS;
  o.origin_x = ox;
  o.origin_y = oy;
  img_draw_opts(key, frame, x, y, &o);
}

int img_w(const char* key, const char* frame) {
  const rr_frame* f = img_frame(key, frame);
  return f ? f->src_w : 0;
}
int img_h(const char* key, const char* frame) {
  const rr_frame* f = img_frame(key, frame);
  return f ? f->src_h : 0;
}

int img_count(void) { return g_nimg; }
size_t img_bytes(void) {
  size_t b = 0;
  for (int i = 0; i < g_nimg; i++) {
    if (!g_img[i].a) continue;
    for (int p = 0; p < rr_atlas_pages(g_img[i].a); p++) b += rr_tex_bytes(rr_atlas_page(g_img[i].a, p));
  }
  return b;
}

/* ------------------------------------------------------------------ windows */

static int g_window_style = GM_DEFAULT_WINDOW_STYLE;
int ui_window_style(void) { return g_window_style; }
void ui_set_window_style(int s) { g_window_style = s < 1 || s > 5 ? GM_DEFAULT_WINDOW_STYLE : s; }

void ui_window(float x, float y, float w, float h, win_variant v) {
  char key[64];
  snprintf(key, sizeof key, "images/ui/windows/window_%d%s", g_window_style,
           v == WIN_THIN ? "_thin" : v == WIN_XTHIN ? "_xthin" : "");
  const rr_frame* f = img_frame(key, NULL);
  if (f)
    rr_nineslice(f, x, y, w, h, 8);
  else
    rr_rect(x, y, w, h, RR_RGBA(0x40, 0x40, 0x58, 255));
}

void ui_cursor(float x, float y) {
  const rr_frame* f = img_frame("images/ui/cursor", NULL);
  if (f) {
    rr_draw_opts o = RR_DEFAULT_OPTS;
    o.origin_x = o.origin_y = 0.5f;
    rr_draw(f, x, y, &o);
  } else {
    rr_rect(x - 3, y - 5, 6, 10, RR_RGBA(0xf8, 0xf8, 0xf8, 255));
  }
}

/* ------------------------------------------------------------------ message box */

void msgbox_init(msgbox* m, text_style style, float wrap_w, int max_lines) {
  memset(m, 0, sizeof *m);
  m->style = style;
  m->opts = text_defaults();
  m->opts.wrap_w = wrap_w;
  m->opts.max_lines = max_lines;
  m->done = true;
}

void msgbox_clear(msgbox* m) {
  m->text[0] = 0;
  m->rest[0] = 0;
  m->shown = m->total = 0;
  m->waiting = false;
  m->done = true;
}

/* Starts one page: strips @c/@d/@s/@f{...} markers (remembering where they were), then
 * inserts upstream's pre-computed line breaks. */
static void start_page(msgbox* m, const char* page) {
  char clean[512];
  int o = 0, chars = 0;
  m->n_marks = 0;
  m->started = false;
  m->variant_new = false;
  for (const char* p = page; *p && o < (int)sizeof clean - 1;) {
    if (p[0] == '@' && strchr("cdsf", p[1]) && p[2] == '{') {
      const char* end = strchr(p + 3, '}');
      if (end) {
        if (m->n_marks < 8 && (p[1] == 'd' || p[1] == 's' || p[1] == 'c')) {
          int k = m->n_marks++;
          m->mark_at[k] = (short)chars;
          m->mark_kind[k] = p[1];
          m->mark_val[k] = (short)atoi(p + 3);
          int len = (int)(end - (p + 3));
          if (len > 23) len = 23;
          memcpy(m->mark_sound[k], p + 3, len);
          m->mark_sound[k][len] = 0;
        }
        p = end + 1;
        continue;
      }
    }
    if (((unsigned char)*p & 0xC0) != 0x80) chars++;
    clean[o++] = *p++;
  }
  clean[o] = 0;
  text_wrap(m->style, clean, &m->opts, m->text, sizeof m->text);
  m->total = text_utf8_len(m->text);
  m->shown = m->delay > 0 ? 0 : m->total;
  m->acc = 0;
  m->pause = 0;
  m->t = 0;
  m->waiting = false;
  m->done = false;
}

static void next_page(msgbox* m) {
  char* d = strchr(m->rest, '$');
  char page[1024];
  if (d) {
    int len = (int)(d - m->rest);
    if (len >= (int)sizeof page) len = sizeof page - 1;
    memcpy(page, m->rest, len);
    page[len] = 0;
    memmove(m->rest, d + 1, strlen(d + 1) + 1);
  } else {
    snprintf(page, sizeof page, "%s", m->rest);
    m->rest[0] = 0;
  }
  /* trim like upstream's .map(m => m.trim()) */
  char* s = page;
  while (*s == ' ' || *s == '\n') s++;
  size_t n = strlen(s);
  while (n && (s[n - 1] == ' ' || s[n - 1] == '\n')) s[--n] = 0;
  start_page(m, s);
}

void msgbox_show(msgbox* m, const char* text, float delay, bool prompt) {
  m->delay = delay < 0 ? 20 : delay;
  m->prompt = prompt;
  if (prompt) {
    snprintf(m->rest, sizeof m->rest, "%s", text);
    next_page(m);
  } else {
    m->rest[0] = 0;
    start_page(m, text);
  }
}

static void hit_marks(msgbox* m, int at) {
  for (int i = 0; i < m->n_marks; i++) {
    if (m->mark_at[i] != at) continue;
    if (m->mark_kind[i] == 'd') m->pause += m->mark_val[i] * (1000.0f / 60.0f); /* getFrameMs */
    if (m->mark_kind[i] == 's') ui_play_sound(m->mark_sound[i]);
    if (m->mark_kind[i] == 'c') {
      memcpy(m->variant, m->mark_sound[i], sizeof m->variant);
      m->variant_new = true;
    }
    m->mark_kind[i] = 0;
  }
}

void msgbox_update(msgbox* m, float dt) {
  if (m->done) return;
  if (m->shown < m->total) {
    if (m->pause > 0) {
      m->pause -= dt;
      return;
    }
    m->acc += dt;
    while (m->acc >= m->delay && m->shown < m->total && m->pause <= 0) {
      m->acc -= m->delay;
      if (!m->started) {
        /* upstream's timer runs text.length + 1 times, the first with no character on screen yet: a marker at the very start (a page that
         * opens with @c{...}) fires on that tick */
        m->started = true;
        hit_marks(m, 0);
        continue;
      }
      m->shown++;
      hit_marks(m, m->shown);
    }
    return;
  }
  m->t += dt;
  if (m->prompt) {
    m->waiting = true;
  } else if (m->t >= m->wait_ms) {
    m->done = true;
  }
}

bool msgbox_input(msgbox* m, ui_button b) {
  if (b != BTN_ACTION && b != BTN_CANCEL) return false;
  if (m->done || !m->waiting) return false; /* upstream doesn't skip typing */
  ui_play_select();
  m->waiting = false;
  if (m->rest[0])
    next_page(m);
  else
    m->done = true;
  return true;
}

bool msgbox_done(const msgbox* m) { return m->done; }

const char* msgbox_take_variant(msgbox* m) {
  if (!m->variant_new) return NULL;
  m->variant_new = false;
  return m->variant;
}
bool msgbox_typing(const msgbox* m) { return !m->done && m->shown < m->total; }

void msgbox_draw(const msgbox* m, float x, float y) {
  if (!m->text[0]) return;
  text_opts o = m->opts;
  o.wrap_w = 0; /* already wrapped */
  o.chars = m->shown;
  text_draw(m->style, x, y, m->text, &o);
  if (m->waiting) {
    /* showPrompt: after the last line; frames 1-4 at 6 fps */
    const char* last = strrchr(m->text, '\n');
    last = last ? last + 1 : m->text;
    int lines = 1;
    for (const char* p = m->text; *p; p++) lines += *p == '\n';
    float lw = text_line_width(m->style, 96, last, -1);
    char fr[4];
    snprintf(fr, sizeof fr, "%d", 1 + (int)(m->t * 6 / 1000) % 4);
    img_draw("images/ui/prompt", fr, x + lw + 2, y + (lines - 1) * 18 + 2, 0, 0);
  }
}

/* ------------------------------------------------------------------ option list */

#define OPT_ROW 16.0f /* (maxOptions + 1) * 96 * scale - 2: 16 px a row */

void optlist_init(optlist* l, const char* const* labels, int n) {
  memset(l, 0, sizeof *l);
  if (n > OPTLIST_MAX) n = OPTLIST_MAX;
  l->n = n;
  for (int i = 0; i < n; i++) l->labels[i] = labels[i];
  l->max_rows = 10; /* DEFAULT_MAX_OPTIONS */
  l->x = 319;
  l->y = 0;
  l->chosen = -1;
  float w = 0;
  for (int i = 0; i < n; i++) {
    float lw = text_width(TS_WINDOW, labels[i], NULL);
    if (lw > w) w = lw;
  }
  l->w = w + 23; /* WINDOW_PADDING */
  if (n > l->max_rows) l->w += 5;
}

static int rows(const optlist* l) { return l->n < l->max_rows ? l->n : l->max_rows; }

bool optlist_input(optlist* l, ui_button b) {
  int vis = rows(l);
  if (b == BTN_ACTION || b == BTN_CANCEL) {
    if (b == BTN_CANCEL) {
      if (l->no_cancel) return false;
      l->chosen = -2;
      ui_play_select();
      return true;
    }
    int idx = l->scroll + l->cursor;
    if (l->disabled[idx]) {
      ui_play_error();
      return false;
    }
    l->chosen = idx;
    ui_play_select();
    return true;
  }
  if (b == BTN_UP) {
    if (l->cursor > 0)
      l->cursor--;
    else if (l->scroll > 0)
      l->scroll--;
    else {
      l->scroll = l->n > vis ? l->n - vis : 0;
      l->cursor = vis - 1;
    }
    ui_play_select();
    return true;
  }
  if (b == BTN_DOWN) {
    if (l->cursor < vis - 1)
      l->cursor++;
    else if (l->scroll < l->n - vis)
      l->scroll++;
    else {
      l->scroll = 0;
      l->cursor = 0;
    }
    ui_play_select();
    return true;
  }
  return false;
}

bool optlist_touch(optlist* l, float x, float y) {
  const int vis = rows(l);
  const float h = (vis + 1) * OPT_ROW - 2, left = l->x - l->w, top = l->y - h;
  if (x < left || x >= l->x || y < top + 7 || y >= top + 7 + vis * OPT_ROW) return false;
  l->cursor = (int)((y - top - 7) / OPT_ROW);
  return optlist_input(l, BTN_ACTION);
}

void optlist_draw(const optlist* l) {
  int vis = rows(l);
  float h = (vis + 1) * OPT_ROW - 2;
  float left = l->x - l->w, top = l->y - h;
  ui_window(left, top, l->w, h, WIN_NORMAL);
  text_opts o = text_defaults();
  o.line_spacing = 12; /* scale * 72 */
  for (int i = 0; i < vis; i++) {
    int idx = l->scroll + i;
    text_style st = l->colors[idx] ? l->colors[idx] : TS_WINDOW;
    if (l->disabled[idx]) st = TS_SHADOW_TEXT;
    text_draw(st, left + 15, top + 7 + i * OPT_ROW, l->labels[idx], &o);
  }
  ui_cursor(left + 10, top + 15 + l->cursor * OPT_ROW);
  if (l->n > vis) {
    /* ScrollBar: 3 px wide, inside the right edge */
    float bh = h - 10, bx = l->x - 10, by = top + 5;
    rr_rect(bx, by, 3, bh, RR_RGBA(0x30, 0x30, 0x30, 160));
    float th = bh * vis / l->n, ty = by + (bh - th) * l->scroll / (float)(l->n - vis);
    rr_rect(bx, ty, 3, th, RR_RGBA(0xf8, 0xf8, 0xf8, 255));
  }
}

/* ------------------------------------------------------------------ option / confirm screens */

typedef struct {
  optlist list;
  void (*on_done)(int, void*);
  void* arg;
  char yes[32], no[32];
} opt_screen_data;

static opt_screen_data g_opt_data;

static bool opt_input(ui_screen* s, ui_button b) {
  opt_screen_data* d = s->data;
  if (!optlist_input(&d->list, b)) return false;
  if (d->list.chosen != -1) {
    int idx = d->list.chosen;
    void (*fn)(int, void*) = d->on_done;
    void* arg = d->arg;
    ui_pop();
    if (fn) fn(idx, arg);
  }
  return true;
}

static void opt_draw(ui_screen* s, rr_screen which) {
  opt_screen_data* d = s->data;
  if (!ui_space(UI_BOTTOM_UI)) return;
  (void)which;
  optlist_draw(&d->list);
}

static bool opt_touch(ui_screen* s, float x, float y) {
  opt_screen_data* d = s->data;
  if (!optlist_touch(&d->list, x, ui_bottom_ui_y(y))) return false;
  if (d->list.chosen != -1) {
    int idx = d->list.chosen;
    void (*fn)(int, void*) = d->on_done;
    void* arg = d->arg;
    ui_pop();
    if (fn) fn(idx, arg);
  }
  return true;
}

static ui_screen g_opt_screen = {"option-select", UI_OVERLAY, NULL, NULL, NULL, opt_input, NULL, opt_draw, &g_opt_data, opt_touch};

void ui_push_options(const char* const* labels, int n, bool can_cancel, void (*on_done)(int, void*), void* arg) {
  optlist_init(&g_opt_data.list, labels, n);
  g_opt_data.list.no_cancel = !can_cancel;
  g_opt_data.on_done = on_done;
  g_opt_data.arg = arg;
  ui_push(&g_opt_screen);
}

typedef struct {
  void (*fn)(int, void*);
  void* arg;
} confirm_cb;
static confirm_cb g_confirm;

static void confirm_route(int idx, void* arg) {
  (void)arg;
  if (g_confirm.fn) g_confirm.fn(idx == 0 ? 1 : 0, g_confirm.arg);
}

void ui_push_confirm(void (*on_done)(int yes, void* arg), void* arg) {
  i18n_t(g_opt_data.yes, sizeof g_opt_data.yes, "menu:yes", NULL);
  i18n_t(g_opt_data.no, sizeof g_opt_data.no, "menu:no", NULL);
  static const char* labels[2];
  labels[0] = g_opt_data.yes;
  labels[1] = g_opt_data.no;
  g_confirm.fn = on_done;
  g_confirm.arg = arg;
  optlist_init(&g_opt_data.list, labels, 2);
  g_opt_data.list.y = -48; /* ConfirmUiHandler yOffset 48 */
  g_opt_data.on_done = confirm_route;
  g_opt_data.arg = NULL;
  ui_push(&g_opt_screen);
}
