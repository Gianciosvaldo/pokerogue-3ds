/* Which mystery encounters a run can actually meet, and that a battle one starts with a real enemy. gc_mystery_select once kept every encounter whose last
 * wave is 180 out of the pool (the rule's waves were int8_t: 180 became -76), so a run met only Department Store Sale or the Mysterious Challengers
 * fallback; and the encounter battles that roll their enemy with gc_gen_wild (Uncommon Breed, Safari Zone, the Winstrate Challenge...) began with an
 * enemy with no stats, which stopped the run on a command screen nothing could answer. */
#include "../src/internal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static gc_run run;
static gc_system sys;

static void start(const char *seed, int wave)
{
    memset(&run, 0, sizeof run);
    gc_run_new(&run, GAME_MODES_CLASSIC, seed, 7);
    gc_system_init_defaults(&sys);
    run.sys = &sys;
    run.wave = wave;
    run.biome = BIOME_ID_PLAINS;
    run.n_party = 3;
    for (int i = 0; i < 3; i++) {
        gc_starter s;
        memset(&s, 0, sizeof s);
        s.species = (uint16_t)(i == 0 ? SPECIES_ID_PIKACHU : i == 1 ? SPECIES_ID_LAPRAS : SPECIES_ID_GENGAR);
        s.tera_type = -1;
        for (int k = 0; k < 6; k++) s.ivs[k] = 15;
        s.moves[0] = MOVE_ID_TACKLE;
        gc_mon_new_player(&run, &run.party[i], &s, 40);
        run.party_order[i] = (int8_t)i;
    }
    run.money = 100000;
    run.battle_type = BATTLE_TYPE_WILD;
    gc_items_to_battle(&run);
    gc_rq_reset(&gc_int(&run)->rq);
}

static void test_selection_reaches_the_pool(void)
{
    int count[GC_ME_COUNT];
    memset(count, 0, sizeof count);
    for (int i = 0; i < 4000; i++) {
        char seed[16];
        snprintf(seed, sizeof seed, "reach-%d", i);
        start(seed, 20 + i % 150);
        run.biome = (i % 3) ? BIOME_ID_PLAINS : (i % 2 ? BIOME_ID_TOWN : BIOME_ID_FOREST);
        pr_run_reset_seed(&run.run, run.wave);
        int type, tier;
        assert(gc_mystery_select(&run, &type, &tier));
        count[type]++;
    }
    int kinds = 0, big = 0;
    for (int t = 0; t < GC_ME_COUNT; t++) {
        kinds += count[t] > 0;
        big += count[t] > 0 && t != GC_ME_DEPARTMENT_STORE_SALE && t != GC_ME_MYSTERIOUS_CHALLENGERS;
    }
    assert(kinds >= 12);
    assert(big >= 10);
    /* every encounter of the rules with waves up to 180 can be drawn at a wave past 127 */
    int late = 0;
    for (int i = 0; i < 2000; i++) {
        char seed[16];
        snprintf(seed, sizeof seed, "late-%d", i);
        start(seed, 130 + i % 50);
        pr_run_reset_seed(&run.run, run.wave);
        int type, tier;
        assert(gc_mystery_select(&run, &type, &tier));
        late += type != GC_ME_MYSTERIOUS_CHALLENGERS && type != GC_ME_DEPARTMENT_STORE_SALE;
    }
    assert(late > 200);
    printf("ok selection (%d encounter types drawn, %d beyond the two it was stuck on; %d late draws)\n", kinds, big, late);
}

static void test_battle_encounter_has_an_enemy(void)
{
    /* Uncommon Breed's first option and the Safari Zone's second: a wild enemy of the biome (gc_gen_wild), boss or not */
    const int cases[2][2] = {{GC_ME_UNCOMMON_BREED, 0}, {GC_ME_SAFARI_ZONE, 1}};
    for (int c = 0; c < 2; c++)
        for (int i = 0; i < 60; i++) {
            char seed[16];
            snprintf(seed, sizeof seed, "enemy-%d-%d", c, i);
            start(seed, 36 + i);
            run.mystery_type = cases[c][0];
            run.screen.mystery_type = cases[c][0];
            gc_rq_unshift(&run, gc_rp(RP_MYSTERY, cases[c][0], GC_ME_COMMON, 0));
            gc_run_until_screen(&run);
            const char *why = "";
            bool ok = false;
            assert(gc_mystery_execute(&run, cases[c][1], 0, 0, &why, &ok) && ok);
            assert(run.n_enemy == 1 && run.me_battle);
            assert(run.enemy[0].b.stats[0] > 0 && run.enemy[0].b.hp == run.enemy[0].b.stats[0]);
            assert(run.bt.field[2] == &run.enemy[0].b && run.enemy[0].b.on_field);
        }
    puts("ok battle encounters start with a real enemy");
}

int main(void)
{
    test_selection_reaches_the_pool();
    test_battle_encounter_has_an_enemy();
    puts("mystery_reach_test: all passed");
    return 0;
}
