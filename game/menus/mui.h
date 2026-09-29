/*
 * mui: the small UI layer the menu screens (party, summary, rewards, starter
 * select...) draw with. It covers what upstream's ui/text.ts, ui/ui-theme.ts,
 * MessageUiHandler and i18next give those handlers, on top of rr/pak/snd.
 *
 * It is a stand-in until the game framework (game/, the first screens thread)
 * has its own text, window, string and sound API: the screens call only these
 * functions, so switching them over touches this file and mui.c, nothing else.
 *
 * Coordinates are upstream's 320x180 logical canvas (x right, y down, 0,0 at
 * the top-left). The caller picks the screen and origin with rr_target and
 * rr_set_origin before drawing.
 */
#ifndef GAME_MENUS_MUI_H
#define GAME_MENUS_MUI_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "rr.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MUI_W 320
#define MUI_H 180

/* upstream Button (src/enums/buttons.ts), same order */
typedef enum {
  MB_UP, MB_DOWN, MB_LEFT, MB_RIGHT, MB_SUBMIT, MB_ACTION, MB_CANCEL, MB_MENU, MB_STATS,
  MB_CYCLE_SHINY, MB_CYCLE_FORM, MB_CYCLE_GENDER, MB_CYCLE_ABILITY, MB_CYCLE_NATURE, MB_CYCLE_TERA,
  MB_SPEED_UP, MB_SLOW_DOWN,
} mui_button;

/* upstream TextStyle (src/enums/text-style.ts), same values */
typedef enum {
  TS_MESSAGE = 1, TS_WINDOW, TS_WINDOW_ALT, TS_WINDOW_BATTLE_COMMAND, TS_BATTLE_INFO, TS_PARTY, TS_PARTY_RED,
  TS_PARTY_CANCEL_BUTTON, TS_INSTRUCTIONS_TEXT, TS_MOVE_LABEL, TS_SUMMARY, TS_SUMMARY_DEX_NUM,
  TS_SUMMARY_DEX_NUM_GOLD, TS_SUMMARY_ALT, TS_SUMMARY_HEADER, TS_SUMMARY_RED, TS_SUMMARY_BLUE, TS_SUMMARY_PINK,
  TS_SUMMARY_GOLD, TS_SUMMARY_GRAY, TS_SUMMARY_GREEN, TS_SUMMARY_STATS, TS_SUMMARY_STATS_BLUE,
  TS_SUMMARY_STATS_PINK, TS_SUMMARY_STATS_GOLD, TS_LUCK_VALUE, TS_STATS_HEXAGON, TS_GROWTH_RATE_TYPE, TS_MONEY,
  TS_MONEY_WINDOW, TS_HEADER_LABEL, TS_STATS_LABEL, TS_STATS_VALUE, TS_SETTINGS_VALUE, TS_SETTINGS_LABEL,
  TS_SETTINGS_LABEL_NAVBAR, TS_SETTINGS_SELECTED, TS_SETTINGS_LOCKED, TS_EGG_LIST, TS_EGG_SUMMARY_NAME,
  TS_EGG_SUMMARY_DEX, TS_STARTER_VALUE_LIMIT, TS_TOOLTIP_TITLE, TS_TOOLTIP_CONTENT, TS_FILTER_BAR_MAIN,
  TS_MOVE_INFO_CONTENT, TS_MOVE_PP_FULL, TS_MOVE_PP_HALF_FULL, TS_MOVE_PP_NEAR_EMPTY, TS_MOVE_PP_EMPTY,
  TS_SMALLER_WINDOW_ALT, TS_BGM_BAR, TS_PERFECT_IV, TS_ME_OPTION_DEFAULT, TS_ME_OPTION_SPECIAL, TS_SHADOW_TEXT,
  TS_COUNT_,
  /* not upstream styles: a style with a font size override */
  TSX_WINDOW_ALT_76, /* WINDOW_ALT at fontSize 76px (summary candy and friendship counts) */
} mui_style;

/* lineSpacing in canvas px: addTextObject (scale * 30), addBBCodeTextObject (scale * 60) */
#define MUI_LS_TEXT 5.0f
#define MUI_LS_BBCODE 10.0f

#define MUI_RGB(hex) ((rr_color)(((uint32_t)(hex) << 8) | 0xFF))

/* ---- lifecycle ---- */
void mui_init(void);          /* after rr_init and pak_mount_default */
void mui_tick(float dt_ms);   /* once per frame: advances timers and typing text */
uint32_t mui_now(void);       /* ms of UI time since mui_init */

/* ---- images ----
 * key: upstream texture path under images/ without extension ("ui/party_bg",
 * "pokemon_icons_4", "items"). frame: atlas frame name, or NULL / "" for the
 * first frame. Loaded once and cached until mui_unload_all. */
const rr_frame* mui_frame(const char* key, const char* frame);
/* Draws with a Phaser-style origin (0..1 of the untrimmed frame). Returns false if missing. */
bool mui_image(const char* key, const char* frame, float x, float y, float ox, float oy);
bool mui_image_ex(const char* key, const char* frame, float x, float y, const rr_draw_opts* o);
void mui_unload_all(void);

