/* mui: see mui.h. Upstream sources are named next to each piece. */
#include "mui.h"

#include <ctype.h>
#include <jansson.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <citro2d.h>

#include "pak.h"
#include "snd.h"

static uint32_t g_now;
static float g_now_frac;

void mui_tick(float dt_ms) {
  g_now_frac += dt_ms;
  uint32_t whole = (uint32_t)g_now_frac;
  g_now += whole;
  g_now_frac -= whole;
}

uint32_t mui_now(void) { return g_now; }

/* ------------------------------------------------------------------ images */

#define IMG_SLOTS 96
typedef struct {
  char key[64];
  rr_atlas* atlas; /* NULL + tried = missing */
  bool tried;
} img_slot;
static img_slot g_imgs[IMG_SLOTS];

static rr_atlas* img_get(const char* key) {
  int free_i = -1;
  for (int i = 0; i < IMG_SLOTS; i++) {
    if (g_imgs[i].tried && !strcmp(g_imgs[i].key, key)) return g_imgs[i].atlas;
    if (!g_imgs[i].tried && free_i < 0) free_i = i;
  }
  if (free_i < 0) { /* full: drop everything (screens reload what they use) */
    mui_unload_all();
    free_i = 0;
  }
  img_slot* s = &g_imgs[free_i];
  snprintf(s->key, sizeof s->key, "%s", key);
  char k[96];
  snprintf(k, sizeof k, "images/%s", key);
  s->atlas = rr_atlas_load_key(k);
  s->tried = true;
  return s->atlas;
}

void mui_unload_all(void) {
  for (int i = 0; i < IMG_SLOTS; i++) {
    rr_atlas_free(g_imgs[i].atlas);
    memset(&g_imgs[i], 0, sizeof g_imgs[i]);
  }
}

const rr_frame* mui_frame(const char* key, const char* frame) {
  rr_atlas* a = img_get(key);
  if (!a) return NULL;
  if (!frame || !*frame) return rr_atlas_frame(a, 0);
  return rr_atlas_find(a, frame);
}

bool mui_image_ex(const char* key, const char* frame, float x, float y, const rr_draw_opts* o) {
  const rr_frame* f = mui_frame(key, frame);
  if (!f) return false;
  rr_draw(f, x, y, o);
  return true;
}

bool mui_image(const char* key, const char* frame, float x, float y, float ox, float oy) {
  rr_draw_opts o = RR_DEFAULT_OPTS;
  o.origin_x = ox;
  o.origin_y = oy;
  return mui_image_ex(key, frame, x, y, &o);
}

/* ------------------------------------------------------------------ view */

static rr_screen g_screen = RR_TOP;
static int g_org[2][2] = {{40, 30}, {0, 60}};
static bool g_clipped;

void mui_set_origin(rr_screen s, int px, int py) {
  rr_set_origin(s, px, py);
  g_screen = s;
  g_org[s][0] = px;
  g_org[s][1] = py;
}

/* The GPU scissor works on the framebuffer, which is the screen turned a
 * quarter: 240 wide, 400 (top) or 320 (bottom) tall, x from the bottom edge. */
void mui_clip(float x, float y, float w, float h) {
  int sw = g_screen == RR_TOP ? 400 : 320;
  int x0 = (int)floorf(x) + g_org[g_screen][0], y0 = (int)floorf(y) + g_org[g_screen][1];
  int x1 = (int)ceilf(x + w) + g_org[g_screen][0], y1 = (int)ceilf(y + h) + g_org[g_screen][1];
  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 > sw) x1 = sw;
  if (y1 > 240) y1 = 240;
  if (x1 < x0) x1 = x0;
  if (y1 < y0) y1 = y0;
  C2D_Flush();
  C3D_SetScissor(GPU_SCISSOR_NORMAL, 240 - y1, sw - x1, 240 - y0, sw - x0);
  g_clipped = true;
}

void mui_clip_off(void) {
  if (!g_clipped) return;
  C2D_Flush();
  C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
  g_clipped = false;
}

static int g_window_style = 1;
void mui_set_window_style(int style) { g_window_style = style < 1 || style > 5 ? 1 : style; }

