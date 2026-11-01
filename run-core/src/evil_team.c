/* The evil teams' admins and bosses: the members that have a partyMemberFunc (src/data/trainers/trainer-config.ts, getRandomPartyMemberFunc).
 * The data is gen/out/run_evil_closures.c (tools/gen-evil-closures.py reads it off the pinned upstream's source). Same member generation as the Gym/E4
 * explicit closures (gym_e4.c): randSeedItem over the pool, getTrainerSpeciesForLevel unless the closure ignores evolution, then addEnemyPokemon.
 * What the postProcess callbacks do (ability slot, forms, boss segments, balls, movesets) happens after the constructor and is not ported. */
#include "pr/run.h"

static const pr_evil_closure *find_closure(int type, int slot)
{
    for (int i = 0; i < pr_evil_closure_count; i++)
        if (pr_evil_closures[i].type == type && pr_evil_closures[i].slot == slot) return &pr_evil_closures[i];
    return NULL;
}

int pr_run_gen_evil_closure_member(pr_run *run, const pr_trainer_party_ctx *ctx, int index, int func_key, pr_enemy_gen *out, int *out_species)
{
    const pr_evil_closure *cl = func_key >= 0 ? find_closure(ctx->enemy.trainer_type, func_key) : NULL;
    if (!cl || !cl->n) return 0;
    const int pick = cl->n == 1 ? 0 : pr_rng_integer_in_range(&run->rng, 0, cl->n - 1);   /* randSeedItem: one item needs no roll */
    int species = pr_evil_closure_species[cl->start + pick];
    const pr_party_template *tpl = ctx->template_;
    const int level = ctx->levels[index];
    if (!cl->ignore_evolution) {
        species = pr_run_determine_enemy_species(&run->rng, species, level, true, (tpl->member[index] >> 2) & 3, true, ctx->classic_wave20_trainer);
        if (species < 0) return -1;
    }
    pr_enemy_gen_ctx ec = ctx->enemy;
    ec.species = species;
    ec.level = level;
    ec.trainer_slot = 1;
    ec.boss = false;
    ec.shiny_lock = false;
    if (pr_run_enemy_pokemon_base(run, &ec, out) < 0) return -1;
    *out_species = species;
    return 1;
}
