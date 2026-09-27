#define _POSIX_C_SOURCE 200809L
/* C half of run-core's wave test: replays the cases written by wave.producer.test.ts (run
 * inside upstream) and compares every answer and the RNG state each roll leaves behind.
 * A check whose function reported an unported mode rule counts as "not covered", never as
 * a pass. */
#include "pr/run.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { C_DET, C_CAP, C_LEVEL, C_TRAINER, C_CHAIN, C_SEGMENTS, C_DOUBLE, C_COUNT_ };
static const char *const check_names[C_COUNT_] = {
    "game-mode rules (15 per case)", "getMaxExpLevel", "getLevelForWave (+ RNG state)",
    "isWaveTrainer (+ RNG state)", "resetSeed + Battle seeding chain", "getEncounterBossSegments",
    "getDoubleBattleChance",
};
static int pass[C_COUNT_], fail[C_COUNT_], uncovered[C_COUNT_];

static char *tok(char **save)
{
    char *t = strtok_r(NULL, " \n", save);
    if (!t) { fprintf(stderr, "short line\n"); exit(2); }
    return t;
}
static double num(char **save) { return strtod(tok(save), NULL); }
static bool same(double c, double js) { return c == js || (c != c && js != js); }
static void read_state(char **save, pr_rng *r)
{
    memset(r, 0, sizeof *r);
    r->c = num(save); r->s0 = num(save); r->s1 = num(save); r->s2 = num(save);
}
static bool same_state(const pr_rng *a, const pr_rng *b)
{
    return a->c == b->c && a->s0 == b->s0 && a->s1 == b->s1 && a->s2 == b->s2;
}

/* Records one check: skipped as uncovered if the call hit an unported rule */
static void record(int check, pr_run *run, bool ok, int line_no, const char *what)
{
    if (run->unported) { uncovered[check]++; run->unported = 0; return; }
    if (ok) { pass[check]++; return; }
    if (++fail[check] <= 10) printf("MISMATCH line %d: %s (%s)\n", line_no, check_names[check], what);
}

int main(int argc, char **argv)
{
    FILE *f = fopen(argc > 1 ? argv[1] : "wave-cases.txt", "r");
    if (!f) { perror("cases"); return 2; }
    static char line[8192];
    int line_no = 0, cases = 0;
    while (fgets(line, sizeof line, f)) {
        line_no++;
        char *save;
        char *t = strtok_r(line, " \n", &save);
        if (!t || strcmp(t, "W")) continue;
        cases++;
        pr_run run;
        memset(&run, 0, sizeof run);
        run.mode = (int)num(&save);
        int wave = (int)num(&save);
        run.offset_gym = num(&save) != 0;
        int biome = (int)num(&save);
        int trainer_chance = (int)num(&save);
        pr_seed_from_ascii(&run.seed, tok(&save));

        /* game-mode rules */
        double det[15];
        for (int i = 0; i < 15; i++) det[i] = num(&save);
        double mine[15] = {
            pr_run_get_wave_for_difficulty(&run, wave, false), pr_run_get_wave_for_difficulty(&run, wave, true),
            pr_run_is_wave_final(run.mode, wave), pr_run_is_boss(wave), pr_run_is_battle_classic_final_boss(&run, wave),
            pr_run_is_endless_boss(&run, wave), pr_run_is_endless_minor_boss(&run, wave),
            pr_run_is_endless_major_boss(&run, wave), pr_run_is_trainer_boss(&run, wave, biome, run.offset_gym),
            pr_run_is_fixed_battle(&run, wave), pr_run_get_clear_score_bonus(&run),
            pr_run_get_enemy_modifier_chance(&run, true), pr_run_get_enemy_modifier_chance(&run, false),
            pr_run_get_starting_level(&run), 0,
        };
        unsigned before_money = run.unported;
        mine[14] = pr_run_get_starting_money(&run);
        for (int i = 0; i < 15; i++) {
            if (i == 14 && run.unported != before_money) { record(C_DET, &run, false, line_no, "getStartingMoney"); continue; }
            char what[48];
            snprintf(what, sizeof what, "rule %d: C %g JS %g", i, mine[i], det[i]);
            unsigned keep = run.unported;
            run.unported = 0;
            record(C_DET, &run, same(mine[i], det[i]), line_no, what);
            run.unported = keep;
        }
        run.unported = 0;

        /* level cap */
        int cap_wave = (int)num(&save);
        double cap = num(&save), cap_ignore = num(&save);
        record(C_CAP, &run, same(pr_run_get_max_exp_level(&run, cap_wave, false), cap)
                             && same(pr_run_get_max_exp_level(&run, cap_wave, true), cap_ignore), line_no, "cap");

        /* getLevelForWave on a given RNG state */
        pr_rng r, after;
        read_state(&save, &r);
        double level = num(&save);
        read_state(&save, &after);
        int my_level = pr_run_get_level_for_wave(&run, &r, wave);
        record(C_LEVEL, &run, same(my_level, level) && same_state(&r, &after), line_no, "level");

        /* isWaveTrainer on a given RNG state */
        read_state(&save, &run.rng);
        double trainer = num(&save);
        read_state(&save, &after);
        bool my_trainer = pr_run_is_wave_trainer(&run, wave, trainer_chance);
        record(C_TRAINER, &run, same(my_trainer, trainer) && same_state(&run.rng, &after), line_no, "trainer");

        /* resetSeed(wave), level inside executeWithSeedOffset(wave << 3, waveSeed), then isWaveTrainer */
        double chain_level = num(&save), chain_trainer = num(&save);
        pr_run_reset_seed(&run, wave);
        pr_rng br = pr_run_battle_rng(&run, wave);
        int cl = pr_run_get_level_for_wave(&run, &br, wave);
        bool ct = pr_run_is_wave_trainer(&run, wave, trainer_chance);
        record(C_CHAIN, &run, same(cl, chain_level) && same(ct, chain_trainer), line_no, "chain");

        /* boss segments */
        int species = (int)num(&save), boss_level = (int)num(&save);
        bool force = num(&save) != 0;
        double segments = num(&save);
        record(C_SEGMENTS, &run, same(pr_run_get_encounter_boss_segments(&run, wave, boss_level, species, force), segments),
               line_no, "segments");

        /* double battle chance */
        int n_lures = (int)num(&save);
        double chance = num(&save);
        record(C_DOUBLE, &run, same(pr_run_get_double_battle_chance(wave, n_lures, 0), chance), line_no, "double");
    }
    printf("cases %d\n", cases);
    int bad = 0;
    for (int c = 0; c < C_COUNT_; c++) {
        printf("  %-36s pass %6d  fail %d  not covered %d\n", check_names[c], pass[c], fail[c], uncovered[c]);
        bad |= fail[c];
    }
    return bad ? 1 : 0;
}
