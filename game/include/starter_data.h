/*
 * starter_data: what the starter select screen (menus/starter.c) gets from the game: the species the player can choose from,
 * what the panel shows for one, and the team handed to game-core when the player confirms.
 *
 * Everything comes from the system data (game-core's gc_system: the dex and the starter unlocks), through game-core's own
 * rules: gc_starter_cost / gc_starter_value for the cost, gc_starter_is_available for what may be picked and
 * gc_starter_budget for the limit. Nothing is decided here that game-core already decides.
 *
 * A species is picked with the defaults upstream chooses for one with no stored preference (getStarterDetailsFromPreferences at the
 * pinned commit, src/ui/utils/starter-select-ui-utils.ts): shiny when a shiny is caught (the highest variant caught), female only
 * for a species that is always female, form 0, the first ability unlocked, the lowest nature caught, the passive when it is
 * unlocked and on, the species' first type as the tera type, the IVs the dex holds, and the default moveset. The cycle buttons that
 * change them are available through Customize. Stored unlocked preferences override these defaults; invalid or stale choices fall back safely.
 */
#ifndef GM_STARTER_DATA_H
#define GM_STARTER_DATA_H

#include <stdbool.h>
#include <stdint.h>

#include "gc/run.h"
#include "starter.h"

#ifdef __cplusplus
extern "C" {
#endif

/* An upper bound for ss_list (game-core has 572 starter species). */
#define SS_MAX_ENTRIES 640

/* The starters to list: upstream's default filter (the species whose dex entry says caught, a non-shiny or a shiny with a variant),
 * in species order, less the ones game-core would refuse with their defaults (gc_starter_is_available): `refused` (may be NULL)
 * gets how many those were. `sys` NULL lists nothing. Returns how many were written.
 *
 * Missing in game-core: a species that is caught but refused. Upstream's setPokemonSpeciesCaught gives a prevolution the evolved
 * Pokemon's form bit, masked by what the prevolution can unlock, so a Litleo reached through a female Pyroar (form 1) ends up
 * with no form bit at all (and a Kubfu reached through a Rapid Strike Urshifu with the bit of a form it does not have). Upstream's
 * starter select still lets it be picked (form 0, nothing checks); game-core's gc_starter_is_available answers lockedForm. Until
 * game-core decides which of the two is right, such a species is not listed. */
int ss_list(const gc_system* sys, sm_entry* out, int cap, int* refused);

/* The species has Pokerus now (three are picked every three hours). */
bool ss_has_pokerus(uint16_t species);

/* The panel's data for `species`: how it would be picked. */
void ss_describe(const gc_system* sys, uint16_t species, starter_info* out);

/* The gc_starter for `species` with the defaults above (what gc_run_set_starters takes). False when the species is not a starter. */
bool ss_starter(const gc_system* sys, uint16_t species, gc_starter* out);

/* The team the player confirmed, as game-core takes it: each one through ss_starter and gc_starter_is_available, the cost against
 * gc_starter_budget. False with `why` (a locale key such as "starterSelectUiHandler:cost" or game-core's own text) when game-core
 * would refuse it; nothing is then written to `out`. */
bool ss_team(const gc_system* sys, const uint16_t* species, int n, gc_starter* out, const char** why);

#ifdef __cplusplus
}
#endif
#endif
