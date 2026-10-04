/* Strict additive replay of simultaneous actual TM/relearner inputs. */
#include "../src/reward_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *input;
static int integer(void)
{
    int v;
    if (fscanf(input, "%d", &v) != 1) { fputs("malformed input\n", stderr); exit(2); }
    return v;
}
static int count(int cap)
{
    int n = integer();
    if (n < 0 || n > cap) { fputs("invalid count\n", stderr); exit(2); }
    return n;
}
static void record(char expected)
{
    char c;
    if (fscanf(input, " %c", &c) != 1 || c != expected) { fputs("wrong record\n", stderr); exit(2); }
}
static pr_rng rng(void)
{
    pr_rng r = {0};
    if (fscanf(input, "%lf %lf %lf %lf", &r.c, &r.s0, &r.s1, &r.s2) != 4) exit(2);
    return r;
}
static int read_rows(pr_reward_memory_move *rows, int cap)
{
    int n = count(cap);
    for (int i = 0; i < n; i++) {
        rows[i].level = integer(); rows[i].move = integer(); rows[i].source = integer();
    }
    return n;
}
static void fill(pr_reward_mon *m, const pr_reward_memory_move *rows, int n, bool relearn)
{
    for (int i = 0; i < n; i++) {
        if (relearn) {
            m->relearn_moves[i] = rows[i].move;
            m->relearn_move_levels[i] = rows[i].level;
            m->relearn_move_sources[i] = rows[i].source;
        } else {
            m->level_moves[i] = rows[i].move;
            m->level_move_levels[i] = rows[i].level;
            m->level_move_sources[i] = rows[i].source;
        }
    }
    if (relearn) m->n_relearn_moves = n; else m->n_level_moves = n;
}
static int selftest(void)
{
    pr_reward_ctx ctx = {0}; ctx.n_party = 1;
    pr_reward_mon *m = &ctx.party[0];
    pr_reward_memory_move out[2];
    m->level_moves[0] = MOVE_ID_TACKLE; m->n_level_moves = 1;
    m->has_explicit_relearn_moves = true;
    if (pr_reward_memory_moves(&ctx, 0, out, 2) != 0) return 1;
    m->has_explicit_relearn_moves = false;
    if (pr_reward_memory_moves(&ctx, 0, out, 2) != 1) return 1;
    m->has_explicit_relearn_moves = true;
    m->n_relearn_moves = PR_REWARD_MAX_LEVEL_MOVES + 1;
    if (pr_reward_memory_moves(&ctx, 0, out, 2) != -1) return 1;
    puts("dual learnset host boundaries: 3 checks passed");
    return 0;
}
int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "--selftest")) return selftest();
    if ((argc != 2 && argc != 3) || (argc == 3 && strcmp(argv[2], "--mutate-shared-list"))
        || !(input = fopen(argv[1], "r"))) return 2;
    record('V'); if (integer() != 1) return 2;
    int n = integer(), fail = 0, memory_fail = 0, tm_fail = 0;
    if (n <= 0) return 2;
    for (int c = 0; c < n; c++) {
        pr_run run = {0}; pr_reward_ctx ctx = {0};
        ctx.run = &run; ctx.n_party = 1;
        pr_reward_mon *m = &ctx.party[0];
        record('C'); run.mode = integer(); m->species = integer(); m->form = integer();
        m->fusion_species = integer(); m->fusion_form = integer(); m->met_as_starter = integer();
        m->egg_relearn_allowed = true; m->has_explicit_relearn_moves = argc == 2;
        record('B'); m->n_base_moves = count(4);
        for (int i = 0; i < m->n_base_moves; i++) m->base_moves[i] = integer();
        record('S'); m->n_moves = count(4);
        for (int i = 0; i < m->n_moves; i++) m->moves[i].move = integer();
        pr_reward_memory_move rows[PR_REWARD_MAX_LEVEL_MOVES + PR_REWARD_MAX_USED_TMS + 4];
        record('L'); fill(m, rows, read_rows(rows, PR_REWARD_MAX_LEVEL_MOVES), false);
        record('R'); fill(m, rows, read_rows(rows, PR_REWARD_MAX_LEVEL_MOVES), true);
        record('E'); m->n_egg_moves = count(4);
        for (int i = 0; i < m->n_egg_moves; i++) m->egg_moves[i] = integer();
        record('U'); m->n_used_tms = count(PR_REWARD_MAX_USED_TMS);
        for (int i = 0; i < m->n_used_tms; i++) m->used_tms[i] = integer();
        record('M'); int expected = read_rows(rows, (int)(sizeof rows / sizeof rows[0]));
        pr_reward_memory_move got[PR_REWARD_MAX_LEVEL_MOVES + PR_REWARD_MAX_USED_TMS + 4];
        int actual = pr_reward_memory_moves(&ctx, 0, got, (int)(sizeof got / sizeof got[0]));
        bool bad_memory = actual != expected;
        if (!bad_memory) for (int i = 0; i < expected; i++)
            if (got[i].move != rows[i].move || got[i].level != rows[i].level || got[i].source != rows[i].source) bad_memory = true;
        bool bad_tm = false;
        const int items[] = {PR_ITEM_TM_COMMON, PR_ITEM_TM_GREAT, PR_ITEM_TM_ULTRA};
        for (int k = 0; k < 3; k++) {
            record('T'); run.rng = rng(); int expected_move = integer(); pr_rng after = rng();
            int move = pr_reward_generate(&ctx, items[k]);
            if (move != expected_move || run.rng.c != after.c || run.rng.s0 != after.s0
                || run.rng.s1 != after.s1 || run.rng.s2 != after.s2 || run.unported) bad_tm = true;
        }
        if (bad_memory) memory_fail++;
        if (bad_tm) tm_fail++;
        if (bad_memory || bad_tm) { if (fail < 10) printf("FAIL CASE %d memory=%d tm=%d\n", c, bad_memory, bad_tm); fail++; }
    }
    record('Z'); if (integer() != n) return 2;
    char extra; if (fscanf(input, " %c", &extra) == 1) return 2;
    fclose(input);
    printf("dual learnset inputs: %d cases, %d identical, %d mismatches; memory failures %d, TM/RNG failures %d; thrown/skipped 0\n",
           n, n - fail, fail, memory_fail, tm_fail);
    return fail ? 1 : 0;
}
