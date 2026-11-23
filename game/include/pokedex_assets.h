/*
 * pokedex_assets: the species icons and type labels the Pokedex screen (pokedex_screen.h) takes from the menus layer.
 *
 * Its own file for the reason party_icon.c is: menus.h and ui_text.h are not included together. Everything here only draws.
 */
#ifndef GM_POKEDEX_ASSETS_H
#define GM_POKEDEX_ASSETS_H

#include "rr.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A species' party icon with its top left corner at (x, y), scaled from there, tinted. The base form, female = false, not shiny. A missing icon
 * draws nothing. */
void pxa_icon(int species, float x, float y, float scale, rr_color tint);
/* The label of a PokemonType (the "types" atlas frame, 32 x 14) with its top left corner at (x, y). Nothing for a type that has none. */
void pxa_type(int type, float x, float y);

#ifdef __cplusplus
}
#endif
#endif