/* ui-theme.ts addWindow: window_<uiWindowStyle>, border 8 (6 in the legacy theme) */
/* draws source rectangle (sx, sy, sw, sh) of the art into (dx, dy, dw, dh): the art scaled and clipped to the destination */
static void slice(const rr_frame* f, float dx, float dy, float dw, float dh, float sx, float sy, float sw, float sh) {
  if (dw <= 0 || dh <= 0 || sw <= 0 || sh <= 0) return;
  mui_clip(dx, dy, dw, dh);
  rr_draw_opts o = RR_DEFAULT_OPTS;
  o.scale_x = dw / sw;
  o.scale_y = dh / sh;
  rr_draw(f, dx - sx * o.scale_x, dy - sy * o.scale_y, &o);
  mui_clip_off();
}

void mui_nineslice(const rr_frame* f, float art_w, float art_h, float x, float y, float w, float h, float left, float right, float top, float bottom) {
  const float mw = w - left - right, mh = h - top - bottom;
  const float cw = art_w - left - right, ch = art_h - top - bottom;
  const float xs[3] = {x, x + left, x + w - right}, ws[3] = {left, mw, right};
  const float sxs[3] = {0, left, art_w - right}, sws[3] = {left, cw, right};
  const float ys[3] = {y, y + top, y + h - bottom}, hs[3] = {top, mh, bottom};
  const float sys_[3] = {0, top, art_h - bottom}, shs[3] = {top, ch, bottom};
  for (int j = 0; j < 3; j++)
    for (int i = 0; i < 3; i++) slice(f, xs[i], ys[j], ws[i], hs[j], sxs[i], sys_[j], sws[i], shs[j]);
}

void mui_window(float x, float y, float w, float h) {
  char key[40];
  snprintf(key, sizeof key, "ui/windows/window_%d", g_window_style);
  const rr_frame* f = mui_frame(key, NULL);
  if (f)
    rr_nineslice(f, floorf(x), floorf(y), floorf(w), floorf(h), 8);
  else
    rr_rect(x, y, w, h, RR_RGBA(0x40, 0x40, 0x48, 255));
}

/* ------------------------------------------------------------------ text */

/* text.ts getTextStyleOptions, measured: upstream draws each text object at
 * canvas size (96 px = 16 here) and scales it by 1/6. Phaser 3.90's measured
 * metrics for those fonts in Chromium (ref: menus README, "text metrics"):
 *   emerald 96 px: ascent 72, line 84; 72 px: 54, 63; 66: 50, 58; 64: 48, 56;
 *   60: 46, 54; 56: 42, 49.   pkmnems 48 px: ascent 66, line 66 (descent 0).
 * The font pack bakes the same TTFs at the logical size with PIL, whose line
 * top sits `pil_asc` above the baseline; the difference moves the glyphs so
 * the baseline lands where Phaser puts it. */
typedef struct {
  const char* font;
  int size;          /* baked font size (logical px) */
  float canvas;      /* upstream font size (canvas px) */
  float asc, line;   /* Phaser metrics (canvas px) */
  float pad_top, pad_bottom;
  int pil_asc;       /* baked font ascent (logical px) */
} style_metrics;

static style_metrics style_metrics_of(mui_style s) {
  switch (s) {
    case TS_PARTY:
    case TS_PARTY_RED:
      return (style_metrics){"pkmnems", 8, 48, 66, 66, 2, 16, 12};
    case TS_PARTY_CANCEL_BUTTON:
      return (style_metrics){"pkmnems", 8, 48, 66, 66, 0, 0, 12};
    case TS_INSTRUCTIONS_TEXT:
    case TS_MOVE_LABEL:
      return (style_metrics){"pkmnems", 8, 48, 66, 66, 0, 0, 12};
    case TS_BATTLE_INFO:
    case TS_MONEY:
    case TS_MONEY_WINDOW:
    case TS_TOOLTIP_TITLE:
    case TS_BGM_BAR:
      return (style_metrics){"emerald", 12, 72, 54, 63, 0, 6, 9};
    case TS_GROWTH_RATE_TYPE:
      return (style_metrics){"emerald", 11, 66, 50, 58, 0, 6, 9};
    case TS_EGG_LIST:
    case TS_STARTER_VALUE_LIMIT:
    case TS_SMALLER_WINDOW_ALT:
      return (style_metrics){"emerald", 10, 60, 46, 54, 0, 6, 8};
    case TS_TOOLTIP_CONTENT:
    case TS_FILTER_BAR_MAIN:
    case TS_STATS_HEXAGON:
      return (style_metrics){"emerald", 11, 64, 48, 56, 0, 6, 9};
    case TS_MOVE_INFO_CONTENT:
      return (style_metrics){"emerald", 9, 56, 42, 49, 0, 6, 7};
    case TSX_WINDOW_ALT_76:
      return (style_metrics){"emerald", 13, 76, 58, 68, 0, 6, 10};
    case TSX_PIXEL_6: /* pkmnems 48 px scaled to 36 */
      return (style_metrics){"pkmnems", 6, 36, 49.5f, 49.5f, 0, 0, 9};
    case TSX_PIXEL_8:
      return (style_metrics){"pkmnems", 8, 48, 66, 66, 0, 0, 12};
    default:
      return (style_metrics){"emerald", 16, 96, 72, 84, 0, 6, 12};
  }
}

