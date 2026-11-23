/*
 * pokedex_model: upstream's Pokedex list (src/ui/handlers/pokedex-ui-handler.ts) as plain data: the grid of species with its cursor and scrolling,
 * what the icon of each cell shows (caught, seen or unseen), and the info panel for the species under the cursor. No drawing and no i18n: the screen
 * (pokedex_screen.c) looks the names up.
 *
 * What is here: the list as the handler opens it (every species the dex lists, default filters, sorted by number), the 9 x 9 visible grid (81 cells of
 * 9 columns, scrolling a row at a time) and processInput's moves in the grid, the icon tints (updateScroll), and the panel's number, name and types
 * (setSpecies / setSpeciesDetails). What is not (later tasks): the filter bar and the text filters (a press that upstream answers by entering one of
 * them is reported as such and changes nothing else), the form tray, the species page (A), the sprite, the decorations (candy, shiny stars, ...)
 * and the form line under the name.
 *
 * Starter preferences: the panel's form is the one the starter select stores for a starter (a form preference, when valid), else the first caught form.
 * Upstream sanitizes a stored preference only for the species its 81 containers hold when it opens; here a preference counts when it is valid.
 */
#ifndef GM_POKEDEX_MODEL_H
#define GM_POKEDEX_MODEL_H

#include <stdbool.h>
#include <stdint.h>

#include "gc/system.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PDX_COLS 9
#define PDX_ROWS 9
#define PDX_CELLS (PDX_COLS * PDX_ROWS)
#define PDX_MAX_SPECIES 1200

typedef enum { PDX_UP, PDX_DOWN, PDX_LEFT, PDX_RIGHT } pdx_button; /* the order of upstream's Button enum */
typedef enum { PDX_GRID, PDX_FILTER_BAR, PDX_FILTER_TEXT } pdx_mode; /* where upstream's handler is after a press */

/* The filter bar's columns (upstream's DropDownColumn) as plain data, but biome and the text filters (name, moves, abilities): a tri-state or radial
 * option is one of these, a multiple choice a set of bits (nothing set counts as everything, as upstream's HYBRID drop-down answers). */
typedef enum { PDF_OFF, PDF_ON, PDF_EXCLUDE, PDF_ONE, PDF_TWO, PDF_UNLOCKABLE } pdf_state;
typedef enum { PDF_SORT_NUMBER, PDF_SORT_COST, PDF_SORT_CANDY, PDF_SORT_IV, PDF_SORT_NAME, PDF_SORT_CAUGHT, PDF_SORT_HATCHED, PDF_SORT_COUNT } pdf_sort;
enum { PDF_C_SHINY3 = 1, PDF_C_SHINY2 = 2, PDF_C_SHINY1 = 4, PDF_C_NORMAL = 8, PDF_C_UNCAUGHT = 16, PDF_C_ALL = 31 };
#define PDF_GENS_ALL 0x1ffu    /* bit g - 1: generation g */
#define PDF_TYPES_ALL 0x3ffffu /* bit t: PokemonType t (normal 0 .. fairy 17) */
typedef struct {
  uint16_t gens;
  uint32_t types;
  uint8_t caught;
  uint8_t passive;   /* PDF_OFF, ON (unlocked), UNLOCKABLE, EXCLUDE (locked, and it could be) */
  uint8_t reduction; /* PDF_OFF, ON (any), ONE, TWO, UNLOCKABLE, EXCLUDE */
  uint8_t starter, favorite, win, hidden_ability, seen, encountered, egg, pokerus; /* PDF_OFF, ON, EXCLUDE */
  uint8_t sort;      /* pdf_sort */
  bool descending;
} pdf_filter;
/* Upstream's defaults: everything on, sorted by number. */
void pdf_reset(pdf_filter* f);
/* How many columns differ from the default (the sort counts when it is not the default). */
int pdf_active(const pdf_filter* f);

typedef struct {
  const gc_system* sys;
  uint16_t ids[PDX_MAX_SPECIES]; /* the list, by species number */
  int n;
  /* the species each of the 81 containers holds: the visible ones follow the list, the others keep what they held (upstream's stale ones) */
  uint16_t cell_species[PDX_CELLS];
  int cursor; /* 0..80, relative to the first visible row */
  int scroll; /* rows scrolled off the top */
  pdf_filter filter; /* what the list was last made with */
  /* the two things the profile does not hold: a species' name (the name sort; NULL: sorted by number) and whether a starter has Pokerus today
   * (NULL: none has). Set after pdx_open (it clears them), before pdx_apply_filter. */
  const char* (*name)(uint16_t species);
  bool (*pokerus)(uint16_t species);
} pdx_state;

/* show(): the list with the default filters, the cursor on the first species, at the top. */
void pdx_open(pdx_state* s, const gc_system* sys);
/* The list again with `filter`: the dex's species that fit it, in its sort order; updateStarters. The scroll goes back to the top, the cursor stays where it
 * was (upstream: the cursor cell then shows whatever species it held); every visible cell takes its species. */
void pdx_apply_filter(pdx_state* s, const pdf_filter* filter);
/* Does one species fit the filter (everything but the sort)? */
bool pdf_fits(const pdx_state* s, const pdf_filter* f, uint16_t species);

/* processInput in the grid. Returns upstream's `success` (the select sound plays); *mode is where upstream ends up (PDX_GRID unless the press
 * entered the filter bar or the filter text: the state is then unchanged, the caller stays in the grid). */
bool pdx_input(pdx_state* s, pdx_button b, pdx_mode* mode);

/* A tap on a cell (the app's own, upstream has no touch): the cursor goes there when the cell shows a species. True when it did. */
bool pdx_select_cell(pdx_state* s, int cell);

/* Is cell `cell` showing a species (the list reaches it)? */
bool pdx_cell_visible(const pdx_state* s, int cell);
/* The icon's tint for a visible cell: 'c' clear (caught: caught & the starter's caught & what it can unlock), 'g' grey (seen), 'b' black (not seen). */
char pdx_cell_tint(const pdx_state* s, int cell);
/* speciesDataRegistry.getStarter */
uint16_t pdx_starter_of(uint16_t species);
/* upstream's isSeen(species, dexEntry): its own entry has seen attributes, or its starter is caught */
bool pdx_is_seen(const gc_system* sys, uint16_t species);
/* the dex attributes it counts as caught with: own caught & the starter's caught & getFullUnlocksData */
uint64_t pdx_caught_attr(const gc_system* sys, uint16_t species);

typedef struct {
  uint16_t species;  /* the one under the cursor */
  int dex_number;    /* getDexNumber: species id % 2000 */
  bool name_shown;   /* its own entry is seen or caught (else the screen writes "???") */
  int type1, type2;  /* PokemonType, -1 = hidden */
} pdx_panel;
void pdx_panel_of(const pdx_state* s, pdx_panel* out);

#ifdef __cplusplus
}
#endif
#endif
