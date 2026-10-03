/* C half of Task 43 Memory Mushroom differential test. */
#include "pr/reward.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int next_int(char **p)
{
    char *end;
    long v = strtol(*p, &end, 10);
    if (end == *p) { fprintf(stderr, "bad integer near: %s\n", *p); exit(2); }
    *p = end;
    return (int)v;
}
static void skip(char **p) { while (**p == ' ' || **p == '\t') (*p)++; }

int main(int argc, char **argv)
{
    if (argc != 2) { fprintf(stderr, "usage: %s <cases>\n", argv[0]); return 2; }
    FILE *f = fopen(argv[1], "r");
    if (!f) { perror(argv[1]); return 2; }

    char line[32768];
    int cases = 0, fail = 0;
    while (fgets(line, sizeof line, f)) {
        char *p = line; skip(&p);
        if (*p++ != 'C') { fprintf(stderr, "bad record\n"); return 2; }
        skip(&p);

        pr_run run; memset(&run, 0, sizeof run);
        pr_reward_ctx ctx; memset(&ctx, 0, sizeof ctx);
        ctx.run = &run; ctx.n_party = 1;
        pr_reward_mon *m = &ctx.party[0];

        run.mode = next_int(&p); skip(&p);
        m->met_as_starter = next_int(&p) != 0; skip(&p);
        m->egg_relearn_allowed = next_int(&p) != 0; skip(&p);

        m->n_moves = (uint8_t)next_int(&p); skip(&p);
        for (int i = 0; i < m->n_moves; i++) {
            m->moves[i].move = (uint16_t)next_int(&p); skip(&p);
        }

        m->n_level_moves = (uint16_t)next_int(&p); skip(&p);
        if (m->n_level_moves > PR_REWARD_MAX_LEVEL_MOVES) return 2;
        for (int i = 0; i < m->n_level_moves; i++) {
            m->level_move_levels[i] = (int16_t)next_int(&p); skip(&p);
            m->level_moves[i] = (uint16_t)next_int(&p); skip(&p);
            m->level_move_sources[i] = (int8_t)next_int(&p); skip(&p);
        }

        m->n_egg_moves = (uint8_t)next_int(&p); skip(&p);
        if (m->n_egg_moves > 4) return 2;
        for (int i = 0; i < m->n_egg_moves; i++) {
            m->egg_moves[i] = (uint16_t)next_int(&p); skip(&p);
        }

        m->n_used_tms = (uint8_t)next_int(&p); skip(&p);
        if (m->n_used_tms > PR_REWARD_MAX_USED_TMS) return 2;
        for (int i = 0; i < m->n_used_tms; i++) {
            m->used_tms[i] = (uint16_t)next_int(&p); skip(&p);
        }

        int nexp = next_int(&p); skip(&p);
        pr_reward_memory_move exp[PR_REWARD_MAX_LEVEL_MOVES + PR_REWARD_MAX_USED_TMS + 4];
        if (nexp > (int)(sizeof exp / sizeof exp[0])) return 2;
        for (int i = 0; i < nexp; i++) {
            exp[i].level = (int16_t)next_int(&p); skip(&p);
            exp[i].move = (uint16_t)next_int(&p); skip(&p);
            exp[i].source = (int8_t)next_int(&p); skip(&p);
        }
        int chosen = next_int(&p); skip(&p);
        int cost = next_int(&p); skip(&p);
        int exp_party = next_int(&p); skip(&p);
        int exp_qmove = next_int(&p); skip(&p);
        int exp_qtype = next_int(&p); skip(&p);
        int exp_qcost = next_int(&p);

        pr_reward_memory_move got[PR_REWARD_MAX_LEVEL_MOVES + PR_REWARD_MAX_USED_TMS + 4];
        int ngot = pr_reward_memory_moves(&ctx, 0, got, (int)(sizeof got / sizeof got[0]));
        pr_reward_target_state st;
        int nbegin = pr_reward_memory_begin(&ctx, 0, got, (int)(sizeof got / sizeof got[0]), &st);

        bool bad = ngot != nexp || nbegin != nexp || (st.mode == PR_REWARD_TARGET_REMEMBER_MOVE) != !!exp_party;
        if (!bad) for (int i = 0; i < nexp; i++)
            if (got[i].level != exp[i].level || got[i].move != exp[i].move || got[i].source != exp[i].source) {
                bad = true; break;
            }

        if (chosen >= 0) {
            pr_reward_target_state q;
            if (!pr_reward_memory_choose(&ctx, 0, chosen, cost, &q)
                || q.mode != PR_REWARD_TARGET_LEARN_MOVE
                || q.party_idx != 0 || q.option_idx != chosen
                || q.move != exp_qmove || q.cost != exp_qcost || !q.copy_reward_screen
                || exp_qtype != 1) bad = true;
        }

        cases++;
        if (bad) {
            if (fail < 20) fprintf(stderr, "FAIL case %d: got %d moves expected %d chosen=%d\n",
                                   cases, ngot, nexp, chosen);
            fail++;
        }
    }
    fclose(f);
    printf("memory summary: %d cases, %d mismatches\n", cases, fail);
    return fail ? 1 : 0;
}