#define FONT_SLOTS 12
static struct {
  char key[32];
  rr_font* f;
  bool tried;
} g_fonts[FONT_SLOTS];

static rr_font* font_get(mui_style s) {
  style_metrics sf = style_metrics_of(s);
  char key[32];
  snprintf(key, sizeof key, "fonts/%s@%d", sf.font, sf.size);
  for (int i = 0; i < FONT_SLOTS; i++) {
    if (g_fonts[i].tried && !strcmp(g_fonts[i].key, key)) return g_fonts[i].f;
    if (!g_fonts[i].tried) {
      snprintf(g_fonts[i].key, sizeof g_fonts[i].key, "%s", key);
      g_fonts[i].f = rr_font_load_key(key);
      g_fonts[i].tried = true;
      return g_fonts[i].f;
    }
  }
  return NULL;
}

/* text.ts getTextColor (default theme, not legacy) */
rr_color mui_style_color(mui_style s, bool sh) {
#define C(fg, shadow) return MUI_RGB(sh ? (shadow) : (fg))
  switch (s) {
    case TS_MESSAGE:
    case TS_WINDOW:
    case TS_WINDOW_BATTLE_COMMAND:
    case TS_MOVE_INFO_CONTENT:
    case TS_STATS_HEXAGON:
    case TS_MOVE_PP_FULL:
    case TS_EGG_LIST:
    case TS_TOOLTIP_CONTENT:
    case TS_FILTER_BAR_MAIN:
    case TS_STARTER_VALUE_LIMIT:
    case TS_SETTINGS_VALUE:
    case TS_BATTLE_INFO:
    case TS_STATS_VALUE:
    case TS_BGM_BAR:
    case TS_ME_OPTION_DEFAULT:
      C(0xf8f8f8, 0x6b5a73);
    case TS_MOVE_PP_HALF_FULL: C(0xccbe00, 0x6e672c);
    case TS_MOVE_PP_NEAR_EMPTY: C(0xd64b00, 0x69402a);
    case TS_MOVE_PP_EMPTY: C(0xe13d3d, 0x632929);
    case TS_WINDOW_ALT:
    case TSX_WINDOW_ALT_76:
    case TS_SUMMARY_ALT:
    case TS_GROWTH_RATE_TYPE:
    case TS_SMALLER_WINDOW_ALT:
      C(0x484848, 0xd0d0c8);
    case TS_PARTY:
    case TS_PARTY_CANCEL_BUTTON:
    case TS_INSTRUCTIONS_TEXT:
    case TS_MOVE_LABEL:
      C(0xf8f8f8, 0x707070);
    case TS_PARTY_RED:
    case TS_SUMMARY_PINK:
    case TS_SUMMARY_STATS_PINK:
      C(0xf89890, 0x984038);
    case TS_SUMMARY:
    case TS_SUMMARY_DEX_NUM:
    case TS_SUMMARY_HEADER:
    case TS_SUMMARY_STATS:
    case TS_EGG_SUMMARY_NAME:
    case TS_EGG_SUMMARY_DEX:
    case TS_LUCK_VALUE:
      C(0xf8f8f8, 0x636363);
    case TS_SUMMARY_RED:
    case TS_TOOLTIP_TITLE:
      C(0xe70808, 0xffbd73);
    case TS_SUMMARY_BLUE:
    case TS_SUMMARY_STATS_BLUE:
      C(0x40c8f8, 0x006090);
    case TS_SUMMARY_GOLD:
    case TS_SUMMARY_DEX_NUM_GOLD:
    case TS_SUMMARY_STATS_GOLD:
    case TS_MONEY:
    case TS_MONEY_WINDOW:
      C(0xe8e8a8, 0xa0a060);
    case TS_SETTINGS_LOCKED:
    case TS_SUMMARY_GRAY:
      C(0xa0a0a0, 0x636363);
    case TS_STATS_LABEL:
    case TS_SETTINGS_LABEL:
    case TS_SETTINGS_LABEL_NAVBAR:
    case TS_HEADER_LABEL:
    case TS_PERFECT_IV:
      C(0xf8b050, 0xc07800);
    case TS_SUMMARY_GREEN:
    case TS_ME_OPTION_SPECIAL:
      C(0x78c850, 0x306850);
    case TS_SETTINGS_SELECTED: C(0xf88880, 0xf83018);
    case TS_SHADOW_TEXT: C(0x6b5a73, 0x6b5a73);
    default: C(0xf8f8f8, 0x6b5a73);
  }
#undef C
}

