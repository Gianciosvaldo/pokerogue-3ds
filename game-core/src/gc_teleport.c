/* Teleporting Hijinks (src/data/mystery-encounters/encounters/teleporting-hijinks-encounter.ts at pinned upstream 8555c08).
 *
 * onInit (no RNG): the price, getWaveMoneyAmount(1.75). The encounter needs the money at hand to be offered at all (the table in gc_mystery.c).
 *
 * Option 1, put money in: onPreOptionPhase pays the price (updatePlayerMoney, no message). Option 2, a Pokemon helps (a Steel or Electric
 * Pokemon that is not fainted; the PRIMARY Pokemon is the first of the party that is of either type, fainted or not, as the option's
 * requirement query does not filter the fainted): no payment. Both then run doBiomeTransitionDialogueAndBattleInit:
 *  - on the option's stream (executeWithSeedOffset(.., getSeedOffset() * 500)): the new biome, randSeedInt over Space, Fairy Cave, Laboratory,
 *    Island, Wasteland and Dojo less the current biome;
 *  - then the first await (showEncounterText) takes the rest out of the option's seed offset: it is all on the run's own RNG. The text
 *    "transport", the arena changes (a new Arena: no weather, terrain, arena tags or Terastallization used; the player's faint count is zero),
 *    "attacked", and in the NEW biome the boss: getEncounterPokemonLevelForWave(1), arena.randomSpecies(wave, level, 0, party luck, true) and
 *    new EnemyPokemon(.., boss = true) with the same rolls as Fight or Flight's boss, then initBattleWithEnemyConfig (the level, the new id,
 *    the pool's thresholds and the item rolls). The boss comes out enraged: its message, then +1 Def, SpDef and Spd below wave 50, +1 Atk, Def,
 *    SpAtk, SpDef and Spd from wave 50 (battle-core). The fight sets the ordinary rewards (fillRemaining), the player cannot flee, can catch.
 *    Option 2 also setEncounterExp(primary.id, 100) with the wave multiplier: the experience the Pokemon alone gets in the rewards phase of a won fight.
 * Option 3, inspect the machine: no money, no teleport. On the option's stream (everything before its first await) the boss as above, in the
 * CURRENT biome; setEncounterRewards({guaranteedModifierTypeOptions: [ATTACK_TYPE_BOOSTER Steel, ATTACK_TYPE_BOOSTER Electric], fillRemaining: true});
 * after the await of the intro visuals' transition, initBattleWithEnemyConfig on the run's RNG. The boss is not enraged.
 *
 * Not ported: the capsule sprite, the biome change's tween and the music (the app's). */
#include "internal.h"
#include <math.h>
#include <string.h>

#define MONEY_COST_MULTIPLIER 1.75

static const uint8_t CANDIDATES[6] = {BIOME_ID_SPACE, BIOME_ID_FAIRY_CAVE, BIOME_ID_LABORATORY, BIOME_ID_ISLAND, BIOME_ID_WASTELAND, BIOME_ID_DOJO};

static bool machine_type(const gc_mon *m)
{
    int8_t types[PR_MAX_TYPES];
    const int n = pr_get_types(&m->b, types, PR_TYPES_NO_TERA);
    for (int i = 0; i < n; i++) if (types[i] == POKEMON_TYPE_ELECTRIC || types[i] == POKEMON_TYPE_STEEL) return true;
    return false;
}

/* the first Pokemon of the party of either type, fainted or not (storage index, -1 for none) */
static int first_helper(const gc_run *run)
{
    for (int k = 0; k < run->n_party; k++)
        if (machine_type(&run->party[run->party_order[k]])) return run->party_order[k];
    return -1;
}

/* TypeRequirement(.., excludeFainted, 1): at least one of them is not fainted */
bool gc_teleport_helper_ok(const gc_run *run)
{
    for (int k = 0; k < run->n_party; k++) {
        const gc_mon *m = &run->party[run->party_order[k]];
        if (m->b.hp > 0 && machine_type(m)) return true;
    }
    return false;
}

bool gc_teleport_tokens(gc_run *run, double *price, int *primary)
{
    *price = pr_run_get_wave_money_amount(run->wave, MONEY_COST_MULTIPLIER);
    *primary = first_helper(run);
    return true;
}

