/* The Mysterious Chest (src/data/mystery-encounters/encounters/mysterious-chest-encounter.ts at pinned upstream 8555c08).
 *
 * onInit builds the config of the trap's boss and rolls nothing: Gimmighoul in its first form, a boss, Nasty Plot / Shadow Ball /
 * Power Gem / Thief, a level boost of half the usual one (levelAdditiveModifier 0.5) and no switching.
 *
 * Option 1, open it:
 *  - onPreOptionPhase (executeWithSeedOffset(.., getSeedOffset())) rolls the chest, randSeedInt(100). The chest sprite turns red below 30.
 *  - onOptionPhase (executeWithSeedOffset(.., getSeedOffset() * 500)) by the roll: 75..99 two Common and two Great reward options
 *    ("normal"), 45..74 three Ultra ("good"), 35..44 two Rogue ("great"), 30..34 one Master ("amazing"); each sets those guaranteed
 *    tiers (fillRemaining unset: the shop holds only those), says its message and leaves with no heal shop.
 *  - 0..29, the trap: the allowed, unfainted Pokemon of the highest level (the first of a tie in party order) faints
 *    (koPlayerPokemon: HP 0, the Faint status, "battle:fainted"), "option.1.bad" is shown, and with nobody left to fight it is game over
 *    (the phase queue cleared). Otherwise the fight with the Gimmighoul starts and winning it opens the ordinary rewards
 *    (setEncounterRewards({fillRemaining: true})). The first await of the option (showEncounterText) takes the rest of it out of the option's
 *    seed offset, so everything from initBattleWithEnemyConfig on - the level, the boss's rolls, the pool thresholds and its items - is
 *    on the run's own RNG. Fleeing stays allowed (no withFleeAllowed(false)) and the Gimmighoul can be caught.
 * Option 2 leaves with the heal shop and no rewards.
 *
 * Not ported: the chest sprite turning red and the opening animation, the intro visuals' fade (the app's). */
#include "internal.h"
#include <math.h>
#include <string.h>

#define CHEST_RAND_LENGTH 100
#define CHEST_TRAP 30
#define CHEST_COMMON 25
#define CHEST_ULTRA 30
#define CHEST_ROGUE 10
#define CHEST_MASTER 5

/* the roll of onPreOptionPhase, on the stream of the first option select */
int gc_chest_roll(const gc_run *run)
{
    pr_rng pre = pr_run_seed_offset_rng(&run->run, run->wave * 1000 + 512, NULL);
    return pr_rng_seed_int(&pre, CHEST_RAND_LENGTH, 0);
}

/* setEncounterRewards({guaranteedModifierTiers: tiers}), then the message and leaveEncounterWithoutBattle() */
static void tier_shop(gc_run *run, const int8_t *tiers, int n, const char *message_key)
{
    pr_reward_custom *c = &gc_int(run)->reward.me_custom;
    memset(c, 0, sizeof *c);
    for (int i = 0; i < n; i++) c->tiers[c->n_tiers++] = tiers[i];
    c->fill_remaining = false;
    c->reroll_multiplier = NAN;
    c->allow_luck_upgrades = true;
    gc_ev_msg(run, message_key, 0);
    gc_me_end_shop(run);
}

/* getHighestLevelPlayerPokemon(true, false): the allowed, unfainted Pokemon of the highest level; on a tie the earlier in party order stays.
 * Returns the storage index, -1 when nobody qualifies */
static int highest_level(const gc_run *run)
{
    int best = -1;
    for (int k = 0; k < run->n_party; k++) {
        const int idx = run->party_order[k];
        if (!gc_mon_allowed(&run->party[idx])) continue;
        if (best < 0 || run->party[best].b.level < run->party[idx].b.level) best = idx;
    }
    return best;
}

