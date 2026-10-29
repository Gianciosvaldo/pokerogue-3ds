/* Training Session (src/data/mystery-encounters/encounters/training-session-encounter.ts at pinned upstream 8555c08).
 *
 * Options 1 to 3 pick a Pokemon (selectPokemonForOption: any that isPokemonValidForEncounterOptionSelection passes, that is, not fainted),
 * options 2 and 3 then a nature (all 25, "(+)" when the species' dex entry has not unlocked it) or an ability (the species form's slots:
 * ability1, ability2 and the hidden one if there is one; a slot whose ability an earlier UNMARKED entry already lists is left out, because
 * the check compares the label with the plain ability name and a "(+) " entry never equals it). Option 4 leaves with the heal shop.
 *
 * onOptionPhase (executeWithSeedOffset(.., getSeedOffset() * 500)): the segments are min(2 + floor(wave / 50), 5) for the light training,
 * min(2 + floor(wave / 40), 6) for the moderate and min(2 + floor(wave / 30), 6) for the heavy one; the Pokemon's summon data is reset;
 * its held items are noted; the Pokemon leaves the party (removePokemonFromPlayerParty(.., false): the modifiers it held go with the
 * update that follows) and its clone, a boss from its own data (initBattleWithEnemyConfig: getLevelForWave on the stream, a new id from
 * it, full HP, no status, no passive flag), fights the rest of the party with clones of its items (not transferable, stacks as they are).
 * The battle cannot be fled, and the enemy is not counted as seen. Winning opens the ordinary rewards, filled up.
 *
 * onBeforeRewardsPhase, in the rewards phase's executeWithSeedOffset(.., wave * 1000): light: two random IVs that are below 31 (shuffled
 * with randSeedShuffle, popped from the end) get +10 (<= 10), +5 (<= 20) or +3, one more for an IV of 1, capped at 31; the dex' IVs of the
 * species' starter root and its prevolutions follow and the Pokemon is counted as caught again. Moderate: the nature becomes the chosen one
 * as a custom nature and the dex unlocks it. Heavy: the ability index becomes the chosen slot. Then the Pokemon returns at the END of the
 * party with its items (added at the end of the item list) and the item update.
 *
 * Not ported: fusions (their ability index and dex rules), the 'MYSTERY_ENCOUNTER_POST_SUMMON' tag of the heavy training (it has nothing
 * to do: the encounter sets no battle effects), the sprites. A party with nobody else able to fight is a lost game. */
#include "internal.h"
#include <math.h>
#include <string.h>

/* isPokemonValidForEncounterOptionSelection: allowed in challenge (not modelled) and in battle */
bool gc_training_pick_ok(const gc_run *run, int idx)
{
    return idx >= 0 && idx < run->n_party && gc_mon_allowed(&run->party[idx]);
}

/* the dex entry of the species itself, not its root, answers checkSpeciesNatureUnlocked */
bool gc_training_nature_new(const gc_run *run, int member, int nature)
{
    if (!run->sys || member < 0 || member >= run->n_party || nature < 0 || nature > 24) return true;
    const uint16_t species = run->party[member].b.species;
    return !(run->sys->dex[species].nature_attr & (1u << (nature + 1)));
}

static int ability_of_slot(const gc_mon *m, int slot)
{
    const pr_form *f = gc_form(m->b.species, m->b.form);
    return slot == 0 ? f->ability1 : slot == 1 ? f->ability2 : f->ability_hidden;
}

int gc_ability_of_slot(const gc_run *run, int member, int slot)
{
    if (member < 0 || member >= run->n_party || slot < 0 || slot > 2) return 0;
    return ability_of_slot(&run->party[member], slot);
}

/* the ability list of option 3: slots and whether each is new to the starter (checkStarterAbilityIndexUnlocked) */
int gc_training_abilities(const gc_run *run, int member, int slots[3], bool is_new[3])
{
    if (member < 0 || member >= run->n_party) return 0;
    const gc_mon *m = &run->party[member];
    const pr_form *f = gc_form(m->b.species, m->b.form);
    const int count = f->ability_hidden == 0 ? 2 : 3;
    const uint16_t root = gc_species_root_for_starter(m->b.species);
    int n = 0;
    for (int i = 0; i < count; i++) {
        const int id = ability_of_slot(m, i);
        const bool fresh = run->sys ? !(run->sys->starters[root].ability_attr & (1u << i)) : true;
        bool dup = false;
        for (int j = 0; j < n; j++) if (!is_new[j] && ability_of_slot(m, slots[j]) == id) dup = true;
        if (dup) continue;
        slots[n] = i;
        is_new[n] = fresh;
        n++;
    }
    return n;
}

