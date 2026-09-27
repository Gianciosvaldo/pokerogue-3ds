/* Which species an enemy of a given level is: forced prevolutions and random evolutions
 * (src/ai/ai-species-gen.ts determineEnemySpecies, src/data/pokemon-species.ts
 * getWildSpeciesForLevel). The JS each function replaces is quoted above it. */
#include "pr/run.h"
#include <math.h>

static const pr_species_evo *evo_info(int id)
{
    int lo = 0, hi = pr_species_evo_count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (pr_species_evos[mid].id == id) return &pr_species_evos[mid];
        if (pr_species_evos[mid].id < id) lo = mid + 1; else hi = mid - 1;
    }
    return NULL;
}

/* getRequiredPrevo(species, level, encounterKind):
 *   const prevolutionLevels = species.getPrevolutionLevels(true);
 *   for (let pl = prevolutionLevels.length - 1; pl >= 0; pl--) {
 *     const [prevoSpecies, levelReq, evoThreshold] = prevolutionLevels[pl];
 *     const threshold = evoThreshold?.[encounterKind] ?? levelReq;
 *     const req = levelReq === 1 ? threshold : Math.min(levelReq, threshold);
 *     if (level < req) return prevoSpecies; }
 *   return null; */
static int required_prevo(const pr_species_evo *e, int level, int kind)
{
    for (int pl = e->n_prevo - 1; pl >= 0; pl--) {
        const pr_evo *p = &pr_prevo_data[e->prevo_start + pl];
        int threshold = p->threshold[kind] >= 0 ? p->threshold[kind] : p->level;
        int req = p->level == 1 ? threshold : (p->level < threshold ? p->level : threshold);
        if (level < req) return p->species;
    }
    return -1;
}

/* determineEnemySpecies(species, level, allowEvolving, forTrainer, strength, encounterKind, tryForcePrevo):
 *   const requiredPrevo = tryForcePrevo && hasPrevolution(species) && getRequiredPrevo(species, level, encounterKind);
 *   if (requiredPrevo) return requiredPrevo;
 *   if (!allowEvolving || evolutions.length <= 0 || (waveIndex === 20 && isClassic && currentBattle.trainer)) return species;
 *   evoPool: for each evolution e: levelThreshold = Math.max(e.level, e.evoLevelThreshold?.[encounterKind] ?? 0);
 *            eligible (pushed as [levelThreshold, e.speciesId]) unless level < e.level || level < levelThreshold;
 *   if (!evoPool.length) return species;
 *   const [choice, evoSpecies] = randSeedItem(evoPool);
 *   multiplier: STRONG 1, NORMAL 1.1, WILD 1.2;
 *   const randomLevel = randSeedIntRange(choice, Math.round(choice * multiplier));
 *   if (randomLevel <= level) return determineEnemySpecies(evoSpecies, level, true, ..., tryForcePrevo = false);
 *   return species;
 * (strength and forTrainer only pick the default kind; they aren't read otherwise.) */
int pr_run_determine_enemy_species(pr_rng *rng, int species, int level, bool allow_evolving, int kind,
                                   bool try_force_prevo, bool classic_wave20_trainer)
{
    const pr_species_evo *e = evo_info(species);
    if (!e) return species;
    if (try_force_prevo && e->has_prevolution) {
        int prevo = required_prevo(e, level, kind);
        if (prevo > 0) return prevo;          /* SpeciesId 0 (NONE) would be falsy upstream */
    }
    if (!allow_evolving || e->n_evo == 0 || classic_wave20_trainer) return species;
    int pool_level[PR_MAX_BIOME_LINKS * 4], pool_species[PR_MAX_BIOME_LINKS * 4], n = 0;
    for (int i = 0; i < e->n_evo && n < PR_MAX_BIOME_LINKS * 4; i++) {
        const pr_evo *v = &pr_evo_data[e->evo_start + i];
        int t = v->threshold[kind] >= 0 ? v->threshold[kind] : 0;
        int threshold = v->level > t ? v->level : t;
        if (level < v->level || level < threshold || threshold <= 0) continue;
        pool_level[n] = threshold;
        pool_species[n++] = v->species;
    }
    if (!n) return species;
    int k = n == 1 ? 0 : pr_rng_integer_in_range(rng, 0, n - 1);
    static const double multiplier[3] = {1, 1.1, 1.2};
    int choice = pool_level[k];
    /* randSeedIntRange(min, max) = randSeedInt(max - min + 1, min) */
    int max = (int)js_round(choice * multiplier[kind]);
    int random_level = pr_rng_seed_int(rng, max - choice + 1, choice);
    if (random_level <= level)
        return pr_run_determine_enemy_species(rng, pool_species[k], level, true, kind, false, classic_wave20_trainer);
    return species;
}

int pr_run_get_wild_species_for_level(pr_rng *rng, int species, int level, bool allow_evolving, bool is_boss,
                                      bool classic_wave20_trainer)
{
    return pr_run_determine_enemy_species(rng, species, level, allow_evolving,
                                          is_boss ? PR_EVO_KIND_NORMAL : PR_EVO_KIND_WILD, true, classic_wave20_trainer);
}
