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
