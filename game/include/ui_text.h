/*
 * ui_text: upstream's text styles (src/ui/text.ts) drawn with the baked fonts from font.pak.
 *
 * Upstream draws text at 96 canvas px and scales it by 1/6. The fonts in font.pak are baked at
 * the logical size (canvas px / 6), and this module places each line where Phaser puts it:
 * the baseline of line i sits at y + (padding.top + ascent + i * (ascent + descent + lineSpacing)) / 6,
 * with ascent/descent as Chromium measures "|MÉqgy" (Phaser's MeasureText), measured once per
 * canvas size and kept in a table. Word wrap is Phaser's basicWordWrap.
 *
 * Positions and wrap widths are logical px (upstream's container coordinates). Text is UTF-8,
 * with '\n' line breaks. Shadows are 1 px (upstream's are 3-5 canvas px, under one logical px).
 */
#ifndef GM_UI_TEXT_H
#define GM_UI_TEXT_H

#include <stdbool.h>

#include "rr.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Upstream's TextStyle enum, same values (src/enums/text-style.ts). */
#ifndef GM_TEXT_STYLE_ENUM
#define GM_TEXT_STYLE_ENUM
typedef enum {
  TS_NONE = 0,
  TS_MESSAGE = 1,
  TS_WINDOW,
  TS_WINDOW_ALT,
  TS_WINDOW_BATTLE_COMMAND,
  TS_BATTLE_INFO,
  TS_PARTY,
  TS_PARTY_RED,
  TS_PARTY_CANCEL_BUTTON,
  TS_INSTRUCTIONS_TEXT,
  TS_MOVE_LABEL,
  TS_SUMMARY,
  TS_SUMMARY_DEX_NUM,
  TS_SUMMARY_DEX_NUM_GOLD,
  TS_SUMMARY_ALT,
  TS_SUMMARY_HEADER,
  TS_SUMMARY_RED,
  TS_SUMMARY_BLUE,
  TS_SUMMARY_PINK,
  TS_SUMMARY_GOLD,
  TS_SUMMARY_GRAY,
  TS_SUMMARY_GREEN,
  TS_SUMMARY_STATS,
  TS_SUMMARY_STATS_BLUE,
  TS_SUMMARY_STATS_PINK,
  TS_SUMMARY_STATS_GOLD,
  TS_LUCK_VALUE,
  TS_STATS_HEXAGON,
  TS_GROWTH_RATE_TYPE,
  TS_MONEY,
  TS_MONEY_WINDOW,
  TS_HEADER_LABEL,
  TS_STATS_LABEL,
  TS_STATS_VALUE,
  TS_SETTINGS_VALUE,
  TS_SETTINGS_LABEL,
  TS_SETTINGS_LABEL_NAVBAR,
  TS_SETTINGS_SELECTED,
  TS_SETTINGS_LOCKED,
  TS_EGG_LIST,
  TS_EGG_SUMMARY_NAME,
  TS_EGG_SUMMARY_DEX,
  TS_STARTER_VALUE_LIMIT,
  TS_TOOLTIP_TITLE,
  TS_TOOLTIP_CONTENT,
  TS_FILTER_BAR_MAIN,
  TS_MOVE_INFO_CONTENT,
  TS_MOVE_PP_FULL,
  TS_MOVE_PP_HALF_FULL,
  TS_MOVE_PP_NEAR_EMPTY,
  TS_MOVE_PP_EMPTY,
  TS_SMALLER_WINDOW_ALT,
  TS_BGM_BAR,
  TS_PERFECT_IV,
  TS_ME_OPTION_DEFAULT,
  TS_ME_OPTION_SPECIAL,
  TS_SHADOW_TEXT,
  TS_COUNT_
} text_style;
#else
typedef int text_style;
#endif

/* Upstream's getTextColor(style, shadow), default (non-legacy) theme. */
rr_color text_color(text_style s, bool shadow);

/* How a text object is laid out. Zero-initialise and set what you need; the defaults are
 * upstream's addTextObject: the style's font size and padding, lineSpacing = 30 * scale
 * (5 canvas px), no wrap, no line limit, left aligned, origin (0,0). */
typedef struct {
  int font_px;          /* canvas px (96 = 16 logical); 0 = the style's */
  float line_spacing;   /* canvas px; < 0 = the default (5; BBCode text uses 10) */
  float wrap_w;         /* logical px; 0 = no wrap (upstream wordWrap.width / 6) */
  int max_lines;        /* 0 = no limit */
  float origin_x, origin_y; /* like setOrigin, of the whole text box */
  int align;            /* 0 left, 1 centre, 2 right (setAlign: lines inside the box) */
  rr_color color, shadow;   /* 0 = the style's colours */
  bool no_shadow;
  int chars;            /* draw only the first N characters (typing effect); < 0 or 0 = all... see TEXT_ALL */
} text_opts;
#define TEXT_ALL (-1)
/* A text_opts with upstream's defaults and chars = TEXT_ALL. */
text_opts text_defaults(void);

/* Draws text as upstream's addTextObject(x, y, s, style) would show it. Returns the width of the
 * widest line in logical px. o may be NULL. */
float text_draw(text_style st, float x, float y, const char* s, const text_opts* o);
/* The same measurements without drawing: width of the widest line, height of the box (as
 * Phaser's displayHeight, padding included), and line count after wrapping. */
float text_width(text_style st, const char* s, const text_opts* o);
float text_height(text_style st, const char* s, const text_opts* o);
int text_lines(text_style st, const char* s, const text_opts* o);
/* Phaser's basicWordWrap: copies s into out (size n) with '\n' where it would break lines. */
void text_wrap(text_style st, const char* s, const text_opts* o, char* out, int n);
/* The distance between two lines' baselines, in logical px (e.g. 14.83 for MESSAGE). */
float text_line_pitch(text_style st, const text_opts* o);
/* Width of a single line with a style's font (no wrap). */
float text_line_width(text_style st, int font_px, const char* s, int nbytes);

/* The rr_font for a family ("emerald", "pkmnems") at a canvas size, loaded on first use. */
rr_font* text_font(const char* family, int font_px);
/* Number of UTF-8 characters in s. */
int text_utf8_len(const char* s);

#ifdef __cplusplus
}
#endif
#endif
