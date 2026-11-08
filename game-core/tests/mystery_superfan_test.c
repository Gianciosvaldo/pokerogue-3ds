/* The Bug-Type Superfan's options 2 and 3 on the host: what the upstream differential (mystery_superfan_diff_test) cannot compare, because its oracle stops at the
 * recorded rewards: the screen (which options are on, the pick kinds and tokens), the choice through gc_run_choose (a rejected pick leaves the screen as it was),
 * and the shop that follows (the rewards the choice leaves with, and the held items after). */
#include "../src/internal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static gc_run run;
static gc_system sys;

static const int BUGS[6] = {SPECIES_ID_BEEDRILL, SPECIES_ID_BUTTERFREE, SPECIES_ID_SCYTHER, SPECIES_ID_PINSIR, SPECIES_ID_HERACROSS, SPECIES_ID_VOLCARONA};
static const int MIX[4] = {SPECIES_ID_SNORLAX, SPECIES_ID_BEEDRILL, SPECIES_ID_PIKACHU, SPECIES_ID_BUTTERFREE};
static const int NONE[3] = {SPECIES_ID_SNORLAX, SPECIES_ID_PIKACHU, SPECIES_ID_MEWTWO};

static void start(const char *seed, int wave, const int *species, int n)
{
    memset(&run, 0, sizeof run);
    gc_run_new(&run, GAME_MODES_CLASSIC, seed, 7);
    gc_system_init_defaults(&sys);
    run.sys = &sys;
    run.wave = wave;
    run.biome = BIOME_ID_PLAINS;
    gc_items_to_battle(&run);
    run.n_party = n;
    for (int i = 0; i < n; i++) {
        gc_starter s;
        memset(&s, 0, sizeof s);
        s.species = species[i];
        s.tera_type = -1;
        for (int k = 0; k < 6; k++) s.ivs[k] = 15;
        s.moves[0] = MOVE_ID_TACKLE;
        gc_mon_new_player(&run, &run.party[i], &s, 40);
        run.party_order[i] = (int8_t)i;
    }
    gc_items_to_battle(&run);
    run.battle_type = BATTLE_TYPE_WILD;
    gc_rq_reset(&gc_int(&run)->rq);
    run.mystery_type = GC_ME_BUG_TYPE_SUPERFAN;
    gc_rq_unshift(&run, gc_rp(RP_MYSTERY, GC_ME_BUG_TYPE_SUPERFAN, GC_ME_GREAT, 0));
    gc_run_until_screen(&run);
    assert(run.screen.kind == GC_SCREEN_MYSTERY);
}

