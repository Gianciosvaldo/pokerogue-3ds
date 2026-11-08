/* The Expert Pokemon Breeder on the host: the parts the upstream differential (mystery_breeder_diff_test) cannot compare, because its oracle does not play the
 * battle or the shop: the three offers (the least friendship first, the fainted left out, fewer than three able not offered), the lone Pokemon's fight against
 * the breeder's three, the win (the others and their items back, the friendship, the eggs and the outro), the loss (nothing earned, the heal shop, the failed outro),
 * and a full party coming back in the same order. */
#include "../src/internal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static gc_run run;
static gc_system sys;

static const int TANKS[4] = {SPECIES_ID_SNORLAX, SPECIES_ID_SNORLAX, SPECIES_ID_SNORLAX, SPECIES_ID_SNORLAX};
static const int WEAK[3] = {SPECIES_ID_MAGIKARP, SPECIES_ID_MAGIKARP, SPECIES_ID_MAGIKARP};
static const int SIX[6] = {SPECIES_ID_SNORLAX, SPECIES_ID_SNORLAX, SPECIES_ID_SNORLAX, SPECIES_ID_SNORLAX, SPECIES_ID_SNORLAX, SPECIES_ID_SNORLAX};

static void start(const char *seed, int wave, const int *species, int n, int level, const int *friendship)
{
    memset(&run, 0, sizeof run);
    gc_run_new(&run, GAME_MODES_CLASSIC, seed, 7);
    gc_system_init_defaults(&sys);
    run.sys = &sys;
    run.wave = wave;
    run.biome = BIOME_ID_PLAINS;
    run.money = 100000;
    gc_items_to_battle(&run);
    run.n_party = n;
    for (int i = 0; i < n; i++) {
        gc_starter s;
        memset(&s, 0, sizeof s);
        s.species = species[i];
        s.tera_type = -1;
        for (int k = 0; k < 6; k++) s.ivs[k] = 15;
        s.moves[0] = MOVE_ID_TACKLE;
        gc_mon_new_player(&run, &run.party[i], &s, level);
        run.party[i].pause_evolutions = true;
        run.party[i].b.friendship = (uint8_t)friendship[i];
        run.party_order[i] = (int8_t)i;
    }
    gc_items_to_battle(&run);
    run.battle_type = BATTLE_TYPE_WILD;
    gc_rq_reset(&gc_int(&run)->rq);
    run.mystery_type = GC_ME_THE_EXPERT_POKEMON_BREEDER;
    gc_rq_unshift(&run, gc_rp(RP_MYSTERY, GC_ME_THE_EXPERT_POKEMON_BREEDER, GC_ME_COMMON, 0));
    gc_run_until_screen(&run);
    assert(run.screen.kind == GC_SCREEN_MYSTERY);
}

static int said_outro, said_failed, said_eggs, outro_arg_kind = -1;
static void drain(void)
{
    gc_event e;
    while (gc_run_next_event(&run, &e)) {
        if (e.kind != GC_EV_MESSAGE || !e.msg.key) continue;
        if (!strcmp(e.msg.key, "mysteryEncounters/theExpertPokemonBreeder:outro")) { said_outro++; outro_arg_kind = e.msg.n_args ? e.msg.args[0].kind : -1; }
        if (!strcmp(e.msg.key, "mysteryEncounters/theExpertPokemonBreeder:outroFailed")) said_failed++;
        if (!strcmp(e.msg.key, "mysteryEncounters/theExpertPokemonBreeder:gainedEggs")) said_eggs++;
    }
}

static int items_of(uint32_t id)
{
    int n = 0;
    for (int i = 0; i < run.n_items; i++) n += run.items[i].pokemon_id == id;
    return n;
}