/* Line pitch of a text with `ls` canvas px of lineSpacing (addTextObject sets
 * scale * 30 = 5, addBBCodeTextObject scale * 60 = 10), in logical px. */
static float line_pitch_ls(const style_metrics* m, float ls) { return (m->line + ls) / 6.0f; }
static float line_pitch(const style_metrics* m) { return line_pitch_ls(m, MUI_LS_TEXT); }

/* Where a line's baked glyphs go so its baseline lands where Phaser's would. */
static float line_y(const style_metrics* m, float y, int line, float ls) {
  return floorf(y + (m->pad_top + m->asc) / 6.0f - m->pil_asc + line * line_pitch_ls(m, ls) + 0.5f);
}

/* Draws each line so its baseline lands where Phaser's would, rounded to the
 * nearest pixel (upstream positions are fractional at 1/6 px). */
int mui_text_ls(mui_style s, float x, float y, float ls, const char* utf8, rr_color c, rr_color shadow) {
  rr_font* f = font_get(s);
  style_metrics m = style_metrics_of(s);
  int widest = 0, line = 0;
  char buf[256];
  const char* p = utf8;
  for (;;) {
    const char* nl = strchr(p, '\n');
    size_t n = nl ? (size_t)(nl - p) : strlen(p);
    if (n >= sizeof buf) n = sizeof buf - 1;
    memcpy(buf, p, n);
    buf[n] = 0;
    int w = rr_text(f, floorf(x + 0.5f), line_y(&m, y, line, ls), buf, c, shadow);
    if (w > widest) widest = w;
    if (!nl) break;
    p = nl + 1;
    line++;
  }
  return widest;
}

int mui_text_c(mui_style s, float x, float y, const char* utf8, rr_color c, rr_color shadow) {
  return mui_text_ls(s, x, y, MUI_LS_TEXT, utf8, c, shadow);
}

int mui_text(mui_style s, float x, float y, const char* utf8) {
  return mui_text_c(s, x, y, utf8, mui_style_color(s, false), mui_style_color(s, true));
}

int mui_text_width(mui_style s, const char* utf8) {
  rr_font* f = font_get(s);
  if (!f) return (int)strlen(utf8) * 6;
  /* widest line */
  int best = 0;
  char line[256];
  const char* p = utf8;
  while (*p) {
    const char* nl = strchr(p, '\n');
    size_t n = nl ? (size_t)(nl - p) : strlen(p);
    if (n >= sizeof line) n = sizeof line - 1;
    memcpy(line, p, n);
    line[n] = 0;
    int w = rr_text_width(f, line);
    if (w > best) best = w;
    if (!nl) break;
    p = nl + 1;
  }
  return best;
}

int mui_line_height(mui_style s) {
  style_metrics m = style_metrics_of(s);
  return (int)floorf(line_pitch(&m) + 0.5f);
}

