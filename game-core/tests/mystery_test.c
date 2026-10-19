#include "../src/internal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void add_party(gc_run *run, int n)
{
    run->n_party = n;
    for (int i = 0; i < n; i++) {
        memset(&run->party[i], 0, sizeof run->party[i]);
        run->party[i].b.hp = 10;
        run->party[i].b.stats[0] = 10;
        run->party[i].b.species = SPECIES_ID_BULBASAUR;
        run->party[i].b.is_player = true;
    }
}

static void test_initial_state(void)
{
    puts("test_mystery_initial_state...");
    gc_run run;
    gc_run_new(&run, GAME_MODES_CLASSIC, "mystery-init", 4001);
    assert(run.me.spawn_chance == 3);
    assert(run.n_mystery_history == 0);
    assert(run.mystery_type == -1 && run.mystery_tier == -1);
    puts("ok");
}

static void test_selector_and_history(void)
{
    puts("test_mystery_selector_and_history...");
    gc_run run;
    gc_run_new(&run, GAME_MODES_CLASSIC, "mystery-select", 4002);
    add_party(&run, 6);
    run.wave = 100;
    run.biome = BIOME_ID_PLAINS;
    run.money = 10000000;
    pr_run_reset_seed(&run.run, run.wave);

    int type, tier;
    assert(gc_mystery_select(&run, &type, &tier));
    assert(type >= 0 && type < GC_ME_COUNT);
    assert(tier >= GC_ME_COMMON && tier <= GC_ME_ROGUE);

    gc_mystery_record(&run, type, tier);
    assert(run.n_mystery_history == 1);
    assert(run.me.n_encountered == 1);
    assert(run.me.last_encounter_wave == 100);
    assert(run.me.spawn_chance == 3);

    /* The immediately previous encounter may not repeat. */
    run.wave = 110;
    pr_run_reset_seed(&run.run, run.wave);
    int type2, tier2;
    assert(gc_mystery_select(&run, &type2, &tier2));
    assert(type2 != type);
    (void)tier2;
    puts("ok");
}

static void test_spawn_weight_miss(void)
{
    puts("test_mystery_spawn_weight_miss...");
    gc_run run;
    gc_run_new(&run, GAME_MODES_CLASSIC, "mystery-miss", 4003);
    for (int i = 0; i < 100; i++) gc_mystery_increment_miss(&run);
    assert(run.me.spawn_chance == 256);
    gc_mystery_increment_miss(&run);
    assert(run.me.spawn_chance == 256);
    puts("ok");
}

static void test_mystery_actions_and_options(void)
{
    puts("test_mystery_actions_and_options...");
    gc_run run;
    gc_run_new(&run, GAME_MODES_CLASSIC, "mystery-actions", 4004);
    add_party(&run, 3);
    run.wave = 50;
    run.money = 100000;

    /* Test 1: Mysterious Challengers screen setup */
    run.screen.kind = GC_SCREEN_MYSTERY;
    run.screen.mystery_type = GC_ME_MYSTERIOUS_CHALLENGERS;
    gc_mystery_setup_screen(&run);
    assert(run.screen.n_options == 3);
    assert(run.screen.option_ok[0] && run.screen.option_ok[1] && run.screen.option_ok[2]);

    /* Test 2: Delibirdy option availability based on items */
    run.screen.mystery_type = GC_ME_DELIBIRDY;
    run.n_items = 0;
    gc_mystery_setup_screen(&run);
    assert(!run.screen.option_ok[0]);

    gc_item_add(&run, "POTION", -1, 0, 1);
    gc_mystery_setup_screen(&run);
    assert(run.screen.option_ok[0]);

    /* Test 3: Dark Deal action execution */
    const char *why;
    bool ok;
    run.screen.mystery_type = GC_ME_DARK_DEAL;
    gc_mystery_setup_screen(&run);
    double initial_money = run.money;
    assert(gc_mystery_execute(&run, 0, 0, 0, &why, &ok));
    assert(ok);
    assert(run.party[0].b.hp == 0);
    assert(run.money > initial_money);

    /* Test 4: Pokemon Salesman action execution */
    run.screen.mystery_type = GC_ME_THE_POKEMON_SALESMAN;
    run.money = 1000000;
    gc_mystery_setup_screen(&run);
    assert(run.screen.option_ok[0]);
    const int n_party_before = run.n_party;
    gc_mon on_sale;
    double price = 0;
    assert(gc_salesman_offer(&run, &on_sale, &price));
    const double money_before = run.money;
    gc_rq_reset(&gc_int(&run)->rq);
    assert(gc_mystery_execute(&run, 0, 0, 0, &why, &ok));
    assert(ok);
    assert(run.money == money_before - price);
    /* the Pokemon arrives through the catch phase, ahead of the heal-only shop */
    gc_run_until_screen(&run);
    assert(run.screen.kind == GC_SCREEN_REWARD);
    assert(run.n_party == n_party_before + 1);
    assert(run.party[run.party_order[n_party_before]].b.species == on_sale.b.species);
    assert(run.party[run.party_order[n_party_before]].b.id == on_sale.b.id);
    puts("ok");
}

int main(void)
{
    test_initial_state();
    test_selector_and_history();
    test_spawn_weight_miss();
    test_mystery_actions_and_options();
    puts("All mystery dispatcher tests passed successfully!");
    return 0;
}
