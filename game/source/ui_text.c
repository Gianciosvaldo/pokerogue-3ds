/* Upstream's text styles drawn with font.pak's baked fonts (see ui_text.h). */
#include "ui_text.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* Chromium's measureText("|MÉqgy") ascent / descent (what Phaser 3.90's MeasureText returns)
 * for canvas sizes 24..132 px, measured with upstream's fonts (assets 056a1f4). */
#define METRIC_MIN 24
#define METRIC_MAX 132
static const unsigned char EMERALD_ASC[] = {
    18, 19, 20, 21, 21, 22, 23, 24, 24, 25, 26, 27, 27, 28, 29, 30, 30, 31, 32, 33, 33, 34,
    35, 36, 36, 37, 38, 39, 39, 40, 41, 42, 42, 43, 44, 45, 45, 46, 47, 48, 48, 49, 50, 51,
    51, 52, 53, 54, 54, 55, 56, 57, 57, 58, 59, 60, 60, 61, 62, 63, 63, 64, 65, 66, 66, 67,
    68, 69, 69, 70, 71, 72, 72, 73, 74, 75, 75, 76, 77, 78, 78, 79, 80, 81, 81, 82, 83, 84,
    84, 85, 86, 87, 87, 88, 89, 90, 90, 91, 92, 93, 93, 94, 95, 96, 96, 97, 98, 99, 99};
static const unsigned char EMERALD_DESC[] = {
    3,  4,  4,  4,  4,  4,  4,  4,  4,  5,  5,  5,  5,  5,  5,  5,  5,  6,  6,  6,  6,  6,
    6,  6,  6,  7,  7,  7,  7,  7,  7,  7,  7,  8,  8,  8,  8,  8,  8,  8,  8,  9,  9,  9,
    9,  9,  9,  9,  9,  10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11, 11, 12,
    12, 12, 12, 12, 12, 12, 12, 13, 13, 13, 13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 14, 14,
    14, 15, 15, 15, 15, 15, 15, 15, 15, 16, 16, 16, 16, 16, 16, 16, 16, 17, 17, 17, 17};
static const unsigned char PKMNEMS_ASC[] = {
    33,  35,  37,  37,  39,  41,  42,  42,  44,  46,  48,  48,  50,  52,  53,  53,  55,  57,  59,  59,  61,  63,
    64,  64,  66,  68,  70,  70,  72,  74,  75,  75,  77,  79,  81,  81,  83,  85,  86,  86,  88,  90,  92,  92,
    94,  96,  97,  97,  99,  101, 103, 103, 105, 107, 108, 108, 110, 112, 114, 114, 116, 118, 119, 119, 121, 123,
    125, 125, 127, 129, 130, 130, 132, 134, 136, 136, 138, 140, 141, 141, 143, 145, 147, 147, 149, 151, 152, 152,
    154, 156, 158, 158, 160, 162, 163, 163, 165, 167, 169, 169, 171, 173, 174, 174, 176, 178, 180, 180, 182};

static void metrics(bool pkmnems, int px, float* asc, float* desc) {
  int c = px < METRIC_MIN ? METRIC_MIN : px > METRIC_MAX ? METRIC_MAX : px;
  float k = (float)px / c;
  if (pkmnems) {
    *asc = PKMNEMS_ASC[c - METRIC_MIN] * k;
    *desc = 0;
  } else {
    *asc = EMERALD_ASC[c - METRIC_MIN] * k;
    *desc = EMERALD_DESC[c - METRIC_MIN] * k;
  }
}

/* ---- styles ---- */
typedef struct {
  unsigned char pkmnems; /* font family */
  unsigned char px;      /* canvas px */
  signed char pad_top, pad_left;
  rr_color col, shadow;
} style_def;

#define C(x) ((rr_color)(((uint32_t)(x) << 8) | 0xFF))
#define S_(fam, px, pt, pl, c, s) {fam, px, pt, pl, C(c), C(s)}
#define S(...) S_(__VA_ARGS__)
#define WHITE 0xf8f8f8, 0x6b5a73

