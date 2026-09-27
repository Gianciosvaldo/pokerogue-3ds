#define _POSIX_C_SOURCE 200809L
/* C half of run-core's biome test: replays the cases written by biome.producer.test.ts and
 * compares the depth table, generateRandomBiome (+ RNG state) and SelectBiomePhase's choice
 * (the next biome or the Map's menu, + RNG state). */
#include "pr/run.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { C_DEPTHS, C_RANDOM, C_SELECT, C_COUNT_ };
static const char *const check_names[C_COUNT_] = {
    "initBiomeDepths", "generateRandomBiome (+ RNG state)", "SelectBiomePhase (+ RNG state)",
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
    FILE *f = fopen(argc > 1 ? argv[1] : "biome-cases.txt", "r");
    if (!f) { perror("cases"); return 2; }
    static char line[16384];
    int line_no = 0, cases = 0;
    while (fgets(line, sizeof line, f)) {
        line_no++;
        char *save;
        char *t = strtok_r(line, " \n", &save);
        if (!t || strcmp(t, "B")) continue;
        cases++;
        unsigned none = 0;

        pr_rng r, after;
        read_state(&save, &r);
        pr_biome_depths d;
        pr_run_init_biome_depths(&d, &r);
        bool ok = true;
        for (int i = 0; i < pr_biome_count; i++) {
            int b = pr_biome_order[i];
            char *v = tok(&save), mine[32];
            if (d.set[b]) snprintf(mine, sizeof mine, "%d,%d", d.depth[b], d.chance[b]);
            else strcpy(mine, "-");
            ok = ok && !strcmp(v, mine);
        }
        record(C_DEPTHS, &none, ok, line_no);

        int rand_wave = (int)num(&save);
        read_state(&save, &r);
        char *rv = tok(&save);
        read_state(&save, &after);
        int mine_random = pr_run_generate_random_biome(&d, &r, rand_wave);
        record(C_RANDOM, &none, strcmp(rv, "NaN") && atoi(rv) == mine_random && same_state(&r, &after), line_no);

        pr_run run;
        memset(&run, 0, sizeof run);
        run.mode = (int)num(&save);
        pr_seed_from_ascii(&run.seed, tok(&save));
        int wave = (int)num(&save), current = (int)num(&save);
        bool has_map = num(&save) != 0;
        char *nv = tok(&save);
        int n_picked = (int)num(&save), picked[PR_MAX_BIOME_LINKS];
        if (n_picked < 0 || n_picked > PR_MAX_BIOME_LINKS) { fprintf(stderr, "bad pick count\n"); return 2; }
        for (int i = 0; i < n_picked; i++) picked[i] = (int)num(&save);
        read_state(&save, &after);
        uint8_t choices[PR_MAX_BIOME_LINKS];
        int n_choices;
        int next = pr_run_select_next_biome(&run, &d, current, wave, has_map, choices, &n_choices);
        bool sel = n_choices == n_picked && same_state(&run.rng, &after);
        for (int i = 0; sel && i < n_choices; i++) sel = choices[i] == picked[i];
        if (!strcmp(nv, "undef")) sel = sel && (run.unported & PR_RUN_UNDEFINED_UPSTREAM);
        else sel = sel && next == atoi(nv) && !(run.unported & PR_RUN_UNDEFINED_UPSTREAM);
        run.unported &= ~PR_RUN_UNDEFINED_UPSTREAM;
        record(C_SELECT, &run.unported, sel, line_no);
    }
    printf("cases %d\n", cases);
    int bad = 0;
    for (int c = 0; c < C_COUNT_; c++) {
        printf("  %-36s pass %6d  fail %d  not covered %d\n", check_names[c], pass[c], fail[c], uncovered[c]);
        bad |= fail[c];
    }
    return bad ? 1 : 0;
}
