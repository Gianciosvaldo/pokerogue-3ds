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

typedef enum { GV_MENU, GV_CONFIRM, GV_MESSAGE } gv_mode;

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
  char hint[64];          /* the buttons */
  char lines[GV_MSG_LINES][48]; /* the question or the message of GV_CONFIRM / GV_MESSAGE / a locked machine, one line each */
  int n_lines;
  char top_message[64];   /* the message box on the top screen */
  uint16_t featured;      /* the legendary machine's species of the day (0: none) */
  char countdown[12];     /* time left of the featured species, "hh:mm" */
  char up_label[GV_MACHINES][16]; /* the plate of each machine: "Move UP!" "UP!" "Shiny UP!" */
} gv_state;

/* Draw one screen's worth. Both draw in the whole 320 x 180 canvas of their screen (the caller sets the origin) and nothing else. */
void gv_draw_top(const gv_state* s);
void gv_draw_bottom(const gv_state* s);

/* ---- the reveal after a pull (capsule_screen.c): the eggs drop, settle and open; the bottom screen lists what came out ---- */

#define GV_CAP_MAX 25
#define GV_LIST_ROWS 5
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
  char hint[64];
} gv_reveal;

void gv_draw_reveal_top(const gv_reveal* s);
void gv_draw_reveal_bottom(const gv_reveal* s);

#ifdef __cplusplus
}
#endif
#endif