/* newArena(biome): a new Arena has no weather, terrain, tags or Terastallizations used, and no player faints */
static void new_arena(gc_run *run, int biome)
{
    pr_battle *bt = &run->bt;
    run->biome = biome;
    bt->weather = WEATHER_TYPE_NONE;
    bt->weather_turns_left = bt->weather_max_duration = 0;
    bt->terrain = TERRAIN_NONE;
    bt->terrain_turns_left = bt->terrain_max_duration = 0;
    bt->n_arena_tags = 0;
    bt->n_pos_tags = 0;
    bt->player_faints = 0;
    bt->player_teras_used_known = true;
    bt->player_teras_used = 0;
    gc_event *ev = gc_ev_new(run, GC_EV_BIOME);
    if (ev) ev->a = biome;
}

/* setEncounterRewards({fillRemaining: true}) and, after it, the fight itself */
static void fight(gc_run *run, const gc_mon *boss, int level, int post_summon, const pr_reward_option *more, int n_more, int primary)
{
    gc_internal *in = gc_int(run);
    in->me_reward.active = true;
    in->me_reward.has_item = false;
    in->me_reward.func = -1;
    in->me_reward.fill = true;
    in->me_reward.before = primary >= 0;
    in->me_reward.n_more = (uint8_t)n_more;
    for (int i = 0; i < n_more; i++) in->me_reward.more[i] = more[i];
    in->tele_exp.pending = primary >= 0;
    in->tele_exp.primary = (int8_t)primary;
    in->tele_exp.base = 100;      /* setEncounterExp(primary.id, 100): useWaveIndex defaults to true */
    in->tele_exp.wave = true;
    gc_me_start_boss_battle(run, boss, level, post_summon, &run->run.rng, GC_ME_TELEPORTING_HIJINKS);
}

/* the experience option 2 set, run by the won fight's rewards phase */
void gc_teleport_reward(gc_run *run)
{
    gc_internal *in = gc_int(run);
    if (!in->tele_exp.pending) return;
    in->tele_exp.pending = false;
    if (in->tele_exp.primary >= 0 && in->tele_exp.primary < run->n_party) gc_me_exp_value(run, in->tele_exp.primary, in->tele_exp.base, in->tele_exp.wave);
}

/* doBiomeTransitionDialogueAndBattleInit, then the rewards and the experience */
static void teleport(gc_run *run, int primary)
{
    pr_rng stream = gc_me_option_rng(run);
    uint8_t filtered[6];
    int n = 0;
    for (int i = 0; i < 6; i++) if (CANDIDATES[i] != run->biome) filtered[n++] = CANDIDATES[i];
    const int next = filtered[pr_rng_seed_int(&stream, n, 0)];
    gc_ev_msg(run, "mysteryEncounters/teleportingHijinks:transport", 0);
    new_arena(run, next);
    gc_ev_msg(run, "mysteryEncounters/teleportingHijinks:attacked", 0);

    /* after the awaits: the boss of the new biome, on the run's RNG */
    pr_rng r = run->run.rng;
    gc_mon boss;
    int level;
    if (!gc_me_roll_boss(run, &r, &boss, &level)) {
        gc_stub(run, GC_STUB_UNPORTED_ENEMY);
        gc_me_leave_with_heal_shop(run);
        return;
    }
    run->run.rng = r;
    fight(run, &boss, level, run->wave < 50 ? PR_ME_POST_TELEPORT_LOW : PR_ME_POST_TELEPORT_HIGH, NULL, 0, primary);
}

/* option 3 */
static void inspect(gc_run *run)
{
    pr_rng stream = gc_me_option_rng(run);
    gc_mon boss;
    int level;
    if (!gc_me_roll_boss(run, &stream, &boss, &level)) {
        gc_stub(run, GC_STUB_UNPORTED_ENEMY);
        gc_me_leave_with_heal_shop(run);
        return;
    }
    pr_reward_ctx *ctx = gc_reward_ctx(run);
    pr_reward_option more[2];
    more[0] = pr_reward_pregen_option(ctx, PR_ITEM_ATTACK_TYPE_BOOSTER, POKEMON_TYPE_STEEL);
    more[1] = pr_reward_pregen_option(ctx, PR_ITEM_ATTACK_TYPE_BOOSTER, POKEMON_TYPE_ELECTRIC);
    fight(run, &boss, level, PR_ME_POST_NONE, more, 2, -1);
}

void gc_teleport_execute(gc_run *run, int option)
{
    if (option == 0) {
        /* onPreOptionPhase: updatePlayerMoney(-price) */
        const double price = pr_run_get_wave_money_amount(run->wave, MONEY_COST_MULTIPLIER);
        run->money = fmin(fmax(run->money - price, 0.0), 9007199254740991.0);
        teleport(run, -1);
    } else if (option == 1) {
        teleport(run, first_helper(run));
    } else {
        inspect(run);
    }
}