static const style_def STYLES[TS_COUNT_] = {
    [TS_NONE] = S(0, 96, 0, 0, WHITE),
    [TS_MESSAGE] = S(0, 96, 0, 0, WHITE),
    [TS_WINDOW] = S(0, 96, 0, 0, WHITE),
    [TS_WINDOW_ALT] = S(0, 96, 0, 0, 0x484848, 0xd0d0c8),
    [TS_WINDOW_BATTLE_COMMAND] = S(0, 96, 0, 0, WHITE),
    [TS_BATTLE_INFO] = S(0, 72, 0, 0, WHITE),
    [TS_PARTY] = S(1, 48, 2, 0, 0xf8f8f8, 0x707070),
    [TS_PARTY_RED] = S(1, 48, 2, 0, 0xf89890, 0x984038),
    [TS_PARTY_CANCEL_BUTTON] = S(1, 48, 0, 12, 0xf8f8f8, 0x707070),
    [TS_INSTRUCTIONS_TEXT] = S(1, 48, 0, 0, 0xf8f8f8, 0x707070),
    [TS_MOVE_LABEL] = S(1, 48, 0, 0, 0xf8f8f8, 0x707070),
    [TS_SUMMARY] = S(0, 96, 0, 0, 0xf8f8f8, 0x636363),
    [TS_SUMMARY_DEX_NUM] = S(0, 96, 0, 0, 0xf8f8f8, 0x636363),
    [TS_SUMMARY_DEX_NUM_GOLD] = S(0, 96, 0, 0, 0xe8e8a8, 0xa0a060),
    [TS_SUMMARY_ALT] = S(0, 96, 0, 0, 0x484848, 0xd0d0c8),
    [TS_SUMMARY_HEADER] = S(0, 96, 0, 0, 0xf8f8f8, 0x636363),
    [TS_SUMMARY_RED] = S(0, 96, 0, 0, 0xe70808, 0xffbd73),
    [TS_SUMMARY_BLUE] = S(0, 96, 0, 0, 0x40c8f8, 0x006090),
    [TS_SUMMARY_PINK] = S(0, 96, 0, 0, 0xf89890, 0x984038),
    [TS_SUMMARY_GOLD] = S(0, 96, 0, 0, 0xe8e8a8, 0xa0a060),
    [TS_SUMMARY_GRAY] = S(0, 96, 0, 0, 0xa0a0a0, 0x636363),
    [TS_SUMMARY_GREEN] = S(0, 96, 0, 0, 0x78c850, 0x306850),
    [TS_SUMMARY_STATS] = S(0, 96, 0, 0, 0xf8f8f8, 0x636363),
    [TS_SUMMARY_STATS_BLUE] = S(0, 96, 0, 0, 0x40c8f8, 0x006090),
    [TS_SUMMARY_STATS_PINK] = S(0, 96, 0, 0, 0xf89890, 0x984038),
    [TS_SUMMARY_STATS_GOLD] = S(0, 96, 0, 0, 0xe8e8a8, 0xa0a060),
    [TS_LUCK_VALUE] = S(0, 96, 0, 0, 0xf8f8f8, 0x636363),
    [TS_STATS_HEXAGON] = S(0, 64, 0, 0, WHITE),
    [TS_GROWTH_RATE_TYPE] = S(0, 66, 0, 0, 0x484848, 0xd0d0c8),
    [TS_MONEY] = S(0, 72, 0, 0, 0xe8e8a8, 0xa0a060),
    [TS_MONEY_WINDOW] = S(0, 72, 0, 0, 0xe8e8a8, 0xa0a060),
    [TS_HEADER_LABEL] = S(0, 96, 0, 0, 0xf8b050, 0xc07800),
    [TS_STATS_LABEL] = S(0, 96, 0, 0, 0xf8b050, 0xc07800),
    [TS_STATS_VALUE] = S(0, 96, 0, 0, WHITE),
    [TS_SETTINGS_VALUE] = S(0, 96, 0, 0, WHITE),
    [TS_SETTINGS_LABEL] = S(0, 96, 0, 0, 0xf8b050, 0xc07800),
    [TS_SETTINGS_LABEL_NAVBAR] = S(0, 96, 0, 0, 0xf8b050, 0xc07800),
    [TS_SETTINGS_SELECTED] = S(0, 96, 0, 0, 0xf88880, 0xf83018),
    [TS_SETTINGS_LOCKED] = S(0, 96, 0, 0, 0xa0a0a0, 0x636363),
    [TS_EGG_LIST] = S(0, 62, 0, 0, WHITE),
    [TS_EGG_SUMMARY_NAME] = S(0, 96, 0, 0, 0xf8f8f8, 0x636363),
    [TS_EGG_SUMMARY_DEX] = S(0, 96, 0, 0, 0xf8f8f8, 0x636363),
    [TS_STARTER_VALUE_LIMIT] = S(0, 60, 0, 0, WHITE),
    [TS_TOOLTIP_TITLE] = S(0, 72, 0, 0, 0xe70808, 0xffbd73),
    [TS_TOOLTIP_CONTENT] = S(0, 64, 0, 0, WHITE),
    [TS_FILTER_BAR_MAIN] = S(0, 64, 0, 0, WHITE),
    [TS_MOVE_INFO_CONTENT] = S(0, 56, 0, 0, WHITE),
    [TS_MOVE_PP_FULL] = S(0, 96, 0, 0, WHITE),
    [TS_MOVE_PP_HALF_FULL] = S(0, 96, 0, 0, 0xccbe00, 0x6e672c),
    [TS_MOVE_PP_NEAR_EMPTY] = S(0, 96, 0, 0, 0xd64b00, 0x69402a),
    [TS_MOVE_PP_EMPTY] = S(0, 96, 0, 0, 0xe13d3d, 0x632929),
    [TS_SMALLER_WINDOW_ALT] = S(0, 60, 0, 0, 0x484848, 0xd0d0c8),
    [TS_BGM_BAR] = S(0, 72, 0, 0, WHITE),
    [TS_PERFECT_IV] = S(0, 96, 0, 0, 0xf8b050, 0xc07800),
    [TS_ME_OPTION_DEFAULT] = S(0, 96, 0, 0, WHITE),
    [TS_ME_OPTION_SPECIAL] = S(0, 96, 0, 0, 0x78c850, 0x306850),
    [TS_SHADOW_TEXT] = S(0, 96, 0, 0, 0x6b5a73, 0x6b5a73),
};

