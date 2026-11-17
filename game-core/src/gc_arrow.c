/* The Stand Arrow: lorenzo's own mystery encounter for Bizarre runs (not upstream; custom policy, so there is no upstream differential).
 * Offered only when the app's pillar hook says the run is a Bizarre one (gc/custom.h; gc_mystery.c gates every type from GC_ME_UPSTREAM_COUNT on).
 *
 * Three options:
 *   0 Pierce one of your Pokemon (the party screen, any Pokemon that is not fainted; needs a second one able to fight). One roll on the
 *     option's stream: 60% it awakens a Stand (two random IVs below 31 rise like Training Session's light training: +10 up to 10, +5 up to 20,
 *     else +3, capped at 31), 30% nothing happens, 10% it rejects the Arrow and leaves the party for good with the items it held.
 *   1 Pierce a wild Pokemon: a boss fight that cannot be fled or caught; the victory opens an Ultra, Ultra and Great shop, filled up.
 *   2 Leave the Arrow: the heal-only shop.
 * The odds are the constants below; the screen's texts get them as tokens, so a change of one number changes what the player reads. */
#include "internal.h"
#include <math.h>
#include <string.h>

#define ARROW_AWAKEN_PERCENT 60
#define ARROW_NOTHING_PERCENT 30   /* the rest (10) rejects */

enum { ARROW_RESULT_AWAKEN, ARROW_RESULT_NOTHING, ARROW_RESULT_REJECT };

int gc_arrow_awaken_percent(void) { return ARROW_AWAKEN_PERCENT; }
int gc_arrow_lose_percent(void) { return 100 - ARROW_AWAKEN_PERCENT - ARROW_NOTHING_PERCENT; }

/* the party screen's filter for the first option: a Pokemon allowed in battle */
bool gc_arrow_pick_ok(const gc_run *run, int idx)
{
    return idx >= 0 && idx < run->n_party && gc_mon_allowed(&run->party[idx]);
}

/* the Arrow can take the pierced Pokemon: at least two are allowed in battle, so the run goes on whatever the roll says */
bool gc_arrow_pierce_ok(const gc_run *run)
{
    int able = 0;
    for (int i = 0; i < run->n_party; i++) able += gc_mon_allowed(&run->party[i]);
    return able >= 2;
}

/* the roll for a draw of the option's stream (0..99) */
int gc_arrow_result(int roll)
{
    return roll < ARROW_AWAKEN_PERCENT ? ARROW_RESULT_AWAKEN : roll < ARROW_AWAKEN_PERCENT + ARROW_NOTHING_PERCENT ? ARROW_RESULT_NOTHING : ARROW_RESULT_REJECT;
}

/* Training Session's light training (gc_training.c, gc_training_return), on `r`: up to two random IVs below 31 rise. Returns how many did, the stats
 * (0..5, -1 = none) in stat[]. The shared IV array of a fused or copied Pokemon is published, and the dex entry of the starter learns the new IVs. */
static int raise_ivs(gc_run *run, gc_mon *m, pr_rng *r, int stat[2])
{
    struct { int iv, index; } list[6];
    int n = 0, improved = 0;
    stat[0] = stat[1] = -1;
    for (int i = 0; i < 6; i++) if (m->b.ivs[i] < 31) { list[n].iv = m->b.ivs[i]; list[n].index = i; n++; }
    while (n > 0 && improved < 2) {
        for (int i = n - 1; i > 0; i--) {   /* randSeedShuffle */
            const int j = pr_rng_seed_int(r, i + 1, 0);
            const int iv = list[i].iv, index = list[i].index;
            list[i] = list[j];
            list[j].iv = iv;
            list[j].index = index;
        }
        const int iv = list[n - 1].iv, index = list[n - 1].index;
        n--;
        stat[improved] = index;
        int value = iv;
        if (iv <= 21 && iv - 1 == 0) value += 1;
        value += iv <= 10 ? 10 : iv <= 20 ? 5 : 3;
        if (value > 31) value = 31;
        m->b.ivs[index] = (uint8_t)value;
        improved++;
    }
    if (improved > 0) {
        gc_mon_publish_ivs(run, m);
        pr_recalculate_stats(&run->bt, &m->b);
        if (run->sys) {
            gc_system_update_species_dex_ivs(run->sys, gc_species_root_for_starter(m->b.species), m->b.ivs);
            gc_system_set_pokemon_caught(run->sys, m, false, false);
        }
    }
    return improved;
}

