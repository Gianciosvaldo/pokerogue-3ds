/* Wild constructor movesets: retain the clean factory, then weighted level pools.
 * Trainer members continue inside their seeded constructor in gc_encounter.c.
 * A wild Keldeo's form change on learning Secret Sword is at the end of this file. */
#include "internal.h"
#include "pr/moveset.h"

static bool clean_enemy_moveset(gc_run *run, gc_mon *m, const pr_enemy_gen *g)
{
    /* Wild dispatch cannot own a trainer member's executeWithSeedOffset stream.
     * The trainer continuation in gc_encounter.c handles its validated domain.
     * Daily/custom and challenge constructors need their own validated boundary. */
    if (m->b.has_trainer || m->trainer_slot || run->me_battle || run->battle_type != BATTLE_TYPE_WILD
        || (run->run.mode != GAME_MODES_CLASSIC && run->run.mode != GAME_MODES_ENDLESS)
        || run->run.has_challenges || g->fusion_species >= 0 || m->b.fusion_species)
        return false;

    /* Before enemy items/summon effects, calculate constructor stats on a copy.
     * run->bt still describes the previous battle and can contain its modifiers. */
    pr_battle constructor = {0};
    constructor.game_mode = run->run.mode;
    constructor.wave_index = run->wave;
    pr_pokemon p = m->b;
    pr_update_passive(&constructor, &p);
    pr_calculate_stats(&constructor, &p);
    pr_moveset_ctx ctx = {
        .species = p.species, .form = p.form, .level = p.level,
        .ability = p.ability, .attack = p.stats[STAT_ATK],
        .special_attack = p.stats[STAT_SPATK], .boss = p.is_boss,
        .passive = pr_has_passive(&constructor, &p),
    };
    pr_generated_moveset generated;
    if (pr_run_enemy_moveset(&run->run.rng, &ctx, &generated) != PR_MOVESET_OK)
        return false; /* Task 63 guarantees unchanged RNG/output on rejection. */

    memset(m->b.moves, 0, sizeof m->b.moves);
    memset(m->b.pp_used, 0, sizeof m->b.pp_used);
    memset(m->b.pp_up, 0, sizeof m->b.pp_up);
    m->b.n_moves = 0;
    for (int i = 0; i < 4; i++) m->b.max_pp_override[i] = -1;
    for (int i = 0; i < generated.count; i++) {
        gc_mon_set_move(m, i, generated.moves[i]);
        /* Keep the Source PokemonMove metadata, including final-boss Recover -4. */
        m->b.pp_up[i] = (int8_t)generated.pp_up[i];
    }
    return true;
}

/* Broader native constructor domain: keep the independently retained clean
 * factory first, then use the weighted level-pool owner for ordinary wilds. */
bool gc_enemy_moveset(gc_run *run, gc_mon *m, const pr_enemy_gen *g)
{
    if (clean_enemy_moveset(run, m, g)) return true;
    if (m->b.has_trainer || m->trainer_slot || run->me_battle
        || run->battle_type != BATTLE_TYPE_WILD || run->run.has_challenges
        || (run->run.mode != GAME_MODES_CLASSIC && run->run.mode != GAME_MODES_ENDLESS
            && run->run.mode != GAME_MODES_SPLICED_ENDLESS)
        || m->b.passive_flag) return false;
    const pr_species *sp = gc_species(m->b.species);
    if (!sp) return false;
    /* A species whose form changes on learning a move (Keldeo) is generated like any other; the QuietFormChangePhase its constructor queues is
     * gc_enemy_learned_form_target's (gen_wild_slot) and runs once EncounterPhase is over (gc_enemy_apply_learned_forms). */
    if (g->fusion_species == 0 || (g->fusion_species > 0) != !!m->b.fusion_species)
        return false;
    if (m->b.fusion_species && (g->fusion_species != m->b.fusion_species
        || g->fusion_form != m->b.fusion_form
        || g->fusion_ability_index != m->b.fusion_ability_index
        || g->fusion_gender != m->b.fusion_gender)) return false;
    /* Native isBoss() reads bossSegments. Reject inconsistent raw snapshots. */
    if (m->b.fusion_species && m->b.is_boss != (m->b.boss_segments > 0)) return false;
    /* Fresh Spliced constructors above level one always have a fusion. */
    if (run->run.mode == GAME_MODES_SPLICED_ENDLESS && m->b.level > 1 && !m->b.fusion_species)
        return false;
    return gc_generate_moveset(run, &run->run.rng, m);
}

