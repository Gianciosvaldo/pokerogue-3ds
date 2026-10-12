/* Lapsing items through the run (BattleEndPhase): a Dire Hit taken from the reward screen is a canonical timed
 * record, X items and Lures taken after it still lapse, a Lure taken with a vitamin held keeps its timer, and Pay Day's
 * scattered money is picked up. The offers are what reward-core really makes (a Dire Hit has arg -1). */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_run run;
static gc_system sys;
static int checks;

#define CHECK(x) do { \
    if (!(x)) { fprintf(stderr, "lapse_test FAIL line %d: %s\n", __LINE__, #x); exit(1); } \
    checks++; \
} while (0)

static void new_run(const char *seed)
{
    memset(&run, 0, sizeof run);
    gc_system_init_defaults(&sys);
    gc_run_new(&run, GAME_MODES_CLASSIC, seed, 42);
    gc_starter team[2];
    memset(team, 0, sizeof team);
    const int species[2] = {SPECIES_ID_BULBASAUR, SPECIES_ID_CHARMANDER};
    for (int i = 0; i < 2; i++) {
        team[i].species = (uint16_t)species[i];
        team[i].tera_type = -1;
        for (int k = 0; k < 6; k++) team[i].ivs[k] = 15;
    }
    const char *why;
    CHECK(gc_run_set_starters(&run, team, 2, &why));
    run.sys = &sys;
    gc_event e;
    while (gc_run_next_event(&run, &e)) {}
}

static bool take(const char *key, int arg)
{
    gc_offer o = {.key = key, .arg = (int16_t)arg, .tier = 0, .target = GC_TARGET_NONE};
    const char *why = NULL;
    return gc_port_apply_reward(&run, &o, -1, -1, &why);
}

static const gc_item *find(const char *key, int arg)
{
    for (int i = 0; i < run.n_items; i++)
        if (!strcmp(run.items[i].key, key) && run.items[i].arg == arg) return &run.items[i];
    return NULL;
}

static void test_dire_hit_is_timed(void)
{
    new_run("lapse-dire");
    CHECK(take("DIRE_HIT", -1));
    const gc_item *d = find("DIRE_HIT", 0);
    CHECK(d && d->battle_count == 5 && d->stack == 1);
    CHECK(!find("DIRE_HIT", -1));
    /* a later X Attack and a Lure are still canonical */
    CHECK(take("TEMP_STAT_STAGE_BOOSTER", STAT_ATK));
    CHECK(find("TEMP_STAT_STAGE_BOOSTER", STAT_ATK)->battle_count == 5);
    CHECK(take("LURE", -1));
    CHECK(find("LURE", 0) && find("LURE", 0)->battle_count == 10);
    int expired;
    for (int b = 0; b < 4; b++) CHECK(gc_temp_lapse(&run, &expired) && expired == 0 && gc_lure_lapse(&run, &expired) && expired == 0);
    CHECK(find("DIRE_HIT", 0)->battle_count == 1 && find("LURE", 0)->battle_count == 6);
    CHECK(gc_temp_lapse(&run, &expired) && expired == 2);
    CHECK(!find("DIRE_HIT", 0) && !find("TEMP_STAT_STAGE_BOOSTER", STAT_ATK));
    CHECK(find("LURE", 0)->battle_count == 6);
    for (int b = 0; b < 4; b++) CHECK(gc_lure_lapse(&run, &expired) && expired == 0);
    CHECK(gc_lure_lapse(&run, &expired) && expired == 0 && find("LURE", 0)->battle_count == 1);
    CHECK(gc_lure_lapse(&run, &expired) && expired == 1 && !find("LURE", 0));
}

static void test_lure_with_vitamin(void)
{
    new_run("lapse-vitamin");
    gc_item_add(&run, "BASE_STAT_BOOSTER", STAT_ATK, 0, 1);
    gc_item_add(&run, "SOUL_DEW", -1, run.party[0].b.id, 1);
    CHECK(take("LURE", -1));
    const gc_item *l = find("LURE", 0);
    CHECK(l && l->battle_count == 10);
    int expired;
    CHECK(gc_lure_lapse(&run, &expired) && expired == 0 && find("LURE", 0)->battle_count == 9);
    CHECK(find("BASE_STAT_BOOSTER", STAT_ATK) && find("SOUL_DEW", -1));
}

static void test_pay_day_money(void)
{
    new_run("lapse-payday");
    run.money = 100;
    run.bt.money_scattered = 60;
    gc_pick_up_scattered_money(&run);
    CHECK(run.money == 160 && run.bt.money_scattered == 0);
    /* two Amulet Coins as separate records: floor(60 * 0.2 * 1) = 12, then floor(72 * 0.2 * 2) = 28 */
    gc_item_add(&run, "AMULET_COIN", -1, 0, 1);
    run.items[run.n_items - 1].stack = 1;
    gc_item *second = &run.items[run.n_items++];
    memset(second, 0, sizeof *second);
    second->key = "AMULET_COIN"; second->arg = 0; second->stack = 2;
    run.money = 0;
    run.bt.money_scattered = 60;
    gc_pick_up_scattered_money(&run);
    CHECK(run.money == 100);
    /* Happy Hour doubles it */
    pr_arena_add_tag(&run.bt, &run.eng, ARENA_TAG_TYPE_HAPPY_HOUR, 5, 0, -1, ARENA_TAG_SIDE_BOTH, true);
    run.n_items = 0;
    run.money = 0;
    run.bt.money_scattered = 60;
    gc_pick_up_scattered_money(&run);
    CHECK(run.money == 120);
}

int main(void)
{
    test_dire_hit_is_timed();
    test_lure_with_vitamin();
    test_pay_day_money();
    printf("lapse_test: %d checks, 0 failures\n", checks);
    return 0;
}
