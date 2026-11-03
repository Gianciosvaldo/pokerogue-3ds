/* The Strong Stuff (src/data/mystery-encounters/encounters/the-strong-stuff-encounter.ts at pinned upstream 8555c08).
 *
 * onInit rolls nothing: it builds the config of the Shuckle (a boss with five bars, never shiny, level boost of a full one per ten waves,
 * Infestation / Salt Cure / Gastro Acid / Heal Order, Sitrus, Enigma, Apicot, Ganlon and two Lum Berries, no switching). The party needs three
 * Pokemon that can fight; the encounter comes once per run; fleeing is off.
 *
 * Option 1, approach (no RNG): the party sorted by the base stat total of its species form, highest first (Array.sort is stable: a tie keeps
 * party order). The first two get Foul Shuckle Juice (-15 to every base stat, halved for HP), the rest Sweet Shuckle Juice (+10), each through
 * addModifier(.., instant) and calculateStats. The result text names the two; the shop is the ordinary one (setEncounterRewards
 * ({fillRemaining: true}), which also keeps the heal shop away), and the encounter leaves.
 * Option 2, battle: setEncounterRewards({guaranteedModifierTypeFuncs: [SOUL_DEW], fillRemaining: true}), startOfBattleEffects Gastro Acid and
 * then Stealth Rock from the Shuckle at the player's lead, and initBattleWithEnemyConfig after an await, so on the run's own RNG: the level, the
 * boss (both IV rolls) with the config applied over it (nature Hardy is the enum's 0, which `if (config.nature)` skips: the rolled one stays;
 * five bars; the listed items, no item rolls), and the boss's post-summon effect: its message, then Def and Sp. Def up one stage (battle-core).
 * The player cannot flee, cannot catch it, and is not asked whether to switch.
 *
 * Not ported: the Shuckle's sprite scale (the app's), the outro text, and Foul/Sweet Shuckle Juice already at its maximum stack of two (only
 * reachable with juice from elsewhere: the encounter comes once per run). */
#include "internal.h"
#include <math.h>
#include <string.h>

#define HIGH_BST_REDUCTION_VALUE 15
#define BST_INCREASE_VALUE 10

static int base_stat_total(const gc_mon *m)
{
    const pr_form *f = gc_form(m->b.species, m->b.form);
    int sum = 0;
    for (int i = 0; i < 6; i++) sum += f->base_stats[i];
    return sum;
}

/* option 1 */
static void approach(gc_run *run)
{
    int order[PR_MAX_PARTY], n = 0;
    for (int k = 0; k < run->n_party; k++) order[n++] = run->party_order[k];
    /* a stable sort by base stat total, highest first */
    for (int i = 1; i < n; i++) {
        const int v = order[i];
        int j = i - 1;
        while (j >= 0 && base_stat_total(&run->party[order[j]]) < base_stat_total(&run->party[v])) { order[j + 1] = order[j]; j--; }
        order[j + 1] = v;
    }
    pr_msg_arg high[2];
    for (int i = 0; i < n; i++) {
        gc_mon *m = &run->party[order[i]];
        const int amount = i < 2 ? -HIGH_BST_REDUCTION_VALUE : BST_INCREASE_VALUE;
        int held = 0;
        for (int k = 0; k < run->n_items; k++) {
            const gc_item *it = &run->items[k];
            if (!strcmp(it->key, "MYSTERY_ENCOUNTER_SHUCKLE_JUICE") && it->arg == amount && it->pokemon_id == m->b.id) held = it->stack + it->virtual_stack;
        }
        if (held >= 2) gc_stub(run, GC_STUB_REWARD_APPLY);
        else gc_item_add(run, "MYSTERY_ENCOUNTER_SHUCKLE_JUICE", (int16_t)amount, m->b.id, 1);
        gc_me_update_modifiers(run);
        if (i < 2) high[i] = pr_arg_pokemon_name(&run->bt, i ? "highBstPokemon2" : "highBstPokemon1", &m->b);
    }
    if (n < 2) { high[1] = high[0]; }
    gc_ev_msg(run, "mysteryEncounters/theStrongStuff:option.1.selected2", 4, high[0], high[1],
              pr_arg_int("reductionValue", HIGH_BST_REDUCTION_VALUE), pr_arg_int("increaseValue", BST_INCREASE_VALUE));
    /* setEncounterRewards({fillRemaining: true}); leaveEncounterWithoutBattle(true): the custom shop wins over the heal shop */
    pr_reward_custom *c = &gc_int(run)->reward.me_custom;
    memset(c, 0, sizeof *c);
    c->fill_remaining = true;
    c->reroll_multiplier = NAN;
    c->allow_luck_upgrades = true;
    gc_me_end_shop(run);
}

