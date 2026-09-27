/* Biomes: the depth table, random biomes and the choice of the next biome
 * (src/init/init-biome-depths.ts, src/battle-scene.ts generateRandomBiome,
 * src/phases/select-biome-phase.ts). The JS each function replaces is quoted above it. */
#include "pr/run.h"
#include <math.h>
#include <string.h>

/* traverseBiome(biomeId, depth):
 *   if (biomeId === END) { const biomeList = Object.values(BiomeId); biomeList.pop();
 *     biomeId = biomeList[randSeedInt(biomeList.length, 1)]; }   // never TOWN
 *   for (const linkedBiomeEntry of allBiomes.get(biomeId).biomeLinks) {
 *     const linkedBiome = entry id; const biomeChance = entry weight ?? 1;
 *     if (!Object.hasOwn(biomeDepths, linkedBiome) || biomeChance < biomeDepths[linkedBiome][1]
 *         || (depth < biomeDepths[linkedBiome][0] && biomeChance === biomeDepths[linkedBiome][1])) {
 *       biomeDepths[linkedBiome] = [depth + 1, biomeChance];
 *       traverseBiome(linkedBiome, depth + 1);
 *     } } */
static void traverse(pr_biome_depths *d, pr_rng *rng, int biome, int depth)
{
    if (biome == BIOME_ID_END) biome = pr_biome_order[pr_rng_seed_int(rng, pr_biome_count - 1, 1)];
    const pr_biome_info *b = &pr_biomes[biome];
    for (int i = 0; i < b->n_links; i++) {
        int linked = b->links[i].biome, chance = b->links[i].weight ? b->links[i].weight : 1;
        if (!d->set[linked] || chance < d->chance[linked] || (depth < d->depth[linked] && chance == d->chance[linked])) {
            d->set[linked] = true;
            d->depth[linked] = depth + 1;
            d->chance[linked] = chance;
            traverse(d, rng, linked, depth + 1);
        }
    }
}

/* initBiomeDepths(): biomeDepths[TOWN] = [0, 1]; traverseBiome(TOWN, 0);
 *   biomeDepths[END] = [max of every entry's depth (END's own included) + 1, 1] */
void pr_run_init_biome_depths(pr_biome_depths *d, pr_rng *rng)
{
    memset(d, 0, sizeof *d);
    d->set[BIOME_ID_TOWN] = true;
    d->depth[BIOME_ID_TOWN] = 0;
    d->chance[BIOME_ID_TOWN] = 1;
    traverse(d, rng, BIOME_ID_TOWN, 0);
    int max = 0;
    for (int i = 0; i < PR_BIOME_ID_LIMIT; i++)
        if (d->set[i] && d->depth[i] > max) max = d->depth[i];
    d->set[BIOME_ID_END] = true;
    d->depth[BIOME_ID_END] = max + 1;
    d->chance[BIOME_ID_END] = 1;
}

/* generateRandomBiome(waveIndex):
 *   const relWave = waveIndex % 250;
 *   const biomes = Object.values(BiomeId).filter(b => b !== TOWN && b !== END);
 *   const maxDepth = biomeDepths[END][0] - 2;
 *   const depthWeights = new Array(maxDepth + 1).fill(null).map((_, i) =>
 *     ((1 - Math.min(Math.abs(i / (maxDepth - 1) - relWave / 250) + 0.25, 1)) / 0.75) * 250);
 *   let totalWeight = 0; for (const biome of biomes) {
 *     totalWeight += Math.ceil(depthWeights[biomeDepths[biome][0] - 1] / biomeDepths[biome][1]);
 *     biomeThresholds.push(totalWeight); }
 *   const randInt = randSeedInt(totalWeight);
 *   first biome whose threshold is above randInt, else randSeedItem(biomes) */
int pr_run_generate_random_biome(const pr_biome_depths *d, pr_rng *rng, int wave)
{
    double rel = wave % 250;
    int max_depth = d->depth[BIOME_ID_END] - 2;
    int biomes[PR_BIOME_ID_LIMIT], n = 0;
    double thresholds[PR_BIOME_ID_LIMIT], total = 0;
    for (int i = 0; i < pr_biome_count; i++) {
        int b = pr_biome_order[i];
        if (b == BIOME_ID_TOWN || b == BIOME_ID_END) continue;
        int k = d->depth[b] - 1;
        double w = NAN; /* depthWeights[k] is undefined outside 0..maxDepth, or for a biome never reached */
        if (d->set[b] && k >= 0 && k <= max_depth) {
            double m = fabs((double)k / (max_depth - 1) - rel / 250) + 0.25;
            w = ((1 - (m < 1 ? m : 1)) / 0.75) * 250;
        }
        total += ceil(w / (d->set[b] ? d->chance[b] : NAN));
        biomes[n] = b;
        thresholds[n++] = total;
    }
    /* randSeedInt(NaN) would roll integerInRange(0, NaN); the tables upstream builds never get there */
    int32_t r = pr_rng_seed_int(rng, (int32_t)total, 0);
    for (int i = 0; i < n; i++)
        if (r < thresholds[i]) return biomes[i];
    return biomes[pr_rng_integer_in_range(rng, 0, n - 1)];
}

