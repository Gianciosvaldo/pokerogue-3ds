/* C runner for run-core's Daily Run 50-wave differential test.
 * Replays daily.producer.test.ts cases and compares starters, starting biome, wave state,
 * battle generation, enemy attributes, shop status and RNG state at every wave boundary (1..50).
 */
#include "pr/run.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool d_near(double a, double b)
{
    return a == b || fabs(a - b) < 1e-9;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <cases.txt>\n", argv[0]);
        return 1;
    }

    FILE *f = fopen(argv[1], "r");
    if (!f) {
        perror(argv[1]);
        return 1;
    }

    char line[1024];
    int line_no = 0;
    int pass = 0, fail = 0;
    int runs = 0, waves_checked = 0;

    pr_run run;
    memset(&run, 0, sizeof(run));
    run.mode = GAME_MODES_DAILY;

    int exp_starting_biome = -1;
    int starter_idx = 0;
    pr_enemy_gen starters[3];
    int starter_species[3];

    while (fgets(line, sizeof(line), f)) {
        line_no++;
        if (line[0] == '#' || line[0] == '\n') continue;

        char tag[32];
        if (sscanf(line, "%31s", tag) != 1) continue;

        if (strcmp(tag, "INIT") == 0) {
            char seedstr[64];
            if (sscanf(line, "INIT %63s %d", seedstr, &exp_starting_biome) != 2) {
                fprintf(stderr, "line %d: malformed INIT line\n", line_no);
                fail++;
                continue;
            }
            runs++;
            memset(&run, 0, sizeof(run));
            run.mode = GAME_MODES_DAILY;
            pr_seed_from_ascii(&run.seed, seedstr);

            if (pr_run_get_daily_starters(&run, starters, starter_species) < 0) {
                fprintf(stderr, "line %d: pr_run_get_daily_starters failed\n", line_no);
                fail++;
            } else {
                pass++;
            }

            int got_biome = pr_run_get_daily_starting_biome(&run);
            if (got_biome != exp_starting_biome) {
                fprintf(stderr, "line %d: FAIL starting biome got %d exp %d\n", line_no, got_biome, exp_starting_biome);
                fail++;
            } else {
                pass++;
            }
            starter_idx = 0;
        } else if (strcmp(tag, "STARTER") == 0) {
            int species, ability, dummy_id, female, form, shiny, variant, nature;
            int ivs[6];
            if (sscanf(line, "STARTER %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
                       &species, &ability, &dummy_id, &female, &form, &shiny, &variant, &nature,
                       &ivs[0], &ivs[1], &ivs[2], &ivs[3], &ivs[4], &ivs[5]) != 14) {
                fprintf(stderr, "line %d: malformed STARTER line\n", line_no);
                fail++;
                continue;
            }
            if (starter_idx < 3) {
                pr_enemy_gen *st = &starters[starter_idx];
                int st_sp = starter_species[starter_idx];
                int st_female = (st->gender == GENDER_FEMALE) ? 1 : 0;
                bool ok = (st_sp == species) && (st->ability_index == ability) && (st_female == female)
                          && (st->form == form) && (st->shiny == (shiny != 0)) && (st->variant == variant)
                          && (st->nature == nature);
                for (int k = 0; k < 6; k++) {
                    if (st->ivs[k] != ivs[k]) ok = false;
                }
                if (!ok) {
                    fprintf(stderr, "line %d: FAIL starter %d got sp %d ab %d fem %d f %d sh %d v %d n %d exp sp %d ab %d fem %d f %d sh %d v %d n %d\n",
                            line_no, starter_idx, st_sp, st->ability_index, st_female, st->form, st->shiny, st->variant, st->nature,
                            species, ability, female, form, shiny, variant, nature);
                    fail++;
                } else {
                    pass++;
                }
                starter_idx++;
            }
        } else if (strcmp(tag, "WAVE") == 0) {
            int wave, biome, btype, is_double, ttype, variant, template_idx, n_levels, exp_shop;
            double exp_c, exp_s0, exp_s1, exp_s2;

            /* Read fixed fields */
            char *p = line + 5;
            if (sscanf(p, "%d %d %d %d %d %d %d %d", &wave, &biome, &btype, &is_double, &ttype, &variant, &template_idx, &n_levels) != 8) {
                fprintf(stderr, "line %d: malformed WAVE line\n", line_no);
                fail++;
                continue;
            }
            /* Advance past the 8 fields */
            for (int k = 0; k < 8; k++) {
                p = strchr(p, ' ');
                if (!p) break;
                p++;
            }
            int exp_levels[PR_MAX_PARTY_TEMPLATE];
            for (int k = 0; k < n_levels; k++) {
                if (sscanf(p, "%d", &exp_levels[k]) != 1) break;
                p = strchr(p, ' ');
                if (!p) break;
                p++;
            }
            if (sscanf(p, "%d %lf %lf %lf %lf", &exp_shop, &exp_c, &exp_s0, &exp_s1, &exp_s2) != 5) {
                fprintf(stderr, "line %d: malformed WAVE line end\n", line_no);
                fail++;
                continue;
            }

            waves_checked++;
            pr_run_reset_seed(&run, wave);

            /* Check shop status */
            bool got_shop = pr_run_get_shop_status(&run);
            if (got_shop != (exp_shop != 0)) {
                fprintf(stderr, "line %d: FAIL wave %d shop status got %d exp %d\n", line_no, wave, got_shop, exp_shop);
                fail++;
            } else {
                pass++;
            }

            /* Perform battle generation in C */
            pr_new_battle_ctx ctx;
            memset(&ctx, 0, sizeof(ctx));
            ctx.biome = biome;
            ctx.trainer_chance = pr_biomes[biome].trainer_chance;
            ctx.prev_wave = wave - 1;

            pr_new_battle b_out;
            if (pr_run_new_battle(&run, wave, &ctx, &b_out) < 0) {
                fprintf(stderr, "line %d: FAIL wave %d newBattle failed unported 0x%x\n", line_no, wave, run.unported);
                fail++;
            } else {
                bool b_ok = (b_out.battle_type == btype) && (b_out.double_battle == (is_double != 0))
                            && (b_out.trainer_type == ttype) && (b_out.variant == variant)
                            && (b_out.template_index == template_idx) && (b_out.n_levels == n_levels);
                for (int k = 0; k < n_levels; k++) {
                    if (b_out.levels[k] != exp_levels[k]) b_ok = false;
                }
                if (!b_ok) {
                    fprintf(stderr, "line %d: FAIL wave %d newBattle mismatch got bt %d d %d tt %d v %d tm %d exp bt %d d %d tt %d v %d tm %d\n",
                            line_no, wave, b_out.battle_type, b_out.double_battle, b_out.trainer_type, b_out.variant, b_out.template_index,
                            btype, is_double, ttype, variant, template_idx);
                    fail++;
                } else {
                    pass++;
                }

                bool rng_ok = d_near(run.rng.c, exp_c) && d_near(run.rng.s0, exp_s0) && d_near(run.rng.s1, exp_s1) && d_near(run.rng.s2, exp_s2);
                if (!rng_ok) {
                    fprintf(stderr, "line %d: FAIL wave %d RNG state mismatch got %.16g %.16g %.16g %.16g exp %.16g %.16g %.16g %.16g\n",
                            line_no, wave, run.rng.c, run.rng.s0, run.rng.s1, run.rng.s2, exp_c, exp_s0, exp_s1, exp_s2);
                    fail++;
                } else {
                    pass++;
                }
            }
        }
    }

    fclose(f);
    printf("dailytest summary: %d daily runs (%d waves), %d checks passed, %d mismatches\n",
           runs, waves_checked, pass, fail);
    return fail > 0 ? 1 : 0;
}