static void test_the_screen(void)
{
    puts("test_superfan_screen...");
    /* two Bug types: the fight and the Bug-types are on, the item gift is not */
    start("sf-a", 63, MIX, 4);
    assert(run.screen.n_options == 3);
    assert(run.screen.option_ok[0] && run.screen.option_ok[1] && !run.screen.option_ok[2]);
    assert(gc_superfan_num_bug_types(&run) == 2 && gc_superfan_spawnable(&run));
    assert(run.screen.option_pick[0] == GC_PICK_NONE && run.screen.option_pick[1] == GC_PICK_NONE && run.screen.option_pick[2] == GC_PICK_MEMBER_ITEM);
    bool count = false, items = false;
    for (int k = 0; k < run.screen.n_me_args; k++) {
        if (!strcmp(run.screen.me_args[k].name, "numBugTypes")) count = run.screen.me_args[k].v == 2;
        if (!strcmp(run.screen.me_args[k].name, "requiredBugItems")) items = run.screen.me_args[k].kind == PR_ARG_TEMPLATE && strstr(run.screen.me_args[k].s, "QUICK_CLAW") && strstr(run.screen.me_args[k].s, "GRIP_CLAW");
    }
    assert(count && items);
    /* a fainted Bug type still counts (TypeRequirement(BUG, excludeFainted false)) */
    run.party[1].b.hp = 0;
    run.party[1].b.status = STATUS_EFFECT_FAINT;
    assert(gc_superfan_num_bug_types(&run) == 2 && gc_superfan_option_ok(&run, 1));
    /* no Bug type and nothing held: not offered, and neither option is on */
    start("sf-b", 63, NONE, 3);   /* the wave's pick can only be this encounter: it is queued directly */
    assert(!gc_superfan_spawnable(&run) && !gc_superfan_option_ok(&run, 1) && !gc_superfan_option_ok(&run, 2) && gc_superfan_option_ok(&run, 0));
    /* a Quick Claw alone spawns it and turns the third option on (not the second) */
    gc_item_add(&run, "QUICK_CLAW", -1, run.party[1].b.id, 1);
    assert(gc_superfan_spawnable(&run) && !gc_superfan_option_ok(&run, 1) && gc_superfan_option_ok(&run, 2));
    assert(gc_superfan_pick_ok(&run, 1) && !gc_superfan_pick_ok(&run, 0));
    /* a booster of another type is no Bug item; a Bug one is */
    start("sf-c", 63, NONE, 3);
    gc_item_add(&run, "ATTACK_TYPE_BOOSTER", POKEMON_TYPE_FIRE, run.party[0].b.id, 1);
    assert(!gc_superfan_spawnable(&run));
    gc_item_add(&run, "ATTACK_TYPE_BOOSTER", POKEMON_TYPE_BUG, run.party[2].b.id, 1);
    assert(gc_superfan_spawnable(&run) && gc_superfan_pick_ok(&run, 2) && !gc_superfan_pick_ok(&run, 0));
    /* what is not a Bug item: another key with the Bug type as its argument (a stat booster of stat 6), and a Bug item that cannot be transferred still passes the party screen's filter */
    start("sf-h", 63, NONE, 3);
    gc_item_add(&run, "BASE_STAT_BOOSTER", POKEMON_TYPE_BUG, run.party[1].b.id, 1);
    assert(!gc_superfan_pick_ok(&run, 1) && !gc_superfan_spawnable(&run));
    gc_item_add(&run, "QUICK_CLAW", -1, run.party[1].b.id, 1);
    for (int i = 0; i < run.n_items; i++) if (!strcmp(run.items[i].key, "QUICK_CLAW")) run.items[i].non_transferable = true;
    int held0[GC_MAX_ITEMS];
    assert(gc_superfan_pick_ok(&run, 1) && gc_superfan_held_items(&run, 1, held0) == 0 && !gc_superfan_spawnable(&run) && !gc_superfan_option_ok(&run, 2));
    assert(!gc_superfan_pick_ok(&run, run.n_party) && !gc_superfan_pick_ok(&run, -1));
    /* an item whose holder is not in the party does not count */
    start("sf-i", 63, NONE, 3);
    gc_item_add(&run, "QUICK_CLAW", -1, 987654u, 1);
    assert(!gc_superfan_spawnable(&run));
    puts("ok");
}

static int shop_options(void) { return run.screen.kind == GC_SCREEN_REWARD ? run.screen.n_options : -1; }

