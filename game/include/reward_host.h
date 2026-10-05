/*
 * reward_host: puts the reward / shop screen (menus/shop_ui.c) and the next-biome map (menus/biome_map.c) on the UI stack.
 *
 * Those two menus draw and take input through their own functions (shop_ui_draw, biome_map_input...) and need a ui_screen
 * to be called from, the way party_screen.c hosts the party and summary screens. Only one is open at a time.
 *   shop:  the bottom screen, as the 320 x 180 canvas upstream's modifier select uses (the battle stays on the top screen)
 *   biome: the top screen's canvas (menus/test/menutest.c draws it there)
 */
#ifndef GM_REWARD_HOST_H
#define GM_REWARD_HOST_H

#include "menus.h"

#ifdef __cplusplus
extern "C" {
#endif

void rh_open_shop(const shop_ui_args* args, const shop_ui_events* ev);
void rh_open_biome_map(const biome_map_args* args, const biome_map_events* ev);
/* Closes whichever is open and takes it off the stack. */
void rh_close(void);

#ifdef __cplusplus
}
#endif
#endif
