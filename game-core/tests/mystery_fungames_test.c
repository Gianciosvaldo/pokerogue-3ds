/* Fun and Games! on the host: the parts the upstream differential (mystery_fungames_diff_test) cannot compare, because the real game
 * has nothing to compare them with in a trace: the menu's shape, the commands it refuses, that the Wobbuffet does not outlive the
 * encounter, that the run goes on to the next wave with the picked Pokemon in front, and that nothing is saved mid-game. */
#include "../src/internal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static gc_run run;
static gc_system sys;

/* a run at wave 45 with `n` Pokemon of `level`, the first knowing only Splash (so the Wobbuffet is never hurt) */
static void start(const char *seed, int n, int level, double money)
{
    static const int species[3] = {SPECIES_ID_PIKACHU, SPECIES_ID_LAPRAS, SPECIES_ID_GENGAR};
    memset(&run, 0, sizeof run);
    gc_run_new(&run, GAME_MODES_CLASSIC, seed, 7);
    gc_system_init_defaults(&sys);
    run.sys = &sys;
    run.wave = 45;
    run.biome = BIOME_ID_PLAINS;
    gc_items_to_battle(&run);
    run.n_party = n;
    for (int i = 0; i < n; i++) {
        gc_starter s;
        memset(&s, 0, sizeof s);
        s.species = (uint16_t)species[i % 3];
        s.tera_type = -1;
        for (int k = 0; k < 6; k++) s.ivs[k] = 15;
        s.moves[0] = i == 0 ? MOVE_ID_SPLASH : MOVE_ID_TACKLE;
        gc_mon_new_player(&run, &run.party[i], &s, level + i);
        run.party[i].pause_evolutions = true;
        run.party_order[i] = (int8_t)i;
    }
    run.money = money;
    run.battle_type = BATTLE_TYPE_WILD;
    gc_rq_reset(&gc_int(&run)->rq);
    run.mystery_type = GC_ME_FUN_AND_GAMES;
    gc_rq_unshift(&run, gc_rp(RP_MYSTERY, GC_ME_FUN_AND_GAMES, GC_ME_GREAT, 0));
    gc_run_until_screen(&run);
    assert(run.screen.kind == GC_SCREEN_MYSTERY);
}

static bool message_seen(const char *key)
{
    gc_event e;
    bool seen = false;
    while (gc_run_next_event(&run, &e)) if (e.kind == GC_EV_MESSAGE && e.msg.key && !strcmp(e.msg.key, key)) seen = true;
    return seen;
}

static void play_splash(void)
{
    const char *why;
    gc_choice fc = {GC_CH_FIGHT, 0, -1, 0};
    assert(gc_run_choose(&run, &fc, &why));
}

static void test_screen_and_menu(void)
{
    puts("test_fungames_screen_and_menu...");
    start("fg-screen", 3, 30, 1e6);
    assert(run.screen.n_options == 2);
    assert(run.screen.option_ok[0] && run.screen.option_ok[1]);
    const double cost = pr_run_get_wave_money_amount(45, 1.5);
    const double money = run.money;
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 0, 1, 0};
    assert(gc_run_choose(&run, &ch, &why));
    /* the price is paid, the picked Pokemon (the second) leads, the Wobbuffet is its level */
    assert(run.money == money - cost);
    assert(run.screen.kind == GC_SCREEN_COMMAND && run.screen.fight_only);
    assert(!run.screen.can_run && !run.screen.can_ball && !run.screen.can_switch);
    assert(run.party[run.party_order[0]].b.species == SPECIES_ID_LAPRAS);
    assert(run.n_enemy == 1 && run.enemy[0].b.species == SPECIES_ID_WOBBUFFET);
    assert(run.enemy[0].b.level == run.party[run.party_order[0]].b.level);
    assert(run.enemy[0].b.nature == NATURE_MILD && !run.enemy[0].b.shiny);
    for (int k = 0; k < 6; k++) assert(run.enemy[0].b.ivs[k] == 0);
    assert(run.enemy[0].b.n_moves == 4);
    /* nothing but a move: no ball, no switch, no flee */
    gc_choice run_away = {GC_CH_RUN, 0, 0, 0}, ball = {GC_CH_BALL, 0, 0, 0}, sw = {GC_CH_POKEMON, 0, 0, 0};
    assert(!gc_run_choose(&run, &run_away, &why));
    assert(!gc_run_choose(&run, &ball, &why));
    assert(!gc_run_choose(&run, &sw, &why));
    /* the one save of the encounter is the one at its start */
    assert(!gc_run_can_save(&run));
    puts("ok");
}

