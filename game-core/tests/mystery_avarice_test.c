/* Absolute Avarice on the host: the parts the upstream differential (mystery_avarice_diff_test) cannot compare, because its oracle does not play the battle:
 * the Berries really leave the party, the Greedent fights holding them (Stuff Cheeks lands before the first command, so a Berry is eaten and its Defense
 * rises), it cannot be fled from or caught, its Sp. Def is up a stage, and winning gives every Pokemon a Reviver Seed once, in the ordinary shop; the plea
 * gives some of the Berries back, within the stack maximums; the feast puts a Greedent in the party through the catch, and a full party is asked what to
 * release; and the encounter is offered only with six Berries but then every option is open. */
#include "../src/internal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static gc_run run;
static gc_system sys;

static int berries_of(int mon_index)
{
    int n = 0;
    for (int i = 0; i < run.n_items; i++)
        if (!strcmp(run.items[i].key, "BERRY") && run.items[i].pokemon_id == run.party[mon_index].b.id) n += run.items[i].stack;
    return n;
}

static int berries_total(void)
{
    int n = 0;
    for (int i = 0; i < run.n_items; i++) if (!strcmp(run.items[i].key, "BERRY")) n += run.items[i].stack;
    return n;
}

static int seeds_of(int mon_index)
{
    int n = 0;
    for (int i = 0; i < run.n_items; i++)
        if (!strcmp(run.items[i].key, "REVIVER_SEED") && run.items[i].pokemon_id == run.party[mon_index].b.id) n += run.items[i].stack;
    return n;
}

static void start(const char *seed, int wave, int n_mons)
{
    memset(&run, 0, sizeof run);
    gc_run_new(&run, GAME_MODES_CLASSIC, seed, 7);
    gc_system_init_defaults(&sys);
    run.sys = &sys;
    run.wave = wave;
    run.biome = BIOME_ID_PLAINS;
    run.n_items = 0;
    gc_items_to_battle(&run);
    run.n_party = n_mons;
    for (int i = 0; i < n_mons; i++) {
        gc_starter s;
        memset(&s, 0, sizeof s);
        s.species = i == 0 ? SPECIES_ID_PIKACHU : i == 1 ? SPECIES_ID_LAPRAS : i == 2 ? SPECIES_ID_ONIX : i == 3 ? SPECIES_ID_EEVEE : i == 4 ? SPECIES_ID_ABRA : SPECIES_ID_GEODUDE;
        s.tera_type = -1;
        for (int k = 0; k < 6; k++) s.ivs[k] = 15;
        s.moves[0] = MOVE_ID_TACKLE;
        gc_mon_new_player(&run, &run.party[i], &s, 40 + 5 * i);
        run.party[i].pause_evolutions = true;
        run.party_order[i] = (int8_t)i;
    }
    /* Berries: Sitrus x2 and Oran-like Petaya x3 on the first, Lum x2 and Starf x3 on the second, Salac x1 on the third: 11 stacks */
    gc_item_add(&run, "BERRY", BERRY_TYPE_SITRUS, run.party[0].b.id, 2);
    gc_item_add(&run, "BERRY", BERRY_TYPE_PETAYA, run.party[0].b.id, 3);
    gc_item_add(&run, "BERRY", BERRY_TYPE_LUM, run.party[1].b.id, 2);
    gc_item_add(&run, "BERRY", BERRY_TYPE_STARF, run.party[1].b.id, 3);
    gc_item_add(&run, "BERRY", BERRY_TYPE_SALAC, run.party[2].b.id, 1);
    gc_items_to_battle(&run);
    run.battle_type = BATTLE_TYPE_WILD;
    gc_rq_reset(&gc_int(&run)->rq);
    run.mystery_type = GC_ME_ABSOLUTE_AVARICE;
    gc_rq_unshift(&run, gc_rp(RP_MYSTERY, GC_ME_ABSOLUTE_AVARICE, GC_ME_GREAT, 0));
    gc_run_until_screen(&run);
    assert(run.screen.kind == GC_SCREEN_MYSTERY);
}

static void test_all_options_open(void)
{
    puts("test_avarice_all_options_open...");
    start("avarice-open", 45, 3);
    assert(berries_total() == 11);
    assert(run.screen.n_options == 3);
    for (int o = 0; o < 3; o++) assert(run.screen.option_ok[o]);
    puts("ok");
}

/* PersistentModifierRequirement("BerryModifier", 6) counts BerryModifier stacks only: a Berry Pouch (PreserveBerryModifier) is no Berry */
static void test_offer_gate(void)
{
    puts("test_avarice_offer_gate...");
    start("avarice-gate", 45, 3);
    gc_item_remove_stacks(&run, "BERRY", 100);
    gc_item_add(&run, "BERRY", BERRY_TYPE_SITRUS, run.party[0].b.id, 2);
    gc_item_add(&run, "BERRY_POUCH", -1, 0, 5);
    assert(!gc_avarice_spawnable(&run));
    gc_item_add(&run, "BERRY", BERRY_TYPE_PETAYA, run.party[0].b.id, 3);
    assert(!gc_avarice_spawnable(&run));   /* five stacks */
    gc_item_add(&run, "BERRY", BERRY_TYPE_LUM, run.party[1].b.id, 1);
    assert(gc_avarice_spawnable(&run));    /* six */
    puts("ok");
}