static void test_show_your_bug_types(void)
{
    puts("test_superfan_show_bug_types...");
    /* two Bug types: Quick Claw, Max Lure and Ultra Ball (three in the shop, nothing else) */
    start("sf-d", 63, MIX, 4);
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 1, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    /* the shop is exactly the three items (fillRemaining false), with the luck upgrades of every encounter shop */
    assert(!gc_int(&run)->reward.custom.fill_remaining && gc_int(&run)->reward.custom.allow_luck_upgrades && gc_int(&run)->reward.custom.n_funcs == 3);
    gc_run_until_screen(&run);
    assert(run.screen.kind == GC_SCREEN_REWARD && shop_options() == 3);
    /* no Bug type: the second option cannot be taken, and the screen stays */
    start("sf-e", 63, NONE, 3);
    gc_item_add(&run, "QUICK_CLAW", -1, run.party[0].b.id, 1);
    assert(!run.screen.option_ok[1]);
    ch = (gc_choice){GC_CH_MYSTERY, 1, 0, 0};
    assert(!gc_run_choose(&run, &ch, &why) && run.screen.kind == GC_SCREEN_MYSTERY);
    /* six Bug types: the Master Ball, the Mega Bracelet and the Dynamax Band, and a special item */
    start("sf-f", 63, BUGS, 6);
    assert(gc_superfan_num_bug_types(&run) == 6);
    ch = (gc_choice){GC_CH_MYSTERY, 1, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    gc_run_until_screen(&run);
    assert(run.screen.kind == GC_SCREEN_REWARD && shop_options() >= 3 && shop_options() <= 4);
    puts("ok");
}

static int count_items(const char *key, uint32_t holder)
{
    int n = 0;
    for (int i = 0; i < run.n_items; i++) if (!strcmp(run.items[i].key, key) && run.items[i].pokemon_id == holder) n += run.items[i].stack;
    return n;
}

static void test_gift_a_bug_item(void)
{
    puts("test_superfan_gift...");
    start("sf-g", 63, MIX, 4);
    const uint32_t holder = run.party[2].b.id;   /* Pikachu holds a Quick Claw x2, a Grip Claw and a Fire booster */
    gc_item_add(&run, "QUICK_CLAW", -1, holder, 2);
    gc_item_add(&run, "ATTACK_TYPE_BOOSTER", POKEMON_TYPE_FIRE, holder, 1);
    gc_item_add(&run, "GRIP_CLAW", -1, holder, 1);
    gc_mystery_setup_screen(&run);
    assert(run.screen.option_ok[2]);
    int held[GC_MAX_ITEMS];
    assert(gc_mystery_held_items(&run, 2, 2, held, GC_MAX_ITEMS) == 2);   /* the Quick Claw and the Grip Claw, not the Fire booster */
    assert(!strcmp(run.items[held[0]].key, "QUICK_CLAW") && !strcmp(run.items[held[1]].key, "GRIP_CLAW"));
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 2, 0, 0};   /* Snorlax holds nothing */
    assert(!gc_run_choose(&run, &ch, &why) && run.screen.kind == GC_SCREEN_MYSTERY);
    ch = (gc_choice){GC_CH_MYSTERY, 2, 2, 5};   /* no such item in the list */
    assert(!gc_run_choose(&run, &ch, &why) && run.screen.kind == GC_SCREEN_MYSTERY);
    ch = (gc_choice){GC_CH_MYSTERY, 2, 2, 2};   /* one past the list's end */
    assert(!gc_run_choose(&run, &ch, &why) && run.screen.kind == GC_SCREEN_MYSTERY);
    ch = (gc_choice){GC_CH_MYSTERY, 2, 2, -1};
    assert(!gc_run_choose(&run, &ch, &why) && run.screen.kind == GC_SCREEN_MYSTERY);
    assert(count_items("QUICK_CLAW", holder) == 2);
    /* updateModifiers(true, true) resets the virtual stacks, and the pruning drops what a Pokemon outside the party holds */
    gc_item_add(&run, "LEFTOVERS", -1, 987654u, 1);
    for (int i = 0; i < run.n_items; i++) if (!strcmp(run.items[i].key, "GRIP_CLAW")) run.items[i].virtual_stack = 3;
    ch = (gc_choice){GC_CH_MYSTERY, 2, 2, 0};   /* one Quick Claw goes */
    assert(gc_run_choose(&run, &ch, &why));
    for (int i = 0; i < run.n_items; i++) assert(run.items[i].pokemon_id != 987654u && run.items[i].virtual_stack == 0);
    gc_run_until_screen(&run);
    assert(count_items("QUICK_CLAW", holder) == 1 && count_items("GRIP_CLAW", holder) == 1);
    /* the shop has the Golden Bug Net and the Reviver Seed */
    assert(run.screen.kind == GC_SCREEN_REWARD && shop_options() == 2);
    puts("ok");
}

int main(void)
{
    test_the_screen();
    test_show_your_bug_types();
    test_gift_a_bug_item();
    puts("all superfan host tests passed");
    return 0;
}