/* fights until the command screen is gone */
static void fight_on(void)
{
    const char *why;
    gc_choice fight = {GC_CH_FIGHT, 0, -1, 0};
    for (int guard = 0; guard < 80 && run.screen.kind == GC_SCREEN_COMMAND; guard++) {
        assert(gc_run_choose(&run, &fight, &why));
        drain();
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

static void test_offers(void)
{
    puts("test_breeder_offers...");
    static const int friendship[4] = {200, 50, 120, 30};
    start("brd-offers", 63, TANKS, 4, 40, friendship);
    assert(run.screen.n_options == 3 && run.screen.option_ok[0] && run.screen.option_ok[1] && run.screen.option_ok[2]);
    int pick[3];
    assert(gc_breeder_picks(&run, pick));
    assert(pick[0] == 3 && pick[1] == 1 && pick[2] == 2);   /* 30, 50, 120 */
    /* a fainted Pokemon is not offered: the next least friendship takes its place */
    run.party[3].b.hp = 0;
    run.party[3].b.status = STATUS_EFFECT_FAINT;
    assert(gc_breeder_picks(&run, pick) && pick[0] == 1 && pick[1] == 2 && pick[2] == 0);
    /* equal friendship keeps the party's order */
    for (int i = 0; i < 4; i++) run.party[i].b.friendship = 70;
    run.party[3].b.hp = run.party[3].b.stats[0];
    run.party[3].b.status = STATUS_EFFECT_NONE;
    assert(gc_breeder_picks(&run, pick) && pick[0] == 0 && pick[1] == 1 && pick[2] == 2);
    /* the eggs (the differential compares the numbers): a Snorlax earns some, never more than the 30 points */
    int c, r;
    gc_breeder_eggs(&run.party[0], &c, &r);
    assert(c + r > 0 && c + r * 4 <= 30);
    /* fewer than three able: nobody is offered, and the option leaves with the heal shop */
    run.party[1].b.hp = 0;
    run.party[1].b.status = STATUS_EFFECT_FAINT;
    run.party[2].b.hp = 0;
    run.party[2].b.status = STATUS_EFFECT_FAINT;
    assert(!gc_breeder_picks(&run, pick));
    gc_breeder_execute(&run, 0);
    assert(run.n_party == 4 && !run.in_battle);
    puts("ok");
}

static void test_the_win(void)
{
    puts("test_breeder_win...");
    static const int friendship[4] = {200, 50, 120, 30};
    start("brd-win", 63, TANKS, 4, 100, friendship);
    /* the others hold items; the chosen one (the second option: friendship 50) holds Leftovers */
    gc_item_add(&run, "LEFTOVERS", -1, run.party[0].b.id, 1);
    gc_item_add(&run, "LEFTOVERS", -1, run.party[1].b.id, 1);
    gc_item_add(&run, "LEFTOVERS", -1, run.party[3].b.id, 1);
    const int n_items_before = run.n_items;
    int pick[3];
    assert(gc_breeder_picks(&run, pick) && pick[1] == 1);
    const uint32_t chosen_id = run.party[1].b.id, others_item_holders = run.party[0].b.id;
    int commons, rares;
    gc_breeder_eggs(&run.party[1], &commons, &rares);
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 1, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    drain();
    assert(run.screen.kind == GC_SCREEN_COMMAND);
    /* the lone Pokemon against the breeder's three */
    assert(run.n_party == 1 && run.party[run.party_order[0]].b.id == chosen_id);
    assert(items_of(chosen_id) == 1 && items_of(others_item_holders) == 0 && run.n_items == 1);
    assert(run.battle_type == BATTLE_TYPE_TRAINER && run.trainer_type == TRAINER_TYPE_EXPERT_POKEMON_BREEDER && !run.double_battle);
    assert(run.n_enemy == 3 && run.me_battle && !run.screen.can_run && !run.me_catch_allowed);
    assert(run.enemy[0].b.species == SPECIES_ID_CLEFAIRY && run.enemy[0].b.nature == NATURE_ADAMANT && run.enemy[0].b.friendship == 255);
    for (int e = 0; e < 3; e++) {
        int bells = 0;
        for (int i = 0; i < run.n_enemy_items; i++) if (run.enemy_items[i].pokemon_id == run.enemy[e].b.id && !strcmp(run.enemy_items[i].key, "SOOTHE_BELL") && run.enemy_items[i].stack == 3) bells++;
        assert(bells == 1);
        run.enemy[e].b.hp = 1;
        run.enemy[e].b.stats[1] = run.enemy[e].b.stats[3] = 1;   /* none can hurt the lead before the win */
    }
    const int friend_before = run.party[1].b.friendship;
    fight_on();
    assert(run.screen.kind == GC_SCREEN_REWARD);
    /* the battle's view of the party has the others again (the shop and the level-ups read it) */
    assert(run.bt.n_party[0] == 4);
    /* the others are back, in the old order with the chosen Pokemon in front of the old lead's place, and every held item */
    assert(run.n_party == 4);
    assert(run.party[run.party_order[0]].b.id == chosen_id);
    assert(run.n_items == n_items_before);
    assert(items_of(run.party[0].b.id) == 1 && items_of(run.party[3].b.id) == 1 && items_of(chosen_id) == 1);
    /* the cleanup's 20 (the battle's own victory gain comes on top of it) */
    assert(run.party[1].b.friendship >= friend_before + 20 || run.party[1].b.friendship == 255);
    /* the eggs: Common first, then Rare, from an event */
    int c_eggs = 0, r_eggs = 0;
    for (int k = 0; k < sys.n_eggs; k++) {
        assert(sys.eggs[k].source_type == 4);
        if (sys.eggs[k].tier == 0) c_eggs++; else r_eggs++;
    }
    assert(c_eggs == commons && r_eggs == rares);
    leave_shop();
    drain();
    assert(said_outro == 1 && said_eggs == (commons > 0) + (rares > 0) && said_failed == 0);
    assert(outro_arg_kind == PR_ARG_POKEMON_NAME);   /* {{chosenPokemon}} is the chosen Pokemon's name */
    puts("ok");
}

static void test_the_loss(void)
{
    puts("test_breeder_loss...");
    static const int friendship[3] = {40, 10, 90};
    said_outro = said_failed = said_eggs = 0;
    start("brd-loss", 63, WEAK, 3, 5, friendship);
    gc_item_add(&run, "LEFTOVERS", -1, run.party[0].b.id, 1);
    gc_item_add(&run, "LEFTOVERS", -1, run.party[2].b.id, 1);
    const int n_items_before = run.n_items;
    const uint32_t chosen_id = run.party[1].b.id;   /* the least friendship */
    const int money_before = run.money, score_before = run.score;
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 0, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    drain();
    assert(run.screen.kind == GC_SCREEN_COMMAND && run.n_party == 1 && run.party[run.party_order[0]].b.id == chosen_id);
    /* a lost fight earns no battle score even when the battle had earned some */
    run.bt.battle_score = 1000;
    fight_on();
    /* the run goes on: the others are back, the chosen one stays fainted with no friendship, and the shop is the heal-only one */
    assert(run.screen.kind == GC_SCREEN_REWARD);
    assert(run.n_party == 3 && run.n_items == n_items_before);
    gc_mon *chosen = NULL;
    for (int i = 0; i < run.n_party; i++) if (run.party[i].b.id == chosen_id) chosen = &run.party[i];
    assert(chosen && chosen->b.hp == 0 && chosen->b.friendship == 0);
    assert(sys.n_eggs == 0);
    assert(run.score == score_before);   /* a lost fight: no battle score */
    leave_shop();
    drain();
    assert(run.money == money_before);   /* no prize money */
    assert(said_failed == 1 && said_outro == 0 && said_eggs == 0);
    puts("ok");
}

static void test_the_stages(void)
{
    puts("test_breeder_stages...");
    static const int friendship[4] = {200, 50, 120, 30};
    start("brd-stage", 63, TANKS, 4, 40, friendship);
    static const int waves[8] = {44, 45, 46, 74, 75, 76, 25, 175};
    static const int expect[8] = {SPECIES_ID_CLEFFA, SPECIES_ID_CLEFAIRY, SPECIES_ID_CLEFAIRY, SPECIES_ID_CLEFAIRY, SPECIES_ID_CLEFABLE, SPECIES_ID_CLEFABLE, SPECIES_ID_CLEFFA, SPECIES_ID_CLEFABLE};
    for (int i = 0; i < 8; i++) {
        int sp[3];
        run.wave = waves[i];
        gc_breeder_party_species(&run, sp);
        assert(sp[0] == expect[i]);
        run.biome = BIOME_ID_SPACE;   /* Space: three of the same */
        gc_breeder_party_species(&run, sp);
        assert(sp[0] == expect[i] && sp[1] == expect[i] && sp[2] == expect[i]);
        run.biome = BIOME_ID_PLAINS;
    }
    puts("ok");
}

static void test_a_lost_fight_with_nobody_else_left(void)
{
    puts("test_breeder_loss_nobody_left...");
    static const int friendship[3] = {40, 10, 90};
    start("brd-over", 63, WEAK, 3, 5, friendship);
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 0, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    drain();
    assert(run.screen.kind == GC_SCREEN_COMMAND && run.n_party == 1);
    /* the two waiting Pokemon cannot fight (as if they had fainted off-field): losing is a lost game */
    gc_internal *in = gc_int(&run);
    for (int k = 0; k < in->breeder.n_rest; k++) {
        in->breeder.rest[k].b.hp = 0;
        in->breeder.rest[k].b.status = STATUS_EFFECT_FAINT;
    }
    fight_on();
    assert(run.screen.kind == GC_SCREEN_GAME_OVER);
    puts("ok");
}

static void test_a_full_party_comes_back_in_order(void)
{
    puts("test_breeder_full_party...");
    static const int friendship[6] = {90, 80, 70, 60, 50, 40};
    said_outro = said_failed = said_eggs = 0;
    start("brd-six", 63, SIX, 6, 100, friendship);
    uint32_t ids[6];
    for (int i = 0; i < 6; i++) ids[i] = run.party[run.party_order[i]].b.id;
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 0, 0, 0};   /* friendship 40: the last one */
    assert(gc_run_choose(&run, &ch, &why));
    drain();
    assert(run.n_party == 1 && run.party[run.party_order[0]].b.id == ids[5]);
    for (int e = 0; e < 3; e++) { run.enemy[e].b.hp = 1; run.enemy[e].b.stats[1] = run.enemy[e].b.stats[3] = 1; }
    fight_on();
    assert(run.screen.kind == GC_SCREEN_REWARD && run.n_party == 6);
    /* the chosen one moves to the front and the old lead takes its place; the rest follow in the order they left */
    assert(run.party[run.party_order[0]].b.id == ids[5]);
    assert(run.party[run.party_order[1]].b.id == ids[1] && run.party[run.party_order[2]].b.id == ids[2]);
    assert(run.party[run.party_order[3]].b.id == ids[3] && run.party[run.party_order[4]].b.id == ids[4] && run.party[run.party_order[5]].b.id == ids[0]);
    puts("ok");
}

int main(void)
{
    test_offers();
    test_the_win();
    test_the_loss();
    test_the_stages();
    test_a_lost_fight_with_nobody_else_left();
    test_a_full_party_comes_back_in_order();
    puts("all breeder host tests passed");
    return 0;
}