/* Phaser's text box height: lines * line + (lines - 1) * lineSpacing + padding */
float mui_text_height_ls(mui_style s, float ls, const char* utf8) {
  style_metrics m = style_metrics_of(s);
  int lines = 1;
  for (const char* p = utf8; *p; p++) lines += *p == '\n';
  return (lines * m.line + (lines - 1) * ls + m.pad_top + m.pad_bottom) / 6.0f;
}

float mui_text_height(mui_style s, const char* utf8) { return mui_text_height_ls(s, MUI_LS_TEXT, utf8); }

/* Phaser Text basicWordWrap: greedy by words, measured with the trailing
 * space; a word that doesn't fit starts a new line (never the first word). */
int mui_wrap(mui_style s, const char* in, float width, char* out, size_t n) {
  rr_font* f = font_get(s);
  size_t o = 0;
  int lines = 1;
  float space = f ? (float)rr_text_width(f, " ") : 4;
  char word[128];
#define PUT(ch) do { if (o + 1 < n) out[o++] = (ch); } while (0)
  const char* p = in;
  while (*p) {
    float left = width;
    const char* end = strchr(p, '\n');
    if (!end) end = p + strlen(p);
    bool first = true;
    while (p <= end) {
      const char* sp = p;
      while (sp < end && *sp != ' ') sp++;
      size_t wl = (size_t)(sp - p);
      if (wl >= sizeof word) wl = sizeof word - 1;
      memcpy(word, p, wl);
      word[wl] = 0;
      bool last = sp >= end;
      float ww = f ? (float)rr_text_width(f, word) : wl * 6.0f;
      float wws = ww + (last ? 0 : space);
      if (wws > left && !first) {
        PUT('\n');
        lines++;
        left = width;
      }
      for (size_t i = 0; i < wl; i++) PUT(word[i]);
      if (!last) {
        PUT(' ');
        left -= wws;
      } else {
        left -= ww;
      }
      first = false;
      p = sp + 1;
      if (last) break;
    }
    if (*end == '\n') {
      PUT('\n');
      lines++;
      p = end + 1;
    } else {
      p = end;
    }
  }
#undef PUT
  if (n) out[o < n ? o : n - 1] = 0;
  return lines;
}

/* rexBBCodeText with the [color=#rrggbb] / [shadow=#rrggbb] tags upstream's
 * getBBCodeFrag writes; [/color] and [/shadow] go back to the base style. */
static bool bb_hex(const char* p, uint32_t* out) {
  if (*p != '#') return false;
  char* e;
  unsigned long v = strtoul(p + 1, &e, 16);
  if (e - p != 7 || *e != ']') return false;
  *out = (uint32_t)v;
  return true;
}

int mui_bbtext(mui_style s, float x, float y, const char* bb) {
  rr_font* f = font_get(s);
  style_metrics m = style_metrics_of(s);
  rr_color base_c = mui_style_color(s, false), base_sh = mui_style_color(s, true);
  rr_color c = base_c, sh = base_sh;
  int line = 0, widest = 0;
  float cx = x;
  char seg[256];
  size_t sl = 0;
  const char* p = bb;
  for (;;) {
    bool flush = !*p || *p == '\n' || *p == '[';
    uint32_t hex = 0;
    const char* tag_end = NULL;
    if (*p == '[') {
      if (!strncmp(p, "[color=", 7) && bb_hex(p + 7, &hex)) tag_end = p + 15;
      else if (!strncmp(p, "[shadow=", 8) && bb_hex(p + 8, &hex)) tag_end = p + 16;
      else if (!strncmp(p, "[/color]", 8)) tag_end = p + 8;
      else if (!strncmp(p, "[/shadow]", 9)) tag_end = p + 9;
      else flush = false;
    }
    if (flush && sl) {
      seg[sl] = 0;
      cx += f ? rr_text(f, floorf(cx + 0.5f), line_y(&m, y, line, MUI_LS_BBCODE), seg, c, sh) : 0;
      sl = 0;
    }
    if (!*p) break;
    if (tag_end) {
      if (p[1] == 'c') c = MUI_RGB(hex);
      else if (p[1] == 's') sh = MUI_RGB(hex);
      else if (p[2] == 'c') c = base_c;
      else sh = base_sh;
      p = tag_end;
      continue;
    }
    if (*p == '\n') {
      if ((int)(cx - x) > widest) widest = (int)(cx - x);
      cx = x;
      line++;
      p++;
      continue;
    }
    if (sl + 1 < sizeof seg) seg[sl++] = *p;
    p++;
  }
  if ((int)(cx - x) > widest) widest = (int)(cx - x);
  return widest;
}

