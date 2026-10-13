/*
 * ui_widgets: the building blocks upstream's screens share.
 *   img_*       images and atlases from gfx.pak, cached by key, drawn with Phaser's origin rules
 *   ui_window   upstream's addWindow (src/ui/ui-theme.ts): the 9-slice window frame
 *   msgbox      upstream's MessageUiHandler text: typing, "$" pages, the prompt arrow
 *   optlist     upstream's OptionSelectUiHandler: a window of options with a cursor
 *   confirm     upstream's ConfirmUiHandler (Yes / No)
 * Everything draws in the current ui_space (see ui.h) with upstream's coordinates.
 */
#ifndef GM_UI_WIDGETS_H
#define GM_UI_WIDGETS_H

#include <stdbool.h>

#include "rr.h"
#include "ui.h"
#include "ui_text.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- images ----
 * Keys are gfx.pak keys: upstream asset paths without ".png", e.g. "images/ui/cursor",
 * "images/pokemon/back/257", "images/arenas/plains_bg". Loaded on first use and cached
 * until dropped. A missing image is remembered as missing (no retry) and draws nothing. */
rr_atlas* img_get(const char* key);
bool img_has(const char* key);
void img_drop(const char* key);
void img_drop_prefix(const char* prefix); /* e.g. "images/pokemon/" between battles */
/* The frame named `frame` (NULL or "" = the first frame / the whole image). */
const rr_frame* img_frame(const char* key, const char* frame);
/* Frame i in upstream's animation order: frames named "0001.png", "0002.png"... sorted by
 * name (Phaser generateFrameNames), or atlas order when the names aren't numbers. */
const rr_frame* img_anim_frame(const char* key, int i);
int img_frame_count(const char* key);
/* Draws a frame like a Phaser image: (x, y) is where the origin point lands; origin is a
 * fraction of the untrimmed frame (Phaser's default for images is 0.5, 0.5). */
void img_draw(const char* key, const char* frame, float x, float y, float ox, float oy);
void img_draw_opts(const char* key, const char* frame, float x, float y, const rr_draw_opts* o);
/* Frame size (untrimmed), 0 when missing. */
int img_w(const char* key, const char* frame);
int img_h(const char* key, const char* frame);
/* Images loaded, and their GPU bytes. */
int img_count(void);
size_t img_bytes(void);

/* ---- windows ---- */
/* uiWindowStyle: upstream's default is 1 (UiWindowStyle.RED_ORANGE, red edges); the screenshots the project is matched against
 * (lorenzo, 2026-10-12) use 5 (MEDIUM_GRAY, the grey bevelled windows). main.c hands this to the menus (mui_set_window_style). */
#define GM_DEFAULT_WINDOW_STYLE 5
typedef enum { WIN_NORMAL, WIN_THIN, WIN_XTHIN } win_variant;
/* Upstream's addWindow(x, y, w, h): window_<style><variant> as a 9-slice with 8 px borders,
 * origin (0,0). ui_window_style() is upstream's uiWindowStyle setting (1..5, default GM_DEFAULT_WINDOW_STYLE). */
void ui_window(float x, float y, float w, float h, win_variant v);
int ui_window_style(void);
void ui_set_window_style(int style);
/* The cursor arrow (images/ui/cursor) with upstream's image origin (0.5, 0.5). */
void ui_cursor(float x, float y);

/* ---- message box ----
 * The text part of MessageUiHandler.showText: characters appear one every `delay` ms
 * (upstream default 20), "$" splits pages, and with a prompt the arrow blinks after the last
 * character until ACTION or CANCEL. @d{frames} pauses, @s{sound} plays a sound. The owner draws
 * it where its message text object sits and forwards input to msgbox_input. */
typedef struct {
  char text[512];     /* current page, already wrapped */
  char rest[1024];    /* pages still to show after this one */
  int shown;          /* characters shown */
  int total;          /* characters on this page */
  float acc;          /* ms towards the next character */
  float delay;        /* ms per character; 0 = all at once */
  float pause;        /* ms left of an @d pause */
  bool prompt;        /* wait for a button after the text */
  bool waiting;       /* the prompt is showing */
  bool done;          /* finished (and prompt answered, if any) */
  float wait_ms;      /* callbackDelay: after the text without a prompt, wait this long */
  float t;            /* time since the page finished, for the prompt animation */
  text_style style;
  text_opts opts;     /* wrap width and max lines of the text object */
  /* @d / @s markers: character index -> value */
  short mark_at[8];
  short mark_val[8];
  char mark_kind[8];
  char mark_sound[8][24];
  int n_marks;
} msgbox;

void msgbox_init(msgbox* m, text_style style, float wrap_w, int max_lines);
/* Starts showing text. delay < 0 = upstream's default 20 ms. */
void msgbox_show(msgbox* m, const char* text, float delay, bool prompt);
void msgbox_clear(msgbox* m);
void msgbox_update(msgbox* m, float dt);
/* ACTION / CANCEL answers the prompt (typing is not skipped, as upstream). True when used. */
bool msgbox_input(msgbox* m, ui_button b);
/* Draws the text at (x, y) and, while waiting, the prompt arrow after the last line. */
void msgbox_draw(const msgbox* m, float x, float y);
bool msgbox_done(const msgbox* m);
bool msgbox_typing(const msgbox* m);

/* ---- option list (OptionSelectUiHandler) ----
 * The window sits in the bottom-right corner of the ui container (x = 319, y = -1, origin
 * (1,1)) unless moved; rows are 16 px apart, at most max_rows visible, with scrolling. */
#define OPTLIST_MAX 32
typedef struct {
  const char* labels[OPTLIST_MAX]; /* pointers the owner keeps alive */
  text_style colors[OPTLIST_MAX];  /* TS_NONE = TS_WINDOW */
  bool disabled[OPTLIST_MAX];
  int n, cursor, scroll, max_rows;
  float x, y;         /* bottom-right corner, ui container coordinates */
  float w;            /* computed from the labels */
  int chosen;         /* -1 while open; the index picked, or -2 when cancelled */
  bool no_cancel;     /* CANCEL does nothing */
} optlist;

void optlist_init(optlist* l, const char* const* labels, int n);
bool optlist_input(optlist* l, ui_button b);
void optlist_draw(const optlist* l);

/* A ready-made overlay screen around an optlist: push it, and on_done gets the index (or -2
 * when cancelled) after it pops itself. */
void ui_push_options(const char* const* labels, int n, bool can_cancel, void (*on_done)(int idx, void* arg),
                     void* arg);
/* Upstream's ConfirmUiHandler: Yes / No over the bottom-right corner. on_done(1) for yes,
 * 0 for no or cancel. */
void ui_push_confirm(void (*on_done)(int yes, void* arg), void* arg);

#ifdef __cplusplus
}
#endif
#endif