/* rr_set_origin that mui remembers (for mui_clip): call it instead. */
void mui_set_origin(rr_screen s, int px, int py);
/* Clips drawing to a canvas rectangle (upstream geometry masks) until
 * mui_clip_off. Rectangles only; no nesting. */
void mui_clip(float x, float y, float w, float h);
void mui_clip_off(void);

/* upstream addWindow: 9-slice window_1 (the default red/orange style) */
void mui_window(float x, float y, float w, float h);

/* ---- text ---- */
/* Draws text with the style's font, colour and shadow; x,y is the top-left of
 * the text box (Phaser origin 0,0). Handles "\n". Returns the width. */
int mui_text(mui_style s, float x, float y, const char* utf8);
/* Same with an origin (0..1 of the text box, like setOrigin). */
int mui_text_o(mui_style s, float x, float y, float ox, float oy, const char* utf8);
/* Same with explicit colours (upstream setColor/setShadowColor). */
int mui_text_c(mui_style s, float x, float y, const char* utf8, rr_color c, rr_color shadow);
/* Same with a lineSpacing other than addTextObject's (canvas px, setLineSpacing). */
int mui_text_ls(mui_style s, float x, float y, float ls, const char* utf8, rr_color c, rr_color shadow);
float mui_text_height_ls(mui_style s, float ls, const char* utf8);
int mui_text_width(mui_style s, const char* utf8);
int mui_line_height(mui_style s);  /* line pitch, rounded */
float mui_text_height(mui_style s, const char* utf8); /* Phaser text box height */
rr_color mui_style_color(mui_style s, bool shadow); /* upstream getTextColor */
/* Phaser's wordWrap ({ width }), width in logical px (upstream's / 6). Writes
 * the text with "\n" breaks into out; returns the line count. */
int mui_wrap(mui_style s, const char* in, float width, char* out, size_t n);
/* addBBCodeTextObject: text with upstream's [color=#rrggbb][shadow=#rrggbb]
 * tags (getBBCodeFrag), drawn at x,y (origin 0,0). Returns the widest line. */
int mui_bbtext(mui_style s, float x, float y, const char* bb);
/* getBBCodeFrag(content, style, close) */
char* mui_bbfrag(char* out, size_t n, const char* content, mui_style s, bool close);

/* ---- strings (English locale files in data.pak) ----
 * key: i18next "namespace:key.sub" (e.g. "partyUiHandler:choosePokemon").
 * Returns the key itself when missing, like i18next. The pointer stays valid. */
const char* mui_t(const char* key);
/* i18next.t with {{name}} interpolation: pairs of name, value, then NULL.
 * Writes into out and returns it. */
char* mui_tf(char* out, size_t n, const char* key, ...);
char* mui_vtf(char* out, size_t n, const char* key, va_list ap);

/* ---- sound ---- */
void mui_sfx(const char* key); /* upstream audio key, e.g. "ui/select" */

/* ---- message box text (MessageUiHandler.showText) ----
 * Types text out one character per `delay_ms` (upstream default 20; 0 = at
 * once). When done: with prompt, waits for ACTION/CANCEL (mui_msg_input), then
 * calls cb; without, calls cb straight away. */
typedef void (*mui_cb)(void* ctx);
typedef struct {
  char text[512];
  int shown;              /* bytes revealed */
  float acc;              /* ms accumulated toward the next character */
  int delay_ms;
  bool prompt, pending_prompt, awaiting_action;
  mui_cb cb;
  void* ctx;
} mui_msg;

void mui_msg_show(mui_msg* m, const char* text, int delay_ms, mui_cb cb, void* ctx, bool prompt);
void mui_msg_set(mui_msg* m, const char* text); /* immediately, no callback */
void mui_msg_update(mui_msg* m, float dt_ms);
/* Returns true when the message consumed the button (it was waiting for it). */
bool mui_msg_input(mui_msg* m, mui_button b);
bool mui_msg_busy(const mui_msg* m); /* typing or waiting for a button */
/* Draws the revealed text; with a prompt showing, the prompt arrow after it. */
void mui_msg_draw(const mui_msg* m, mui_style s, float x, float y);
int mui_msg_lines(const mui_msg* m);

/* ---- yes/no box (upstream ConfirmUiHandler, the part the menus use) ---- */
typedef struct {
  bool active;
  int cursor;             /* 0 yes, 1 no */
  mui_cb yes, no;
  void* ctx;
  float x, y;             /* bottom-right corner (upstream places it at the canvas right) */
} mui_confirm;
void mui_confirm_show(mui_confirm* c, mui_cb yes, mui_cb no, void* ctx);
bool mui_confirm_input(mui_confirm* c, mui_button b);
void mui_confirm_draw(const mui_confirm* c);

/* ---- small helpers ---- */
/* Truncates a name with "." until it fits (party slot names). */
void mui_fit_name(mui_style s, char* name, int max_w);

#ifdef __cplusplus
}
#endif
#endif
