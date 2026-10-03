/* Mystery Encounter dispatcher (Task 45).
 * Selection order mirrors BattleScene.getMysteryEncounter() and mystery-encounter-biomes.ts
 * at pinned upstream 8555c08. Encounter actions are intentionally Task 46. */
#include "internal.h"
#include <math.h>
#include <string.h>

typedef struct {
    int8_t tier, min_wave, max_wave, max_seen, min_party;
    uint16_t disallowed_challenges;
    double money_mult;
    uint8_t wave_mod_mask; /* bit r means wave % 10 == r is legal; 0 = no modulus gate */
} me_rule;

static const me_rule me_rules[GC_MYSTERY_TYPE_COUNT] = {
    [GC_ME_MYSTERIOUS_CHALLENGERS] = {GC_ME_TIER_GREAT, 10,180,0,0,0,0,0},
    [GC_ME_MYSTERIOUS_CHEST]       = {GC_ME_TIER_COMMON,10,180,0,2,0,0,0},
    [GC_ME_DARK_DEAL]               = {GC_ME_TIER_ROGUE, 30,180,0,2,(1u<<0)|(1u<<9),0,0},
    [GC_ME_FIGHT_OR_FLIGHT]         = {GC_ME_TIER_COMMON,10,180,0,0,0,0,0},
    [GC_ME_SLUMBERING_SNORLAX]      = {GC_ME_TIER_GREAT, 15,150,0,0,(1u<<11),0,0},
    [GC_ME_TRAINING_SESSION]        = {GC_ME_TIER_ULTRA, 10,180,0,2,0,0,0},
    [GC_ME_DEPARTMENT_STORE_SALE]   = {GC_ME_TIER_COMMON,10,100,0,0,0,0,0},
    [GC_ME_SHADY_VITAMIN_DEALER]    = {GC_ME_TIER_COMMON,10,180,0,0,0,1.5,0},
    [GC_ME_FIELD_TRIP]              = {GC_ME_TIER_COMMON,10,100,0,0,0,0,0},
    [GC_ME_SAFARI_ZONE]             = {GC_ME_TIER_GREAT, 10,180,0,0,0,2.0,0},
    [GC_ME_LOST_AT_SEA]             = {GC_ME_TIER_COMMON,10,180,0,0,0,0,0},
    [GC_ME_FIERY_FALLOUT]           = {GC_ME_TIER_COMMON,40,180,0,0,0,0,0},
    [GC_ME_THE_STRONG_STUFF]        = {GC_ME_TIER_COMMON,10,180,1,3,0,0,0},
    [GC_ME_THE_POKEMON_SALESMAN]    = {GC_ME_TIER_ULTRA, 10,180,0,0,0,4.0,0},
    [GC_ME_AN_OFFER_YOU_CANT_REFUSE]= {GC_ME_TIER_GREAT, 10,180,0,2,0,0,0},
    [GC_ME_DELIBIRDY]               = {GC_ME_TIER_GREAT, 10,180,4,0,0,2.0,0},
    [GC_ME_ABSOLUTE_AVARICE]        = {GC_ME_TIER_GREAT, 20,180,0,0,0,0,0},
    [GC_ME_A_TRAINERS_TEST]         = {GC_ME_TIER_ROGUE,100,180,0,0,0,0,0},
    [GC_ME_TRASH_TO_TREASURE]       = {GC_ME_TIER_ULTRA,100,180,1,3,0,0,0},
    [GC_ME_BERRIES_ABOUND]          = {GC_ME_TIER_COMMON,10,180,0,0,0,0,0},
    [GC_ME_CLOWNING_AROUND]         = {GC_ME_TIER_ULTRA, 80,180,0,0,(1u<<1),0,0},
    [GC_ME_PART_TIMER]              = {GC_ME_TIER_COMMON,10,180,0,0,0,0,0},
    [GC_ME_DANCING_LESSONS]         = {GC_ME_TIER_GREAT, 30,180,0,0,0,0,0},
    [GC_ME_WEIRD_DREAM]             = {GC_ME_TIER_ROGUE, 30,140,1,3,(1u<<0)|(1u<<1),0,0},
    [GC_ME_THE_WINSTRATE_CHALLENGE] = {GC_ME_TIER_ROGUE,100,180,1,3,0,0,0},
    [GC_ME_TELEPORTING_HIJINKS]     = {GC_ME_TIER_COMMON,10,180,0,0,0,1.75,(1u<<2)|(1u<<3)|(1u<<4)},
    [GC_ME_BUG_TYPE_SUPERFAN]       = {GC_ME_TIER_GREAT, 30,180,1,3,0,0,0},
    [GC_ME_FUN_AND_GAMES]           = {GC_ME_TIER_GREAT, 10,180,0,0,0,1.5,0},
    [GC_ME_UNCOMMON_BREED]          = {GC_ME_TIER_COMMON,10,180,0,0,0,0,0},
    [GC_ME_GLOBAL_TRADE_SYSTEM]     = {GC_ME_TIER_COMMON,10,180,0,2,0,0,0},
    [GC_ME_THE_EXPERT_POKEMON_BREEDER]={GC_ME_TIER_ULTRA,25,180,0,4,(1u<<9),0,0},
};

