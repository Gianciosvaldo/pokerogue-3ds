/* Trainer.genNewPartyMemberSpecies (src/field/trainer.ts): the species of a trainer's next party member
 * that no partyMemberFunc fixes, with globalScene.randomSpecies(waveIndex, level, false, speciesFilter)
 * (src/battle-scene.ts) for trainers without species pools. The JS each step replaces is quoted where
 * it happens. */
#include "pr/run.h"

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

/* the species' index in pr_species_gens (and pr_species_trainer), -1 if unknown */
static int species_index(int id)
{
    int lo = 0, hi = pr_species_gen_count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (pr_species_gens[mid].id == id) return mid;
        if (pr_species_gens[mid].id < id) lo = mid + 1; else hi = mid - 1;
    }
    return -1;
}

/* randSeedItem over n items: one item needs no roll */
static int seed_item(pr_rng *rng, int n) { return n == 1 ? 0 : pr_rng_integer_in_range(rng, 0, n - 1); }

/* PokemonSpeciesForm.isOfType(type): type1 === type || (type2 !== null && type2 === type) */
static bool is_of_type(int si, int type)
{
    const pr_species_trainer_info *s = &pr_species_trainer[si];
    return s->types[0] == type || (s->types[1] != -2 && s->types[1] == type);
}

/* randomSpecies(waveIndex, level, false, speciesFilter): randSeedItem over the catchable species the
 * filter keeps, each replaced by the root of its chain, duplicates dropped (a Set, first place kept);
 * without a filter, every catchable species as it is */
static int random_species(pr_run *run, int filter)
{
    static uint16_t list[1400];
    static uint8_t seen[1400];
    int n = 0;
    for (int i = 0; i < pr_species_gen_count; i++) seen[i] = 0;
    for (int i = 0; i < pr_species_gen_count; i++) {
        if (filter < 0) {
            if (pr_species_trainer[i].catchable) list[n++] = pr_species_gens[i].id;
            continue;
        }
        if (!(pr_trainer_species_filters[filter * pr_trainer_filter_bytes + (i >> 3)] >> (i & 7) & 1)) continue;
        int r = species_index(pr_species_trainer[i].root);
        if (r < 0 || seen[r]) continue;
        seen[r] = 1;
        list[n++] = pr_species_gens[r].id;
    }
    if (!n) { run->unported |= PR_RUN_UNDEFINED_UPSTREAM; return -1; }   /* randSeedItem([]) */
    return list[seed_item(&run->rng, n)];
}

int pr_run_gen_new_party_member_species(pr_run *run, const pr_party_species_ctx *ctx)
{
    const pr_trainer_species *ts = trainer_species(ctx->trainer_type);
    if (!ts) { run->unported |= PR_RUN_UNPORTED_TRAINER; return -1; }
    const int specialty = ctx->trainer_type < pr_trainer_specialty_count ? pr_trainer_specialty[ctx->trainer_type] : -1;
    for (int attempt = 0;; attempt++) {
        int base;
        if (ts->has_pools) {
            /* tierValue = randSeedInt(512): >= 156 COMMON, >= 32 UNCOMMON, >= 6 RARE, >= 1 SUPER_RARE, else
             * ULTRA_RARE; a missing or empty tier drops to the one below */
            int v = pr_rng_seed_int(&run->rng, 512, 0);
            int tier = v >= 156 ? 0 : v >= 32 ? 1 : v >= 6 ? 2 : v >= 1 ? 3 : 4;
            while (tier >= 0 && ts->tiers[tier].n <= 0) tier--;
            if (tier < 0) { run->unported |= PR_RUN_UNDEFINED_UPSTREAM; return -1; }
            const int16_t *pool = &pr_trainer_pool_entries[ts->tiers[tier].start];
            const int n = ts->tiers[tier].n;
            bool any = false;
            for (int i = 0; i < n; i++) any |= pool[i] >= 0;
            if (!any) { run->unported |= PR_RUN_UNDEFINED_UPSTREAM; return -1; }   /* rerolls forever */
            /* randSeedItem(tierPool), rerolled while it lands on a list of species */
            do base = pool[seed_item(&run->rng, n)]; while (base < 0);
        } else {
            base = random_species(run, ts->filter);
            if (base < 0) return -1;
        }
        const int bi = species_index(base);
        if (bi < 0) { run->unported |= PR_RUN_UNPORTED_SPECIES; return -1; }

        /* baseSpecies.getTrainerSpeciesForLevel(level, true, strength, template.evoLevelThresholdKind) */
        int ret = pr_run_determine_enemy_species(&run->rng, base, ctx->level, true, ctx->evo_kind, true,
                                                 ctx->classic_wave20_trainer);
        int ri = species_index(ret);
        if (ri < 0) { run->unported |= PR_RUN_UNPORTED_SPECIES; return -1; }
        bool retry = false;
        if (pr_species_trainer[bi].root != base && ret != base) {   /* hasPrevolution(baseSpecies) */
            retry = true;
        } else if (ctx->balanced) {
            /* partyMemberTypes.indexOf(ret.type1) > -1 || (ret.type2 !== null && indexOf(ret.type2) > -1) */
            const int8_t *t = pr_species_trainer[ri].types;
            for (int i = 0; i < ctx->n_party_types; i++)
                if (ctx->party_types[i] == t[0] || (t[1] != -2 && ctx->party_types[i] == t[1])) retry = true;
        }
        /* a specialty-type trainer rerolls the evolution up to 10 times to fit its type */
        if (!retry && specialty >= 0 && !is_of_type(ri, specialty)) {
            retry = true;
            int evo_attempt = 0;
            while (retry && evo_attempt++ < 10) {
                ret = pr_run_determine_enemy_species(&run->rng, base, ctx->level, true, ctx->evo_kind, true,
                                                     ctx->classic_wave20_trainer);
                ri = species_index(ret);
                if (ri < 0) { run->unported |= PR_RUN_UNPORTED_SPECIES; return -1; }
                if (is_of_type(ri, specialty)) retry = false;
            }
        }
        /* checkDuplicateSpecies(baseSpecies.speciesId): the enemy party's roots and the trainer's
         * signature species' roots */
        for (int i = 0; i < ctx->n_party; i++) if (ctx->party_roots[i] == base) retry = true;
        for (int i = 0; i < ts->n_sig; i++) if (pr_trainer_signature_roots[ts->sig_start + i] == base) retry = true;
        if (retry && attempt < 10) continue;
        return ret;
    }
}

int pr_run_species_root(int species)
{
    const int si = species_index(species);
    return si < 0 ? -1 : pr_species_trainer[si].root;
}
