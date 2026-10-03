/* Trainer.genPartyMember (src/field/trainer.ts) for each party member in turn, as the EncounterPhase
 * calls it: the member's own RNG (executeWithSeedOffset on the run seed), the species (partyMemberFuncs
 * are closures and report PR_RUN_UNPORTED_TRAINER; the double named trainers' split signature pools;
 * same-species templates; genNewPartyMemberSpecies; the final getSpeciesForLevel) and addEnemyPokemon up
 * to the moveset, whose form gives the member's types for the next members' balance check. The JS each
 * step replaces is quoted where it happens. */
#include "pr/run.h"
#include <string.h>

static const pr_trainer_species *trainer_species(int type)
{
    int lo = 0, hi = pr_trainer_species_count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (pr_trainer_species_data[mid].type == type) return &pr_trainer_species_data[mid];
        if (pr_trainer_species_data[mid].type < type) lo = mid + 1; else hi = mid - 1;
    }
    return NULL;
}

/* Object.hasOwn(config.partyMemberFuncs, key) */
static bool has_func(const pr_trainer_species *ts, int key)
{
    return key >= -8 && key < 8 && (ts->member_funcs >> (key + 8) & 1);
}

static int seed_item(pr_rng *rng, int n) { return n == 1 ? 0 : pr_rng_integer_in_range(rng, 0, n - 1); }

static int rival_fight(int type)
{
    switch (type) {
    case TRAINER_TYPE_RIVAL: return 1;
    case TRAINER_TYPE_RIVAL_2: return 2;
    case TRAINER_TYPE_RIVAL_3: return 3;
    case TRAINER_TYPE_RIVAL_4: return 4;
    case TRAINER_TYPE_RIVAL_5: return 5;
    case TRAINER_TYPE_RIVAL_6: return 6;
    default: return 0;
    }
}

/* signatureSpecies[TrainerType[type]] without the groups that hold a species already in the party,
 * flattened */
static int filtered_signature(const pr_trainer_species *ts, const pr_trainer_party *party, uint16_t *out)
{
    int n = 0;
    for (int g = 0; g < ts->n_groups; g++) {
        const pr_signature_group *grp = &pr_trainer_signature_groups[ts->group_start + g];
        bool used = false;
        for (int k = 0; k < grp->n && !used; k++)
            for (int m = 0; m < party->n; m++)
                if (party->species[m] == pr_trainer_signature_species[grp->start + k]) used = true;
        if (used) continue;
        for (int k = 0; k < grp->n && n < 64; k++) out[n++] = pr_trainer_signature_species[grp->start + k];
    }
    return n;
}

