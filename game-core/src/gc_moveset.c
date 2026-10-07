/* Task 64, first slice: the clean wild constructor boundary supported by Task 63.
 * Trainer member streams and the wider moveset pools remain explicit fallbacks. */
#include "internal.h"
#include "pr/moveset.h"

bool gc_enemy_moveset(gc_run *run, gc_mon *m, const pr_enemy_gen *g)
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
        /* Battle storage has unsigned PP Ups. Eternatus Recover uses -4 upstream;
         * represent its actual maximum PP with the existing override, not 252 Ups. */
        if (generated.pp_up[i] < 0) m->b.max_pp_override[i] = generated.pp[i];
        else m->b.pp_up[i] = (uint8_t)generated.pp_up[i];
    }
    return true;
}
