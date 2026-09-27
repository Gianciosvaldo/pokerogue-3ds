#define _POSIX_C_SOURCE 200809L
/* C half of run-core's encounter test: replays the cases written by encounter.producer.test.ts and
 * compares the generated wave-cycle and gym offsets, the time of day, the wild species roll (up to
 * the evolution step, + RNG state) and the trainer type roll (+ RNG state). */
#include "pr/run.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { C_OFFSETS, C_TOD, C_SPECIES, C_TRAINER, C_COUNT_ };
static const char *const check_names[C_COUNT_] = {
    "wave-cycle and gym offsets", "getTimeOfDay", "randomSpecies (+ RNG state)", "randomTrainerType (+ RNG state)",
};
static int pass[C_COUNT_], fail[C_COUNT_], uncovered[C_COUNT_];

static char *tok(char **save)
{
    char *t = strtok_r(NULL, " \n", save);
    if (!t) { fprintf(stderr, "short line\n"); exit(2); }
    return t;
}
static double num(char **save) { return strtod(tok(save), NULL); }
static void read_state(char **save, pr_rng *r)
{
    memset(r, 0, sizeof *r);
    r->c = num(save); r->s0 = num(save); r->s1 = num(save); r->s2 = num(save);
}
static bool same_state(const pr_rng *a, const pr_rng *b)
{
    return a->c == b->c && a->s0 == b->s0 && a->s1 == b->s1 && a->s2 == b->s2;
}
static void record(int check, unsigned *unported, bool ok, int line_no)
{
    if (*unported) { uncovered[check]++; *unported = 0; return; }
    if (ok) { pass[check]++; return; }
    if (++fail[check] <= 10) printf("MISMATCH line %d: %s\n", line_no, check_names[check]);
}

int main(int argc, char **argv)
{
    FILE *f = fopen(argc > 1 ? argv[1] : "encounter-cases.txt", "r");
    if (!f) { perror("cases"); return 2; }
    static char line[8192];
    int line_no = 0, cases = 0;
    while (fgets(line, sizeof line, f)) {
        line_no++;
        char *save;
        char *t = strtok_r(line, " \n", &save);
        if (!t || strcmp(t, "E")) continue;
        cases++;
        pr_run run;
        memset(&run, 0, sizeof run);
        run.mode = (int)num(&save);
        pr_seed_from_ascii(&run.seed, tok(&save));
        int gen_cycle = (int)num(&save), gen_gym = (int)num(&save);
        record(C_OFFSETS, &run.unported,
               pr_run_get_generated_wave_cycle_offset(&run) == gen_cycle && pr_run_get_generated_offset_gym(&run) == gen_gym,
               line_no);
        int biome = (int)num(&save), wave = (int)num(&save), cycle = (int)num(&save);
        run.offset_gym = num(&save) != 0;
        int tod = (int)num(&save);
        record(C_TOD, &run.unported, pr_run_get_time_of_day(biome, wave, cycle) == tod, line_no);

        int level = (int)num(&save), luck = (int)num(&save), is_boss = (int)num(&save);
        pr_rng after;
        read_state(&save, &run.rng);
        char *sv = tok(&save);
        int evo_species = (int)num(&save), evo_boss = (int)num(&save);
        read_state(&save, &after);
        bool my_evo_boss = false;
        int species = pr_run_random_species(&run, biome, wave, level, luck, is_boss, cycle, &my_evo_boss);
        record(C_SPECIES, &run.unported,
               strcmp(sv, "NaN") && species == atoi(sv) && species == evo_species && my_evo_boss == (evo_boss != 0)
                   && same_state(&run.rng, &after),
               line_no);

        bool trainer_boss = num(&save) != 0;
        read_state(&save, &run.rng);
        int trainer = (int)num(&save);
        read_state(&save, &after);
        record(C_TRAINER, &run.unported,
               pr_run_random_trainer_type(&run, biome, wave, trainer_boss) == trainer && same_state(&run.rng, &after), line_no);
    }
    printf("cases %d\n", cases);
    int bad = 0;
    for (int c = 0; c < C_COUNT_; c++) {
        printf("  %-36s pass %6d  fail %d  not covered %d\n", check_names[c], pass[c], fail[c], uncovered[c]);
        bad |= fail[c];
    }
    return bad ? 1 : 0;
}
