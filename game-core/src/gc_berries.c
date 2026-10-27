/* Berries Abound (src/data/mystery-encounters/encounters/berries-abound-encounter.ts at pinned upstream 8555c08).
 *
 * onInit (executeWithSeedOffset(.., waveIndex)): the boss exactly as Fight or Flight's (gc_me_init_boss); the number of berries (2 up to wave 40,
 * 4 up to 120, 5 up to 160, then 7); the fastest Pokemon of the party that is not fainted (the first of equal Speed; getHighestStatPlayerPokemon);
 * the boss' Speed. Nothing of it is kept: it is worked out again from the run when it is needed.
 *
 * Every option that does not leave first rolls the shop (onOptionPhase, executeWithSeedOffset(.., getSeedOffset() * 500)): five times
 * generateModifierTypeOption(BERRY), each a random berry (randSeedInt(12): 2/12 Sitrus, 2/12 Lum, 2/12 Leppa, else one of the others), on the stream
 * the battle's initBattleWithEnemyConfig goes on with (getLevelForWave, then the boss' new id).
 * Option 1: the boss fights (a boss from its data source, no passive flag, full HP; the player cannot flee); winning opens the five berries (no
 * fill), and the berries callback runs before it.
 * Option 2: the fastest Pokemon races (the shop is rolled first, in the seed offset). speedDiff = its Speed / (boss Speed * 1.1). Below 1: the same fight, with the boss enraged (but its level and new id come after an await of the selected text, so from the run's own RNG, not the option's stream) when it comes out
 * (a message, then +1 Def, SpDef and Spd below wave 50, +1 Atk, Def, SpAtk, SpDef and Spd from wave 50 on). Otherwise the Pokemon grabs
 * max(min(round((speedDiff - 1) / 0.08), numBerries), 2) berries without a fight: experience for it alone (the boss species' base exp), the
 * berries callback, the five-berry shop, no heal shop.
 * Option 3 leaves with the heal shop.
 *
 * The berries callback (setEncounterRewards' third argument, run by the rewards phase in executeWithSeedOffset(.., wave * 1000)): the message
 * "battle:rewardGainCount" (Berries x n), then n times randSeedItem(BerryType) (randSeedInt(11)) given to the first Pokemon of the party, the race's
 * winner first, that holds fewer than its cap of that berry (2 for Sitrus, Lum, Leppa and Enigma, else 3).
 *
 * Not ported: the sprites, the shiny and hidden-ability rerolls of timed events. */
#include "internal.h"
#include <math.h>
#include <string.h>

extern int pr_reward_generate(pr_reward_ctx *ctx, int item);   /* reward-core/src/reward_internal.h: generateType of a generator item */

static int num_berries(int wave) { return wave > 160 ? 7 : wave > 120 ? 5 : wave > 40 ? 4 : 2; }

/* getHighestStatPlayerPokemon(SPD, true, false): the first Pokemon of the party order that is not fainted and is the fastest; a storage index */
int gc_berries_fastest(const gc_run *run)
{
    int best = -1;
    for (int k = 0; k < run->n_party; k++) {
        const int idx = run->party_order[k];
        if (!gc_mon_allowed(&run->party[idx])) continue;
        if (best < 0 || run->party[best].b.stats[STAT_SPD] < run->party[idx].b.stats[STAT_SPD]) best = idx;
    }
    return best >= 0 ? best : (run->n_party > 0 ? run->party_order[0] : -1);
}

typedef struct {
    gc_mon boss;
    int level, fastest, n;
    double speed_diff;
} berries_t;

static bool init(gc_run *run, berries_t *b)
{
    pr_rng after;
    if (!gc_me_init_boss(run, &b->boss, &b->level, &after)) return false;
    b->n = num_berries(run->wave);
    b->fastest = gc_berries_fastest(run);
    b->speed_diff = b->fastest >= 0 ? (double)run->party[b->fastest].b.stats[STAT_SPD] / ((double)b->boss.b.stats[STAT_SPD] * 1.1) : 0;
    return true;
}

/* the berries the race is worth when it is won: Math.max(Math.min(Math.round((speedDiff - 1) / 0.08), numBerries), 2) */
static int grabbed(const berries_t *b)
{
    const int rounded = (int)floor((b->speed_diff - 1) / 0.08 + 0.5);
    const int n = rounded < b->n ? rounded : b->n;
    return n > 2 ? n : 2;
}

bool gc_berries_tokens(gc_run *run, pr_msg_arg *enemy_name, pr_msg_arg *fastest_name, int *grab)
{
    static berries_t b;
    if (!init(run, &b) || b.fastest < 0) return false;
    *enemy_name = pr_arg_pokemon_name(&run->bt, "enemyPokemon", &b.boss.b);
    *fastest_name = pr_arg_pokemon_name(&run->bt, "fastestPokemon", &run->party[b.fastest].b);
    *grab = grabbed(&b);
    return true;
}

/* true when the race is lost: the boss is faster (the "selectedBad" text, and a fight) */
bool gc_berries_race_lost(gc_run *run)
{
    static berries_t b;
    return init(run, &b) && b.speed_diff < 1;
}

