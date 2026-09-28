#define _POSIX_C_SOURCE 200809L
/* C half of run-core's enemy item test: replays the cases written by enemyitems.producer.test.ts and
 * compares each enemy's items (in the order they're added) and the RNG state after the item step. */
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
static int item_index(const char *name)
{
    for (int i = 0; i < pr_item_count; i++)
        if (!strcmp(pr_item_names[i], name)) return i;
    return -1;
}

int main(int argc, char **argv)
{
    FILE *f = fopen(argc > 1 ? argv[1] : "enemyitems-cases.txt", "r");
    if (!f) { perror("cases"); return 2; }
    static char line[8192];
    int line_no = 0, cases = 0, pass = 0, fail = 0, uncovered = 0, threw = 0, items = 0, boosters = 0, retried = 0;
    pr_run run;
    pr_enemy_items_ctx ctx;
    pr_enemy_items mine;
    pr_rng after;
    int rc = 0, member = 0;
    bool ok = true;
    while (fgets(line, sizeof line, f)) {
        line_no++;
        char *save;
        char *t = strtok_r(line, " \n", &save);
        if (!t || t[0] == '#') continue;
        if (!strcmp(t, "X")) { threw++; continue; }
        if (!strcmp(t, "I")) {
            memset(&run, 0, sizeof run);
            memset(&ctx, 0, sizeof ctx);
            cases++;
            run.mode = (int)num(&save);
            pr_seed_from_ascii(&run.seed, tok(&save));
            ctx.wave = (int)num(&save);
            ctx.trainer_battle = num(&save) != 0;
            ctx.double_battle = num(&save) != 0;
            ctx.trainer_type = (int)num(&save);
            ctx.n_party = (int)num(&save);
            for (int p = 0; p < ctx.n_party; p++) {
                ctx.party[p].boss = num(&save) != 0;
                ctx.party[p].n_attack_types = (int)num(&save);
                for (int k = 0; k < ctx.party[p].n_attack_types; k++) {
                    const int ty = (int)num(&save);
                    if (k < 16) ctx.party[p].attack_types[k] = (int8_t)ty;
                }
                if (ctx.party[p].n_attack_types > 16) ctx.party[p].n_attack_types = 16;
                retried += ctx.party[p].n_attack_types == 0;
            }
            read_state(&save, &run.rng);
            read_state(&save, &after);
            rc = pr_run_generate_enemy_items(&run, &ctx, &mine);
            member = 0;
            ok = rc == 0 && run.rng.c == after.c && run.rng.s0 == after.s0 && run.rng.s1 == after.s1
                 && run.rng.s2 == after.s2;
            continue;
        }
        if (strcmp(t, "M")) continue;
        const int p = member++;
        const int n = (int)num(&save);
        bool same = n == mine.n[p];
        for (int k = 0; k < n; k++) {
            const int item = item_index(tok(&save)), arg = (int)num(&save);
            items++;
            boosters += item == 2;
            if (same) same = item == mine.items[p][k].item && arg == mine.items[p][k].arg;
        }
        ok = ok && same;
        if (member == ctx.n_party) {
            if (run.unported) uncovered++;
            else if (ok) pass++;
            else if (++fail <= 10) printf("MISMATCH line %d\n", line_no);
        }
    }
    printf("cases %d (upstream threw: %d); %d items, %d attack type boosters; %d enemies without attack moves\n",
           cases, threw, items, boosters, retried);
    printf("  %-36s pass %6d  fail %d  not covered %d\n", "enemy items (+ RNG)", pass, fail, uncovered);
    return fail ? 1 : 0;
}
