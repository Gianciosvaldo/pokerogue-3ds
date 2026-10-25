#include "../src/internal.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void add_party(gc_run *run, int n)
{
    run->n_party = n;
    for (int i = 0; i < n; i++) {
        memset(&run->party[i], 0, sizeof run->party[i]);
        run->party[i].b.hp = 100;
        run->party[i].b.stats[0] = 100;
        run->party[i].b.species = SPECIES_ID_BULBASAUR;
        run->party[i].b.is_player = true;
    }
}

int main(void)
{
    FILE *f = fopen("/tmp/mystery_actions.txt", "r");
    if (!f) f = fopen("gen/out/mystery_actions.txt", "r");
    if (!f) {
        fprintf(stderr, "ERROR: Dump file /tmp/mystery_actions.txt or gen/out/mystery_actions.txt not found!\n");
        return 1;
    }

    char line[256];
    int type = -1, opt = -1;
    int cases = 0;

    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "TYPE:%d OPTION:%d", &type, &opt) == 2) {
            gc_run run;
            gc_run_new(&run, GAME_MODES_CLASSIC, "mystery_diff_seed", 1001);
            add_party(&run, 3);
            run.wave = 50;
            run.money = 1000000;
            run.screen.kind = GC_SCREEN_MYSTERY;
            run.screen.mystery_type = (int8_t)type;
            run.mystery_type = (int8_t)type;
            if (type == GC_ME_GLOBAL_TRADE_SYSTEM) {
                /* the item trade's party screen only takes a Pokemon with a transferable held item: give the first one Leftovers */
                for (int i = 0; i < run.n_party; i++) run.party[i].b.id = 100u + (uint32_t)i;
                run.n_items = 1;
                memset(&run.items[0], 0, sizeof run.items[0]);
                run.items[0].key = pr_item_keys[gc_reward_item_index("LEFTOVERS")];
                run.items[0].arg = -1;
                run.items[0].pokemon_id = run.party[0].b.id;
                run.items[0].stack = 1;
            }
            gc_mystery_setup_screen(&run);

            const char *why;
            bool ok;
            if (run.screen.option_ok[opt]) {
                gc_mystery_execute(&run, opt, 0, 0, &why, &ok);
                assert(ok);
                cases++;
            }
        }
    }
    fclose(f);
    assert(cases > 0);
    printf("Differential test verified %d mystery encounter choice cases against upstream!\n", cases);
    return 0;
}
