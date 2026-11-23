/*
 * pokedex_filter_menu: the Pokedex's filter bar (upstream's FilterBar: generation, type, caught, unlocks, misc and sort drop-downs) as a list menu, the way
 * the starter select shows its own (starter_host.c): one row per column, a row that steps through a tri-state or opens a list of check boxes. What it
 * changes is a pdf_filter (pokedex_model.h); the owner applies it after every change.
 *
 * Not here (later lanes): the biome column and the text filters (name, moves, abilities); the Favourite row (the app has no favourites).
 */
#ifndef GM_POKEDEX_FILTER_MENU_H
#define GM_POKEDEX_FILTER_MENU_H

#include "pokedex_model.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  pdf_filter* filter;          /* changed in place; must outlive the menu */
  void (*on_change)(void* arg); /* after every change: the owner rebuilds its list from *filter */
  void* arg;
} pfm_args;

/* Pushes the menu (an overlay on the current screen); it closes itself on Done or cancel. */
void pfm_open(const pfm_args* a);
/* "Filter", or "Filter (3)" with the number of columns that differ from the default. */
const char* pfm_button_label(const pdf_filter* f, char* out, unsigned n);

#ifdef __cplusplus
}
#endif
#endif
