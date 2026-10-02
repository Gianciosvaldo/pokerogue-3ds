/* Daily Run fixed initial seeding, starters and starting biome (src/data/daily-seed/daily-run.ts) */
#include "pr/run.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static double rand_seed_gauss(pr_rng *rng, double stdev, double mean)
{
    if (stdev == 0.0) return mean;
    double u = 1.0 - pr_rng_seed_float(rng);
    double v = pr_rng_seed_float(rng);
    double z = sqrt(-2.0 * log(u)) * cos(2.0 * M_PI * v);
    return z * stdev + mean;
}

int pr_run_get_daily_starters(const pr_run *run, pr_enemy_gen out_starters[3], int out_species[3])
{
    pr_rng r = pr_run_seed_offset_rng(run, 0, NULL);

    double z = rand_seed_gauss(&r, 1.0, 0.0);
    int cost0 = (int)fmin(js_round(3.5 + fabs(z)), 8.0);
    int cost1 = pr_rng_seed_int(&r, 9 - cost0, 1);
    int cost2 = 10 - (cost0 + cost1);
    int costs[3] = {cost0, cost1, cost2};

    int n_starters = 0;

    for (int i = 0; i < 3; i++) {
        int cost = costs[i];
        uint16_t cost_species[PR_SPECIES_COUNT];
        int n_cost = 0;

        for (int s = 0; s < pr_species_gen_count; s++) {
            const pr_species_gen *sp = &pr_species_gens[s];
            if (sp->starter_cost != cost) continue;
            bool dup = false;
            for (int k = 0; k < n_starters; k++) {
                if (sp->id == out_species[k] || pr_run_species_root(sp->id) == pr_run_species_root(out_species[k])) {
                    dup = true;
                    break;
                }
            }
            if (!dup) {
                cost_species[n_cost++] = sp->id;
            }
        }
        if (n_cost == 0) {
            for (int s = 0; s < pr_species_gen_count; s++) {
                const pr_species_gen *sp = &pr_species_gens[s];
                if (!sp->starter_cost) continue;
                bool dup = false;
                for (int k = 0; k < n_starters; k++) {
                    if (sp->id == out_species[k] || pr_run_species_root(sp->id) == pr_run_species_root(out_species[k])) {
                        dup = true;
                        break;
                    }
                }
                if (!dup) {
                    cost_species[n_cost++] = sp->id;
                }
            }
        }
        if (n_cost == 0) return -1;

        int pick_idx = n_cost == 1 ? 0 : pr_rng_integer_in_range(&r, 0, n_cost - 1);
        uint16_t rand_pkm_species = cost_species[pick_idx];

        int starter_species = pr_run_determine_enemy_species(&r, rand_pkm_species, 20, true, PR_EVO_KIND_STRONG, true, false);

        out_species[n_starters] = starter_species;

        pr_enemy_gen_ctx ctx;
        memset(&ctx, 0, sizeof(ctx));
        ctx.species = starter_species;
        ctx.level = 20;
        ctx.trainer_slot = 0;
        ctx.wave = -1;
        ctx.biome = -1;

        if (pr_run_enemy_pokemon_base_rng(&r, (pr_run *)run, &ctx, &out_starters[n_starters]) < 0) return -1;

        n_starters++;
    }
    return 0;
}

int pr_run_get_daily_starting_biome(const pr_run *run)
{
    static const uint8_t daily_biome_weights[PR_BIOME_ID_LIMIT] = {
        [BIOME_ID_CAVE] = 3, [BIOME_ID_LAKE] = 3, [BIOME_ID_PLAINS] = 3, [BIOME_ID_SNOWY_FOREST] = 3,
        [BIOME_ID_SWAMP] = 3, [BIOME_ID_TALL_GRASS] = 3,
        [BIOME_ID_ABYSS] = 2, [BIOME_ID_RUINS] = 2, [BIOME_ID_BADLANDS] = 2, [BIOME_ID_BEACH] = 2,
        [BIOME_ID_CONSTRUCTION_SITE] = 2, [BIOME_ID_DESERT] = 2, [BIOME_ID_DOJO] = 2, [BIOME_ID_FACTORY] = 2,
        [BIOME_ID_FAIRY_CAVE] = 2, [BIOME_ID_FOREST] = 2, [BIOME_ID_GRASS] = 2, [BIOME_ID_MEADOW] = 2,
        [BIOME_ID_MOUNTAIN] = 2, [BIOME_ID_SEA] = 2, [BIOME_ID_SEABED] = 2, [BIOME_ID_SLUM] = 2,
        [BIOME_ID_TEMPLE] = 2, [BIOME_ID_VOLCANO] = 2,
        [BIOME_ID_GRAVEYARD] = 1, [BIOME_ID_ICE_CAVE] = 1, [BIOME_ID_ISLAND] = 1, [BIOME_ID_JUNGLE] = 1,
        [BIOME_ID_LABORATORY] = 1, [BIOME_ID_METROPOLIS] = 1, [BIOME_ID_POWER_PLANT] = 1, [BIOME_ID_SPACE] = 1,
        [BIOME_ID_WASTELAND] = 1,
        [BIOME_ID_TOWN] = 0, [BIOME_ID_END] = 0
    };

    pr_run copy = *run;
    pr_run_reset_seed(&copy, 0);

    int total_weight = 0;
    int thresholds[35];
    for (int i = 0; i < pr_biome_count; i++) {
        int b = pr_biome_order[i];
        total_weight += daily_biome_weights[b];
        thresholds[i] = total_weight;
    }

    int rand_int = pr_rng_seed_int(&copy.rng, total_weight, 0);

    for (int i = 0; i < pr_biome_count; i++) {
        if (rand_int < thresholds[i]) {
            return pr_biome_order[i];
        }
    }
    return pr_biome_order[0];
}