/* SelectBiomePhase.start():
 *   globalScene.resetSeed();                        // the current wave's seed
 *   if ((isClassic && isWaveFinal(nextWaveIndex + 9)) || (isDaily && isWaveFinal(nextWaveIndex))
 *       || (hasShortBiomes && !(nextWaveIndex % 50))) → END
 *   if (hasRandomBiomes) → generateNextBiome(nextWaveIndex)   // no game mode sets it
 *   const { biomeLinks } = allBiomes.get(currentBiome);
 *   if (biomeLinks.length > 1) {
 *     const biomes = biomeLinks.filter(b => !Array.isArray(b) || !randSeedInt(b[1])).map(b => id);
 *     if (biomes.length > 1 && findModifier(m => m instanceof MapModifier)) → the player picks
 *     else → randSeedItem(biomes) }
 *   if (biomeLinks.length === 1) → biomeLinks[0] (its id, even with a weight)
 *   else → generateNextBiome(nextWaveIndex)
 * generateNextBiome(waveIndex): waveIndex % 50 === 0 ? END : generateRandomBiome(waveIndex) */
int pr_run_select_next_biome(pr_run *run, const pr_biome_depths *d, int current_biome, int current_wave, bool has_map,
                             uint8_t choices[PR_MAX_BIOME_LINKS], int *n_choices)
{
    *n_choices = 0;
    pr_run_reset_seed(run, current_wave);
    int next = current_wave + 1;
    bool classic = run->mode == GAME_MODES_CLASSIC || run->mode == GAME_MODES_CHALLENGE;
    bool short_biomes = run->mode == GAME_MODES_ENDLESS || run->mode == GAME_MODES_SPLICED_ENDLESS;
    if ((classic && pr_run_is_wave_final(run->mode, next + 9))
        || (run->mode == GAME_MODES_DAILY && pr_run_is_wave_final(run->mode, next)) || (short_biomes && next % 50 == 0))
        return BIOME_ID_END;
    const pr_biome_info *b = &pr_biomes[current_biome];
    if (b->n_links > 1) {
        uint8_t biomes[PR_MAX_BIOME_LINKS];
        int n = 0;
        for (int i = 0; i < b->n_links; i++)
            if (!b->links[i].weight || !pr_rng_seed_int(&run->rng, b->links[i].weight, 0)) biomes[n++] = b->links[i].biome;
        if (n > 1 && has_map) {
            memcpy(choices, biomes, (size_t)n);
            *n_choices = n;
            return -1;
        }
        if (n == 1) return biomes[0];                     /* randSeedItem: one item, no roll */
        if (n == 0) {                                     /* pick([]) is undefined upstream */
            run->unported |= PR_RUN_UNDEFINED_UPSTREAM;
            pr_rng_integer_in_range(&run->rng, 0, -1);
            return -1;
        }
        return biomes[pr_rng_integer_in_range(&run->rng, 0, n - 1)];
    }
    if (b->n_links == 1) return b->links[0].biome;
    return next % 50 == 0 ? BIOME_ID_END : pr_run_generate_random_biome(d, &run->rng, next);
}

/* randSeedInt(range) for a range that may be fractional (luck lowers the species roll's ceiling
 * by 0.5 per point for bosses): integerInRange(0, range - 1) = Math.floor(realInRange(0, range)) */
static int seed_int_frac(pr_rng *rng, double range)
{
    if (range <= 1) return 0;
    return (int)floor(pr_rng_real_in_range(rng, 0, range));
}

/* getGeneratedWaveCycleOffset(): executeWithSeedOffset(() => ret = randSeedInt(8) * 5, 0, seed) */
int pr_run_get_generated_wave_cycle_offset(const pr_run *run)
{
    pr_rng r = pr_run_seed_offset_rng(run, 0, NULL);
    return pr_rng_seed_int(&r, 8, 0) * 5;
}

/* getGeneratedOffsetGym(): executeWithSeedOffset(() => ret = !randSeedInt(2), 0, seed);
 * the scene keeps it only in classic modes (offsetGym = isClassic && getGeneratedOffsetGym()) */
bool pr_run_get_generated_offset_gym(const pr_run *run)
{
    pr_rng r = pr_run_seed_offset_rng(run, 0, NULL);
    return !pr_rng_seed_int(&r, 2, 0);
}

