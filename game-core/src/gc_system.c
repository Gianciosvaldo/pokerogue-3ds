/* game-core: overall system data across runs (Dex entries, starter data, unlocks).
 * Follows upstream GameData (src/system/game-data.ts). */
#include "gc/system.h"
#include "gc/data.h"
#include <string.h>

void gc_system_init_defaults(gc_system *sys) {
    if (!sys) return;
    memset(sys, 0, sizeof(*sys));

    /* Upstream initDexData:
     * defaultStarterAttr = DexAttr.NON_SHINY | DexAttr.MALE | DexAttr.FEMALE | DexAttr.DEFAULT_VARIANT | DexAttr.DEFAULT_FORM
     * = 1 | 4 | 8 | 16 | 128 = 157 (0x9D) */
    const uint64_t default_starter_attr = 157ULL;

    for (int i = 0; i < gc_default_starter_count; i++) {
        uint16_t species = gc_default_starters[i];
        if (species > 0 && species <= GC_MAX_SPECIES_ID) {
            sys->dex[species].seen_attr = default_starter_attr;
            sys->dex[species].caught_attr = default_starter_attr;
            sys->dex[species].nature_attr = 1U << (gc_default_starter_natures[i] + 1);
            for (int k = 0; k < 6; k++) {
                sys->dex[species].ivs[k] = 15;
            }
        }
    }

    /* Upstream initStarterData:
     * abilityAttr = defaultStarterSpecies.includes(speciesId) ? AbilityAttr.ABILITY_1 : 0 */
    for (int i = 0; i < gc_starter_cost_count; i++) {
        uint16_t species = gc_starter_costs[i].id;
        if (species > 0 && species <= GC_MAX_SPECIES_ID) {
            /* If it's a default starter, caught_attr was set above */
            if (sys->dex[species].caught_attr != 0) {
                sys->starters[species].ability_attr = 1; /* ABILITY_1 */
            } else {
                sys->starters[species].ability_attr = 0;
            }
        }
    }

    sys->unlocks.endless_mode = false;
    sys->unlocks.mini_black_hole = false;
    sys->unlocks.spliced_endless_mode = false;
    sys->unlocks.eviolite = false;
}