static const style_def* style_of(text_style s) { return (unsigned)s < TS_COUNT_ ? &STYLES[s] : &STYLES[0]; }

rr_color text_color(text_style s, bool shadow) {
  const style_def* d = style_of(s);
  return shadow ? d->shadow : d->col;
}

text_opts text_defaults(void) {
  text_opts o;
  memset(&o, 0, sizeof o);
  o.line_spacing = -1;
  o.chars = TEXT_ALL;
  return o;
}

/* ---- fonts ---- */
#define FONT_CACHE 24
static struct {
  bool pkmnems;
  int size; /* logical */
  rr_font* f;
  bool tried;
} g_fonts[FONT_CACHE];
static int g_nfonts;

static rr_font* font_at(bool pkmnems, int size) {
  if (size < 5) size = 5;
  if (size > 16 && size != 21) size = size >= 19 ? 21 : 16; /* baked sizes: 5..16 and 21 */
  for (int i = 0; i < g_nfonts; i++)
    if (g_fonts[i].pkmnems == pkmnems && g_fonts[i].size == size) return g_fonts[i].f;
  char key[48];
  snprintf(key, sizeof key, "fonts/%s@%d", pkmnems ? "pkmnems" : "emerald", size);
  rr_font* f = rr_font_load_key(key);
  if (g_nfonts < FONT_CACHE) {
    g_fonts[g_nfonts].pkmnems = pkmnems;
    g_fonts[g_nfonts].size = size;
    g_fonts[g_nfonts].f = f;
    g_fonts[g_nfonts].tried = true;
    g_nfonts++;
  }
  return f;
}

