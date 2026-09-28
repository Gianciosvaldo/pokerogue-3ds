#define _POSIX_C_SOURCE 200809L
/* C half of run-core's endless enemy buff test: replays the cases written by enemybuffs.producer.test.ts,
 * wave by wave, and compares the buffs held after each wave and the RNG state at the end. */
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
    FILE *f = fopen(argc > 1 ? argv[1] : "enemybuffs-cases.txt", "r");
    if (!f) { perror("cases"); return 2; }
    static char line[8192];
    int line_no = 0, cases = 0, pass = 0, fail = 0, uncovered = 0, threw = 0, waves = 0;
    pr_run run;
    pr_enemy_buffs buffs;
    bool ok = true;
    while (fgets(line, sizeof line, f)) {
        line_no++;
        char *save;
        char *t = strtok_r(line, " \n", &save);
        if (!t || t[0] == '#') continue;
        if (!strcmp(t, "B")) {
            cases++;
            memset(&run, 0, sizeof run);
            memset(&buffs, 0, sizeof buffs);
            run.mode = GAME_MODES_ENDLESS;
            num(&save);
            read_state(&save, &run.rng);
            ok = true;
        } else if (!strcmp(t, "X")) {
            threw++;
        } else if (!strcmp(t, "W")) {
            const int wave = (int)num(&save), n = (int)num(&save);
            waves++;
            if (pr_run_add_enemy_buffs(&run, wave, &buffs)) ok = false;
            if (n != buffs.n) ok = false;
            if (getenv("PRDBG") && ok) { printf("wave %d mine:", wave); for (int i = 0; i < buffs.n; i++) printf(" %s:%d", pr_item_names[buffs.buff[i].item], buffs.buff[i].stack); printf("\n"); }
            for (int i = 0; i < n; i++) {
                const int item = item_index(tok(&save)), stack = (int)num(&save);
                if (i < buffs.n && (buffs.buff[i].item != item || buffs.buff[i].stack != stack)) ok = false;
            }
        } else if (!strcmp(t, "E")) {
            pr_rng after;
            read_state(&save, &after);
            ok = ok && run.rng.c == after.c && run.rng.s0 == after.s0 && run.rng.s1 == after.s1 && run.rng.s2 == after.s2;
            if (run.unported) uncovered++;
            else if (ok) pass++;
            else if (++fail <= 10) printf("MISMATCH line %d\n", line_no);
        }
    }
    printf("cases %d (upstream threw: %d); %d buff waves\n", cases, threw, waves);
    printf("  %-36s pass %6d  fail %d  not covered %d\n", "endless enemy buffs (+ RNG)", pass, fail, uncovered);
    return fail ? 1 : 0;
}
