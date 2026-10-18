/* Real command/save continuation; this is a singles host consistency fixture. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run run, loaded;
static unsigned char saved[GC_SAVE_MAX];
static int checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"Acupressure host line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
int main(void)
{
    const char *why = NULL;
    const int abilities[] = {ABILITY_ID_NONE, ABILITY_ID_SIMPLE, ABILITY_ID_CONTRARY};
    for (int mode = 0; mode < 9; mode++) {
        gc_run_new(&run, GAME_MODES_CLASSIC, "acupressure-command-save", 777);
        gc_starter starter = {.species=SPECIES_ID_BULBASAUR, .tera_type=-1, .moves={MOVE_ID_ACUPRESSURE}};
        memset(starter.ivs, 15, sizeof starter.ivs);
        CHECK(gc_run_set_starters(&run, &starter, 1, &why));
        gc_event ev; while (gc_run_next_event(&run, &ev)) {}
        CHECK(gc_run_can_save(&run));
        pr_pokemon *p = &run.party[0].b, *enemy = &run.enemy[0].b;
        p->custom_ability = abilities[mode/3]; pr_refresh_ability(p);
        for (int j = 0; j < 7; j++) p->stat_stages[j] = mode%3 == 0 ? 0 : mode%3 == 1 ? 5 : 6;
        enemy->n_moves = 1; pr_set_original_move(enemy, 0, MOVE_ID_SPLASH);
        enemy->custom_ability = ABILITY_ID_NONE; pr_refresh_ability(enemy);
        size_t n = gc_run_save(&run, saved, sizeof saved);
        CHECK(n > 0); CHECK(gc_run_load(&loaded, saved, n, &why)); CHECK(!loaded.load_dropped);
        CHECK(!memcmp(p->stat_stages, loaded.party[0].b.stat_stages, sizeof p->stat_stages));
        gc_choice fight = {GC_CH_FIGHT, 0, -1, 0};
        CHECK(gc_run_choose(&run, &fight, &why)); CHECK(gc_run_choose(&loaded, &fight, &why));
        CHECK(!memcmp(p->stat_stages, loaded.party[0].b.stat_stages, sizeof p->stat_stages));
        CHECK(!memcmp(&run.bt.battle_rng, &loaded.bt.battle_rng, sizeof run.bt.battle_rng));
        CHECK(!memcmp(&run.bt.rng, &loaded.bt.rng, sizeof run.bt.rng));
        CHECK(!memcmp(&run.run.rng, &loaded.run.rng, sizeof run.run.rng));
        int changed = 0, before = mode%3 == 0 ? 0 : mode%3 == 1 ? 5 : 6;
        for (int j = 0; j < 7; j++) if (p->stat_stages[j] != before) {
            changed++;
            int expected = before + (mode/3 == 2 ? -2 : mode/3 == 1 ? 4 : 2);
            CHECK(p->stat_stages[j] == (expected > 6 ? 6 : expected));
        }
        CHECK(changed == (mode%3 == 2 ? 0 : 1));
    }
    printf("Acupressure command/save: %d checks passed\n", checks);
    return 0;
}