/* option 2 */
static void fight(gc_run *run)
{
    gc_internal *in = gc_int(run);
    static const gc_item items[5] = {
        {.key = "BERRY", .arg = BERRY_TYPE_SITRUS, .stack = 1}, {.key = "BERRY", .arg = BERRY_TYPE_ENIGMA, .stack = 1},
        {.key = "BERRY", .arg = BERRY_TYPE_APICOT, .stack = 1}, {.key = "BERRY", .arg = BERRY_TYPE_GANLON, .stack = 1},
        {.key = "BERRY", .arg = BERRY_TYPE_LUM, .stack = 2},
    };
    run->double_battle = false;
    /* the first await (transitionMysteryEncounterIntroVisuals) took the rest out of the option's seed offset: the run's RNG */
    pr_rng r = run->run.rng;
    const int base = pr_run_get_level_for_wave(&run->run, &r, run->wave);
    const int level = base + (int)fmax(floor((double)run->wave / 10 * 1.0 + 0.5), 0);
    gc_mon *boss = &run->enemy[0];
    if (!gc_me_new_boss(run, &r, SPECIES_ID_SHUCKLE, level, true, boss)) {
        gc_stub(run, GC_STUB_UNPORTED_ENEMY);
        run->n_enemy = 0;
        gc_me_leave_with_heal_shop(run);
        return;
    }
    run->run.rng = r;
    run->n_enemy = 1;
    run->battle_type = BATTLE_TYPE_WILD;
    /* the config: five bars, never shiny, its moves; Hardy (0) is skipped by `if (config.nature)` */
    boss->b.boss_segments = 5;
    boss->b.boss_segment_index = 4;
    boss->b.is_boss = true;
    boss->b.passive_flag = false;
    boss->b.shiny = false;
    boss->b.n_moves = 0;
    memset(boss->b.moves, 0, sizeof boss->b.moves);
    memset(boss->b.pp_used, 0, sizeof boss->b.pp_used);
    memset(boss->b.pp_up, 0, sizeof boss->b.pp_up);
    for (int i = 0; i < 4; i++) boss->b.max_pp_override[i] = -1;
    gc_mon_set_move(boss, 0, MOVE_ID_INFESTATION);
    gc_mon_set_move(boss, 1, MOVE_ID_SALT_CURE);
    gc_mon_set_move(boss, 2, MOVE_ID_GASTRO_ACID);
    gc_mon_set_move(boss, 3, MOVE_ID_HEAL_ORDER);
    boss->b.me_post_summon = PR_ME_POST_DEF_SPDEF;
    gc_enemy_finish_party_items(run, items, 5);

    in->me_reward.active = true;
    in->me_reward.has_item = false;
    in->me_reward.func = (int16_t)gc_reward_item_index("SOUL_DEW");
    in->me_reward.fill = true;
    in->me_no_flee = true;
    in->me_no_switch = true;
    gc_me_add_start_effect(run, 2, 0, MOVE_ID_GASTRO_ACID);
    gc_me_add_start_effect(run, 2, 0, MOVE_ID_STEALTH_ROCK);
    gc_me_begin_battle(run, GC_ME_THE_STRONG_STUFF);
}

void gc_strong_execute(gc_run *run, int option)
{
    if (option == 0) approach(run);
    else fight(run);
}
