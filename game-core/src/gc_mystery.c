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

static const me_rule me_rules[GC_ME_COUNT] = {
    [GC_ME_MYSTERIOUS_CHALLENGERS] = {GC_ME_GREAT, 10,180,0,0,0,0,0},
    [GC_ME_MYSTERIOUS_CHEST]       = {GC_ME_COMMON,10,180,0,2,0,0,0},
    [GC_ME_DARK_DEAL]               = {GC_ME_ROGUE, 30,180,0,2,(1u<<0)|(1u<<9),0,0},
    [GC_ME_FIGHT_OR_FLIGHT]         = {GC_ME_COMMON,10,180,0,0,0,0,0},
    [GC_ME_SLUMBERING_SNORLAX]      = {GC_ME_GREAT, 15,150,0,0,(1u<<11),0,0},
    [GC_ME_TRAINING_SESSION]        = {GC_ME_ULTRA, 10,180,0,2,0,0,0},
    [GC_ME_DEPARTMENT_STORE_SALE]   = {GC_ME_COMMON,10,100,0,0,0,0,0},
    [GC_ME_SHADY_VITAMIN_DEALER]    = {GC_ME_COMMON,10,180,0,0,0,1.5,0},
    [GC_ME_FIELD_TRIP]              = {GC_ME_COMMON,10,100,0,0,0,0,0},
    [GC_ME_SAFARI_ZONE]             = {GC_ME_GREAT, 10,180,0,0,0,2.0,0},
    [GC_ME_LOST_AT_SEA]             = {GC_ME_COMMON,10,180,0,0,0,0,0},
    [GC_ME_FIERY_FALLOUT]           = {GC_ME_COMMON,40,180,0,0,0,0,0},
    [GC_ME_THE_STRONG_STUFF]        = {GC_ME_COMMON,10,180,1,3,0,0,0},
    [GC_ME_THE_POKEMON_SALESMAN]    = {GC_ME_ULTRA, 10,180,0,0,0,4.0,0},
    [GC_ME_AN_OFFER_YOU_CANT_REFUSE]= {GC_ME_GREAT, 10,180,0,2,0,0,0},
    [GC_ME_DELIBIRDY]               = {GC_ME_GREAT, 10,180,4,0,0,2.0,0},
    [GC_ME_ABSOLUTE_AVARICE]        = {GC_ME_GREAT, 20,180,0,0,0,0,0},
    [GC_ME_A_TRAINERS_TEST]         = {GC_ME_ROGUE,100,180,0,0,0,0,0},
    [GC_ME_TRASH_TO_TREASURE]       = {GC_ME_ULTRA,100,180,1,3,0,0,0},
    [GC_ME_BERRIES_ABOUND]          = {GC_ME_COMMON,10,180,0,0,0,0,0},
    [GC_ME_CLOWNING_AROUND]         = {GC_ME_ULTRA, 80,180,0,0,(1u<<1),0,0},
    [GC_ME_PART_TIMER]              = {GC_ME_COMMON,10,180,0,0,0,0,0},
    [GC_ME_DANCING_LESSONS]         = {GC_ME_GREAT, 30,180,0,0,0,0,0},
    [GC_ME_WEIRD_DREAM]             = {GC_ME_ROGUE, 30,140,1,3,(1u<<0)|(1u<<1),0,0},
    [GC_ME_THE_WINSTRATE_CHALLENGE] = {GC_ME_ROGUE,100,180,1,3,0,0,0},
    [GC_ME_TELEPORTING_HIJINKS]     = {GC_ME_COMMON,10,180,0,0,0,1.75,(1u<<2)|(1u<<3)|(1u<<4)},
    [GC_ME_BUG_TYPE_SUPERFAN]       = {GC_ME_GREAT, 30,180,1,3,0,0,0},
    [GC_ME_FUN_AND_GAMES]           = {GC_ME_GREAT, 10,180,0,0,0,1.5,0},
    [GC_ME_UNCOMMON_BREED]          = {GC_ME_COMMON,10,180,0,0,0,0,0},
    [GC_ME_GLOBAL_TRADE_SYSTEM]     = {GC_ME_COMMON,10,180,0,2,0,0,0},
    [GC_ME_THE_EXPERT_POKEMON_BREEDER]={GC_ME_ULTRA,25,180,0,4,(1u<<9),0,0},
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
    if (type < 0 || type >= GC_ME_COUNT || !biome_has(r, type)) return false;
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
        if (run->mystery_history[i].tier == GC_ME_COMMON) weights[0] -= 6;
        else if (run->mystery_history[i].tier == GC_ME_GREAT) weights[1] -= 4;
    }
    for (int i = 0; i < 4; i++) if (weights[i] < 0) weights[i] = 0;
    int total = weights[0] + weights[1] + weights[2] + weights[3];
    if (total <= 0) return false;

    int roll = pr_rng_seed_int(&run->run.rng, total, 0);
    int tier = GC_ME_COMMON;
    if (roll > weights[0] + weights[1] + weights[2]) tier = GC_ME_ROGUE;
    else if (roll > weights[0] + weights[1]) tier = GC_ME_ULTRA;
    else if (roll > weights[0]) tier = GC_ME_GREAT;

    int candidates[GC_ME_COUNT], n = 0;
    for (;;) {
        n = 0;
        for (int t = 0; t < GC_ME_COUNT; t++) if (available(run, t, tier)) candidates[n++] = t;
        if (n || tier == GC_ME_COMMON) break;
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

static bool bug_in_party(const gc_run *r)
{
    for (int i = 0; i < r->n_party; i++) {
        int8_t ty[PR_MAX_TYPES];
        int nt = pr_get_types(&r->party[i].b, ty, PR_TYPES_NO_TERA);
        for (int k = 0; k < nt; k++) if (ty[k] == POKEMON_TYPE_BUG) return true;
    }
    return false;
}

void gc_mystery_setup_screen(gc_run *run)
{
    gc_screen *s = &run->screen;
    const int type = s->mystery_type;
    int n = 2;
    switch (type) {
    case GC_ME_MYSTERIOUS_CHALLENGERS: case GC_ME_FIGHT_OR_FLIGHT: case GC_ME_SLUMBERING_SNORLAX:
    case GC_ME_SHADY_VITAMIN_DEALER: case GC_ME_LOST_AT_SEA: case GC_ME_FIERY_FALLOUT:
    case GC_ME_ABSOLUTE_AVARICE: case GC_ME_CLOWNING_AROUND: case GC_ME_PART_TIMER:
    case GC_ME_DANCING_LESSONS: case GC_ME_WEIRD_DREAM: case GC_ME_TELEPORTING_HIJINKS:
    case GC_ME_FUN_AND_GAMES: case GC_ME_UNCOMMON_BREED: case GC_ME_THE_EXPERT_POKEMON_BREEDER:
        n = 3; break;
    case GC_ME_TRAINING_SESSION: case GC_ME_DEPARTMENT_STORE_SALE: case GC_ME_FIELD_TRIP:
    case GC_ME_SAFARI_ZONE: case GC_ME_DELIBIRDY: case GC_ME_GLOBAL_TRADE_SYSTEM:
        n = 4; break;
    default:
        n = 2; break;
    }

    s->n_options = n;
    for (int i = 0; i < 8; i++) s->option_ok[i] = i < n;

    if (type == GC_ME_DARK_DEAL || type == GC_ME_AN_OFFER_YOU_CANT_REFUSE) {
        s->option_ok[0] = legal_party(run) > 0;
    } else if (type == GC_ME_SHADY_VITAMIN_DEALER) {
        bool ok = any_over_half_hp(run);
        s->option_ok[0] = ok; s->option_ok[1] = ok;
    } else if (type == GC_ME_THE_POKEMON_SALESMAN) {
        s->option_ok[0] = run->money >= pr_run_get_wave_money_amount(run->wave, 4.0) && run->n_party < 6;
    } else if (type == GC_ME_DELIBIRDY) {
        s->option_ok[0] = run->n_items > 0;
        s->option_ok[1] = run->n_items > 0;
        s->option_ok[2] = run->n_items > 0;
    } else if (type == GC_ME_ABSOLUTE_AVARICE) {
        s->option_ok[1] = berry_stacks(run) >= 6;
    } else if (type == GC_ME_TRASH_TO_TREASURE) {
        s->option_ok[0] = run->n_items >= 3;
    } else if (type == GC_ME_BUG_TYPE_SUPERFAN) {
        s->option_ok[0] = bug_in_party(run);
    } else if (type == GC_ME_GLOBAL_TRADE_SYSTEM) {
        s->option_ok[0] = run->n_party >= 2;
        s->option_ok[1] = run->n_items > 0;
        s->option_ok[2] = berry_stacks(run) > 0;
    } else if (type == GC_ME_THE_EXPERT_POKEMON_BREEDER) {
        s->option_ok[0] = run->n_party >= 1;
        s->option_ok[1] = run->n_party >= 1;
    }
}

static void end_mystery_encounter_leave(gc_run *run)
{
    run->in_battle = false;
    run->mystery_type = run->mystery_tier = -1;
    if (gc_is_new_biome(run, run->wave)) gc_rq_push(run, gc_rp(RP_SELECT_BIOME, 0, 0, 0));
    gc_rq_push(run, gc_rp(RP_NEW_BATTLE, 0, 0, 0));
}

static void init_player_mon(gc_run *run, gc_mon *m, int species, int level)
{
    memset(m, 0, sizeof *m);
    m->b.species = (uint16_t)(species > 0 ? species : SPECIES_ID_BULBASAUR);
    m->b.level = level > 0 ? level : 10;
    m->b.is_player = true;
    m->met_wave = (int16_t)run->wave;
    m->met_level = m->b.level;
    m->met_biome = (int8_t)run->biome;
    m->met_species = m->b.species;
    m->pokeball = POKEBALL_TYPE_POKEBALL;

    for (int i = 0; i < 6; i++) m->b.ivs[i] = 15;
    gc_starter_default_moves(m->b.species, m->b.moves);
    m->b.n_moves = 0;
    for (int i = 0; i < 4; i++) {
        if (m->b.moves[i] > 0) {
            m->b.n_moves++;
            m->b.pp_used[i] = 0;
        }
    }

    pr_reset_summon_data(&m->b);
    pr_reset_turn_data(&m->b);
    pr_calculate_stats(&run->bt, &m->b);
    m->b.hp = m->b.stats[0];
}

/* MysteryEncounter.catchAllowed: the encounters built withCatchAllowed(true) */
static bool me_catch_allowed(int type)
{
    switch (type) {
    case GC_ME_FIGHT_OR_FLIGHT: case GC_ME_TELEPORTING_HIJINKS: case GC_ME_DARK_DEAL: case GC_ME_MYSTERIOUS_CHEST:
    case GC_ME_BERRIES_ABOUND: case GC_ME_SLUMBERING_SNORLAX: case GC_ME_FIERY_FALLOUT: case GC_ME_UNCOMMON_BREED:
    case GC_ME_DANCING_LESSONS:
        return true;
    default:
        return false;
    }
}

static void start_mystery_wild_battle(gc_run *run, int species, int level, bool is_boss, int boss_segments, int omni_boost)
{
    /* the encounter's battle is a BattleType.MYSTERY_ENCOUNTER upstream; here it runs as a wild one, and what a
     * ball may do in it is the encounter's own rule */
    run->me_battle = true;
    run->me_catch_allowed = me_catch_allowed(run->mystery_type >= 0 ? run->mystery_type : run->screen.mystery_type);
    run->mystery_type = run->mystery_tier = -1;
    run->in_battle = true;
    run->battle_type = BATTLE_TYPE_WILD;
    int lvl = level > 0 ? level : (int)pr_run_get_max_exp_level(&run->run, run->wave, false);

    if (species <= 0) {
        gc_gen_wild(run, lvl);
    } else {
        pr_enemy_gen g;
        memset(&g, 0, sizeof g);
        g.id = (uint32_t)floor(pr_rng_real_in_range(&run->run.rng, 0, 4294967296.0));
        g.fusion_species = -1;
        g.friendship = -1;

        gc_mon *m = &run->enemy[0];
        gc_mon_new_enemy(run, m, species, lvl, &g, 0);
        gc_port_enemy_moveset(run, m);
        run->n_enemy = 1;
    }

    gc_mon *m = &run->enemy[0];
    if (is_boss) {
        /* setBoss(true, bossSegments): the encounter's own count */
        m->b.boss_segments = (uint8_t)(boss_segments > 0 ? boss_segments : 2);
        m->b.boss_segment_index = (uint8_t)(m->b.boss_segments - 1);
        m->b.is_boss = true;
        if (omni_boost > 0) {
            for (int k = 1; k <= 6; k++) m->b.stat_stages[k] = (int8_t)omni_boost;
        }
    }
    gc_battle_setup_field(run, true);
    gc_rq_push(run, gc_rp(RP_TURN_INIT, 0, 0, 0));
}

bool gc_mystery_execute(gc_run *run, int option, int b, int c, const char **why, bool *ok)
{
    if (option < 0 || option >= run->screen.n_options || !run->screen.option_ok[option]) {
        *why = "option not allowed";
        *ok = false;
        return false;
    }
    *ok = true;
    const int type = run->mystery_type >= 0 ? run->mystery_type : run->screen.mystery_type;
    const int wave_level = (int)pr_run_get_max_exp_level(&run->run, run->wave, false);

    switch (type) {
    case GC_ME_MYSTERIOUS_CHALLENGERS:
        if (option == 0) {
            gc_port_give_reward(run, "MEMORY_MUSHROOM");
            end_mystery_encounter_leave(run);
            return true;
        } else if (option == 1) {
            gc_port_give_reward(run, "RARE_CANDY");
            end_mystery_encounter_leave(run);
            return true;
        } else if (option == 2) {
            gc_port_give_reward(run, "EXP_CHARM");
            end_mystery_encounter_leave(run);
            return true;
        }
        end_mystery_encounter_leave(run);
        return true;

    case GC_ME_MYSTERIOUS_CHEST:
        if (option == 0) {
            int roll = pr_rng_seed_int(&run->run.rng, 100, 0);
            if (roll < 30) {
                /* Trap! Highest level unfainted mon gets KOed */
                int max_idx = -1, max_lvl = -1;
                for (int i = 0; i < run->n_party; i++) {
                    if (run->party[i].b.hp > 0 && run->party[i].b.level > max_lvl) {
                        max_lvl = run->party[i].b.level;
                        max_idx = i;
                    }
                }
                if (max_idx >= 0) run->party[max_idx].b.hp = 0;
                if (legal_party(run) == 0) {
                    gc_rq_clear(run);
                    gc_rq_unshift(run, gc_rp(RP_GAME_OVER, 0, 0, 0));
                    return true;
                }
                /* Fight Gimmighoul boss */
                start_mystery_wild_battle(run, SPECIES_ID_GIMMIGHOUL, wave_level, true, 3, 1);
                return true;
            }
            /* Rewards depending on roll */
            if (roll >= 75) gc_port_give_reward(run, "POTION");
            else if (roll >= 45) gc_port_give_reward(run, "SUPER_POTION");
            else if (roll >= 35) gc_port_give_reward(run, "HYPER_POTION");
            else gc_port_give_reward(run, "MAX_POTION");
            end_mystery_encounter_leave(run);
            return true;
        }
        end_mystery_encounter_leave(run);
        return true;

    case GC_ME_DARK_DEAL:
        if (option == 0) {
            int target = (b >= 0 && b < run->n_party && run->party[b].b.hp > 0) ? b : -1;
            if (target < 0) {
                for (int i = 0; i < run->n_party; i++) if (run->party[i].b.hp > 0) { target = i; break; }
            }
            if (target >= 0) run->party[target].b.hp = 0;
            run->money = pr_run_add_money(run->money, pr_run_get_wave_money_amount(run->wave, 5.0));
            end_mystery_encounter_leave(run);
            return true;
        }
        end_mystery_encounter_leave(run);
        return true;

    case GC_ME_FIGHT_OR_FLIGHT:
        if (option == 0) {
            start_mystery_wild_battle(run, 0, wave_level, true, 2, 1);
            return true;
        }
        end_mystery_encounter_leave(run);
        return true;

    case GC_ME_SLUMBERING_SNORLAX:
        if (option == 0 || option == 1) {
            start_mystery_wild_battle(run, SPECIES_ID_SNORLAX, wave_level, true, 3, 1);
            return true;
        }
        end_mystery_encounter_leave(run);
        return true;

    case GC_ME_TRAINING_SESSION:
        if (option == 0 || option == 1 || option == 2) {
            int target = (b >= 0 && b < run->n_party) ? b : 0;
            /* Increase lowest IV to 31 */
            int min_iv_idx = 0;
            for (int i = 1; i < 6; i++) {
                if (run->party[target].b.ivs[i] < run->party[target].b.ivs[min_iv_idx]) min_iv_idx = i;
            }
            run->party[target].b.ivs[min_iv_idx] = 31;
            pr_calculate_stats(&run->bt, &run->party[target].b);
            end_mystery_encounter_leave(run);
            return true;
        }
        end_mystery_encounter_leave(run);
        return true;

    case GC_ME_DEPARTMENT_STORE_SALE:
        if (option >= 0 && option <= 2) {
            double mult = option == 0 ? 1.0 : option == 1 ? 2.0 : 3.0;
            double cost = pr_run_get_wave_money_amount(run->wave, mult);
            if (run->money >= cost) run->money -= cost;
            gc_port_give_reward(run, option == 0 ? "POTION" : option == 1 ? "SUPER_POTION" : "MAX_POTION");
            end_mystery_encounter_leave(run);
            return true;
        }
        end_mystery_encounter_leave(run);
        return true;

    case GC_ME_SHADY_VITAMIN_DEALER:
        if (option == 0 || option == 1) {
            double cost = pr_run_get_wave_money_amount(run->wave, option == 0 ? 1.5 : 3.5);
            if (run->money >= cost) run->money -= cost;
            double mult = option == 0 ? 0.75 : 0.50;
            for (int i = 0; i < run->n_party; i++) {
                if (run->party[i].b.hp > 0) run->party[i].b.hp = (int32_t)(run->party[i].b.hp * mult);
            }
            gc_port_give_reward(run, "HP_UP");
            end_mystery_encounter_leave(run);
            return true;
        }
        end_mystery_encounter_leave(run);
        return true;

    case GC_ME_FIELD_TRIP:
        if (option == 0) {
            gc_port_give_reward(run, "RARE_CANDY");
            end_mystery_encounter_leave(run);
            return true;
        } else if (option == 1) {
            for (int i = 0; i < run->n_party; i++) run->party[i].b.hp = run->party[i].b.stats[0];
            end_mystery_encounter_leave(run);
            return true;
        } else if (option == 2) {
            start_mystery_wild_battle(run, 0, wave_level, true, 2, 1);
            return true;
        }
        end_mystery_encounter_leave(run);
        return true;

    case GC_ME_SAFARI_ZONE:
        if (option == 0) {
            double cost = pr_run_get_wave_money_amount(run->wave, 2.0);
            if (run->money >= cost) run->money -= cost;
            gc_port_give_reward(run, "GREAT_BALL");
            end_mystery_encounter_leave(run);
            return true;
        } else if (option == 1) {
            start_mystery_wild_battle(run, 0, wave_level, false, 0, 0);
            return true;
        } else if (option == 2) {
            double cost = pr_run_get_wave_money_amount(run->wave, 1.5);
            if (run->money >= cost) run->money -= cost;
            gc_port_give_reward(run, "SUPER_POTION");
            end_mystery_encounter_leave(run);
            return true;
        }
        end_mystery_encounter_leave(run);
        return true;

    case GC_ME_LOST_AT_SEA:
        if (option == 0) {
            for (int i = 0; i < run->n_party; i++) run->party[i].b.hp = run->party[i].b.stats[0];
            gc_port_give_reward(run, "ETHER");
            end_mystery_encounter_leave(run);
            return true;
        } else if (option == 1) {
            start_mystery_wild_battle(run, SPECIES_ID_LAPRAS, wave_level, true, 3, 1);
            return true;
        }
        end_mystery_encounter_leave(run);
        return true;

    case GC_ME_FIERY_FALLOUT:
        if (option == 0) {
            for (int i = 0; i < run->n_party; i++) {
                run->party[i].b.hp = run->party[i].b.stats[0];
                run->party[i].b.status = STATUS_EFFECT_NONE;
            }
            end_mystery_encounter_leave(run);
            return true;
        } else if (option == 1) {
            start_mystery_wild_battle(run, SPECIES_ID_VOLCARONA, wave_level, true, 3, 1);
            return true;
        }
        end_mystery_encounter_leave(run);
        return true;

    case GC_ME_THE_STRONG_STUFF:
        if (option == 0) {
            int target = (b >= 0 && b < run->n_party) ? b : 0;
            if (run->party[target].b.stat_stages[STAT_ATK] < 6) run->party[target].b.stat_stages[STAT_ATK]++;
            end_mystery_encounter_leave(run);
            return true;
        }
        end_mystery_encounter_leave(run);
        return true;

    case GC_ME_THE_POKEMON_SALESMAN:
        if (option == 0) {
            double cost = pr_run_get_wave_money_amount(run->wave, 4.0);
            if (run->money >= cost) run->money -= cost;
            if (run->n_party < 6) {
                init_player_mon(run, &run->party[run->n_party], SPECIES_ID_MAGIKARP, wave_level);
                run->party_order[run->n_party] = (int8_t)run->n_party;
                run->n_party++;
            }
            end_mystery_encounter_leave(run);
            return true;
        }
        end_mystery_encounter_leave(run);
        return true;

    case GC_ME_AN_OFFER_YOU_CANT_REFUSE:
        if (option == 0) {
            int target = (b >= 0 && b < run->n_party) ? b : 0;
            init_player_mon(run, &run->party[target], SPECIES_ID_DITTO, wave_level);
            end_mystery_encounter_leave(run);
            return true;
        }
        end_mystery_encounter_leave(run);
        return true;

    case GC_ME_DELIBIRDY:
        if (option >= 0 && option <= 2) {
            int item_idx = (b >= 0 && b < run->n_items) ? b : 0;
            if (run->n_items > 0) gc_item_remove_at(run, item_idx);
            gc_port_give_reward(run, option == 0 ? "SUPER_POTION" : option == 1 ? "HYPER_POTION" : "MAX_POTION");
            end_mystery_encounter_leave(run);
            return true;
        }
        end_mystery_encounter_leave(run);
        return true;

    case GC_ME_ABSOLUTE_AVARICE:
        if (option == 0) {
            start_mystery_wild_battle(run, SPECIES_ID_GREEDENT, wave_level, true, 4, 2);
            return true;
        } else if (option == 1) {
            gc_item_remove_stacks(run, "BERRY", 6);
            gc_port_give_reward(run, "REVIVER_SEED");
            end_mystery_encounter_leave(run);
            return true;
        }
        end_mystery_encounter_leave(run);
        return true;

    case GC_ME_A_TRAINERS_TEST:
        if (option == 0) {
            start_mystery_wild_battle(run, 0, wave_level + 5, true, 3, 1);
            return true;
        } else if (option == 1) {
            for (int i = 0; i < run->n_party; i++) run->party[i].b.hp = run->party[i].b.stats[0];
            end_mystery_encounter_leave(run);
            return true;
        }
        end_mystery_encounter_leave(run);
        return true;

    case GC_ME_TRASH_TO_TREASURE:
        if (option == 0) {
            int to_remove = run->n_items >= 3 ? 3 : run->n_items;
            for (int i = 0; i < to_remove; i++) gc_item_remove_at(run, 0);
            gc_port_give_reward(run, "GOLDEN_POKEBALL");
            end_mystery_encounter_leave(run);
            return true;
        }
        end_mystery_encounter_leave(run);
        return true;

    case GC_ME_BERRIES_ABOUND:
        if (option == 0) {
            gc_port_give_reward(run, "SITRUS_BERRY");
            end_mystery_encounter_leave(run);
            return true;
        }
        end_mystery_encounter_leave(run);
        return true;

    case GC_ME_CLOWNING_AROUND:
        if (option == 0) {
            start_mystery_wild_battle(run, SPECIES_ID_BLACEPHALON, wave_level, true, 3, 1);
            return true;
        } else if (option == 1) {
            gc_port_give_reward(run, "LUM_BERRY");
            end_mystery_encounter_leave(run);
            return true;
        }
        end_mystery_encounter_leave(run);
        return true;

    case GC_ME_PART_TIMER:
        if (option == 0 || option == 1) {
            double mult = option == 0 ? 0.8 : 0.5;
            for (int i = 0; i < run->n_party; i++) {
                if (run->party[i].b.hp > 0) run->party[i].b.hp = (int32_t)(run->party[i].b.hp * mult);
            }
            run->money = pr_run_add_money(run->money, pr_run_get_wave_money_amount(run->wave, option == 0 ? 2.0 : 5.0));
            end_mystery_encounter_leave(run);
            return true;
        }
        end_mystery_encounter_leave(run);
        return true;

    case GC_ME_DANCING_LESSONS:
        if (option == 0) {
            int target = (b >= 0 && b < run->n_party) ? b : 0;
            if (run->party[target].b.stat_stages[STAT_SPD] < 6) run->party[target].b.stat_stages[STAT_SPD]++;
            end_mystery_encounter_leave(run);
            return true;
        } else if (option == 1) {
            start_mystery_wild_battle(run, SPECIES_ID_ORICORIO, wave_level, true, 2, 1);
            return true;
        }
        end_mystery_encounter_leave(run);
        return true;

    case GC_ME_WEIRD_DREAM:
        if (option == 0) {
            start_mystery_wild_battle(run, SPECIES_ID_DARKRAI, wave_level, true, 4, 1);
            return true;
        } else if (option == 1) {
            for (int i = 0; i < run->n_party; i++) run->party[i].b.hp = run->party[i].b.stats[0];
            end_mystery_encounter_leave(run);
            return true;
        }
        end_mystery_encounter_leave(run);
        return true;

    case GC_ME_THE_WINSTRATE_CHALLENGE:
        if (option == 0) {
            start_mystery_wild_battle(run, 0, wave_level, true, 3, 1);
            return true;
        }
        end_mystery_encounter_leave(run);
        return true;

    case GC_ME_TELEPORTING_HIJINKS:
        if (option == 0) {
            run->money = pr_run_add_money(run->money, pr_run_get_wave_money_amount(run->wave, 2.0));
            end_mystery_encounter_leave(run);
            return true;
        } else if (option == 1) {
            start_mystery_wild_battle(run, SPECIES_ID_ALAKAZAM, wave_level, true, 3, 1);
            return true;
        }
        end_mystery_encounter_leave(run);
        return true;

    case GC_ME_BUG_TYPE_SUPERFAN:
        if (option == 0) {
            gc_port_give_reward(run, "EXP_SHARE");
            end_mystery_encounter_leave(run);
            return true;
        }
        end_mystery_encounter_leave(run);
        return true;

    case GC_ME_FUN_AND_GAMES:
        if (option == 0 || option == 1) {
            run->money = pr_run_add_money(run->money, pr_run_get_wave_money_amount(run->wave, 1.5));
            end_mystery_encounter_leave(run);
            return true;
        }
        end_mystery_encounter_leave(run);
        return true;

    case GC_ME_UNCOMMON_BREED:
        if (option == 0) {
            start_mystery_wild_battle(run, 0, wave_level, true, 2, 1);
            return true;
        } else if (option == 1) {
            if (run->n_party < 6) {
                init_player_mon(run, &run->party[run->n_party], SPECIES_ID_EEVEE, wave_level);
                run->party_order[run->n_party] = (int8_t)run->n_party;
                run->n_party++;
            }
            end_mystery_encounter_leave(run);
            return true;
        }
        end_mystery_encounter_leave(run);
        return true;

    case GC_ME_GLOBAL_TRADE_SYSTEM:
        if (option == 0) {
            int target = (b >= 0 && b < run->n_party) ? b : 0;
            init_player_mon(run, &run->party[target], SPECIES_ID_PIKACHU, wave_level);
            end_mystery_encounter_leave(run);
            return true;
        } else if (option == 1) {
            int item_idx = (b >= 0 && b < run->n_items) ? b : 0;
            if (run->n_items > 0) gc_item_remove_at(run, item_idx);
            gc_port_give_reward(run, "EXP_CHARM");
            end_mystery_encounter_leave(run);
            return true;
        } else if (option == 2) {
            gc_item_remove_stacks(run, "BERRY", 1);
            gc_port_give_reward(run, "SITRUS_BERRY");
            end_mystery_encounter_leave(run);
            return true;
        }
        end_mystery_encounter_leave(run);
        return true;

    case GC_ME_THE_EXPERT_POKEMON_BREEDER:
        if (option == 0 || option == 1) {
            int target = (b >= 0 && b < run->n_party) ? b : 0;
            int min_iv_idx = 0;
            for (int i = 1; i < 6; i++) {
                if (run->party[target].b.ivs[i] < run->party[target].b.ivs[min_iv_idx]) min_iv_idx = i;
            }
            run->party[target].b.ivs[min_iv_idx] = 31;
            pr_calculate_stats(&run->bt, &run->party[target].b);
            end_mystery_encounter_leave(run);
            return true;
        }
        end_mystery_encounter_leave(run);
        return true;

    default:
        end_mystery_encounter_leave(run);
        return true;
    }
}
