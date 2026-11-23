/*
 * pokedex_screen: upstream's Pokedex list (src/ui/handlers/pokedex-ui-handler.ts), from the title's "Pokédex" entry.
 *
 * The bottom screen is upstream's layout: the 9 x 9 grid of species icons on the right (a caught species in colour, a seen one grey, an unseen one
 * black), the species under the cursor on the left (its number, its name or "???", its two types); D-pad moves the cursor (upstream's wrapping,
 * scrolling a row at a time), B leaves. What is shown is pokedex_model.h. The top screen has only the background.
 *
 * Not ported yet (later tasks, as in the model): the filter bar and text filters (UP in the first row does nothing), the form tray, the species
 * page, the big sprite, the decorations on the icons, the form line. The icons are the base form's: upstream shows the form the species opens with.
 * A tap on an icon moves the cursor there (the app's own).
 */
#ifndef GM_POKEDEX_SCREEN_H
#define GM_POKEDEX_SCREEN_H

#include "gc/system.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  const gc_system* sys;      /* the profile; read only */
  void (*on_exit)(void* arg); /* B: back to where it came from */
  void* arg;
} pokedex_args;

/* Makes the Pokedex the screen (ui_set, with a fade). */
void pokedex_screen_open(const pokedex_args* a);

#ifdef __cplusplus
}
#endif
#endif
