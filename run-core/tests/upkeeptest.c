#define _POSIX_C_SOURCE 200809L
/* C half of run-core's upkeep test: replays upkeep.producer.test.ts's cases. */
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

int main(int argc, char **argv)
{
    FILE *f = fopen(argc > 1 ? argv[1] : "upkeep-cases.txt", "r");
    if (!f) { perror("cases"); return 2; }
    static char line[1024];
    int pass[2] = {0, 0}, fail[2] = {0, 0}, spread = 0, line_no = 0;
    while (fgets(line, sizeof line, f)) {
        line_no++;
        char *save;
        char *t = strtok_r(line, " \n", &save);
        if (!t || strcmp(t, "U")) continue;
        pr_run run;
        memset(&run, 0, sizeof run);
        pr_seed_from_ascii(&run.seed, tok(&save));
        int wave = (int)num(&save), n = (int)num(&save);
        if (n < 1 || n > PR_MAX_EXP_PARTY) { fprintf(stderr, "bad party\n"); return 2; }
        bool pokerus[PR_MAX_EXP_PARTY];
        int after[PR_MAX_EXP_PARTY];
        for (int i = 0; i < n; i++) pokerus[i] = num(&save) != 0;
        for (int i = 0; i < n; i++) after[i] = (int)num(&save);
        spread += pr_run_try_spread_pokerus(&run, wave, pokerus, n);
        bool ok = true;
        for (int i = 0; i < n; i++) ok = ok && pokerus[i] == (after[i] != 0);
        if (ok) pass[0]++; else if (++fail[0] <= 10) printf("MISMATCH line %d: trySpreadPokerus\n", line_no);

        double money = num(&save);
        int n_stacks = (int)num(&save), stacks[8];
        if (n_stacks < 0 || n_stacks > 8) { fprintf(stderr, "bad coin cases\n"); return 2; }
        for (int i = 0; i < n_stacks; i++) stacks[i] = (int)num(&save);
        double expected = num(&save), mine = pr_run_apply_money_interest(money, stacks, n_stacks);
        if (mine == expected) pass[1]++;
        else if (++fail[1] <= 10) printf("MISMATCH line %d: interest C %.17g JS %.17g\n", line_no, mine, expected);
    }
    printf("trySpreadPokerus: pass %d  fail %d (%d Pokemon caught it)\n", pass[0], fail[0], spread);
    printf("Coin Case interest: pass %d  fail %d\n", pass[1], fail[1]);
    return fail[0] || fail[1] ? 1 : 0;
}
