/*
 * party_screen: game/menus' party and summary screens (party.c, summary.c) as a ui_screen on the
 * bottom screen, over the battle, filled from battle-core Pokemon.
 */
#ifndef GM_PARTY_SCREEN_H
#define GM_PARTY_SCREEN_H

#include "pr/battle.h"

/* (menus.h's party_mode and PartyOption values, so callers needn't include menus.h, whose text
 * style names clash with ui_text.h's) */
typedef enum { PS_SWITCH = 0, PS_FAINT_SWITCH = 1 } ps_mode;
enum { PS_CANCEL = -1, PS_SEND_OUT = 0 };

void ps_init(void); /* once at boot, after ui_init */

/* Opens the party screen for the player's party. on_pick gets the slot and the PartyOption
 * (PS_SEND_OUT), or slot 6 / PS_CANCEL when the player backs out (never in FAINT_SWITCH). The
 * screen stays open until ps_close. */
void ps_open(const pr_battle* bt, ps_mode mode, void (*on_pick)(int slot, int option, void* arg), void* arg);
void ps_close(void);

#endif
