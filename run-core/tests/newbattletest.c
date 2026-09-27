#define _POSIX_C_SOURCE 200809L
/* C half of run-core's new-battle test: replays the cases written by newbattle.producer.test.ts and
 * compares isMysteryEncounterValidForWave, isWaveMysteryEncounter and Trainer.getPartyLevels. */
#include "pr/run.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { C_VALID, C_ME, C_LEVELS, C_COUNT_ };
static const char *const check_names[C_COUNT_] = {
    "isMysteryEncounterValidForWave", "isWaveMysteryEncounter", "Trainer.getPartyLevels",
};
static int pass[C_COUNT_], fail[C_COUNT_], uncovered[C_COUNT_];

static char *tok(char **save)
{
    char *t = strtok_r(NULL, " \n", save);
    if (!t) { fprintf(stderr, "short line\n"); exit(2); }
    return t;
}
static double num(char **save) { return strtod(tok(save), NULL); }
static void record(int check, unsigned *unported, bool ok, int line_no)
{
    if (*unported) { uncovered[check]++; *unported = 0; return; }
    if (ok) { pass[check]++; return; }
    if (++fail[check] <= 10) printf("MISMATCH line %d: %s\n", line_no, check_names[check]);
}

int main(int argc, char **argv)
{
    FILE *f = fopen(argc > 1 ? argv[1] : "newbattle-cases.txt", "r");
    if (!f) { perror("cases"); return 2; }
    static char line[4096];
    int line_no = 0, cases = 0, no_trainer = 0;
    pr_run run;
    memset(&run, 0, sizeof run);
    while (fgets(line, sizeof line, f)) {
        line_no++;
        char *save;
        char *t = strtok_r(line, " \n", &save);
        if (!t) continue;
        if (!strcmp(t, "N")) {
            cases++;
            memset(&run, 0, sizeof run);
            run.mode = (int)num(&save);
            pr_seed_from_ascii(&run.seed, tok(&save));
            int wave = (int)num(&save);
            bool wild = num(&save) == 0;   /* BattleType.WILD */
            pr_me_save me;
            me.spawn_chance = num(&save);
            me.n_encountered = (int)num(&save);
            me.last_encounter_wave = (int)num(&save);
            char *vv = tok(&save), *mv = tok(&save);
            unsigned none = 0;
            if (!strcmp(vv, "NaN")) { uncovered[C_VALID]++; uncovered[C_ME]++; continue; }
            record(C_VALID, &none, pr_run_is_mystery_encounter_valid_for_wave(&run, wild, wave) == atoi(vv), line_no);
            bool mine = pr_run_is_wave_mystery_encounter(&run, wild, wave, &me);
            record(C_ME, &run.unported, mine == atoi(mv), line_no);
        } else if (!strcmp(t, "L")) {
            char *first = tok(&save);
            if (!strcmp(first, "none")) { no_trainer++; continue; }
            int wave = atoi(first);
            bool dbl = num(&save) != 0;
            int size = (int)num(&save);
            uint8_t strengths[PR_MAX_PARTY_TEMPLATE] = {0};
            if (size < 0 || size > PR_MAX_PARTY_TEMPLATE) { fprintf(stderr, "bad template size\n"); return 2; }
            for (int i = 0; i < size; i++) strengths[i] = (uint8_t)num(&save);
            int base = (int)num(&save), n = (int)num(&save), levels[PR_MAX_PARTY_TEMPLATE];
            for (int i = 0; i < n && i < PR_MAX_PARTY_TEMPLATE; i++) levels[i] = (int)num(&save);
            int mine[PR_MAX_PARTY_TEMPLATE];
            int my_n = pr_run_get_party_levels(&run, wave, size, strengths, base, dbl, mine);
            bool ok = my_n == n;
            for (int i = 0; ok && i < n; i++) ok = mine[i] == levels[i];
            unsigned none = 0;
            record(C_LEVELS, &none, ok, line_no);
        }
    }
    printf("cases %d (trainers that couldn't be built upstream: %d)\n", cases, no_trainer);
    int bad = 0;
    for (int c = 0; c < C_COUNT_; c++) {
        printf("  %-36s pass %6d  fail %d  not covered %d\n", check_names[c], pass[c], fail[c], uncovered[c]);
        bad |= fail[c];
    }
    return bad ? 1 : 0;
}