char* mui_bbfrag(char* out, size_t n, const char* content, mui_style s, bool close) {
  uint32_t c = mui_style_color(s, false) >> 8, sh = mui_style_color(s, true) >> 8;
  snprintf(out, n, "[color=#%06lx][shadow=#%06lx]%s%s", (unsigned long)c, (unsigned long)sh, content,
           close ? "[/color][/shadow]" : "");
  return out;
}

int mui_text_o(mui_style s, float x, float y, float ox, float oy, const char* utf8) {
  int w = mui_text_width(s, utf8);
  float h = mui_text_height(s, utf8);
  return mui_text(s, x - ox * w, y - oy * h, utf8);
}

void mui_fit_name(mui_style s, char* name, int max_w) {
  /* party-ui-handler.ts PartySlot.setup: drop a character (two if it already
   * ends in ".") and add "." until it fits */
  while (mui_text_width(s, name) > max_w) {
    size_t n = strlen(name);
    if (n <= 1) return;
    size_t cut = (n >= 2 && name[n - 1] == '.') ? n - 2 : n - 1;
    /* don't split a UTF-8 sequence */
    while (cut > 0 && ((unsigned char)name[cut] & 0xC0) == 0x80) cut--;
    name[cut] = 0;
    while (cut > 0 && name[cut - 1] == ' ') name[--cut] = 0;
    strcat(name, ".");
  }
}

/* ------------------------------------------------------------------ strings */

#define NS_SLOTS 24
static struct {
  char ns[48];
  json_t* root; /* NULL + tried = missing */
  bool tried;
} g_ns[NS_SLOTS];

/* "partyUiHandler" -> "party-ui-handler" (the locale file names) */
/* i18n-namespace-map.ts: namespaces whose file name isn't the kebab-case name */
static const char* const g_ns_map[][2] = {
    {"titles", "trainer-titles"}, {"moveTriggers", "move-trigger"}, {"abilityTriggers", "ability-trigger"},
    {"battlePokemonForm", "pokemon-form-battle"}, {"miscDialogue", "dialogue-misc"},
    {"battleSpecDialogue", "dialogue-final-boss"}, {"doubleBattleDialogue", "dialogue-double-battle"},
    {"splashMessages", "splash-texts"}, {"mysteryEncounterMessages", "mystery-encounter-texts"}, {"biome", "biomes"},
};

static void ns_file(const char* ns, char* out, size_t n) {
  for (size_t i = 0; i < sizeof g_ns_map / sizeof g_ns_map[0]; i++)
    if (!strcmp(ns, g_ns_map[i][0])) {
      snprintf(out, n, "%s", g_ns_map[i][1]);
      return;
    }
  size_t o = 0;
  for (const char* p = ns; *p && o + 2 < n; p++) {
    if (isupper((unsigned char)*p)) {
      if (o) out[o++] = '-';
      out[o++] = (char)tolower((unsigned char)*p);
    } else {
      out[o++] = *p;
    }
  }
  out[o] = 0;
}

static json_t* ns_get(const char* ns) {
  for (int i = 0; i < NS_SLOTS; i++) {
    if (g_ns[i].tried && !strcmp(g_ns[i].ns, ns)) return g_ns[i].root;
    if (!g_ns[i].tried) {
      char file[64], key[96];
      ns_file(ns, file, sizeof file);
      snprintf(key, sizeof key, "locales/en/%s.json", file);
      size_t len;
      char* d = pak_read(key, &len);
      g_ns[i].tried = true;
      snprintf(g_ns[i].ns, sizeof g_ns[i].ns, "%s", ns);
      if (d) {
        g_ns[i].root = json_loadb(d, len, 0, NULL);
        free(d);
      }
      return g_ns[i].root;
    }
  }
  return NULL;
}

