/*
 * pokedex_screen: upstream's Pokedex list (src/ui/handlers/pokedex-ui-handler.ts), from the title's "Pokédex" entry.
 *
 * The bottom screen is upstream's layout: the 9 x 9 grid of species icons on the right (a caught species in colour, a seen one grey, an unseen one
 * black), the species under the cursor on the left (its number, its name or "???", its two types); D-pad moves the cursor (upstream's wrapping,
 * scrolling a row at a time), B leaves. What is shown is pokedex_model.h. The top screen has only the background.
 *
 * The filter bar is a menu (pokedex_filter_menu.h), opened by UP in the first row as upstream does, or by a tap on the "Filter" button: generation, type,
 * caught, passive, cost reduction, starter, won, hidden ability, seen, encountered, egg, Pokerus and the sort. After a change the list is rebuilt and the
 * cursor goes to its first species (upstream keeps the cursor's place, which can then be an empty cell).
 *
 * Not ported yet (later tasks, as in the model): the biome column and the text filters (LEFT / RIGHT at the edges do nothing), the form tray, the species
 * page, the big sprite, the decorations on the icons, the form line. The icons are the base form's: upstream shows the form the species opens with.
 * A tap on an icon moves the cursor there (the app's own).
 */
#ifndef GM_POKEDEX_SCREEN_H
#define GM_POKEDEX_SCREEN_H

#include <stdbool.h>
#include <stdint.h>

#include "gc/system.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  const gc_system* sys;      /* the profile; read only */
  bool (*pokerus)(uint16_t species); /* has this starter Pokerus today (the Pokerus filter); NULL: none */
  void (*on_exit)(void* arg); /* B: back to where it came from */
  void* arg;
} pokedex_args;

/* Makes the Pokedex the screen (ui_set, with a fade). */
void pokedex_screen_open(const pokedex_args* a);

#ifdef __cplusplus
}
#endif
#endif
