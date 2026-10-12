/* Replays the upstream Pay Day pickup trace through gc_pick_up_scattered_money: the money after the pickup, and that
 * the scattered money is cleared. Usage: payday_diff_test <trace>; exit 0 only with 0 mismatches. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_run run;
static gc_system sys;

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: payday_diff_test <trace>\n"); return 2; }
    FILE *fp = fopen(argv[1], "r");
    if (!fp) { perror(argv[1]); return 2; }
    memset(&run, 0, sizeof run);
    gc_system_init_defaults(&sys);
    gc_run_new(&run, GAME_MODES_CLASSIC, "payday-diff", 42);
    gc_starter team[1];
    memset(team, 0, sizeof team);
    team[0].species = SPECIES_ID_BULBASAUR;
    team[0].tera_type = -1;
    for (int k = 0; k < 6; k++) team[0].ivs[k] = 15;
    const char *why;
    if (!gc_run_set_starters(&run, team, 1, &why)) { fprintf(stderr, "starters: %s\n", why); return 2; }
    run.sys = &sys;
    gc_event e;
    while (gc_run_next_event(&run, &e)) {}
    char line[256];
    int cases = 0, mismatches = 0, total = -1, version = 0, hh_cases = 0, coin_cases = 0;
    while (fgets(line, sizeof line, fp)) {
        long i, scattered, stack; int hh; double money, want;
        if (sscanf(line, "V %d %d", &version, &total) == 2) continue;
        if (line[0] == 'Z') { if (atoi(line + 2) != cases) { fprintf(stderr, "case count %d != %s", cases, line + 2); return 2; } continue; }
        if (sscanf(line, "C %ld %ld %lf %ld %d %lf", &i, &scattered, &money, &stack, &hh, &want) != 6) { fprintf(stderr, "bad line: %s", line); return 2; }
        run.n_items = 0;
        memset(run.items, 0, sizeof run.items);
        if (stack) { gc_item_add(&run, "AMULET_COIN", -1, 0, 1); run.items[run.n_items - 1].stack = (int)stack; }
        while (pr_arena_has_tag(&run.bt, ARENA_TAG_TYPE_HAPPY_HOUR, ARENA_TAG_SIDE_BOTH))
            pr_arena_remove_tag_on_side(&run.bt, &run.eng, ARENA_TAG_TYPE_HAPPY_HOUR, ARENA_TAG_SIDE_BOTH, true);
        if (hh) pr_arena_add_tag(&run.bt, &run.eng, ARENA_TAG_TYPE_HAPPY_HOUR, 5, 0, -1, ARENA_TAG_SIDE_BOTH, true);
        run.money = money;
        run.bt.money_scattered = (int)scattered;
        gc_pick_up_scattered_money(&run);
        if (run.money != want || run.bt.money_scattered != 0) {
            if (mismatches < 10) fprintf(stderr, "case %ld: scattered %ld money %.0f coin %ld happy %d: got %.0f want %.0f\n", i, scattered, money, stack, hh, run.money, want);
            mismatches++;
        }
        cases++; hh_cases += hh; coin_cases += stack > 0;
    }
    printf("Pay Day pickup: %d cases (happy hour %d, amulet coin %d), TOTAL mismatches %d\n", cases, hh_cases, coin_cases, mismatches);
    return mismatches ? 1 : 0;
}
