/* The Bug-Type Superfan on the host (the fight, option 2 and option 3): what the upstream differential (mystery_superfan_diff_test) cannot compare, because its oracle stops at the
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

static int said_won, said_outro, said_appeared, said_send_out, trainer_events;
static void drain(void)
{
    gc_event e;
    while (gc_run_next_event(&run, &e)) {
        if (e.kind == GC_EV_TRAINER) trainer_events++;
        if (e.kind != GC_EV_MESSAGE || !e.msg.key) continue;
        if (!strcmp(e.msg.key, "mysteryEncounters/bugTypeSuperfan:outro")) said_outro++;
        if (!strcmp(e.msg.key, "mysteryEncounters/bugTypeSuperfan:battleWon")) said_won++;
        if (!strcmp(e.msg.key, "battle:trainerAppeared")) said_appeared++;
        if (!strcmp(e.msg.key, "battle:trainerSendOut")) said_send_out++;
    }
}

static void leave_shop(void)
{
    const char *why;
    gc_choice skip = {GC_CH_SKIP, 0, 0, 0};
    for (int guard = 0; guard < 6 && run.screen.kind == GC_SCREEN_REWARD; guard++) {
        assert(gc_run_choose(&run, &skip, &why));
        drain();
    }
}

/* option 1: the trainer and his team (by the wave's band), the Bug-type moves drawn, the win's shop and the outro after it */
static void test_the_fight(void)
{
    puts("test_superfan_fight...");
    static const struct { int wave, size; } bands[7] = {{35, 3}, {55, 4}, {75, 5}, {105, 5}, {125, 5}, {145, 5}, {175, 5}};
    for (int b = 0; b < 7; b++) {
        char seed[16];
        snprintf(seed, sizeof seed, "sf-f%d", b);
        start(seed, bands[b].wave, MIX, 4);
        said_appeared = said_send_out = trainer_events = 0;
        const char *why;
        gc_choice ch = {GC_CH_MYSTERY, 0, 0, 0};
        assert(gc_run_choose(&run, &ch, &why));
        drain();
        gc_run_until_screen(&run);
        drain();
        assert(run.in_battle && run.me_battle && run.battle_type == BATTLE_TYPE_TRAINER && !run.double_battle);
        assert(run.trainer_type == TRAINER_TYPE_BUG_TYPE_SUPERFAN && run.trainer_variant == TRAINER_VARIANT_FEMALE);
        assert(run.n_enemy == bands[b].size && gc_superfan_band(bands[b].wave) == b + 1);
        assert(!run.me_catch_allowed);
        assert(!strcmp(gc_int(&run)->custom_trainer_name, "$t(trainerNames:bugTypeSuperfan)"));   /* config.name */
        assert(sys.dex[run.enemy[0].b.species].seen_count >= 1);   /* the encounter does not prevent the game stats' updates: the team is seen */
        assert(said_appeared == 1 && said_send_out == 1 && trainer_events == 1);
        assert(run.enemy[0].b.species == SPECIES_ID_BEEDRILL && run.enemy[1].b.species == SPECIES_ID_BUTTERFREE);
        /* the four Bug-type moves were drawn, one of each kind */
        const gc_internal *in = gc_int(&run);
        assert(in->superfan.tutor[0] && in->superfan.tutor[1] && in->superfan.tutor[2] && in->superfan.tutor[3]);
        /* the shop that follows the win has fillRemaining, and the outro waits for it */
        assert(in->me_reward.active && in->me_reward.fill && in->me_reward.func == -1 && in->me_reward.before && in->superfan.tutor_pending);
        assert(in->me_post.pending && in->me_post.a == GC_ME_BUG_TYPE_SUPERFAN);
        gc_rq_reset(&gc_int(&run)->rq);   /* the turn the fight would have played is not queued: the win is */
        gc_me_win_rewards(&run);
        gc_run_until_screen(&run);
        drain();
        assert(said_won == 1 && !gc_int(&run)->superfan.tutor_pending);   /* showEncounterDialogue(battleWon); the callback runs once */
        said_won = 0;
        /* the tutor: the four moves and the cancel, any Pokemon of the party for the moves */
        assert(run.screen.kind == GC_SCREEN_MYSTERY && run.screen.me_stage == GC_ME_STAGE_TUTOR && run.screen.n_options == 5);
        for (int k = 0; k < 5; k++) assert(run.screen.option_ok[k] && run.screen.option_pick[k] == (k < 4 ? GC_PICK_MEMBER : GC_PICK_NONE));
        for (int k = 0; k < 4; k++) assert(gc_superfan_tutor_move(&run, k) == (int)in->superfan.tutor[k] && gc_superfan_tutor_move(&run, k) > 0);
        for (int k = 0; k < 4; k++) for (int i = 0; i < run.n_party; i++) assert(gc_mystery_pick_ok(&run, GC_ME_BUG_TYPE_SUPERFAN, k, i));
        assert(!gc_mystery_pick_ok(&run, GC_ME_BUG_TYPE_SUPERFAN, 0, run.n_party));
        /* a move and a Pokemon that do not exist are refused and the screen stays */
        gc_choice bad = {GC_CH_MYSTERY, 5, 0, 0};
        assert(!gc_run_choose(&run, &bad, &why) && run.screen.kind == GC_SCREEN_MYSTERY);
        bad = (gc_choice){GC_CH_MYSTERY, 0, run.n_party, 0};
        assert(!gc_run_choose(&run, &bad, &why) && run.screen.kind == GC_SCREEN_MYSTERY);
        bad = (gc_choice){GC_CH_MYSTERY, 0, -1, 0};
        assert(!gc_run_choose(&run, &bad, &why) && run.screen.kind == GC_SCREEN_MYSTERY);
        const int want = (int)in->superfan.tutor[b % 4];
        const int member = b % run.n_party;
        if (b % 2) {
            /* the cancel button: nothing is learned and the shop follows */
            gc_choice cancel = {GC_CH_MYSTERY, 4, 0, 0};
            assert(gc_run_choose(&run, &cancel, &why));
            gc_run_until_screen(&run);
            assert(run.screen.kind == GC_SCREEN_REWARD);
        } else {
            gc_choice teach = {GC_CH_MYSTERY, b % 4, member, 0};
            assert(gc_run_choose(&run, &teach, &why));
            gc_run_until_screen(&run);
            drain();
            /* LearnMovePhase: it learns the move, or asks which to forget when it knows four others */
            bool knows = false;
            for (int k = 0; k < run.party[member].b.n_moves; k++) knows = knows || run.party[member].b.moves[k] == want;
            assert(knows || run.screen.kind == GC_SCREEN_LEARN_MOVE || run.screen.kind == GC_SCREEN_REWARD);
            if (run.screen.kind == GC_SCREEN_LEARN_MOVE) {
                gc_choice forget = {GC_CH_FORGET, -1, 0, 0};
                assert(gc_run_choose(&run, &forget, &why));
                gc_run_until_screen(&run);
            }
            assert(run.screen.kind == GC_SCREEN_REWARD);
        }
        said_outro = 0;
        leave_shop();
        gc_run_until_screen(&run);
        drain();
        assert(said_outro == 1);
    }
    puts("ok");
}

