/* Berries Abound on the host: the parts the upstream differential (mystery_berries_diff_test) cannot compare, because its oracle does not play
 * the battle: a won fight gives the berries before the five-berry shop, the lost race's boss is really enraged when it comes out, a won race
 * leaves without a battle with the berries on the fastest Pokemon, leaving opens the heal shop. */
#include "../src/internal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static gc_run run;
static gc_system sys;

static void start(const char *seed, int wave, int level_a, int level_b)
{
    memset(&run, 0, sizeof run);
    gc_run_new(&run, GAME_MODES_CLASSIC, seed, 7);
    gc_system_init_defaults(&sys);
    run.sys = &sys;
    run.wave = wave;
    run.biome = BIOME_ID_PLAINS;
    gc_items_to_battle(&run);
    run.n_party = 2;
    for (int i = 0; i < 2; i++) {
        gc_starter s;
        memset(&s, 0, sizeof s);
        s.species = i ? SPECIES_ID_LAPRAS : SPECIES_ID_PIKACHU;
        s.tera_type = -1;
        for (int k = 0; k < 6; k++) s.ivs[k] = 15;
        s.moves[0] = MOVE_ID_TACKLE;
        gc_mon_new_player(&run, &run.party[i], &s, i ? level_b : level_a);
        run.party[i].pause_evolutions = true;
        run.party_order[i] = (int8_t)i;
    }
    gc_items_to_battle(&run);
    run.battle_type = BATTLE_TYPE_WILD;
    gc_rq_reset(&gc_int(&run)->rq);
    run.mystery_type = GC_ME_BERRIES_ABOUND;
    gc_rq_unshift(&run, gc_rp(RP_MYSTERY, GC_ME_BERRIES_ABOUND, GC_ME_COMMON, 0));
    gc_run_until_screen(&run);
    assert(run.screen.kind == GC_SCREEN_MYSTERY);
}

static int berries_held(void)
{
    int n = 0;
    for (int i = 0; i < run.n_items; i++) if (!strcmp(run.items[i].key, "BERRY")) n += run.items[i].stack;
    return n;
}

static int berry_options(void)
{
    int n = 0;
    for (int i = 0; i < run.screen.n_options; i++) n += !strcmp(run.screen.options[i].key, "BERRY");
    return n;
}

static void win_the_fight(void)
{
    const char *why;
    assert(run.screen.kind == GC_SCREEN_COMMAND);
    run.enemy[0].b.hp = 1;
    gc_choice fight = {GC_CH_FIGHT, 0, -1, 0};
    for (int guard = 0; guard < 12 && run.screen.kind == GC_SCREEN_COMMAND; guard++) assert(gc_run_choose(&run, &fight, &why));
}

static void test_fight_gives_berries_then_shop(void)
{
    puts("test_berries_fight_gives_berries_then_shop...");
    start("bry-fight", 45, 60, 60);
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 0, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    assert(run.screen.kind == GC_SCREEN_COMMAND && run.n_enemy == 1 && run.enemy[0].b.is_boss && !run.screen.can_run);
    assert(berries_held() == 0);
    win_the_fight();
    /* wave 45: 4 berries, then the shop of five random berries (no fill) */
    assert(run.screen.kind == GC_SCREEN_REWARD);
    assert(berries_held() == 4);
    assert(run.screen.n_options == 5 && berry_options() == 5);
    puts("ok");
}

static void test_lost_race_enrages_the_boss(void)
{
    puts("test_berries_lost_race_enrages_the_boss...");
    /* two weak Pokemon against a boss of wave 55: the race is lost */
    start("bry-lost", 55, 5, 5);
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 1, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    assert(run.screen.kind == GC_SCREEN_COMMAND && run.n_enemy == 1 && !run.screen.can_run);
    const gc_mon *e = &run.enemy[0];
    const int ab = e->b.ability, pa = e->b.passive;
    if (ab != ABILITY_ID_SIMPLE && ab != ABILITY_ID_CONTRARY && pa != ABILITY_ID_SIMPLE && pa != ABILITY_ID_CONTRARY &&
        ab != ABILITY_ID_DAUNTLESS_SHIELD && ab != ABILITY_ID_INTREPID_SWORD && pa != ABILITY_ID_DAUNTLESS_SHIELD && pa != ABILITY_ID_INTREPID_SWORD) {
        /* from wave 50 on all five of Atk, Def, SpAtk, SpDef and Spd are up one stage (the player's Pikachu and Lapras have no Intimidate) */
        for (int s = 0; s < 5; s++) assert(e->b.stat_stages[s] == 1);
    }
    puts("ok");
}

static void test_won_race_grabs_berries(void)
{
    puts("test_berries_won_race_grabs_berries...");
    /* Lapras and Pikachu at level 100 at wave 12 outrun the boss */
    start("bry-won", 12, 100, 100);
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 1, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    assert(!run.me_battle && run.n_enemy == 0);
    const int n = berries_held();
    assert(n == 2);   /* wave 12: the encounter's count is 2, and a race is worth at least 2 */
    puts("ok");
}

static void test_leave_opens_the_heal_shop(void)
{
    puts("test_berries_leave...");
    start("bry-leave", 45, 40, 40);
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 2, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    assert(berries_held() == 0 && !run.me_battle && run.n_enemy == 0);
    puts("ok");
}

int main(void)
{
    test_fight_gives_berries_then_shop();
    test_lost_race_enrages_the_boss();
    test_won_race_grabs_berries();
    test_leave_opens_the_heal_shop();
    puts("all berries host tests passed");
    return 0;
}