static bool biome_has(const gc_run *r, int type)
{
    const int b = r->biome;
    bool non_extreme = b != BIOME_ID_SEA && b != BIOME_ID_SEABED && b != BIOME_ID_BADLANDS
        && b != BIOME_ID_DESERT && b != BIOME_ID_ICE_CAVE && b != BIOME_ID_VOLCANO
        && b != BIOME_ID_WASTELAND && b != BIOME_ID_ABYSS && b != BIOME_ID_SPACE && b != BIOME_ID_END;
    bool human = b != BIOME_ID_SEA && b != BIOME_ID_SEABED && b != BIOME_ID_VOLCANO
        && b != BIOME_ID_WASTELAND && b != BIOME_ID_ABYSS && b != BIOME_ID_SPACE && b != BIOME_ID_END;
    bool civ = b == BIOME_ID_TOWN || b == BIOME_ID_PLAINS || b == BIOME_ID_GRASS || b == BIOME_ID_TALL_GRASS
        || b == BIOME_ID_METROPOLIS || b == BIOME_ID_BEACH || b == BIOME_ID_LAKE || b == BIOME_ID_MEADOW
        || b == BIOME_ID_POWER_PLANT || b == BIOME_ID_GRAVEYARD || b == BIOME_ID_DOJO || b == BIOME_ID_FACTORY
        || b == BIOME_ID_CONSTRUCTION_SITE || b == BIOME_ID_SLUM || b == BIOME_ID_ISLAND;

    if (type == GC_ME_BUG_TYPE_SUPERFAN && non_extreme) return true;
    if ((type == GC_ME_MYSTERIOUS_CHALLENGERS || type == GC_ME_SHADY_VITAMIN_DEALER
         || type == GC_ME_THE_POKEMON_SALESMAN || type == GC_ME_THE_WINSTRATE_CHALLENGE
         || type == GC_ME_THE_EXPERT_POKEMON_BREEDER) && human) return true;
    if ((type == GC_ME_DEPARTMENT_STORE_SALE || type == GC_ME_PART_TIMER
         || type == GC_ME_FUN_AND_GAMES || type == GC_ME_GLOBAL_TRADE_SYSTEM) && civ) return true;

    switch (type) {
    case GC_ME_FIGHT_OR_FLIGHT: case GC_ME_DARK_DEAL: case GC_ME_ABSOLUTE_AVARICE:
    case GC_ME_TRAINING_SESSION: case GC_ME_DELIBIRDY: case GC_ME_A_TRAINERS_TEST:
    case GC_ME_TRASH_TO_TREASURE: case GC_ME_BERRIES_ABOUND: case GC_ME_CLOWNING_AROUND:
    case GC_ME_WEIRD_DREAM: case GC_ME_TELEPORTING_HIJINKS: case GC_ME_UNCOMMON_BREED:
        return true; /* all biomes */
    case GC_ME_SLUMBERING_SNORLAX:
        return b == BIOME_ID_PLAINS;
    case GC_ME_DANCING_LESSONS:
        return b == BIOME_ID_GRASS || b == BIOME_ID_TALL_GRASS || b == BIOME_ID_BEACH
            || b == BIOME_ID_BADLANDS || b == BIOME_ID_MEADOW || b == BIOME_ID_GRAVEYARD
            || b == BIOME_ID_RUINS || b == BIOME_ID_TEMPLE || b == BIOME_ID_ISLAND;
    case GC_ME_SAFARI_ZONE:
        return b == BIOME_ID_FOREST || b == BIOME_ID_SWAMP || b == BIOME_ID_JUNGLE;
    case GC_ME_LOST_AT_SEA:
        return b == BIOME_ID_SEA;
    case GC_ME_FIERY_FALLOUT:
        return b == BIOME_ID_VOLCANO;
    case GC_ME_THE_STRONG_STUFF:
        return b == BIOME_ID_CAVE;
    default:
        return false;
    }
}

static int seen_count(const gc_run *r, int type)
{
    int n = 0;
    for (int i = 0; i < r->n_mystery_history; i++) if (r->mystery_history[i].type == type) n++;
    return n;
}

static int legal_party(const gc_run *r)
{
    int n = 0;
    for (int i = 0; i < r->n_party; i++) if (gc_mon_allowed(&r->party[i])) n++;
    return n;
}

static int berry_stacks(const gc_run *r)
{
    int n = 0;
    for (int i = 0; i < r->n_items; i++)
        if (strstr(r->items[i].key, "BERRY") != NULL) n += r->items[i].stack;
    return n;
}