static int segments_for(int wave, int option)
{
    const int every = option == 0 ? 50 : option == 1 ? 40 : 30;
    const int cap = option == 0 ? 5 : 6;
    const int s = 2 + wave / every;
    return s < cap ? s : cap;
}

void gc_training_execute(gc_run *run, int option, int member, int choice)
{
    if (option == 3) { gc_me_leave_with_heal_shop(run); return; }
    gc_internal *in = gc_int(run);
    if (!gc_training_pick_ok(run, member)) {
        /* the party screen would not let a Pokemon the filter refuses be picked: take the first that passes */
        member = -1;
        for (int i = 0; member < 0 && i < run->n_party; i++) if (gc_training_pick_ok(run, i)) member = i;
        if (member < 0) { gc_me_leave_with_heal_shop(run); return; }
    }
    int nature = 0, slot = 0;
    if (option == 1) nature = choice >= 0 && choice <= 24 ? choice : 0;
    if (option == 2) {
        int slots[3];
        bool fresh[3];
        const int n = gc_training_abilities(run, member, slots, fresh);
        slot = n > 0 ? slots[0] : 0;
        for (int i = 0; i < n; i++) if (slots[i] == choice) slot = choice;
    }

    /* getEnemyConfig: resetSummonData, the held items (the clones and the originals) */
    gc_mon *away = &in->training.mon;
    *away = run->party[member];
    pr_reset_summon_data(&away->b);
    in->training.n_kept = 0;
    for (int i = 0; i < run->n_items; i++) {
        const gc_item *it = &run->items[i];
        if (it->pokemon_id != away->b.id || it->pokemon_id == 0) continue;
        if (in->training.n_kept >= 64) { gc_stub(run, GC_STUB_REWARD_APPLY); break; }
        in->training.kept[in->training.n_kept++] = *it;
    }
    in->training.kind = (int8_t)option;
    in->training.nature = (int8_t)nature;
    in->training.ability = (int8_t)slot;
    in->training.away = true;

    /* removePokemonFromPlayerParty(pokemon, false) and its updateModifiers(true) */
    gc_party_remove(run, member);
    gc_me_prune_items(run);
    gc_me_update_modifiers(run);
    bool able = false;
    for (int i = 0; i < run->n_party; i++) able = able || gc_mon_allowed(&run->party[i]);
    if (!able) {
        /* nobody left to fight the clone: the run is over */
        in->training.away = false;
        gc_rq_clear(run);
        gc_rq_unshift(run, gc_rp(RP_GAME_OVER, 0, 0, 0));
        return;
    }

    /* initBattleWithEnemyConfig: a single battle; getLevelForWave on the option's stream, then the boss's new id */
    run->double_battle = false;
    pr_rng r = gc_me_option_rng(run);
    (void)pr_run_get_level_for_wave(&run->run, &r, run->wave);
    gc_mon *e = &run->enemy[0];
    *e = *away;
    e->b.is_player = false;
    e->b.on_field = false;
    e->b.has_trainer = false;
    e->b.trainer_slot = 0;
    e->trainer_slot = 0;
    e->b.id = (uint32_t)floor(pr_rng_real_in_range(&r, 0, 4294967296.0));
    e->b.status = STATUS_EFFECT_NONE;
    e->b.passive_flag = false;
    const int segments = segments_for(run->wave, option);
    e->b.is_boss = true;
    e->b.boss_segments = (uint8_t)segments;
    e->b.boss_segment_index = (uint8_t)(segments - 1);
    e->b.hp = e->b.stats[STAT_HP];
    run->n_enemy = 1;
    run->battle_type = BATTLE_TYPE_WILD;

    /* modifierConfigs: clones of the held items, not transferable, stacks as they are */
    static gc_item given[64];
    for (int i = 0; i < in->training.n_kept; i++) {
        given[i] = in->training.kept[i];
        given[i].non_transferable = true;
    }
    in->me_no_stats = true;
    gc_enemy_finish_party_items(run, given, in->training.n_kept);
    in->me_no_flee = true;
    in->me_reward.active = true;
    in->me_reward.has_item = false;
    in->me_reward.func = -1;
    in->me_reward.fill = true;
    in->me_reward.before = true;
    gc_me_begin_battle(run, GC_ME_TRAINING_SESSION);
}