/* the fight won: the callback before the rewards has opened the tutor's screen */
static void win_to_tutor(const char *seed, int wave)
{
    start(seed, wave, MIX, 4);
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 0, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    gc_run_until_screen(&run);
    drain();
    gc_rq_reset(&gc_int(&run)->rq);
    gc_me_win_rewards(&run);
    gc_run_until_screen(&run);
    drain();
    assert(run.screen.kind == GC_SCREEN_MYSTERY && run.screen.me_stage == GC_ME_STAGE_TUTOR);
}

static bool knows(int member, int move)
{
    for (int k = 0; k < run.party[member].b.n_moves; k++) if (run.party[member].b.moves[k] == move) return true;
    return false;
}

/* the tutor teaches the chosen move to the chosen Pokemon only, nothing on the cancel, and refuses what is not a pick */
static void test_the_tutor(void)
{
    puts("test_superfan_tutor...");
    for (int k = 0; k < 4; k++) {
        for (int member = 0; member < 4; member += 3) {
            char seed[16];
            snprintf(seed, sizeof seed, "sf-t%d%d", k, member);
            win_to_tutor(seed, 63);
            const int move = gc_superfan_tutor_move(&run, k);
            const char *why;
            gc_choice teach = {GC_CH_MYSTERY, k, member, 0};
            assert(gc_run_choose(&run, &teach, &why));
            gc_run_until_screen(&run);
            for (int i = 0; i < 4; i++) assert(knows(i, move) == (i == member));
            assert(run.screen.kind == GC_SCREEN_REWARD);
        }
    }
    /* the cancel button teaches nothing */
    win_to_tutor("sf-tc", 63);
    gc_choice cancel = {GC_CH_MYSTERY, 4, 0, 0};
    const char *why;
    assert(gc_run_choose(&run, &cancel, &why));
    gc_run_until_screen(&run);
    for (int k = 0; k < 4; k++) for (int i = 0; i < 4; i++) assert(!knows(i, gc_superfan_tutor_move(&run, k)));
    assert(run.screen.kind == GC_SCREEN_REWARD);
    /* what is no answer is refused and the screen stays */
    win_to_tutor("sf-tr", 63);
    gc_choice skip = {GC_CH_SKIP, 0, 0, 0};
    assert(!gc_run_choose(&run, &skip, &why) && run.screen.kind == GC_SCREEN_MYSTERY);
    gc_choice low = {GC_CH_MYSTERY, -1, 0, 0};
    assert(!gc_run_choose(&run, &low, &why) && run.screen.kind == GC_SCREEN_MYSTERY);
    assert(!gc_mystery_pick_ok(&run, GC_ME_BUG_TYPE_SUPERFAN, 4, 0));   /* the cancel asks for no Pokemon */
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
    /* the outro follows the shop */
    said_outro = 0;
    leave_shop();
    gc_run_until_screen(&run);
    drain();
    assert(said_outro == 1);
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
    test_the_fight();
    test_the_tutor();
    test_show_your_bug_types();
    test_gift_a_bug_item();
    puts("all superfan host tests passed");
    return 0;
}