static bool any_over_half_hp(const gc_run *r)
{
    for (int i = 0; i < r->n_party; i++) {
        const pr_pokemon *p = &r->party[i].b;
        if (p->hp > 0 && p->stats[0] > 0 && (double)p->hp / p->stats[0] >= 0.51) return true;
    }
    return false;
}

static bool available(const gc_run *r, int type, int tier)
{
    if (type < 0 || type >= GC_MYSTERY_TYPE_COUNT || !biome_has(r, type)) return false;
    const me_rule *q = &me_rules[type];
    if (q->tier != tier || r->wave < q->min_wave || r->wave > q->max_wave) return false;
    if (r->n_mystery_history && r->mystery_history[r->n_mystery_history - 1].type == type) return false;
    if (q->max_seen && seen_count(r, type) >= q->max_seen) return false;
    if (q->min_party && legal_party(r) < q->min_party) return false;
    if (q->disallowed_challenges && (r->run.challenge_flags & q->disallowed_challenges)) return false;
    if (q->wave_mod_mask && !(q->wave_mod_mask & (1u << (r->wave % 10)))) return false;
    if (q->money_mult > 0 && r->money < pr_run_get_wave_money_amount(r->wave, q->money_mult)) return false;

    /* Encounter-level requirements whose state already exists in game-core. */
    if (type == GC_ME_ABSOLUTE_AVARICE && berry_stacks(r) < 6) return false;
    if (type == GC_ME_SHADY_VITAMIN_DEALER && !any_over_half_hp(r)) return false;

    /* These encounters have additional held-item/primary-Pokemon spawn gates upstream.
     * If the simple sufficient condition below is not met, exclude instead of selecting an
     * encounter upstream may reject. */
    if (type == GC_ME_BUG_TYPE_SUPERFAN) {
        bool bug = false;
        for (int i = 0; i < r->n_party && !bug; i++) {
            int8_t ty[PR_MAX_TYPES];
            int nt = pr_get_types(&r->party[i].b, ty, PR_TYPES_NO_TERA);
            for (int k = 0; k < nt; k++) if (ty[k] == POKEMON_TYPE_BUG) bug = true;
        }
        if (!bug) return false;
    }
    if (type == GC_ME_DELIBIRDY && r->n_items == 0) return false;
    return true;
}

bool gc_mystery_select(gc_run *run, int *out_type, int *out_tier)
{
    int weights[4] = {66, 40, 19, 3};
    for (int i = 0; i < run->n_mystery_history; i++) {
        if (run->mystery_history[i].tier == GC_ME_TIER_COMMON) weights[0] -= 6;
        else if (run->mystery_history[i].tier == GC_ME_TIER_GREAT) weights[1] -= 4;
    }
    for (int i = 0; i < 4; i++) if (weights[i] < 0) weights[i] = 0;
    int total = weights[0] + weights[1] + weights[2] + weights[3];
    if (total <= 0) return false;

    int roll = pr_rng_seed_int(&run->run.rng, total, 0);
    int tier = GC_ME_TIER_COMMON;
    if (roll > weights[0] + weights[1] + weights[2]) tier = GC_ME_TIER_ROGUE;
    else if (roll > weights[0] + weights[1]) tier = GC_ME_TIER_ULTRA;
    else if (roll > weights[0]) tier = GC_ME_TIER_GREAT;

    int candidates[GC_MYSTERY_TYPE_COUNT], n = 0;
    for (;;) {
        n = 0;
        for (int t = 0; t < GC_MYSTERY_TYPE_COUNT; t++) if (available(run, t, tier)) candidates[n++] = t;
        if (n || tier == GC_ME_TIER_COMMON) break;
        tier--;
    }

    int type = n ? candidates[n == 1 ? 0 : pr_rng_integer_in_range(&run->run.rng, 0, n - 1)]
                 : GC_ME_MYSTERIOUS_CHALLENGERS;
    *out_type = type;
    *out_tier = tier;
    return true;
}

void gc_mystery_record(gc_run *run, int type, int tier)
{
    if (run->n_mystery_history < GC_MAX_MYSTERY_HISTORY) {
        gc_mystery_seen *s = &run->mystery_history[run->n_mystery_history++];
        s->type = (int8_t)type; s->tier = (int8_t)tier; s->wave = (int16_t)run->wave;
    }
    run->me.n_encountered++;
    run->me.last_encounter_wave = run->wave;
    run->me.spawn_chance = 3;
    run->mystery_type = (int8_t)type;
    run->mystery_tier = (int8_t)tier;
}

void gc_mystery_increment_miss(gc_run *run)
{
    if (run->me.spawn_chance < 256) {
        run->me.spawn_chance += 3;
        if (run->me.spawn_chance > 256) run->me.spawn_chance = 256;
    }
}
