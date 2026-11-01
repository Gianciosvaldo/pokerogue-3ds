/* Delibird-y on the host: what the upstream differential (mystery_delibirdy_diff_test) cannot show because it stops at the option's own code:
 * through the real run, the money really leaves, the gift really arrives (the ModifierRewardPhase runs ahead of the heal shop and says what
 * the player got), the held item really goes, a full Shell Bell stack stops adding, and the encounter is only offered to a party that holds
 * something to give. */
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
    run.money = 1000000;
    run.n_party = 2;
    for (int i = 0; i < 2; i++) {
        gc_starter s;
        memset(&s, 0, sizeof s);
        s.species = i ? SPECIES_ID_LAPRAS : SPECIES_ID_PIKACHU;
        s.tera_type = -1;
        for (int k = 0; k < 6; k++) s.ivs[k] = 15;
        s.moves[0] = MOVE_ID_TACKLE;
        gc_mon_new_player(&run, &run.party[i], &s, 30);
        run.party[i].pause_evolutions = true;
        run.party_order[i] = (int8_t)i;
    }
    run.n_items = 0;
    memset(run.items, 0, sizeof run.items);
    run.battle_type = BATTLE_TYPE_WILD;
    gc_rq_reset(&gc_int(&run)->rq);
    run.mystery_type = GC_ME_DELIBIRDY;
}

static void open(void)
{
    gc_items_to_battle(&run);
    gc_rq_unshift(&run, gc_rp(RP_MYSTERY, GC_ME_DELIBIRDY, GC_ME_GREAT, 0));
    gc_run_until_screen(&run);
    assert(run.screen.kind == GC_SCREEN_MYSTERY);
}

static int held(const char *key, int16_t arg, int mon)
{
    for (int i = 0; i < run.n_items; i++)
        if (!strcmp(run.items[i].key, key) && (arg < 0 || run.items[i].arg == arg) && (mon < 0 || run.items[i].pokemon_id == run.party[mon].b.id)) return run.items[i].stack;
    return 0;
}

/* the texts the run says after the choice, "key" or "key:ITEM" */
static char said[8][64];
static int n_said;
static void listen(void)
{
    gc_event e;
    n_said = 0;
    while (gc_run_next_event(&run, &e)) {
        if (e.kind != GC_EV_MESSAGE || !e.msg.key || n_said >= 8) continue;
        const char *k = strrchr(e.msg.key, ':');
        snprintf(said[n_said], sizeof said[0], "%s", k ? k + 1 : e.msg.key);
        for (int a = 0; a < e.msg.n_args; a++)
            if (e.msg.args[a].name && !strcmp(e.msg.args[a].name, "modifierName") && e.msg.args[a].s) {
                const char *p = strstr(e.msg.args[a].s, "ModifierType.");
                if (p) snprintf(said[n_said] + strlen(said[n_said]), sizeof said[0] - strlen(said[n_said]), ":%s", p + 13);
            }
        n_said++;
    }
}
static bool said_has(const char *text)
{
    for (int i = 0; i < n_said; i++) if (strstr(said[i], text)) return true;
    return false;
}

static void choose(int option, int member, int item)
{
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, option, member, item};
    assert(gc_run_choose(&run, &ch, &why));
}

static void test_screen(void)
{
    puts("test_delibirdy_screen...");
    start("deli-screen", 40);
    gc_item_add(&run, "BERRY", BERRY_TYPE_SITRUS, run.party[1].b.id, 2);
    gc_item_add(&run, "LEFTOVERS", -1, run.party[0].b.id, 1);
    gc_item_add(&run, "VITAMIN_NOT_THERE", -1, 0, 0);
    run.n_items--;
    open();
    /* three options, never a fourth; the first is the money, the other two pick a Pokemon and then one of its items */
    assert(run.screen.n_options == 3);
    assert(run.screen.option_ok[0] && run.screen.option_ok[1] && run.screen.option_ok[2]);
    assert(run.screen.option_pick[0] == GC_PICK_NONE && run.screen.option_pick[1] == GC_PICK_MEMBER_ITEM && run.screen.option_pick[2] == GC_PICK_MEMBER_ITEM);
    /* only Lapras holds food, only Pikachu holds an item of the other kind */
    assert(!gc_mystery_member_ok(&run, 1, 0) && gc_mystery_member_ok(&run, 1, 1));
    assert(gc_mystery_member_ok(&run, 2, 0) && !gc_mystery_member_ok(&run, 2, 1));
    int list[GC_MAX_ITEMS];
    assert(gc_mystery_held_items(&run, 1, 1, list, GC_MAX_ITEMS) == 1 && !strcmp(run.items[list[0]].key, "BERRY"));
    assert(gc_mystery_held_items(&run, 2, 0, list, GC_MAX_ITEMS) == 1 && !strcmp(run.items[list[0]].key, "LEFTOVERS"));
    assert(gc_mystery_held_items(&run, 1, 0, list, GC_MAX_ITEMS) == 0);
    /* short of the price: the money option is off */
    start("deli-poor", 40);
    gc_item_add(&run, "LEFTOVERS", -1, run.party[0].b.id, 1);
    run.money = pr_run_get_wave_money_amount(run.wave, 2.0) - 10;
    open();
    assert(!run.screen.option_ok[0] && !run.screen.option_ok[1] && run.screen.option_ok[2]);
    puts("ok");
}

