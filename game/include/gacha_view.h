/*
 * gacha_view: how the capsule machine's menu looks (drawing only; the menu logic is gacha_screen.c, the purchase logic gacha_host.c).
 *
 * It follows upstream's EggGachaUiHandler on the top screen (the field canvas): the three machines side by side with the chosen one
 * centred, each with its art, eggs, glass, knob, hatch and "UP!" plate (the legendary one with the species of the day and the time
 * left), the four voucher counters in the top right, the option list and the message box. The bottom screen shows the same options in
 * detail (icon, eggs, price, how many are held) in the grey 8 px style the other 3DS screens use.
 *
 * Plain C types only, so the menu (which uses the game framework's headers) and this file (which uses the menus' layer) never include
 * each other's headers. The words arrive composed: the view places them, it never makes a rule or a sentence of its own.
 */
#ifndef GM_GACHA_VIEW_H
#define GM_GACHA_VIEW_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GV_MACHINES 3
#define GV_OPTIONS 5
#define GV_VOUCHERS 4
#define GV_MSG_LINES 6
#define GV_LIST_ROWS 5

typedef enum { GV_MENU, GV_CONFIRM, GV_MESSAGE } gv_mode;

/* The line of buttons at the foot of a bottom screen, one text each: they are drawn one after the other (GV_HINT_GAP apart) so that a tap on
 * one can be the button it names (gv_hint_at). */
#define GV_HINT_MAX 4
#define GV_HINT_LEN 36
#define GV_HINT_GAP 14.0f
#define GV_HINT_X 8.0f
typedef struct {
  char text[GV_HINT_MAX][GV_HINT_LEN];
  int n;
} gv_hints;
/* The hint a tap at canvas x is on (the gap after a text belongs to it; right of the last: none), or -1. */
int gv_hint_at(const gv_hints* h, float x);

typedef struct {
  char eggs[16];    /* "10 eggs" */
  char price[40];   /* "10 Regular vouchers" */
  char have[16];    /* "have 12" */
  char short_label[16]; /* the top screen's option list: "10 Pulls" */
  uint8_t voucher;  /* 0 regular .. 3 golden (picks the icon) */
  bool can_buy;
} gv_option;

typedef struct {
  float slide;            /* which machine is centred (0..2, fractional while it moves) */
  int machine;            /* the chosen machine */
  int sel;                /* the chosen option */
  gv_mode mode;
  int yes;                /* GV_CONFIRM: 1 = Yes */
  char machine_name[32];
  char machine_note[64];
  char voucher_count[GV_VOUCHERS][12];
  char egg_count[24];     /* "Eggs 7 / 99" */
  gv_option opt[GV_OPTIONS];
  char refusal[64];       /* why the chosen option cannot be bought ("" when it can) */
  gv_hints hints;         /* the buttons */
  char lines[GV_MSG_LINES][48]; /* the question or the message of GV_CONFIRM / GV_MESSAGE / a locked machine, one line each */
  int n_lines;
  char top_message[64];   /* the message box on the top screen */
  uint16_t featured;      /* the legendary machine's species of the day (0: none) */
  char countdown[12];     /* time left of the featured species, "hh:mm" */
  char up_label[GV_MACHINES][16]; /* the plate of each machine: "Move UP!" "UP!" "Shiny UP!" */
} gv_state;

/* ---- where things are on the bottom screens: the drawing and the taps both read these. Canvas pixels: the screen's physical y minus
 * GV_CANVAS_Y (the canvas starts 30 px down). ---- */
#define GV_CANVAS_Y 30.0f
#define GV_W 320.0f
#define GV_H 180.0f
/* the top screen is 400 x 240: the 320 x 180 canvas has a band of this many px at each side and above and below it, which the machine's
 * screens draw on too (the background, the machines, the windows at the screen's edges) */
#define GV_SIDE 40.0f
#define GV_BAND 30.0f