static void test_fight(void)
{
    puts("test_avarice_fight...");
    start("avarice-fight", 45, 3);
    run.ask_switch = true;   /* the SWITCH style asks "Will you switch?": the Greedent's config does not disable switching, so it is asked */
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 0, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    /* switching is not disabled, so the SWITCH style asks "Will you switch?" (Strong Stuff's config disables it and is not asked) */
    assert(run.screen.kind == GC_SCREEN_CHECK_SWITCH);
    gc_choice no = {GC_CH_CONFIRM, 0, 0, 0};
    assert(gc_run_choose(&run, &no, &why));
    /* the Berries are gone from the party, and on the Greedent */
    assert(run.screen.kind == GC_SCREEN_COMMAND);
    assert(berries_total() == 0);
    assert(run.n_enemy == 1 && run.enemy[0].b.species == SPECIES_ID_GREEDENT && run.enemy[0].b.is_boss && run.enemy[0].b.boss_segments == 3);
    assert(!run.screen.can_run);
    assert(!run.me_catch_allowed);
    int on_boss = 0;
    for (int i = 0; i < run.n_enemy_items; i++) if (!strcmp(run.enemy_items[i].key, "BERRY")) on_boss += run.enemy_items[i].stack;
    /* 11 stacks stolen; each type fits its stack's maximum, and Stuff Cheeks ate one of them before the first command */
    assert(on_boss == 10);
    /* Stuff Cheeks raised its Defense two stages (Simple doubles it, Contrary turns it down) and the post-summon effect raised Sp. Def one */
    const gc_mon *e = &run.enemy[0];
    assert(e->b.stat_stages[STAT_DEF - 1] != 0);
    assert(e->b.stat_stages[STAT_SPDEF - 1] != 0);
    assert(e->b.stat_stages[STAT_SPD - 1] == 0);   /* wave 45: below wave 50, no Speed */
    /* winning gives each Pokemon a Reviver Seed among the ordinary rewards, once */
    run.enemy[0].b.hp = 1;
    gc_choice fight = {GC_CH_FIGHT, 0, -1, 0};
    for (int guard = 0; guard < 8 && run.screen.kind == GC_SCREEN_COMMAND; guard++) assert(gc_run_choose(&run, &fight, &why));
    assert(run.screen.kind == GC_SCREEN_REWARD);
    for (int i = 0; i < run.n_party; i++) assert(seeds_of(i) == 1);
    assert(berries_total() == 0);
    puts("ok");
}

static void test_plea(void)
{
    puts("test_avarice_plea...");
    start("avarice-plea", 45, 3);
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 1, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    /* floor(5 * 2 / 5) = 2 back for each of the first two, floor(1 * 2 / 5) = 0 for the third, and nothing for the others */
    assert(berries_of(0) == 2 && berries_of(1) == 2 && berries_of(2) == 0);
    assert(run.screen.kind == GC_SCREEN_REWARD && !run.me_battle && run.n_enemy == 0);
    puts("ok");
}

static void test_feast(void)
{
    puts("test_avarice_feast...");
    start("avarice-feast", 45, 3);
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 2, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    assert(berries_total() == 0);
    /* the Greedent joined: two levels under the party's highest (Onix, level 50), with its passive on and its moves */
    assert(run.n_party == 4);
    const gc_mon *g = &run.party[run.party_order[3]];
    assert(g->b.species == SPECIES_ID_GREEDENT && g->b.level == 48 && g->b.passive_flag && !g->b.shiny);
    assert(g->b.n_moves == 4 && g->b.moves[0] == MOVE_ID_THRASH && g->b.moves[1] == MOVE_ID_BODY_PRESS && g->b.moves[2] == MOVE_ID_STUFF_CHEEKS && g->b.moves[3] == MOVE_ID_SLACK_OFF);
    puts("ok");
}

static void test_feast_full_party(void)
{
    puts("test_avarice_feast_full_party...");
    start("avarice-full", 45, 6);
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 2, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    /* a full party is asked what to release, as for any caught Pokemon */
    assert(run.n_party == 6);
    assert(run.screen.kind == GC_SCREEN_SWITCH && run.screen.can_skip);
    /* letting it go (skip) ends the encounter without a seventh Pokemon */
    gc_choice skip = {GC_CH_PICK, -1, 0, 0};
    assert(gc_run_choose(&run, &skip, &why));
    assert(run.n_party == 6);
    puts("ok");
}

int main(void)
{
    test_all_options_open();
    test_offer_gate();
    test_fight();
    test_plea();
    test_feast();
    test_feast_full_party();
    puts("all avarice host tests passed");
    return 0;
}