/* arena.getTimeOfDay() (TIME_OF_DAY_OVERRIDE, a dev override, isn't modelled):
 *   ABYSS → NIGHT; const waveCycle = ((currentBattle?.waveIndex ?? 0) + waveCycleOffset) % 40;
 *   < 15 DAY, < 20 DUSK, < 35 NIGHT, else DAWN */
int pr_run_get_time_of_day(int biome, int wave, int wave_cycle_offset)
{
    if (biome == BIOME_ID_ABYSS) return TIME_OF_DAY_NIGHT;
    int cycle = (wave + wave_cycle_offset) % 40;
    return cycle < 15 ? TIME_OF_DAY_DAY : cycle < 20 ? TIME_OF_DAY_DUSK : cycle < 35 ? TIME_OF_DAY_NIGHT : TIME_OF_DAY_DAWN;
}

/* arena.pokemonPool[tier] after updatePoolsForTimeOfDay(): [...pool[ALL], ...pool[timeOfDay]] */
static int pool_size(int biome, int tier, int tod)
{
    const pr_biome_info *b = &pr_biomes[biome];
    return b->species[tier][0].n + b->species[tier][tod + 1].n;
}
static int pool_item(int biome, int tier, int tod, int i)
{
    const pr_pool_slice *all = &pr_biomes[biome].species[tier][0], *now = &pr_biomes[biome].species[tier][tod + 1];
    return i < all->n ? pr_biome_pool_data[all->start + i] : pr_biome_pool_data[now->start + i - all->n];
}

/* generateBossBiomeTier / generateNonBossBiomeTier */
static int boss_tier(int v)
{
    return v >= 20 ? BIOME_POOL_TIER_BOSS : v >= 6 ? BIOME_POOL_TIER_BOSS_RARE : v >= 1 ? BIOME_POOL_TIER_BOSS_SUPER_RARE
                                                                                        : BIOME_POOL_TIER_BOSS_ULTRA_RARE;
}
static int non_boss_tier(int v)
{
    return v >= 156 ? BIOME_POOL_TIER_COMMON : v >= 32 ? BIOME_POOL_TIER_UNCOMMON : v >= 6 ? BIOME_POOL_TIER_RARE
           : v >= 1 ? BIOME_POOL_TIER_SUPER_RARE : BIOME_POOL_TIER_ULTRA_RARE;
}

static const pr_species *species_by_id(int id)
{
    int lo = 0, hi = PR_SPECIES_COUNT - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (pr_species_table[mid].id == id) return &pr_species_table[mid];
        if (pr_species_table[mid].id < id) lo = mid + 1; else hi = mid - 1;
    }
    return NULL;
}

/* gameMode.getOverrideSpecies(waveIndex):
 *   if (isDaily && isWaveFinal(waveIndex)) {
 *     const eventBoss = getDailyEventSeedBoss(); if (eventBoss?.speciesId != null) return it;  // custom seeds
 *     return randSeedItem(getAllSpecies().filter(s => (s.subLegendary || s.legendary || s.mythical)
 *       && s.baseTotal >= 600 && s.speciesId !== ETERNATUS && s.speciesId !== ARCEUS));
 *   }
 *   return getDailyForcedWaveSpecies(waveIndex);   // custom daily seeds only, else null */
int pr_run_get_override_species(pr_run *run, int wave)
{
    if (run->mode != GAME_MODES_DAILY) return -1;
    if (run->daily_custom) { run->unported |= PR_RUN_UNPORTED_DAILY; return -1; }
    if (!pr_run_is_wave_final(run->mode, wave)) return -1;
    static uint16_t list[256];
    int n = 0;
    for (int i = 0; i < pr_species_gen_count && n < 256; i++) {   /* getAllSpecies(): ascending id */
        const pr_species_gen *g = &pr_species_gens[i];
        const pr_species *sp = species_by_id(g->id);
        if (!sp || !(g->sub_legendary || g->legendary || g->mythical)) continue;
        if (pr_forms[sp->form0].base_total < 600 || g->id == SPECIES_ID_ETERNATUS || g->id == SPECIES_ID_ARCEUS) continue;
        list[n++] = g->id;
    }
    if (!n) { run->unported |= PR_RUN_UNDEFINED_UPSTREAM; return -1; }
    return list[n == 1 ? 0 : pr_rng_integer_in_range(&run->rng, 0, n - 1)];
}

