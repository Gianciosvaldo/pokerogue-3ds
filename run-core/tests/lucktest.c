#define _POSIX_C_SOURCE 200809L
/* C half of run-core's party luck test: replays the cases written by luck.producer.test.ts and compares
 * getPartyLuckValue and the RNG state it leaves (the daily roll is on its own RNG, so none moves). */
#include "pr/run.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

int main(int argc, char **argv)
{
    FILE *f = fopen(argc > 1 ? argv[1] : "luck-cases.txt", "r");
    if (!f) { perror("cases"); return 2; }
    static char line[4096];
    int line_no = 0, cases = 0, pass = 0, fail = 0, uncovered = 0, daily = 0, capped = 0;
    while (fgets(line, sizeof line, f)) {
        line_no++;
        char *save;
        char *t = strtok_r(line, " \n", &save);
        if (!t || strcmp(t, "L")) continue;
        cases++;
        pr_run run;
        memset(&run, 0, sizeof run);
        run.mode = (int)num(&save);
        pr_seed_from_ascii(&run.seed, tok(&save));
        pr_seed_from_ascii(&run.wave_seed, tok(&save));
        const int boost = (int)num(&save), n_ev = (int)num(&save);
        int ev[8];
        for (int i = 0; i < n_ev && i < 8; i++) ev[i] = (int)num(&save);
        const int n = (int)num(&save);
        pr_luck_member party[8];
        for (int i = 0; i < n && i < 8; i++) {
            party[i].species = (int)num(&save);
            party[i].luck = (int)num(&save);
            party[i].fusion_luck = (int)num(&save);
            party[i].fusion = num(&save) != 0;
            party[i].allowed = num(&save) != 0;
        }
        read_state(&save, &run.rng);
        const int up = (int)num(&save);
        pr_rng after;
        read_state(&save, &after);
        const int mine = pr_run_get_party_luck_value(&run, party, n, ev, n_ev, boost);
        if (run.unported) { uncovered++; continue; }
        const bool ok = mine == up && run.rng.c == after.c && run.rng.s0 == after.s0 && run.rng.s1 == after.s1
                        && run.rng.s2 == after.s2;
        daily += run.mode == GAME_MODES_DAILY;
        capped += up == 14;
        if (ok) pass++;
        else if (++fail <= 10) printf("MISMATCH line %d: %d, upstream %d\n", line_no, mine, up);
    }
    printf("cases %d (daily %d, at the cap of 14: %d)\n", cases, daily, capped);
    printf("  %-36s pass %6d  fail %d  not covered %d\n", "getPartyLuckValue (+ RNG)", pass, fail, uncovered);
    return fail ? 1 : 0;
}
