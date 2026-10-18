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

#ifdef __cplusplus
}
#endif
#endif