/* the machine menu: the option list, the buttons under it, the question over it */
#define GV_OPT_Y 30.0f    /* the list's window */
#define GV_OPT_ROW 24.0f
#define GV_DLG_Y 24.0f
static inline float gv_opt_row_y(int i) { return GV_OPT_Y + 4.0f + GV_OPT_ROW * (float)i; }          /* a row's text */
static inline float gv_menu_hints_y(void) { return GV_OPT_Y + GV_OPT_ROW * (float)GV_OPTIONS + 12.0f; } /* the window of the buttons */
static inline float gv_dlg_h(int n_lines, bool confirm) { return 20.0f + 11.0f * (float)n_lines + (confirm ? 40.0f : 0.0f); }
static inline float gv_yes_y(int n_lines) { return GV_DLG_Y + 14.0f + 11.0f * (float)n_lines; }       /* Yes is drawn 4 px under it, No 20 */
/* the option row a tap is on (the row's focus frame, whole rows side by side), or -1 */
static inline int gv_option_at(float x, float y) {
  if (x < 8.0f || x >= GV_W - 8.0f) return -1;
  for (int i = 0; i < GV_OPTIONS; i++) {
    const float top = gv_opt_row_y(i) - 1.0f;
    if (y >= top && y < top + GV_OPT_ROW) return i;
  }
  return -1;
}
/* the answer a tap on the question is on: 1 Yes, 0 No, -1 neither (two rows of 16 px, the cursor in the middle of each) */
static inline int gv_confirm_at(int n_lines, float x, float y) {
  const float yy = gv_yes_y(n_lines);
  if (x < 12.0f || x >= GV_W - 12.0f) return -1;
  if (y >= yy + 1.0f && y < yy + 17.0f) return 1;
  if (y >= yy + 17.0f && y < yy + 33.0f) return 0;
  return -1;
}

/* the reveal: the list of what came out, the notes under it, the knob before it */
#define GV_REV_LIST_Y 26.0f
#define GV_REV_ROW 18.0f
#define GV_REV_HINTS_Y (GV_H - 20.0f)
static inline float gv_rev_row_y(int i) { return 30.0f + GV_REV_ROW * (float)i; }
static inline float gv_rev_note_y(void) { return GV_REV_LIST_Y + GV_REV_ROW * (float)GV_LIST_ROWS + 10.0f; } /* "more above" and "more below" */
static inline int gv_rev_row_at(int n_rows, float x, float y) {
  if (x < 4.0f || x >= GV_W - 4.0f) return -1;
  for (int i = 0; i < n_rows && i < GV_LIST_ROWS; i++) {
    const float top = gv_rev_row_y(i) - 1.0f;
    if (y >= top && y < top + GV_REV_ROW) return i;
  }
  return -1;
}
#define GV_KNOB_X 160.0f
#define GV_KNOB_Y 112.0f
#define GV_KNOB_R 40.0f  /* the knob is drawn 3x as big as its sprite; a tap within this far of its centre (a square) is on it */
static inline bool gv_knob_at(float x, float y) {
  return x >= GV_KNOB_X - GV_KNOB_R && x < GV_KNOB_X + GV_KNOB_R && y >= GV_KNOB_Y - GV_KNOB_R && y < GV_KNOB_Y + GV_KNOB_R;
}

/* Draw one screen's worth. Both draw in the whole 320 x 180 canvas of their screen (the caller sets the origin) and nothing else. */
void gv_draw_top(const gv_state* s);
void gv_draw_bottom(const gv_state* s);

/* ---- the reveal after a pull (capsule_screen.c): the eggs drop, settle and open; the bottom screen lists what came out ---- */

#define GV_CAP_MAX 25
#define GV_TIERS 4

typedef enum { GV_REVEAL_IDLE, GV_REVEAL_RUNNING, GV_REVEAL_SETTLED } gv_reveal_stage;

typedef struct {
  float x, y;       /* centre in the field's pixels (320 x 180) */
  float open;       /* 0 closed .. 1 open */
  uint8_t phase;    /* 0 still in the chute (not drawn), 1 falling, 2 landed */
  uint8_t tier;     /* 0 .. GV_TIERS - 1 */
  bool picked;      /* the result the list points at */
} gv_egg;

typedef struct {
  gv_reveal_stage stage;
  int n;
  gv_egg egg[GV_CAP_MAX];
  float knob;                 /* turns of the knob */
  int machine;                /* the machine the eggs came out of (0..GV_MACHINES - 1; anything else: none is drawn) */
  int hatch;                  /* its hatch: 1 closed .. 4 open */
  char top_title[48];         /* the window at the top of the field */
  char top_sub[72];
  int n_rows;
  struct {
    char name[40];
    char tier_name[16];
    uint8_t tier;
    bool selected;
  } row[GV_LIST_ROWS];
  char more_above[24], more_below[24];
  char title[40];             /* the header of the bottom screen */
  char big[64], small[72];    /* what the knob screen says */
  char note[2][48];           /* under the list */
  gv_hints hints;
} gv_reveal;

void gv_draw_reveal_top(const gv_reveal* s);
void gv_draw_reveal_bottom(const gv_reveal* s);

#ifdef __cplusplus
}
#endif
#endif