/* arena.randomSpecies(waveIndex, level, attempt, luckValue, isBoss), up to the call of
 * species.getWildSpeciesForLevel(level, true, isBoss ?? isBossSpecies, gameMode):
 *   const overrideSpecies = gameMode.getOverrideSpecies(waveIndex); if (overrideSpecies) return it;
 *   const isBossSpecies = getEncounterBossSegments(waveIndex, level) > 0 && pokemonPool[BOSS].length > 0
 *     && (biomeId !== END || gameMode.isClassic || gameMode.isWaveFinal(waveIndex));
 *   const forcedTier = getDailyForcedWaveBiomePoolTier(waveIndex);   // null outside daily event seeds
 *   const rngRoll = randSeedInt((isBossSpecies ? 64 : 512) - luckValue * (isBossSpecies ? 0.5 : 2));
 *   tier = (isBossSpecies ? generateBossBiomeTier : generateNonBossBiomeTier)(rngRoll);
 *   while (pokemonPool[tier].length === 0 && tier > COMMON) tier--;
 *   species = tierPool.length ? getSpecies(randSeedItem(tierPool)) : globalScene.randomSpecies(...);
 *   if (checkLegendBST(species, getWaveForDifficulty(waveIndex, true)) && attempt < 10) return randomSpecies(..., ++attempt, ...);
 * checkLegendBST: legend-like && (baseTotal >= 660 ? adjustedWave < 80 : adjustedWave < 55).
 * Returns the species handed to getWildSpeciesForLevel and the isBoss it gets; is_boss < 0 = undefined.
 * On the daily run's final wave it returns the override species (see pr_run_get_override_species). */
int pr_run_random_species(pr_run *run, int biome, int wave, int level, int luck, int is_boss, int wave_cycle_offset,
                          bool *evo_is_boss)
{
    /* getOverrideSpecies (checked again on each reroll: only the daily final wave has one) and the forced
     * tiers of custom daily seeds */
    if (run->mode == GAME_MODES_DAILY && (run->daily_custom || pr_run_is_wave_final(run->mode, wave))) {
        *evo_is_boss = false;
        return pr_run_get_override_species(run, wave);
    }
    int tod = pr_run_get_time_of_day(biome, wave, wave_cycle_offset);
    bool classic = run->mode == GAME_MODES_CLASSIC || run->mode == GAME_MODES_CHALLENGE;
    for (int attempt = 0;; attempt++) {
        bool boss = pr_run_get_encounter_boss_segments(run, wave, level, -1, false) > 0
                    && pool_size(biome, BIOME_POOL_TIER_BOSS, tod) > 0
                    && (biome != BIOME_ID_END || classic || pr_run_is_wave_final(run->mode, wave));
        int roll = seed_int_frac(&run->rng, (boss ? 64 : 512) - luck * (boss ? 0.5 : 2));
        int tier = boss ? boss_tier(roll) : non_boss_tier(roll);
        while (pool_size(biome, tier, tod) == 0 && tier > BIOME_POOL_TIER_COMMON) tier--;
        int n = pool_size(biome, tier, tod);
        if (!n) { /* globalScene.randomSpecies isn't ported */
            run->unported |= PR_RUN_UNPORTED_SPECIES;
            return -1;
        }
        int species = pool_item(biome, tier, tod, n == 1 ? 0 : pr_rng_integer_in_range(&run->rng, 0, n - 1));
        const pr_species *sp = species_by_id(species);
        int adjusted = pr_run_get_wave_for_difficulty(run, wave, true);
        bool regen = sp && sp->rarity && (pr_forms[sp->form0].base_total >= 660 ? adjusted < 80 : adjusted < 55);
        if (regen && attempt < 10) continue;
        *evo_is_boss = is_boss >= 0 ? is_boss : boss;
        return species;
    }
}

/* arena.randomTrainerType(waveIndex, isBoss):
 *   const isTrainerBoss = trainerPool[BOSS].length > 0 && (gameMode.isTrainerBoss(waveIndex, biomeId, offsetGym) || isBoss);
 *   let tier = (isTrainerBoss ? generateBossBiomeTier : generateNonBossBiomeTier)(randSeedInt(isTrainerBoss ? 64 : 512));
 *   while (trainerPool[tier].length === 0 && tier > COMMON) tier--;
 *   return tierPool.length > 0 ? randSeedItem(tierPool) : TrainerType.BREEDER; */
int pr_run_random_trainer_type(pr_run *run, int biome, int wave, bool is_boss)
{
    const pr_biome_info *b = &pr_biomes[biome];
    bool boss = b->trainers[BIOME_POOL_TIER_BOSS].n > 0 && (pr_run_is_trainer_boss(run, wave, biome, run->offset_gym) || is_boss);
    int v = pr_rng_seed_int(&run->rng, boss ? 64 : 512, 0);
    int tier = boss ? boss_tier(v) : non_boss_tier(v);
    while (b->trainers[tier].n == 0 && tier > BIOME_POOL_TIER_COMMON) tier--;
    const pr_pool_slice *p = &b->trainers[tier];
    if (!p->n) return TRAINER_TYPE_BREEDER;
    return pr_biome_pool_data[p->start + (p->n == 1 ? 0 : pr_rng_integer_in_range(&run->rng, 0, p->n - 1))];
}
