/* Timed events in a run (lorenzo's own events: custom-core's cc/tevents.h says which are open and what they change).
 *
 * game-core knows no clock and no schedule. The app installs a source; game-core asks it for the effects open RIGHT NOW at the moment of each
 * roll they change (the enemy's shiny roll, the party's luck, the catch rate, the candy rate, the boss voucher), so a window that opens or
 * closes while a run is going is seen by the next roll, never frozen at the run's start. With no source installed (every host test, every
 * differential, a run the app has not given one) nothing is open: game-core plays exactly as upstream does with no event.
 *
 * Not saved: the app installs the source again after a load, like the custom hooks (gc/custom.h). A save does not hold an effect either;
 * what an effect changed (a shiny Pokemon, a candy) is in the run already.
 *
 * Every field is "0 = this event does not change it", as in cc_tevent_effects. game-core keeps its own copy of the struct so it does not
 * depend on custom-core. */
#ifndef GC_EVENTS_H
#define GC_EVENTS_H

#include <stdint.h>

struct gc_run;

#define GC_EVENT_SPECIES_MAX 12   /* luck-boosted species of all the open events together (custom-core's CC_TEVENT_SPECIES_MAX) */
#define GC_EVENT_REWARDS_MAX 8   /* wave rewards of all the open events together (custom-core's CC_TEVENT_REWARDS_MAX) */

/* One item an event gives after a Classic wave's battle (upstream's classicWaveRewards entry). `key` is the item's modifierTypes key
 * (reward-core's pr_item_keys: "SHINY_CHARM", "MEGA_BRACELET"...), a string that outlives the run. */
typedef struct gc_event_reward {
    uint16_t wave;           /* 1..199; 0 = an unused slot, the list ends at the first one */
    const char *key;
} gc_event_reward;

typedef struct gc_event_effects {
    uint8_t shiny_encounter_mult;    /* a wild or trainer Pokemon's shiny odds x this (upstream getShinyEncounterMultiplier); 0 = x1 */
    uint8_t shiny_catch_mult;        /* a shiny target's catch-rate multiplier (getShinyCatchMultiplier); 0 = the non-event value */
    uint8_t candy_mult;              /* Classic candy friendship multiplier (getClassicFriendshipMultiplier); 0 = the non-event value */
    uint8_t luck_boost;              /* added to the party luck (getLuckBoost), the cap of 14 still applies; 0 = none */
    uint16_t trainer_shiny_chance;   /* a trainer's Pokemon's shiny threshold out of 65536 (getClassicTrainerShinyChance); 0 = none */
    uint8_t upgrade_vouchers;        /* getUpgradeUnlockedVouchers: nonzero = a defeated boss gives the upgraded voucher */
    uint16_t luck_species[GC_EVENT_SPECIES_MAX];   /* getEventLuckBoostedSpecies: each party member of one of these adds 1 to the luck sum; ends at the first 0 */
    uint8_t boost_fusions;           /* areFusionsBoosted: a fusion feeds each candy bar 1/1.5 of the gain instead of 1/2, and the shop offers DNA Splicers more */
    gc_event_reward rewards[GC_EVENT_REWARDS_MAX];   /* getFixedBattleEventRewards: given in a Classic run after the wave's battle, before the shop */
} gc_event_effects;

typedef struct gc_event_source {
    void *user;
    /* Fills `out` (zeroed by the caller) with the effects open now. Must be cheap and must not call back into game-core. */
    void (*effects)(void *user, gc_event_effects *out);
} gc_event_source;

/* Installs (or, with NULL, removes) the source. The pointer is kept: it must outlive the run. Safe at any time. */
void gc_run_set_events(struct gc_run *run, const gc_event_source *src);

#endif