/* onBeforeRewardsPhase (the rewards phase runs it in executeWithSeedOffset(.., wave * 1000)) */
void gc_training_return(gc_run *run)
{
    gc_internal *in = gc_int(run);
    if (!in->training.away) return;
    in->training.away = false;
    gc_mon *m = &in->training.mon;
    pr_rng r = pr_run_seed_offset_rng(&run->run, run->wave * 1000, NULL);
    const int kind = in->training.kind;
    const pr_msg_arg name_arg = pr_arg_pokemon_name(&run->bt, "selectedPokemon", &m->b);

    if (kind == 1) {
        gc_ev_msg(run, "mysteryEncounters/trainingSession:option.2.finished", 2, name_arg, pr_arg_key("nature", gc_nature_key(in->training.nature)));
        if (!m->has_custom_nature) m->base_nature = m->b.nature;
        m->has_custom_nature = true;
        m->b.nature = (uint8_t)in->training.nature;
        pr_recalculate_stats(&run->bt, &m->b);
        if (run->sys) gc_system_unlock_species_nature(run->sys, m->b.species, (uint8_t)in->training.nature);
    } else if (kind == 2) {
        gc_ev_msg(run, "mysteryEncounters/trainingSession:option.3.finished", 2, name_arg,
                  pr_arg_ability("ability", ability_of_slot(m, in->training.ability) ? ability_of_slot(m, in->training.ability) : gc_form(m->b.species, 0)->ability1));
        m->ability_index = (uint8_t)in->training.ability;
        m->b.ability_index = (int8_t)in->training.ability;
        m->b.ability = m->b.base_ability = (uint16_t)gc_form_ability(m->b.species, gc_form(m->b.species, m->b.form), in->training.ability);
        pr_recalculate_stats(&run->bt, &m->b);
        if (run->sys) gc_system_set_pokemon_caught(run->sys, m, false, false);
    }
    int stat1 = -1, stat2 = -1;
    if (kind == 0) {
        struct { int iv, index; } list[6];
        int n = 0, improved = 0;
        for (int i = 0; i < 6; i++) if (m->b.ivs[i] < 31) { list[n].iv = m->b.ivs[i]; list[n].index = i; n++; }
        while (n > 0 && improved < 2) {
            for (int i = n - 1; i > 0; i--) {   /* randSeedShuffle */
                const int j = pr_rng_seed_int(&r, i + 1, 0);
                const int iv = list[i].iv, index = list[i].index;
                list[i] = list[j];
                list[j].iv = iv;
                list[j].index = index;
            }
            const int iv = list[n - 1].iv, index = list[n - 1].index;
            n--;
            if (improved == 0) stat1 = index; else stat2 = index;
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
    }

    /* the Pokemon joins the end of the party, its items the end of the item list, and the modifiers are updated */
    if (run->n_party < PR_MAX_PARTY) {
        run->party[run->n_party] = *m;
        run->party_order[run->n_party] = (int8_t)run->n_party;
        run->n_party++;
        for (int i = 0; i < in->training.n_kept && run->n_items < GC_MAX_ITEMS && run->n_items < PR_MAX_MODIFIERS; i++) run->items[run->n_items++] = in->training.kept[i];
    }
    gc_me_update_modifiers(run);
    /* the battle's view of the party (the reward shop and the level-ups after it read it) has the Pokemon again */
    run->bt.n_party[0] = (uint8_t)run->n_party;
    for (int k = 0; k < run->n_party; k++) run->bt.party[0][k] = &run->party[run->party_order[k]].b;
    if (kind == 0) {
        static const char *const stat_keys[6] = {"pokemonInfo:stat.hp", "pokemonInfo:stat.atk", "pokemonInfo:stat.def", "pokemonInfo:stat.spatk",
                                                  "pokemonInfo:stat.spdef", "pokemonInfo:stat.spd"};
        gc_ev_msg(run, "mysteryEncounters/trainingSession:option.1.finished", 3, name_arg,
                  pr_arg_key("stat1", stat1 >= 0 ? stat_keys[stat1] : "-"), pr_arg_key("stat2", stat2 >= 0 ? stat_keys[stat2] : "-"));
    }
}