/* ---- a wild enemy's learned-move form change (Keldeo with Secret Sword) ----
 * Pokemon.generateAndPopulateMoveset ends with triggerPokemonFormChange(this, SpeciesFormChangeMoveLearnedTrigger), except for an enemy of a mystery
 * encounter. For an enemy Keldeo it finds the registered ordinary <-> resolute change whose condition holds (the moveset holds Secret Sword or does not; not in
 * Daily mode) and queues a QuietFormChangePhase with that change, which runs once EncounterPhase is over: the enemy is then on the field and visible, so the
 * phase changes its form and shows battlePokemonForm:formChange. EncounterPhase saves the session before that, so the save holds the form the Pokemon was made with. */
int gc_enemy_learned_form_target(gc_run *run, const gc_mon *m)
{
    const pr_species *sp = gc_species(m->b.species);
    if (!sp || m->b.fusion_species || !(sp->form_change_triggers & (1u << PR_FCT_SpeciesFormChangeMoveLearnedTrigger))) return -1;
    /* the battle record still describes the previous battle: the mode is the run's */
    const int mode = run->bt.game_mode;
    run->bt.game_mode = run->run.mode;
    uint16_t target = 0;
    const bool queued = pr_learned_form_target(&run->bt, &m->b, &target);
    run->bt.game_mode = mode;
    return queued ? target : -1;
}

/* QuietFormChangePhase.start for a wild enemy on the field: nothing if it is in the form already; else changeForm (the ability slot kept inside the new form's
 * count, the form marked seen without a count, the stats calculated again) and the form change's text */
void gc_enemy_quiet_form_change(gc_run *run, int idx, int form)
{
    gc_mon *m = &run->enemy[idx];
    if (m->b.form == form) return;
    const pr_msg_arg pre_name = gc_m_mon(run, "preName", 1, idx);
    m->b.form = (uint8_t)form;
    const pr_form *f = gc_form(m->b.species, form);
    const int count = f->ability_hidden ? 3 : 2;
    if (m->ability_index >= count) m->ability_index = (uint8_t)(count - 1);
    m->b.ability_index = (int8_t)m->ability_index;
    m->b.base_ability = (uint16_t)gc_form_ability(m->b.species, f, m->ability_index);
    if (m->b.ability_override <= 0) m->b.ability = m->b.base_ability;
    pr_refresh_ability(&m->b);
    if (run->sys) gc_system_set_pokemon_seen(run->sys, m, false, false);
    pr_change_half_form_state(&run->bt, &m->b, 0, form);
    if (m->b.ability_lookup_known) m->ability_index = (uint8_t)m->b.ability_index;
    if (form == 0) gc_ev_msg(run, "battlePokemonForm:revertChange", 1, gc_m_mon(run, "pokemonName", 1, idx));
    else if (m->b.ability == ABILITY_ID_DISGUISE) gc_ev_msg(run, "battlePokemonForm:disguiseChange", 0);
    else gc_ev_msg(run, "battlePokemonForm:formChange", 1, pre_name);
}

/* the phases the wild enemies' constructors queued, in the order they were made (a double battle's first enemy first) */
void gc_enemy_apply_learned_forms(gc_run *run)
{
    gc_internal *in = gc_int(run);
    for (int i = 0; i < run->n_enemy && i < 6; i++) {
        const int queued = in->enemy_learned_form[i];
        in->enemy_learned_form[i] = 0;
        in->enemy_pre_form[i] = 0;
        if (!queued || run->battle_type != BATTLE_TYPE_WILD || run->me_battle) continue;
        const gc_mon *m = &run->enemy[i];
        const pr_species *sp = gc_species(m->b.species);
        /* a custom rematch may have replaced the enemy since */
        if (!sp || m->b.fusion_species || !(sp->form_change_triggers & (1u << PR_FCT_SpeciesFormChangeMoveLearnedTrigger))) continue;
        if (m->b.form == queued - 1) continue;
        in->enemy_pre_form[i] = (uint8_t)(m->b.form + 1);
        gc_enemy_quiet_form_change(run, i, queued - 1);
    }
}