/* The trap's fight: initBattleWithEnemyConfig(Gimmighoul), after the await, so on the run's RNG */
static void trap_fight(gc_run *run)
{
    gc_internal *in = gc_int(run);
    run->double_battle = false;
    pr_rng r = run->run.rng;
    /* battle.enemyLevels = [getLevelForWave()], then Math.max(Math.round(wave / 10 * 0.5), 0) more */
    const int base = pr_run_get_level_for_wave(&run->run, &r, run->wave);
    const int level = base + (int)fmax(floor((double)run->wave / 10 * 0.5 + 0.5), 0);
    gc_mon *boss = &run->enemy[0];
    if (!gc_me_new_boss(run, &r, SPECIES_ID_GIMMIGHOUL, level, true, boss)) {
        /* a species or biome run-core has no rolls for: the chest is empty, the encounter ends as if declined */
        gc_stub(run, GC_STUB_UNPORTED_ENEMY);
        run->n_enemy = 0;
        gc_me_leave_with_heal_shop(run);
        return;
    }
    run->run.rng = r;
    run->n_enemy = 1;
    run->battle_type = BATTLE_TYPE_WILD;
    /* formIndex 0, then setBoss: the stats are made again for the form (the moveset was rolled for the form the constructor drew) */
    const pr_species_gen *sg = gc_species_gen(SPECIES_ID_GIMMIGHOUL);
    if (sg && sg->n_forms > 0) {
        boss->b.form = 0;
        pr_update_passive(&run->bt, &boss->b);
        pr_calculate_stats(&run->bt, &boss->b);
        boss->b.hp = boss->b.stats[STAT_HP];
    }
    /* the config's moveset */
    boss->b.n_moves = 0;
    memset(boss->b.moves, 0, sizeof boss->b.moves);
    memset(boss->b.pp_used, 0, sizeof boss->b.pp_used);
    memset(boss->b.pp_up, 0, sizeof boss->b.pp_up);
    for (int i = 0; i < 4; i++) boss->b.max_pp_override[i] = -1;
    gc_mon_set_move(boss, 0, MOVE_ID_NASTY_PLOT);
    gc_mon_set_move(boss, 1, MOVE_ID_SHADOW_BALL);
    gc_mon_set_move(boss, 2, MOVE_ID_POWER_GEM);
    gc_mon_set_move(boss, 3, MOVE_ID_THIEF);

    /* after the first await of initBattleWithEnemyConfig: the wild pool's thresholds and the item rolls */
    gc_enemy_finish_party_me(run);
    /* setEncounterRewards({fillRemaining: true}) */
    in->me_reward.active = true;
    in->me_reward.has_item = false;
    in->me_reward.func = -1;
    in->me_reward.fill = true;
    in->me_no_switch = true;
    gc_me_begin_battle(run, GC_ME_MYSTERIOUS_CHEST);
}

void gc_chest_execute(gc_run *run, int option)
{
    if (option != 0) { gc_me_leave_with_heal_shop(run); return; }

    const int roll = gc_chest_roll(run);
    if (roll >= CHEST_RAND_LENGTH - CHEST_COMMON) {
        static const int8_t tiers[4] = {MODIFIER_TIER_COMMON, MODIFIER_TIER_COMMON, MODIFIER_TIER_GREAT, MODIFIER_TIER_GREAT};
        tier_shop(run, tiers, 4, "mysteryEncounters/mysteriousChest:option.1.normal");
    } else if (roll >= CHEST_RAND_LENGTH - CHEST_COMMON - CHEST_ULTRA) {
        static const int8_t tiers[3] = {MODIFIER_TIER_ULTRA, MODIFIER_TIER_ULTRA, MODIFIER_TIER_ULTRA};
        tier_shop(run, tiers, 3, "mysteryEncounters/mysteriousChest:option.1.good");
    } else if (roll >= CHEST_RAND_LENGTH - CHEST_COMMON - CHEST_ULTRA - CHEST_ROGUE) {
        static const int8_t tiers[2] = {MODIFIER_TIER_ROGUE, MODIFIER_TIER_ROGUE};
        tier_shop(run, tiers, 2, "mysteryEncounters/mysteriousChest:option.1.great");
    } else if (roll >= CHEST_RAND_LENGTH - CHEST_COMMON - CHEST_ULTRA - CHEST_ROGUE - CHEST_MASTER) {
        static const int8_t tiers[1] = {MODIFIER_TIER_MASTER};
        tier_shop(run, tiers, 1, "mysteryEncounters/mysteriousChest:option.1.amazing");
    } else {
        const int idx = highest_level(run);
        if (idx < 0) { gc_me_leave_with_heal_shop(run); return; }   /* upstream would throw: nobody can fight, the run is over before this */
        gc_mon *ko = &run->party[idx];
        ko->b.hp = 0;
        ko->b.status = STATUS_EFFECT_FAINT;
        const pr_msg_arg name = pr_arg_pokemon_name(&run->bt, "pokeName", &ko->b);
        /* showEncounterText(option.1.bad) is shown at once; koPlayerPokemon's "battle:fainted" is queued and comes after it */
        gc_ev_msg(run, "mysteryEncounters/mysteriousChest:option.1.bad", 2, name, pr_arg_key("gimmighoulName", "pokemon:gimmighoul"));
        gc_ev_msg(run, "battle:fainted", 1, pr_arg_pokemon(&run->bt, "pokemonNameWithAffix", &ko->b));
        /* no legal Pokemon left: game over, the phase queue cleared */
        bool any = false;
        for (int i = 0; i < run->n_party; i++) any |= gc_mon_allowed(&run->party[i]);
        if (!any) {
            gc_rq_clear(run);
            gc_rq_unshift(run, gc_rp(RP_GAME_OVER, 0, 0, 0));
            return;
        }
        trap_fight(run);
    }
}