static int berry_cap(int type)
{
    return type == BERRY_TYPE_SITRUS || type == BERRY_TYPE_LUM || type == BERRY_TYPE_LEPPA || type == BERRY_TYPE_ENIGMA ? 2 : 3;
}

/* tryGiveBerry: a random berry to the prioritized Pokemon, else to the first of the party with room for it */
static void try_give_berry(gc_run *run, pr_rng *r, int prioritized)
{
    const int type = pr_rng_seed_int(r, BERRY_TYPE_LEPPA + 1, 0);   /* randSeedItem(getEnumValues(BerryType)) */
    int order[PR_MAX_PARTY + 1], n = 0;
    if (prioritized >= 0) order[n++] = prioritized;
    for (int k = 0; k < run->n_party; k++) order[n++] = run->party_order[k];
    for (int k = 0; k < n; k++) {
        const gc_mon *m = &run->party[order[k]];
        int held = 0;
        for (int i = 0; i < run->n_items; i++) {
            const gc_item *it = &run->items[i];
            if (it->pokemon_id == m->b.id && !strcmp(it->key, "BERRY") && it->arg == type) held += it->stack;
        }
        if (held < berry_cap(type)) {
            gc_item_add(run, pr_item_keys[PR_ITEM_BERRY], (int16_t)type, m->b.id, 1);
            return;
        }
    }
}

static void give_berries(gc_run *run, int count, int prioritized)
{
    pr_rng r = pr_run_seed_offset_rng(&run->run, run->wave * 1000, NULL);
    gc_ev_msg(run, "battle:rewardGainCount", 2, pr_arg_key("modifierName", "mysteryEncounters/berriesAbound:berries"), pr_arg_int("count", count));
    for (int i = 0; i < count; i++) try_give_berry(run, &r, prioritized);
}

/* onBeforeRewardsPhase after a fight: the berries callback */
void gc_berries_reward(gc_run *run)
{
    gc_internal *in = gc_int(run);
    if (!in->berries.pending) return;
    in->berries.pending = false;
    give_berries(run, in->berries.n, -1);
    gc_items_to_battle(run);
}

/* five times generateModifierTypeOption(BERRY) on the option's stream */
static void roll_shop(gc_run *run, pr_rng *r, pr_reward_option out[5])
{
    const pr_rng saved = run->run.rng;
    run->run.rng = *r;
    pr_reward_ctx *ctx = gc_reward_ctx(run);
    for (int i = 0; i < 5; i++) {
        const int arg = pr_reward_generate(ctx, PR_ITEM_BERRY);
        out[i] = pr_reward_pregen_option(ctx, PR_ITEM_BERRY, arg);
    }
    *r = run->run.rng;
    run->run.rng = saved;
}

static void fight(gc_run *run, const berries_t *b, pr_rng *stream, const pr_reward_option shop[5], int post_summon)
{
    gc_internal *in = gc_int(run);
    in->me_reward.active = true;
    in->me_reward.has_item = false;
    in->me_reward.func = -1;
    in->me_reward.fill = false;
    in->me_reward.before = true;
    in->me_reward.n_more = 5;
    for (int i = 0; i < 5; i++) in->me_reward.more[i] = shop[i];
    in->berries.pending = true;
    in->berries.n = (uint8_t)b->n;
    gc_me_start_boss_battle(run, &b->boss, b->level, post_summon, stream, GC_ME_BERRIES_ABOUND);
}

void gc_berries_execute(gc_run *run, int option)
{
    if (option == 2) { gc_me_leave_with_heal_shop(run); return; }
    static berries_t b;
    if (!init(run, &b)) {
        gc_stub(run, GC_STUB_UNPORTED_ENEMY);
        gc_me_leave_with_heal_shop(run);
        return;
    }
    pr_rng stream = gc_me_option_rng(run);
    pr_reward_option shop[5];
    roll_shop(run, &stream, shop);
    if (option == 0) { fight(run, &b, &stream, shop, PR_ME_POST_NONE); return; }
    if (b.speed_diff < 1) {
        /* showEncounterText is awaited before initBattleWithEnemyConfig: the battle is set up outside the option's seed offset */
        fight(run, &b, &run->run.rng, shop, run->wave < 50 ? PR_ME_POST_ENRAGED_LOW : PR_ME_POST_ENRAGED_HIGH);
        return;
    }
    /* the Pokemon wins the race: the berries, its experience, the shop of the five */
    const int count = grabbed(&b);
    give_berries(run, count, b.fastest);
    gc_items_to_battle(run);
    pr_reward_custom *c = &gc_int(run)->reward.me_custom;
    memset(c, 0, sizeof *c);
    for (int i = 0; i < 5; i++) c->options[c->n_options++] = shop[i];
    c->fill_remaining = false;
    c->reroll_multiplier = NAN;
    c->allow_luck_upgrades = true;
    if (b.fastest >= 0) gc_me_steal_exp(run, b.fastest, b.boss.b.species);
    gc_me_end_shop(run);
}
