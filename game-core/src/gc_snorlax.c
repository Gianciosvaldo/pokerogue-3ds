/* Slumbering Snorlax (src/data/mystery-encounters/encounters/slumbering-snorlax-encounter.ts at pinned upstream 8555c08).
 *
 * onInit (executeWithSeedOffset(.., waveIndex)): the boss's EnemyPokemonConfig - Snorlax, a boss, not shiny, asleep with six turns left,
 * Docile, Body Slam / Crunch / Sleep Talk / Rest, a Sitrus Berry, an Enigma Berry, an HP Up, and a Soothe Bell and a Lucky Egg of
 * randSeedInt(2, 0) each (the only rolls: Soothe Bell's first, then Lucky Egg's; a count of zero is no item). The levels are the
 * wave's plus Math.round(wave / 10 * 0.5). Nothing of it is kept: the two stack counts are worked out again from the wave seed when the
 * fight starts, as Fight or Flight does.
 *
 * Option 1, battle (onOptionPhase, executeWithSeedOffset(.., getSeedOffset() * 500)): setEncounterRewards({guaranteedModifierTypeFuncs:
 * [LEFTOVERS], fillRemaining: true}); startOfBattleEffects gets Snore from the boss at the player's lead; initBattleWithEnemyConfig as
 * Dark Deal's (getLevelForWave on the stream, the boss made by addEnemyPokemon: both IV rolls), then the config applied over it, and
 * the held items given as listed (no item rolls). The player cannot flee. Winning opens the Leftovers shop (and the rest of the
 * ordinary rewards).
 * Option 2, rest: the party is healed (PartyHealPhase), the result message, and the encounter leaves without a heal shop.
 * Option 3, steal (a Pokemon allowed in battle with Pluck, Covet, Knock Off, Thief, Trick or Switcheroo; the first in party order):
 * setEncounterRewards({LEFTOVERS, fillRemaining: false}), setEncounterExp(instance.primaryPokemon.id, Snorlax's base experience), and
 * the encounter leaves without a heal shop. Note that `instance.primaryPokemon` is the ENCOUNTER's primary Pokemon, not the option's: with
 * no encounter-level requirement it is the first Pokemon on the field that is allowed in battle, else the first allowed in the party,
 * so the experience goes to the lead, not to the Pokemon that stole (upstream's quirk, ported as it is).
 *
 * Not ported: the boss's sprite scale of 1.25 (customPokemonData, the app's), the intro sprite's tint and shadow. */
#include "internal.h"
#include <math.h>
#include <string.h>

/* the boss's listed items; the two counts are onInit's rolls */
static int items_of(gc_run *run, gc_item *out)
{
    const pr_rng saved = run->run.rng;
    pr_rng r = pr_run_seed_offset_rng(&run->run, run->wave, NULL);
    const int soothe = pr_rng_seed_int(&r, 2, 0);
    const int egg = pr_rng_seed_int(&r, 2, 0);
    run->run.rng = saved;
    memset(out, 0, 5 * sizeof *out);
    int n = 0;
    out[n++] = (gc_item){.key = "BERRY", .arg = BERRY_TYPE_SITRUS, .stack = 1};
    out[n++] = (gc_item){.key = "BERRY", .arg = BERRY_TYPE_ENIGMA, .stack = 1};
    out[n++] = (gc_item){.key = "BASE_STAT_BOOSTER", .arg = STAT_HP, .stack = 1};
    /* generateEnemyModifiers gives the stack count as listed, and a held item of none is not held */
    if (soothe > 0) out[n++] = (gc_item){.key = "SOOTHE_BELL", .arg = -1, .stack = soothe};
    if (egg > 0) out[n++] = (gc_item){.key = "LUCKY_EGG", .arg = -1, .stack = egg};
    return n;
}

void gc_snorlax_tokens(gc_run *run, pr_msg_arg *name)
{
    (void)run;
    *name = pr_arg_key("snorlaxName", "pokemon:snorlax");
}

static void reward_funcs(pr_reward_custom *c, bool fill)
{
    memset(c, 0, sizeof *c);
    c->funcs[c->n_funcs++] = (uint16_t)gc_reward_item_index("LEFTOVERS");
    c->fill_remaining = fill;
    c->reroll_multiplier = NAN;
    c->allow_luck_upgrades = true;
}

static void fight(gc_run *run)
{
    gc_internal *in = gc_int(run);
    static gc_item items[5];
    const int n_items = items_of(run, items);

    /* the option's stream: initBattleWithEnemyConfig's getLevelForWave, then the boss */
    run->double_battle = false;
    pr_rng r = gc_me_option_rng(run);
    const int base = pr_run_get_level_for_wave(&run->run, &r, run->wave);
    const int level = base + (int)fmax(floor((double)run->wave / 10 * 0.5 + 0.5), 0);
    gc_mon *boss = &run->enemy[0];
    if (!gc_me_new_boss(run, &r, SPECIES_ID_SNORLAX, level, true, boss)) {
        /* a species or biome run-core has no rolls for: nobody sleeps on the road, the encounter ends as if declined */
        gc_stub(run, GC_STUB_UNPORTED_ENEMY);
        run->n_enemy = 0;
        gc_me_leave_with_heal_shop(run);
        return;
    }
    run->n_enemy = 1;
    run->battle_type = BATTLE_TYPE_WILD;
    /* the clean data, then the config */
    boss->b.passive_flag = false;
    boss->b.shiny = false;
    boss->b.nature = NATURE_DOCILE;
    boss->base_nature = NATURE_DOCILE;
    boss->b.status = STATUS_EFFECT_SLEEP;
    boss->b.status_sleep_turns = 6;
    boss->b.n_moves = 0;
    memset(boss->b.moves, 0, sizeof boss->b.moves);
    memset(boss->b.pp_used, 0, sizeof boss->b.pp_used);
    memset(boss->b.pp_up, 0, sizeof boss->b.pp_up);
    for (int i = 0; i < 4; i++) boss->b.max_pp_override[i] = -1;
    gc_mon_set_move(boss, 0, MOVE_ID_BODY_SLAM);
    gc_mon_set_move(boss, 1, MOVE_ID_CRUNCH);
    gc_mon_set_move(boss, 2, MOVE_ID_SLEEP_TALK);
    gc_mon_set_move(boss, 3, MOVE_ID_REST);

    gc_enemy_finish_party_items(run, items, n_items);
    in->me_reward.active = true;
    in->me_reward.has_item = false;
    in->me_reward.func = (int16_t)gc_reward_item_index("LEFTOVERS");
    in->me_reward.fill = true;
    in->me_no_flee = true;
    gc_me_add_start_effect(run, 2, 0, MOVE_ID_SNORE);
    gc_me_begin_battle(run, GC_ME_SLUMBERING_SNORLAX);
}

void gc_snorlax_execute(gc_run *run, int option)
{
    if (option == 0) { fight(run); return; }
    if (option == 1) {
        gc_rq_push(run, gc_rp(RP_PARTY_HEAL, 0, 0, 0));
        gc_ev_msg(run, "mysteryEncounters/slumberingSnorlax:option.2.restResult", 0);
        gc_me_leave(run);
        return;
    }
    reward_funcs(&gc_int(run)->reward.me_custom, false);
    /* the encounter's primaryPokemon: the first Pokemon allowed in battle in party order (the field slots are its first ones) */
    for (int k = 0; k < run->n_party; k++) {
        const int idx = run->party_order[k];
        if (!gc_mon_allowed(&run->party[idx])) continue;
        gc_me_steal_exp(run, idx, SPECIES_ID_SNORLAX);
        break;
    }
    gc_me_end_shop(run);
}