const char* mui_t(const char* key) {
  const char* colon = strchr(key, ':');
  if (!colon) return key;
  char ns[48];
  size_t n = (size_t)(colon - key);
  if (n >= sizeof ns) return key;
  memcpy(ns, key, n);
  ns[n] = 0;
  json_t* j = ns_get(ns);
  const char* p = colon + 1;
  while (j && *p) {
    const char* dot = strchr(p, '.');
    char part[96];
    size_t m = dot ? (size_t)(dot - p) : strlen(p);
    if (m >= sizeof part) return key;
    memcpy(part, p, m);
    part[m] = 0;
    j = json_object_get(j, part);
    p = dot ? dot + 1 : p + m;
  }
  return (j && json_is_string(j)) ? json_string_value(j) : key;
}

char* mui_vtf(char* out, size_t n, const char* key, va_list ap) {
  const char* names[8];
  const char* vals[8];
  int nv = 0;
  for (;;) {
    const char* name = va_arg(ap, const char*);
    if (!name) break;
    const char* val = va_arg(ap, const char*);
    if (nv < 8) {
      names[nv] = name;
      vals[nv] = val ? val : "";
      nv++;
    }
  }
  const char* src = mui_t(key);
  size_t o = 0;
  while (*src && o + 1 < n) {
    if (src[0] == '{' && src[1] == '{') {
      const char* end = strstr(src + 2, "}}");
      if (end) {
        const char* repl = NULL;
        for (int i = 0; i < nv; i++)
          if (strlen(names[i]) == (size_t)(end - src - 2) && !strncmp(names[i], src + 2, end - src - 2)) repl = vals[i];
        if (repl) {
          while (*repl && o + 1 < n) out[o++] = *repl++;
          src = end + 2;
          continue;
        }
      }
    }
    out[o++] = *src++;
  }
  out[o] = 0;
  return out;
}

char* mui_tf(char* out, size_t n, const char* key, ...) {
  va_list ap;
  va_start(ap, key);
  mui_vtf(out, n, key, ap);
  va_end(ap);
  return out;
}

/* ------------------------------------------------------------------ sound */

void mui_sfx(const char* key) {
  char k[96];
  snprintf(k, sizeof k, "audio/%s", key);
  snd_sfx_play(k);
}

/* ------------------------------------------------------------------ messages */

void mui_msg_set(mui_msg* m, const char* text) {
  snprintf(m->text, sizeof m->text, "%s", text);
  m->shown = (int)strlen(m->text);
  m->acc = 0;
  m->cb = NULL;
  m->prompt = m->pending_prompt = m->awaiting_action = false;
}

static void msg_done(mui_msg* m) {
  if (m->prompt) {
    m->pending_prompt = false;
    m->awaiting_action = true;
    return;
  }
  mui_cb cb = m->cb;
  m->cb = NULL;
  if (cb) cb(m->ctx);
}

void mui_msg_show(mui_msg* m, const char* text, int delay_ms, mui_cb cb, void* ctx, bool prompt) {
  /* a new text finishes the old one first (upstream fires the pending callback timer) */
  snprintf(m->text, sizeof m->text, "%s", text);
  m->delay_ms = delay_ms;
  m->cb = cb;
  m->ctx = ctx;
  m->prompt = prompt;
  m->awaiting_action = false;
  m->acc = 0;
  if (delay_ms > 0) {
    m->shown = 0;
    m->pending_prompt = prompt;
  } else {
    m->shown = (int)strlen(m->text);
    m->pending_prompt = false;
    msg_done(m);
  }
}

void mui_msg_update(mui_msg* m, float dt_ms) {
  int len = (int)strlen(m->text);
  if (m->shown >= len || m->delay_ms <= 0) return;
  m->acc += dt_ms;
  while (m->acc >= m->delay_ms && m->shown < len) {
    m->acc -= m->delay_ms;
    /* advance one UTF-8 character */
    m->shown++;
    while (m->shown < len && ((unsigned char)m->text[m->shown] & 0xC0) == 0x80) m->shown++;
  }
  if (m->shown >= len) msg_done(m);
}

bool mui_msg_busy(const mui_msg* m) {
  return m->pending_prompt || m->awaiting_action || (m->delay_ms > 0 && m->shown < (int)strlen(m->text));
}