rr_font* text_font(const char* family, int font_px) {
  return font_at(family && !strcmp(family, "pkmnems"), (int)floorf(font_px / 6.0f + 0.5f));
}

int text_utf8_len(const char* s) {
  int n = 0;
  for (; *s; s++)
    if (((unsigned char)*s & 0xC0) != 0x80) n++;
  return n;
}

/* byte offset of the n-th character */
static int utf8_offset(const char* s, int nchars) {
  int i = 0, c = 0;
  while (s[i]) {
    if (((unsigned char)s[i] & 0xC0) != 0x80) {
      if (c == nchars) return i;
      c++;
    }
    i++;
  }
  return i;
}

/* ---- layout ---- */
typedef struct {
  const style_def* d;
  rr_font* f;
  int px;
  float asc, desc, spacing; /* canvas px */
} layout;

static void layout_init(layout* L, text_style st, const text_opts* o) {
  L->d = style_of(st);
  L->px = o && o->font_px ? o->font_px : L->d->px;
  L->f = font_at(L->d->pkmnems, (int)floorf(L->px / 6.0f + 0.5f));
  metrics(L->d->pkmnems, L->px, &L->asc, &L->desc);
  L->spacing = o && o->line_spacing >= 0 ? o->line_spacing : 5.0f; /* setLineSpacing(scale * 30) */
}

static float line_w(rr_font* f, const char* s, int nbytes) {
  if (!f || nbytes <= 0) return 0;
  char buf[256];
  if (nbytes >= (int)sizeof buf) nbytes = sizeof buf - 1;
  memcpy(buf, s, nbytes);
  buf[nbytes] = 0;
  return (float)rr_text_width(f, buf);
}

float text_line_width(text_style st, int font_px, const char* s, int nbytes) {
  text_opts o = text_defaults();
  o.font_px = font_px;
  layout L;
  layout_init(&L, st, &o);
  return line_w(L.f, s, nbytes < 0 ? (int)strlen(s) : nbytes);
}

static void wrap_into(const layout* L, const char* s, float wrap_w, char* out, int n) {
  int o = 0;
  if (n <= 0) return;
  if (wrap_w <= 0) {
    snprintf(out, n, "%s", s);
    return;
  }
  float space = line_w(L->f, " ", 1);
  const char* line = s;
  while (1) {
    const char* nl = strchr(line, '\n');
    const char* lend = nl ? nl : line + strlen(line);
    float left = wrap_w;
    const char* w = line;
    bool first = true;
    while (1) {
      const char* sp = memchr(w, ' ', lend - w);
      const char* wend = sp ? sp : lend;
      float ww = line_w(L->f, w, (int)(wend - w));
      float wws = ww + (sp ? space : 0);
      if (wws > left && !first) {
        if (o < n - 1) out[o++] = '\n';
        left = wrap_w;
      }
      for (const char* c = w; c < wend && o < n - 1; c++) out[o++] = *c;
      if (sp) {
        if (o < n - 1) out[o++] = ' ';
        left -= wws;
        w = sp + 1;
      } else {
        left -= ww;
        break;
      }
      first = false;
    }
    if (!nl) break;
    if (o < n - 1) out[o++] = '\n';
    line = nl + 1;
  }
  out[o] = 0;
}

void text_wrap(text_style st, const char* s, const text_opts* o, char* out, int n) {
  layout L;
  layout_init(&L, st, o);
  wrap_into(&L, s, o ? o->wrap_w : 0, out, n);
}

typedef struct {
  const char* start[16];
  int len[16];
  float w[16];
  int n;
  float maxw;
} lines;

