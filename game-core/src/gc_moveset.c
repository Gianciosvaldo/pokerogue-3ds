/* Wild constructor movesets: retain the clean factory, then weighted level pools.
 * Trainer member streams and post-moveset form callbacks remain separate owners. */
#include "internal.h"
#include "pr/moveset.h"

static bool clean_enemy_moveset(gc_run *run, gc_mon *m, const pr_enemy_gen *g)
{
    /* Trainer generation currently restores executeWithSeedOffset before returning
     * its constructor snapshots. Using the run RNG here would shift the wrong stream.
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
        || (m->b.is_boss && m->b.fusion_species) || m->b.passive_flag) return false;
    const pr_species *sp = gc_species(m->b.species);
    if (!sp || (sp->form_change_triggers & (1u << PR_FCT_SpeciesFormChangeMoveLearnedTrigger)))
        return false; /* The constructor's post-moveset form callback is separate. */
    if (g->fusion_species == 0 || (g->fusion_species > 0) != !!m->b.fusion_species)
        return false;
    if (m->b.fusion_species && (g->fusion_species != m->b.fusion_species
        || g->fusion_form != m->b.fusion_form
        || g->fusion_ability_index != m->b.fusion_ability_index
        || g->fusion_gender != m->b.fusion_gender)) return false;
    /* Fresh Spliced constructors above level one always have a fusion. */
    if (run->run.mode == GAME_MODES_SPLICED_ENDLESS && m->b.level > 1 && !m->b.fusion_species)
        return false;
    return gc_generate_moveset(run, &run->run.rng, m);
}