int pr_run_gen_trainer_party(pr_run *run, const pr_trainer_party_ctx *ctx, pr_trainer_party *out)
{
    memset(out, 0, sizeof *out);
    const pr_trainer_species *ts = trainer_species(ctx->enemy.trainer_type);
    if (!ts) { run->unported |= PR_RUN_UNPORTED_TRAINER; return -1; }
    const int rf = rival_fight(ctx->enemy.trainer_type);
    if (rf) return pr_run_gen_rival_party(run, ctx, rf, out);
    const pr_party_template *tpl = ctx->template_;
    pr_party_species_ctx sc;
    memset(&sc, 0, sizeof sc);
    sc.trainer_type = ctx->enemy.trainer_type;
    sc.wave = ctx->enemy.wave;
    sc.evo_kind = tpl->evo_kind;
    sc.classic_wave20_trainer = ctx->classic_wave20_trainer;
    /* config.trainerTypeDouble && isDouble() && !config.doubleOnly: a double of two named trainers */
    const bool named_double = ts->type_double && ctx->is_double && !ts->double_only;
    const pr_rng saved = run->rng;
    for (int e = 0; e < ctx->n_levels && e < PR_MAX_PARTY_TEMPLATE; e++) {
        const int level = ctx->levels[e];
        /* seedOffset: hasStaticParty ? derivedType + ((index + 1) << 8)
         *   : waveIndex + (derivedType << 10) + (((useSameSeedForAllMembers ? 0 : index) + 1) << 8) */
        const int offset = ts->static_party ? ts->derived_type + ((e + 1) << 8)
                                            : ctx->enemy.wave + (ts->derived_type << 10)
                                                  + (((ts->same_seed ? 0 : e) + 1) << 8);
        run->rng = pr_run_seed_offset_rng(run, offset, NULL);
        if (!named_double
            && (has_func(ts, e) || has_func(ts, e - tpl->size))) {
            run->unported |= PR_RUN_UNPORTED_TRAINER;   /* config.partyMemberFuncs[index]: a closure */
            run->rng = saved;
            return -1;
        }
        const int inner_offset = tpl->offsets[e];

        uint16_t pool[64];
        int n_pool = 0;
        if (named_double) {
            /* the even members from the trainer's own signature species, the odd ones from the partner's;
             * Tate's first member is Solrock and Liza's Lunatone */
            if (!(e % 2)) {
                if (e == 0 && ts->type == TRAINER_TYPE_TATE) pool[n_pool++] = SPECIES_ID_SOLROCK;
                else if (e == 0 && ts->type == TRAINER_TYPE_LIZA) pool[n_pool++] = SPECIES_ID_LUNATONE;
                else n_pool = filtered_signature(ts, out, pool);
            } else if (e == 1 && ts->type_double == TRAINER_TYPE_TATE) {
                pool[n_pool++] = SPECIES_ID_SOLROCK;
            } else if (e == 1 && ts->type_double == TRAINER_TYPE_LIZA) {
                pool[n_pool++] = SPECIES_ID_LUNATONE;
            } else {
                const pr_trainer_species *partner = trainer_species(ts->type_double);
                if (!partner) { run->unported |= PR_RUN_UNPORTED_TRAINER; run->rng = saved; return -1; }
                n_pool = filtered_signature(partner, out, pool);
            }
        }

        /* let species = this.genNewPartyMemberSpecies(level, strength): rolled even when a pool replaces it */
        sc.level = level;
        sc.balanced = tpl->member[e] >> 1 & 1;   /* template.isBalanced(battle.enemyParty.length) */
        int species = pr_run_gen_new_party_member_species(run, &sc);
        if (species < 0) { run->rng = saved; return -1; }
        if (n_pool) {
            species = pool[seed_item(&run->rng, n_pool)];
        } else if ((tpl->member[e] & 1) && e > inner_offset) {
            /* same species as the inner template's first member: its getTrainerSpeciesForLevel(level, false, ...) */
            species = pr_run_determine_enemy_species(&run->rng, out->species[inner_offset], level, false, tpl->evo_kind,
                                                     true, ctx->classic_wave20_trainer);
        }
        /* if (newSpeciesPool) — an array, always truthy: getSpeciesForLevel(level, true, true, strength, kind) */
        species = pr_run_determine_enemy_species(&run->rng, species, level, true, tpl->evo_kind, true,
                                                 ctx->classic_wave20_trainer);

        /* addEnemyPokemon(species, level, !isDouble() || !(index % 2) ? TRAINER : TRAINER_PARTNER) */
        pr_enemy_gen_ctx ec = ctx->enemy;
        ec.species = species;
        ec.level = level;
        ec.trainer_slot = !ctx->is_double || !(e % 2) ? 1 : 2;
        ec.boss = false;
        ec.shiny_lock = false;
        if (pr_run_enemy_pokemon_base(run, &ec, &out->mons[e]) < 0) { run->rng = saved; return -1; }
        out->species[e] = (uint16_t)species;
        out->levels[e] = level;
        out->n = e + 1;

        /* what the next members see: the root (checkDuplicateSpecies) and getTypes() of the new member
         * (its fusion's types too, in spliced endless) */
        int8_t types[2];
        const int root = pr_run_species_root(species), n_types = pr_run_enemy_types(&out->mons[e], species, types);
        for (int i = 0; i < n_types && sc.n_party_types < PR_MAX_PARTY_TYPES; i++)
            sc.party_types[sc.n_party_types++] = types[i];
        sc.party_roots[e] = (uint16_t)root;
        if (root < 0 || n_types < 0) {
            run->unported |= PR_RUN_UNDEFINED_UPSTREAM;
            run->rng = saved;
            return -1;
        }
        sc.n_party = e + 1;
    }
    run->rng = saved;   /* each member rolled inside executeWithSeedOffset: RND comes back */
    return 0;
}