static void test_three_turns_then_the_shop(void)
{
    puts("test_fungames_three_turns_then_the_shop...");
    start("fg-turns", 2, 30, 1e6);
    const double cost = pr_run_get_wave_money_amount(45, 1.5);
    const double money = run.money;
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 0, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    /* Splash three times: the Wobbuffet is untouched, which is the worst result */
    for (int t = 0; t < 3; t++) {
        assert(run.screen.kind == GC_SCREEN_COMMAND);
        play_splash();
    }
    assert(run.screen.kind == GC_SCREEN_REWARD);
    assert(run.screen.n_options == 0);
    assert(run.money == money - cost);
    assert(!gc_fg_active(&run) && run.n_enemy == 0);
    /* the run goes on: the shop is left, the next wave's battle starts with the same Pokemon in front */
    gc_choice skip = {GC_CH_SKIP, 0, 0, 0};
    assert(gc_run_choose(&run, &skip, &why));
    assert(run.wave == 46);
    assert(!run.me_battle && !gc_fg_active(&run));
    assert(run.screen.kind == GC_SCREEN_COMMAND || run.screen.kind == GC_SCREEN_MYSTERY || run.screen.kind == GC_SCREEN_SWITCH);
    assert(run.party[run.party_order[0]].b.species == SPECIES_ID_PIKACHU);
    puts("ok");
}

static void test_knock_out_loses_the_game(void)
{
    puts("test_fungames_knock_out_loses_the_game...");
    start("fg-ko", 2, 30, 1e6);
    /* the lead's Tackle (it cannot miss a Wobbuffet) finishes a Wobbuffet with 1 HP left */
    gc_mon_set_move(&run.party[1], 0, MOVE_ID_TACKLE);
    const double cost = pr_run_get_wave_money_amount(45, 1.5);
    const double money = run.money;
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 0, 1, 0};
    assert(gc_run_choose(&run, &ch, &why));
    assert(run.screen.kind == GC_SCREEN_COMMAND);
    run.bt.field[2]->hp = 1;
    gc_choice fc = {GC_CH_FIGHT, 0, -1, 0};
    assert(gc_run_choose(&run, &fc, &why));
    assert(message_seen("mysteryEncounters/funAndGames:ko"));
    assert(run.screen.kind == GC_SCREEN_REWARD);
    assert(!gc_fg_active(&run) && run.n_enemy == 0);
    /* the revive costs the price again */
    assert(run.money == money - 2 * cost);
    puts("ok");
}

static void test_leave(void)
{
    puts("test_fungames_leave...");
    start("fg-leave", 3, 30, 1e6);
    const double money = run.money;
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 1, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    assert(run.screen.kind == GC_SCREEN_REWARD);
    assert(run.money == money && !gc_fg_active(&run) && run.n_enemy == 0);
    puts("ok");
}

static void test_too_poor(void)
{
    puts("test_fungames_too_poor...");
    start("fg-poor", 3, 30, pr_run_get_wave_money_amount(45, 1.5) - 1);
    assert(!run.screen.option_ok[0] && run.screen.option_ok[1]);
    puts("ok");
}

int main(void)
{
    test_screen_and_menu();
    test_three_turns_then_the_shop();
    test_knock_out_loses_the_game();
    test_leave();
    test_too_poor();
    puts("all fun and games tests passed");
    return 0;
}
