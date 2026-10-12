/*
 * radar_assets: the sprites, icons and lookups the Nemesis Radar (radar_screen.h) takes from the menus layer and battle-core's tables.
 *
 * Its own file for the reason party_icon.c is: menus.h and ui_text.h are not included together, and the Radar draws with the game
 * framework's text API. Everything here only reads: no table is changed, nothing is written, no random number is drawn.
 */
#ifndef GM_RADAR_ASSETS_H
#define GM_RADAR_ASSETS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "rr.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The types of a species form (PokemonType, type2 -1 for none) from battle-core's tables. False when the species or form is not in
 * them: the type is then unknown and nothing is tinted. */
bool ra_species_types(int species, int form, int* type1, int* type2);

/* A party icon with its centre at (x, y), scaled around it, in the current drawing space. A missing icon draws nothing.
 * tint RR_RGBA(255, 255, 255, a) draws it at alpha a. */
void ra_icon(int species, int form, int gender, bool shiny, float x, float y, float scale, rr_color tint);

/* The front battle sprite of one Pokemon at a time (the previous one is dropped). False: no sprite for it in the packs. */
bool ra_sprite_load(int species, int form, int gender, bool shiny);
void ra_sprite_free(void);
/* Draws the loaded sprite's frame `ms` into its animation, centred on (x, y), shrunk to fit a box `max_h` px tall when it is taller. */
bool ra_sprite_draw(float x, float y, uint32_t ms, float max_h);

/* Names the menus layer owns the tables for. */
const char* ra_move_name(int move);
/* "Adamant" (nature:adamant); "" for a value that is not a nature. */
const char* ra_nature_name(int nature, char* out, size_t n);

#ifdef __cplusplus
}
#endif
#endif