static void test_give_money(void)
{
    puts("test_delibirdy_give_money...");
    start("deli-money", 40);
    gc_item_add(&run, "AMULET_COIN", -1, 0, 2);
    gc_item_add(&run, "LEFTOVERS", -1, run.party[0].b.id, 1);
    open();
    const double price = pr_run_get_wave_money_amount(40, 2.0), before = run.money;
    choose(0, 0, 0);
    /* the heal shop; the Amulet Coin arrived ahead of it, and the player was told */
    assert(run.screen.kind == GC_SCREEN_REWARD);
    assert(run.money == before - price);
    assert(held("AMULET_COIN", -1, -1) == 3);
    listen();
    assert(said_has("rewardGain:AMULET_COIN"));
    assert(held("SHELL_BELL", -1, -1) == 0);
    puts("ok");
}

static void test_amulet_coin_full(void)
{
    puts("test_delibirdy_amulet_coin_full...");
    start("deli-full", 40);
    gc_item_add(&run, "AMULET_COIN", -1, 0, 5);
    gc_item_add(&run, "LEFTOVERS", -1, run.party[0].b.id, 1);
    open();
    const double price = pr_run_get_wave_money_amount(40, 2.0), before = run.money;
    choose(0, 0, 0);
    /* at the maximum the lead gets a Shell Bell instead (the coin stays at 5), and the money is gone all the same */
    assert(run.screen.kind == GC_SCREEN_REWARD);
    assert(run.money == before - price);
    assert(held("AMULET_COIN", -1, -1) == 5);
    assert(held("SHELL_BELL", -1, 0) == 1);
    listen();
    assert(said_has("rewardGain:SHELL_BELL") && !said_has("AMULET_COIN"));
    /* a Shell Bell already at six stays at six, and the text is said all the same */
    start("deli-full-bell", 40);
    gc_item_add(&run, "AMULET_COIN", -1, 0, 5);
    gc_item_add(&run, "SHELL_BELL", -1, run.party[0].b.id, 6);
    open();
    choose(0, 0, 0);
    assert(held("SHELL_BELL", -1, 0) == 6);
    listen();
    assert(said_has("rewardGain:SHELL_BELL"));
    puts("ok");
}

static void test_give_food(void)
{
    puts("test_delibirdy_give_food...");
    start("deli-food", 40);
    gc_item_add(&run, "BERRY", BERRY_TYPE_SITRUS, run.party[1].b.id, 2);
    gc_item_add(&run, "REVIVER_SEED", -1, run.party[0].b.id, 1);
    gc_item_add(&run, "LEFTOVERS", -1, run.party[0].b.id, 1);
    open();
    const double before = run.money;
    /* Lapras gives one of its two Sitrus Berries: a Candy Jar, one berry left, the money untouched */
    choose(1, 1, 0);
    assert(run.screen.kind == GC_SCREEN_REWARD);
    assert(run.money == before);
    assert(held("BERRY", BERRY_TYPE_SITRUS, 1) == 1);
    assert(held("CANDY_JAR", -1, -1) == 1 && held("BERRY_POUCH", -1, -1) == 0);
    listen();
    assert(said_has("rewardGain:CANDY_JAR"));
    /* Pikachu's Reviver Seed (its only food; Leftovers are not listed) is a Berry Pouch, and the seed goes with its last stack */
    start("deli-seed", 40);
    gc_item_add(&run, "REVIVER_SEED", -1, run.party[0].b.id, 1);
    gc_item_add(&run, "LEFTOVERS", -1, run.party[0].b.id, 1);
    open();
    int list[GC_MAX_ITEMS];
    assert(gc_mystery_held_items(&run, 1, 0, list, GC_MAX_ITEMS) == 1);
    choose(1, 0, 0);
    assert(held("REVIVER_SEED", -1, 0) == 0 && held("LEFTOVERS", -1, 0) == 1);
    assert(held("BERRY_POUCH", -1, -1) == 1);
    /* a Berry Pouch at its maximum of 3: the lead gets a Shell Bell */
    start("deli-pouch-full", 40);
    gc_item_add(&run, "REVIVER_SEED", -1, run.party[1].b.id, 1);
    gc_item_add(&run, "BERRY_POUCH", -1, 0, 3);
    open();
    choose(1, 1, 0);
    assert(held("BERRY_POUCH", -1, -1) == 3 && held("SHELL_BELL", -1, 0) == 1);
    puts("ok");
}

