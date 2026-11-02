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
    /* the rest of the constructor and the postProcess run on this member's RNG (executeWithSeedOffset spans all of genPartyMember's genMon) */
    out->evil_pending = true;
    out->evil_closure = (int)(cl - pr_evil_closures);
    out->evil_rng = run->rng;
    return 1;
}

static bool guard_holds(const pr_evil_op *op, const pr_enemy_gen_ctx *ctx, const pr_enemy_gen *g)
{
    switch (op->g) {
    case PR_EG_SPECIES_EQ: return ctx->species == op->gv;
    case PR_EG_SPECIES_NE: return ctx->species != op->gv;
    case PR_EG_WAVE_EQ: return ctx->wave == op->gv;
    case PR_EG_WAVE_NE: return ctx->wave != op->gv;
    case PR_EG_FORM_EQ: return g->form == op->gv;
    case PR_EG_LACKS_MOVE:
        for (int k = 0; k < 4; k++) if (g->moveset[k] == op->gv) return false;
        return true;
    default: return true;
    }
}

/* addEnemyPokemon's postProcess(pokemon), after the constructor: the closure's ops in order, each IF skipping its block when its condition fails */
int pr_run_evil_finish(pr_run *run, const pr_enemy_gen_ctx *ctx, pr_enemy_gen *out)
{
    (void)run;
    if (!out->evil_pending) return 0;
    out->evil_pending = false;
    const pr_evil_closure *cl = &pr_evil_closures[out->evil_closure];
    pr_rng *rng = &out->evil_rng;
    for (int i = 0; i < cl->op_n; i++) {
        const pr_evil_op *op = &pr_evil_ops[cl->op_start + i];
        switch (op->op) {
        case PR_EOP_IF: if (!guard_holds(op, ctx, out)) i += op->a; break;
        case PR_EOP_REGEN:
            if (ctx->post_regen && ctx->post_regen(ctx->post_regen_user, out, rng) < 0) return -1;
            break;
        case PR_EOP_BALL: out->pokeball = op->a; break;
        case PR_EOP_BOSS: out->boss_segments = op->a; break;
        case PR_EOP_ABILITY: out->ability_index = op->a; break;
        case PR_EOP_GENDER: out->gender = op->a; break;
        case PR_EOP_FORM: out->form = op->a; break;
        case PR_EOP_FORM_RAND: out->form = op->a <= 1 ? op->b : pr_rng_integer_in_range(rng, op->b, op->a - 1 + op->b); break;   /* randSeedInt(range, min) */
        case PR_EOP_FRIENDSHIP: out->friendship = op->a; break;
        case PR_EOP_MOVE: out->moveset[op->a] = op->b; break;
        case PR_EOP_MOVE_RAND: {
            const int n = op->b;   /* randSeedItem: one item needs no roll */
            out->moveset[op->a] = pr_evil_op_items[op->c + (n == 1 ? 0 : pr_rng_integer_in_range(rng, 0, n - 1))];
            break;
        }
        case PR_EOP_REPLACE_MOVE:
            for (int k = 0; k < 4; k++) if (out->moveset[k] == op->a) { out->moveset[k] = op->b; break; }
            break;
        case PR_EOP_MOVESET:
            for (int k = 0; k < 4; k++) out->moveset[k] = pr_evil_op_items[op->a + k];
            break;
        default: return -1;
        }
    }
    return 0;
}