static const char *const STAT_KEYS[6] = {"pokemonInfo:stat.hp", "pokemonInfo:stat.atk", "pokemonInfo:stat.def", "pokemonInfo:stat.spatk",
                                         "pokemonInfo:stat.spdef", "pokemonInfo:stat.spd"};

static void pierce(gc_run *run, int member)
{
    if (!gc_arrow_pick_ok(run, member)) {
        /* the party screen would not let a Pokemon the filter refuses be picked: take the first that passes */
        member = -1;
        for (int i = 0; member < 0 && i < run->n_party; i++) if (gc_arrow_pick_ok(run, i)) member = i;
        if (member < 0) { gc_me_leave_with_heal_shop(run); return; }
    }
    pr_rng r = gc_me_option_rng(run);
    const int result = gc_arrow_result(pr_rng_seed_int(&r, 100, 0));
    gc_mon *m = &run->party[member];
    const pr_msg_arg name = pr_arg_pokemon_name(&run->bt, "pokemonName", &m->b);

    if (result == ARROW_RESULT_AWAKEN) {
        int stat[2];
        const int improved = raise_ivs(run, m, &r, stat);
        gc_ev_msg(run, "mysteryEncounters/standArrow:result.awakens", 1, name);
        if (improved >= 2) {
            gc_ev_msg(run, "mysteryEncounters/standArrow:result.stats", 2, pr_arg_key("stat1", STAT_KEYS[stat[0]]), pr_arg_key("stat2", STAT_KEYS[stat[1]]));
        } else if (improved == 1) {
            gc_ev_msg(run, "mysteryEncounters/standArrow:result.statsOne", 1, pr_arg_key("stat1", STAT_KEYS[stat[0]]));
        } else {
            gc_ev_msg(run, "mysteryEncounters/standArrow:result.statsNone", 0);   /* every IV was at 31 already */
        }
    } else if (result == ARROW_RESULT_NOTHING) {
        gc_ev_msg(run, "mysteryEncounters/standArrow:result.nothing", 1, name);
    } else {
        gc_ev_msg(run, "mysteryEncounters/standArrow:result.rejects", 1, name);
        gc_ev_msg(run, "mysteryEncounters/standArrow:result.gone", 1, name);
        gc_party_remove(run, member);
        gc_me_prune_items(run);
        gc_me_update_modifiers(run);
    }
    gc_me_leave_with_heal_shop(run);
}

static void wild(gc_run *run)
{
    gc_internal *in = gc_int(run);
    pr_rng stream = gc_me_option_rng(run);
    gc_mon boss;
    int level;
    if (!gc_me_roll_boss(run, &stream, &boss, &level)) {
        /* a biome or species run-core has no roll for: nobody fights, the encounter ends as if declined */
        gc_stub(run, GC_STUB_UNPORTED_ENEMY);
        gc_me_leave_with_heal_shop(run);
        return;
    }
    gc_ev_msg(run, "mysteryEncounters/standArrow:result.wild", 0);
    in->me_reward.active = true;
    in->me_reward.has_item = false;
    in->me_reward.func = -1;
    in->me_reward.fill = true;
    in->me_reward.before = false;
    in->me_reward.n_more = 0;
    in->me_reward.n_tiers = 3;
    in->me_reward.tiers[0] = MODIFIER_TIER_ULTRA;
    in->me_reward.tiers[1] = MODIFIER_TIER_ULTRA;
    in->me_reward.tiers[2] = MODIFIER_TIER_GREAT;
    in->me_exp.pending = false;   /* a boss fight with nothing set up for the experience; it cannot be fled (gc_me_start_boss_battle says so itself) */
    gc_me_start_boss_battle(run, &boss, level, PR_ME_POST_NONE, &stream, GC_ME_STAND_ARROW);
}

void gc_arrow_execute(gc_run *run, int option, int member)
{
    if (option == 0) pierce(run, member);
    else if (option == 1) wild(run);
    else gc_me_leave_with_heal_shop(run);
}
