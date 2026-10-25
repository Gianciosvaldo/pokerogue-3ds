/* The Global Trade System on the host: what the upstream differential (mystery_gts_diff_test) cannot compare because a trace has
 * nothing to compare it with: the screen's shape, the party screens' filters, that a trade leaves the run in a playable state (party size,
 * the received Pokemon last, the traded one gone with its items, the heal-only shop queued), and that the item trade only works on a
 * transferable held item. The numbers themselves are checked against upstream by tools/gtsdiff.sh. */
#include "../src/internal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static gc_run run;
static gc_system sys;

static void start(const char *seed, int n)
{
    static const int species[4] = {SPECIES_ID_PIKACHU, SPECIES_ID_LAPRAS, SPECIES_ID_GENGAR, SPECIES_ID_MEWTWO};
    memset(&run, 0, sizeof run);
    gc_run_new(&run, GAME_MODES_CLASSIC, seed, 7);
    gc_system_init_defaults(&sys);
    run.sys = &sys;
    run.wave = 45;
    run.biome = BIOME_ID_PLAINS;
    run.n_items = 0;
    run.n_party = n;
    for (int i = 0; i < n; i++) {
        gc_starter s;
        memset(&s, 0, sizeof s);
        s.species = (uint16_t)species[i % 4];
        s.tera_type = -1;
        for (int k = 0; k < 6; k++) s.ivs[k] = 15;
        s.moves[0] = MOVE_ID_TACKLE;
        gc_mon_new_player(&run, &run.party[i], &s, 20 + i);
        run.party[i].pause_evolutions = true;
        run.party_order[i] = (int8_t)i;
    }
    run.money = 1000;
    run.battle_type = BATTLE_TYPE_WILD;
    gc_items_to_battle(&run);
    gc_rq_reset(&gc_int(&run)->rq);
    run.mystery_type = GC_ME_GLOBAL_TRADE_SYSTEM;
    gc_rq_unshift(&run, gc_rp(RP_MYSTERY, GC_ME_GLOBAL_TRADE_SYSTEM, GC_ME_COMMON, 0));
    gc_run_until_screen(&run);
    assert(run.screen.kind == GC_SCREEN_MYSTERY);
}

static void give(const char *key, int arg, int holder)
{
    const int idx = gc_reward_item_index(key);
    assert(idx >= 0);
    gc_item *it = &run.items[run.n_items++];
    memset(it, 0, sizeof *it);
    it->key = pr_item_keys[idx];
    it->arg = (int16_t)arg;
    it->pokemon_id = holder < 0 ? 0 : run.party[holder].b.id;
    it->stack = 1;
    gc_items_to_battle(&run);
}

static void test_screen_and_filters(void)
{
    start("gts-screen", 3);
    assert(run.screen.n_options == 4);
    assert(run.screen.option_ok[0] && run.screen.option_ok[1] && run.screen.option_ok[2] && run.screen.option_ok[3]);
    for (int i = 0; i < 3; i++) {
        assert(gc_gts_pick_ok(&run, 0, i) && gc_gts_pick_ok(&run, 1, i));
        assert(!gc_gts_pick_ok(&run, 2, i));   /* no held items yet */
    }
    give("LEFTOVERS", -1, 1);
    give("BASE_STAT_BOOSTER", 0, 2);   /* vitamins cannot be traded */
    assert(!gc_gts_pick_ok(&run, 2, 0) && gc_gts_pick_ok(&run, 2, 1) && !gc_gts_pick_ok(&run, 2, 2));
    /* a lone Pokemon can only take the item trade or leave: GTS has no party-size requirement, its trades need two allowed in battle */
    start("gts-one", 1);
    assert(!run.screen.option_ok[0] && !run.screen.option_ok[1] && run.screen.option_ok[2] && run.screen.option_ok[3]);
    puts("ok screen and filters");
}

static void test_trade_options_stable(void)
{
    start("gts-options", 4);
    gc_mon a[3], b[3];
    assert(gc_gts_trade_options(&run, 0, a) && gc_gts_trade_options(&run, 0, b));
    assert(!memcmp(a, b, sizeof a));   /* a pure function of the state: asking twice gives the same Pokemon */
    for (int j = 0; j < 3; j++) assert(a[j].b.species != run.party[0].b.species && a[j].b.level == run.party[0].b.level);
    gc_mon m3[3];
    assert(gc_gts_trade_options(&run, 3, m3));   /* Mewtwo: a legendary is offered its generation's three level 5 commons */
    for (int j = 0; j < 3; j++) assert(m3[j].b.level == 5);
    puts("ok trade options");
}

static void test_check_trade(void)
{
    start("gts-trade", 3);
    give("LEFTOVERS", -1, 0);
    give("SOUL_DEW", -1, 0);
    const uint32_t gone = run.party[0].b.id;
    gc_mon offers[3];
    assert(gc_gts_trade_options(&run, 0, offers));
    const char *why = "";
    bool ok = false;
    assert(gc_mystery_execute(&run, 0, 0, 1, &why, &ok) && ok);
    assert(run.n_party == 3);
    const gc_mon *got = &run.party[run.party_order[2]];
    assert(got->b.species == offers[1].b.species && got->b.id == offers[1].b.id);
    for (int i = 0; i < run.n_party; i++) assert(run.party[run.party_order[i]].b.id != gone);
    int carried = 0;
    for (int i = 0; i < run.n_items; i++) {
        assert(run.items[i].pokemon_id != gone);
        carried += run.items[i].pokemon_id == got->b.id;
    }
    assert(carried == 2);   /* both held items travel */
    gc_rqueue copy = gc_int(&run)->rq;
    gc_rphase p;
    bool shop = false;
    while (gc_rq_next(&copy, &p)) shop |= p.type == RP_SELECT_MODIFIER && p.a == 1;
    assert(shop);   /* the heal-only shop */
    puts("ok check trade offers");
}

static void test_wonder_and_item_trade(void)
{
    start("gts-wonder", 2);
    const char *why = "";
    bool ok = false;
    assert(gc_mystery_execute(&run, 1, 1, 0, &why, &ok) && ok);
    assert(run.n_party == 2);   /* one out, one in */

    start("gts-item", 3);
    give("LEFTOVERS", -1, 2);
    ok = false;
    assert(!gc_mystery_execute(&run, 2, 1, 0, &why, &ok) || !ok);   /* Pokemon 1 holds nothing */
    ok = false;
    assert(gc_mystery_execute(&run, 2, 2, 0, &why, &ok) && ok);
    assert(run.n_items == 0);   /* the Leftovers went */
    assert(gc_int(&run)->reward.me_custom.n_options == 1);
    gc_rqueue copy = gc_int(&run)->rq;
    gc_rphase p;
    bool shop = false;
    while (gc_rq_next(&copy, &p)) shop |= p.type == RP_SELECT_MODIFIER && p.a == 2;
    assert(shop);   /* the plain shop, not the heal-only one */

    start("gts-leave", 3);
    ok = false;
    assert(gc_mystery_execute(&run, 3, 0, 0, &why, &ok) && ok);
    assert(run.n_party == 3);
    puts("ok wonder trade, item trade and leave");
}

int main(void)
{
    test_screen_and_filters();
    test_trade_options_stable();
    test_check_trade();
    test_wonder_and_item_trade();
    puts("mystery_gts_test: all passed");
    return 0;
}
