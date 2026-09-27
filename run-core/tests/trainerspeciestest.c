#define _POSIX_C_SOURCE 200809L
/* C half of run-core's trainer party species test: replays the cases written by
 * trainerspecies.producer.test.ts and compares genNewPartyMemberSpecies and the RNG state it leaves. */
#include "pr/run.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int pass, fail, uncovered;
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
    FILE *f = fopen(argc > 1 ? argv[1] : "trainerspecies-cases.txt", "r");
    if (!f) { perror("cases"); return 2; }
    static char line[4096];
    int line_no = 0, cases = 0, pools = 0, threw = 0;
    while (fgets(line, sizeof line, f)) {
        line_no++;
        char *save;
        char *t = strtok_r(line, " \n", &save);
        if (!t || t[0] == '#') continue;
        if (!strcmp(t, "X")) { threw++; continue; }
        if (strcmp(t, "T")) continue;
        cases++;
        pr_run run;
        pr_party_species_ctx ctx;
        memset(&run, 0, sizeof run);
        memset(&ctx, 0, sizeof ctx);
        run.mode = (int)num(&save);
        ctx.trainer_type = (int)num(&save);
        ctx.level = (int)num(&save);
        ctx.wave = (int)num(&save);
        ctx.evo_kind = (int)num(&save);
        ctx.balanced = num(&save) != 0;
        ctx.classic_wave20_trainer = num(&save) != 0;
        ctx.n_party = (int)num(&save);
        for (int i = 0; i < ctx.n_party; i++) ctx.party_roots[i] = (uint16_t)num(&save);
        ctx.n_party_types = (int)num(&save);
        for (int i = 0; i < ctx.n_party_types; i++) ctx.party_types[i] = (int8_t)num(&save);
        pools += num(&save) != 0;
        read_state(&save, &run.rng);
        int up = (int)num(&save);
        pr_rng after;
        read_state(&save, &after);
        int mine = pr_run_gen_new_party_member_species(&run, &ctx);
        if (run.unported) { uncovered++; continue; }
        bool ok = mine == up && run.rng.c == after.c && run.rng.s0 == after.s0 && run.rng.s1 == after.s1
                  && run.rng.s2 == after.s2;
        if (ok) pass++;
        else if (++fail <= 10) printf("MISMATCH line %d: species %d, upstream %d\n", line_no, mine, up);
    }
    printf("cases %d (%d from species pools; upstream threw: %d)\n", cases, pools, threw);
    printf("  %-36s pass %6d  fail %d  not covered %d\n", "genNewPartyMemberSpecies (+ RNG)", pass, fail, uncovered);
    return fail ? 1 : 0;
}