static void test_give_item(void)
{
    puts("test_delibirdy_give_item...");
    start("deli-item", 40);
    gc_item_add(&run, "BERRY", BERRY_TYPE_LUM, run.party[0].b.id, 1);   /* food is not what this option takes */
    gc_item_add(&run, "LEFTOVERS", -1, run.party[1].b.id, 1);
    gc_item_add(&run, "WIDE_LENS", -1, run.party[1].b.id, 2);
    open();
    int list[GC_MAX_ITEMS];
    assert(gc_mystery_held_items(&run, 2, 0, list, GC_MAX_ITEMS) == 0);
    assert(gc_mystery_held_items(&run, 2, 1, list, GC_MAX_ITEMS) == 2);
    assert(!strcmp(run.items[list[0]].key, "LEFTOVERS") && !strcmp(run.items[list[1]].key, "WIDE_LENS"));
    /* the second of Lapras' items: a Healing Charm, one Wide Lens left */
    choose(2, 1, 1);
    assert(run.screen.kind == GC_SCREEN_REWARD);
    assert(held("WIDE_LENS", -1, 1) == 1 && held("LEFTOVERS", -1, 1) == 1 && held("HEALING_CHARM", -1, -1) == 1);
    listen();
    assert(said_has("rewardGain:HEALING_CHARM"));
    /* a Healing Charm at its maximum of 5: the party's first Pokemon, fainted or not, gets the Shell Bell */
    start("deli-charm-full", 40);
    gc_item_add(&run, "LEFTOVERS", -1, run.party[1].b.id, 1);
    gc_item_add(&run, "HEALING_CHARM", -1, 0, 5);
    run.party[0].b.hp = 0;
    open();
    choose(2, 1, 0);
    assert(held("HEALING_CHARM", -1, -1) == 5 && held("SHELL_BELL", -1, 0) == 1 && held("LEFTOVERS", -1, 1) == 0);
    puts("ok");
}

static void test_refused_picks(void)
{
    puts("test_delibirdy_refused_picks...");
    start("deli-refuse", 40);
    gc_item_add(&run, "BERRY", BERRY_TYPE_SITRUS, run.party[1].b.id, 1);
    open();
    const char *why;
    /* a Pokemon with nothing to give, an item past the list, and an option that is off leave the screen as it was */
    gc_choice ch = {GC_CH_MYSTERY, 1, 0, 0};
    assert(!gc_run_choose(&run, &ch, &why));
    ch = (gc_choice){GC_CH_MYSTERY, 1, 1, 1};
    assert(!gc_run_choose(&run, &ch, &why));
    ch = (gc_choice){GC_CH_MYSTERY, 2, 1, 0};
    assert(!gc_run_choose(&run, &ch, &why));
    assert(run.screen.kind == GC_SCREEN_MYSTERY && held("BERRY", BERRY_TYPE_SITRUS, 1) == 1 && run.money == 1000000);
    puts("ok");
}

/* gc_mystery_select: never offered to a party that holds nothing to give, offered to one that does */
static void test_spawn(void)
{
    puts("test_delibirdy_spawn...");
    int with_item = 0, without_item = 0, with_nothing_transferable = 0;
    for (int n = 0; n < 600; n++) {
        char seed[24];
        snprintf(seed, sizeof seed, "deli-spawn-%d", n);
        for (int variant = 0; variant < 3; variant++) {
            start(seed, 20 + (n % 150));
            run.biome = BIOME_ID_PLAINS;
            if (variant == 1) gc_item_add(&run, "BERRY", BERRY_TYPE_SITRUS, run.party[1].b.id, 1);
            if (variant == 2) gc_item_add(&run, "BASE_STAT_BOOSTER", 0, run.party[0].b.id, 1);   /* a vitamin cannot be given */
            run.party[0].b.hp = n % 3 ? run.party[0].b.stats[0] : 0;   /* a fainted holder still counts */
            int type, tier;
            if (!gc_mystery_select(&run, &type, &tier) || type != GC_ME_DELIBIRDY) continue;
            if (variant == 1) with_item++;
            else if (variant == 0) without_item++;
            else with_nothing_transferable++;
        }
    }
    assert(without_item == 0 && with_nothing_transferable == 0);
    assert(with_item > 0);
    puts("ok");
}

int main(void)
{
    test_screen();
    test_give_money();
    test_amulet_coin_full();
    test_give_food();
    test_give_item();
    test_refused_picks();
    test_spawn();
    puts("all delibirdy host tests passed");
    return 0;
}
