#ifndef FOOTBALL_PROFILE_FIXTURE_H
#define FOOTBALL_PROFILE_FIXTURE_H
#include "gc/system.h"
#include "gc/data.h"
/* Host fixture: explicit classic unlock records, never a production fallback. */
static void football_test_unlock_all(gc_system *profile)
{
    gc_system_init_defaults(profile);
    for(int i=0;i<gc_starter_cost_count;++i){unsigned id=gc_starter_costs[i].id;if(id>GC_MAX_SPECIES_ID)continue;
        profile->dex[id].caught_attr=gc_species_full_unlocks_data((uint16_t)id);
        profile->dex[id].nature_attr=UINT32_C(0x03fffffe);profile->starters[id].ability_attr=7;
    }
}
#endif
