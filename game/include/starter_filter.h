/*
 * starter_filter: upstream's starter filter bar and sort (StarterSelectUiHandler.filterStarters, sortStarterSpecies, setupFilterBar at the
 * pinned commit) as plain data, without a screen. The starter screen shows a menu for it (starter_host.c) and applies the result
 * to the list it gets from ss_list.
 *
 * What upstream has and is here: generation (1-9), type (the 18; a species fits when either of its types is on), caught (the shiny
 * tiers and a plain catch), unlocks (passive, cost reduction), misc (won, hidden ability, egg purchasable, Pokerus) and the seven
 * sorts, ascending or descending. A column with all its options on filters nothing.
 *
 * What is not: Uncaught (the list holds caught species only, see ss_list) and Favourite (the app has no favourites). Added beyond
 * upstream, lorenzo's request: a rarity filter (ordinary, sub-legendary, legendary, mythical: the species' `rarity`).
 */
#ifndef GM_STARTER_FILTER_H
#define GM_STARTER_FILTER_H

#include <stdbool.h>
#include <stdint.h>

#include "gc/run.h"
#include "starter.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A tri-state or radial option (upstream's DropDownState). */
typedef enum {
  SSF_OFF,        /* not filtered */
  SSF_ON,         /* has it */
  SSF_EXCLUDE,    /* lacks it */
  SSF_ONE,        /* cost reduction: bought once */
  SSF_TWO,        /* cost reduction: bought twice */
  SSF_UNLOCKABLE, /* not yet, but the candies are there */
  SSF_STATES
} ssf_state;

typedef enum {
  SSF_SORT_NUMBER, SSF_SORT_COST, SSF_SORT_CANDY, SSF_SORT_IV, SSF_SORT_NAME, SSF_SORT_CAUGHT, SSF_SORT_HATCHED, SSF_SORT_COUNT
} ssf_sort;

/* Bits of `caught`, and of `rarity` */
enum { SSF_C_SHINY3 = 1, SSF_C_SHINY2 = 2, SSF_C_SHINY1 = 4, SSF_C_NORMAL = 8, SSF_C_ALL = 15 };
enum { SSF_R_ORDINARY = 1, SSF_R_SUB_LEGENDARY = 2, SSF_R_LEGENDARY = 4, SSF_R_MYTHICAL = 8, SSF_R_ALL = 15 };
#define SSF_GENS_ALL 0x1ffu       /* bit g - 1: generation g */
#define SSF_TYPES_ALL 0x3ffffu    /* bit t: PokemonType t (normal 0 .. fairy 17) */

typedef struct {
  uint16_t gens;
  uint32_t types;
  uint8_t caught, rarity;
  uint8_t passive;    /* SSF_OFF, SSF_ON (unlocked), SSF_UNLOCKABLE, SSF_EXCLUDE (locked) */
  uint8_t reduction;  /* SSF_OFF, SSF_ON (any), SSF_ONE, SSF_TWO, SSF_UNLOCKABLE, SSF_EXCLUDE (none bought) */
  uint8_t win, hidden_ability, egg, pokerus; /* SSF_OFF, SSF_ON, SSF_EXCLUDE */
  uint8_t sort;       /* ssf_sort */
  bool descending;
} ssf;

/* Upstream's defaults: everything on, sorted by number. */
void ssf_reset(ssf* f);
/* How many columns differ from the default (the sort counts when it is not the default). */
int ssf_active(const ssf* f);
/* The filter menu's labels for the options of a column that has no locale text of its own are in the host; these are the rules. */
bool ssf_fits(const gc_system* sys, const ssf* f, uint16_t species);
/* Keeps the species of `all` (ascending) that fit, in the filter's sort order, into `out` (room for n_all). Returns how many. */
int ssf_apply(const gc_system* sys, const ssf* f, const sm_entry* all, int n_all, sm_entry* out);

#ifdef __cplusplus
}
#endif
#endif
