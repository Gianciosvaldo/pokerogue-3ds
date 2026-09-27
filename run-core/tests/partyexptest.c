#define _POSIX_C_SOURCE 200809L
/* C half of run-core's victory-experience test: replays partyexp.producer.test.ts's cases. */
#include "pr/run.h"
#include <math.h>
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
    FILE *f = fopen(argc > 1 ? argv[1] : "partyexp-cases.txt", "r");
    if (!f) { perror("cases"); return 2; }
    static char line[4096];
    int pass = 0, fail = 0, line_no = 0, given = 0;
    while (fgets(line, sizeof line, f)) {
        line_no++;
        char *save;
        char *t = strtok_r(line, " \n", &save);
        if (!t || strcmp(t, "P")) continue;
        pr_exp_party pp;
        memset(&pp, 0, sizeof pp);
        int wave = (int)num(&save);
        pp.trainer_battle = num(&save) != 0;
        pp.me_exp_multiplier = NAN;
        pp.max_exp_level = num(&save);
        double exp_value = num(&save);
        bool use_wave = num(&save) != 0;
        pp.n_participants = (int)num(&save);
        pp.exp_share_stacks = (int)num(&save);
        pp.exp_balance_stacks = (int)num(&save);
        pp.oval_charm_stacks = (int)num(&save);
        pp.n_party = (int)num(&save);
        if (pp.n_party < 0 || pp.n_party > PR_MAX_EXP_PARTY) { fprintf(stderr, "bad party\n"); return 2; }
        for (int i = 0; i < pp.n_party; i++) {
            pp.party[i].id = (uint32_t)num(&save);
            pp.party[i].level = (int)num(&save);
            pp.party[i].hp = (int)num(&save);
            pp.party[i].pokerus = num(&save) != 0;
            pp.party[i].participated = num(&save) != 0;
        }
        pp.n_eggs = (int)num(&save);
        if (pp.n_eggs < 0 || pp.n_eggs > 16) { fprintf(stderr, "bad eggs\n"); return 2; }
        for (int i = 0; i < pp.n_eggs; i++) {
            pp.eggs[i].pokemon_id = (uint32_t)num(&save);
            pp.eggs[i].stacks = (int)num(&save);
            pp.eggs[i].boost_percent = num(&save);
        }
        int n = (int)num(&save), idx[PR_MAX_EXP_PARTY], my_idx[PR_MAX_EXP_PARTY];
        double exp[PR_MAX_EXP_PARTY], my_exp[PR_MAX_EXP_PARTY];
        for (int i = 0; i < n; i++) { idx[i] = (int)num(&save); exp[i] = num(&save); }
        given += n;
        int my_n = pr_run_apply_party_exp(&pp, exp_value, use_wave, wave, my_idx, my_exp);
        bool ok = my_n == n;
        for (int i = 0; ok && i < n; i++) ok = my_idx[i] == idx[i] && my_exp[i] == exp[i];
        if (ok) pass++;
        else if (++fail <= 10) printf("MISMATCH line %d\n", line_no);
    }
    printf("applyPartyExp: pass %d  fail %d (experience given to %d party members)\n", pass, fail, given);
    return fail ? 1 : 0;
}