static void split_lines(const layout* L, const char* s, int max_lines, lines* out) {
  out->n = 0;
  out->maxw = 0;
  const char* p = s;
  while (out->n < 16) {
    const char* nl = strchr(p, '\n');
    int len = nl ? (int)(nl - p) : (int)strlen(p);
    out->start[out->n] = p;
    out->len[out->n] = len;
    out->w[out->n] = line_w(L->f, p, len);
    if (out->w[out->n] > out->maxw) out->maxw = out->w[out->n];
    out->n++;
    if (!nl || (max_lines > 0 && out->n >= max_lines)) break;
    p = nl + 1;
  }
}

static float box_h(const layout* L, int n) {
  float lh = L->asc + L->desc;
  float h = lh * n + (n > 1 ? L->spacing * (n - 1) : 0);
  int pb = L->d->pkmnems && L->d->pad_top == 2 ? 16 : 6; /* padding.bottom: PARTY styles 16, else 6 */
  return (h + L->d->pad_top + pb) / 6.0f;
}

float text_line_pitch(text_style st, const text_opts* o) {
  layout L;
  layout_init(&L, st, o);
  return (L.asc + L.desc + L.spacing) / 6.0f;
}

static float run(text_style st, float x, float y, const char* s, const text_opts* o, bool draw, float* out_h,
                 int* out_lines) {
  layout L;
  layout_init(&L, st, o);
  char wrapped[1024];
  const char* txt = s;
  if (o && o->wrap_w > 0) {
    wrap_into(&L, s, o->wrap_w, wrapped, sizeof wrapped);
    txt = wrapped;
  }
  lines ls;
  split_lines(&L, txt, o ? o->max_lines : 0, &ls);
  float pad_l = L.d->pad_left / 6.0f;
  float w = ls.maxw + pad_l;
  float h = box_h(&L, ls.n);
  if (out_h) *out_h = h;
  if (out_lines) *out_lines = ls.n;
  if (!draw || !L.f) return w;

  float x0 = x - (o ? o->origin_x : 0) * w;
  float y0 = y - (o ? o->origin_y : 0) * h;
  rr_color col = o && o->color ? o->color : L.d->col;
  rr_color sh = o && o->shadow ? o->shadow : L.d->shadow;
  if (o && o->no_shadow) sh = 0;
  int limit = o && o->chars >= 0 ? o->chars : 1 << 30; /* characters, newlines included */
  int used = 0;
  int ascent = rr_font_ascent(L.f);
  for (int i = 0; i < ls.n && used < limit; i++) {
    float base = y0 + (L.d->pad_top + L.asc + i * (L.asc + L.desc + L.spacing)) / 6.0f;
    float lx = x0 + pad_l;
    if (o && o->align == 1) lx += (ls.maxw - ls.w[i]) / 2;
    if (o && o->align == 2) lx += ls.maxw - ls.w[i];
    char buf[256];
    int len = ls.len[i] < (int)sizeof buf - 1 ? ls.len[i] : (int)sizeof buf - 1;
    memcpy(buf, ls.start[i], len);
    buf[len] = 0;
    int nch = text_utf8_len(buf);
    if (used + nch > limit) buf[utf8_offset(buf, limit - used)] = 0;
    used += nch + 1; /* the newline */
    rr_text(L.f, floorf(lx + 0.5f), floorf(base + 0.5f) - ascent, buf, col, sh);
  }
  return w;
}

float text_draw(text_style st, float x, float y, const char* s, const text_opts* o) {
  return run(st, x, y, s, o, true, NULL, NULL);
}

float text_width(text_style st, const char* s, const text_opts* o) { return run(st, 0, 0, s, o, false, NULL, NULL); }

float text_height(text_style st, const char* s, const text_opts* o) {
  float h = 0;
  run(st, 0, 0, s, o, false, &h, NULL);
  return h;
}

int text_lines(text_style st, const char* s, const text_opts* o) {
  int n = 0;
  run(st, 0, 0, s, o, false, NULL, &n);
  return n;
}
