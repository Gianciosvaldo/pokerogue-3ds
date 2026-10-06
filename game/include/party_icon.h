/*
 * party_icon: a party member's icon (upstream's addPokemonIcon) for screens that are not menus' own: the save slots and
 * the result screens draw from a run summary (run_saves.h), not from the menus' display structs.
 */
#ifndef GM_PARTY_ICON_H
#define GM_PARTY_ICON_H

#include "run_saves.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Draws the icon with its horizontal centre at x and its top at y, scaled around that point (the slot list uses 0.75), in
 * the current drawing space. A missing icon draws nothing. */
void pi_draw(const rsv_mon* m, float x, float y, float scale);

#ifdef __cplusplus
}
#endif
#endif