bool mui_msg_input(mui_msg* m, mui_button b) {
  if (m->pending_prompt) return true; /* upstream ignores input until the prompt shows */
  if (!m->awaiting_action) return false;
  if (b != MB_ACTION && b != MB_CANCEL) return true;
  mui_sfx("ui/select");
  m->awaiting_action = false;
  mui_cb cb = m->cb;
  m->cb = NULL;
  if (cb) cb(m->ctx);
  return true;
}

int mui_msg_lines(const mui_msg* m) {
  int lines = 1;
  for (const char* p = m->text; *p; p++) lines += *p == '\n';
  return lines;
}

void mui_msg_draw(const mui_msg* m, mui_style s, float x, float y) {
  char buf[512];
  int n = m->shown < (int)sizeof buf - 1 ? m->shown : (int)sizeof buf - 1;
  memcpy(buf, m->text, n);
  buf[n] = 0;
  mui_text(s, x, y, buf);
  if (m->awaiting_action) {
    /* MessageUiHandler.showPrompt: after the last line, +2 px; 18 px per line;
     * the "prompt" animation cycles frames 1-4 */
    const char* last = strrchr(buf, '\n');
    last = last ? last + 1 : buf;
    int lines = 1;
    for (const char* p = buf; *p; p++) lines += *p == '\n';
    static const char* frames[4] = {"1", "2", "3", "4"};
    int fi = (int)(mui_now() / 150) % 4;
    mui_image("ui/prompt", frames[fi], x + mui_text_width(s, last) + 2, y + (lines - 1) * 18 + 2, 0, 0);
  }
}

/* ------------------------------------------------------------------ confirm */

/* base-option-select-ui-handler.ts with ConfirmUiHandler's two options:
 * window anchored bottom-right at (319, 180 - yOffset 48), height
 * (2 + 1) * 16 - 2, width = widest label + 23, text 15 px in, 16 px per line. */
void mui_confirm_show(mui_confirm* c, mui_cb yes, mui_cb no, void* ctx) {
  c->active = true;
  c->cursor = 0;
  c->yes = yes;
  c->no = no;
  c->ctx = ctx;
  c->x = MUI_W - 1;
  c->y = MUI_H - 48;
}

bool mui_confirm_input(mui_confirm* c, mui_button b) {
  if (!c->active) return false;
  switch (b) {
    case MB_UP:
    case MB_DOWN:
      c->cursor ^= 1;
      mui_sfx("ui/select");
      return true;
    case MB_ACTION:
    case MB_CANCEL: {
      bool yes = b == MB_ACTION && c->cursor == 0;
      c->active = false;
      mui_sfx("ui/select");
      mui_cb cb = yes ? c->yes : c->no;
      if (cb) cb(c->ctx);
      return true;
    }
    default:
      return true;
  }
}

bool mui_confirm_touch(mui_confirm* c, float x, float y) {
  if (!c->active) return false;
  int w = mui_text_width(TS_WINDOW, mui_t("menu:yes")), w2 = mui_text_width(TS_WINDOW, mui_t("menu:no"));
  if (w2 > w) w = w2;
  const float bw = w + 23, bh = 3 * 16 - 2, left = c->x - bw, top = c->y - bh;
  if (x < left || x >= c->x || y < top + 7 || y >= top + 7 + 2 * 16) return false;
  c->cursor = (int)((y - top - 7) / 16);
  return mui_confirm_input(c, MB_ACTION);
}

void mui_confirm_draw(const mui_confirm* c) {
  if (!c->active) return;
  const char* yes = mui_t("menu:yes");
  const char* no = mui_t("menu:no");
  int w = mui_text_width(TS_WINDOW, yes);
  int w2 = mui_text_width(TS_WINDOW, no);
  if (w2 > w) w = w2;
  float bw = w + 23, bh = 3 * 16 - 2;
  float left = c->x - bw, top = c->y - bh;
  mui_window(left, top, bw, bh);
  mui_text(TS_WINDOW, left + 15, top + 7, yes);
  mui_text(TS_WINDOW, left + 15, top + 7 + 16, no);
  mui_image("ui/cursor", NULL, left + 10, top + 15 + c->cursor * 16, 0.5f, 0.5f);
}

void mui_init(void) {
  g_now = 0;
  g_now_frac = 0;
}
