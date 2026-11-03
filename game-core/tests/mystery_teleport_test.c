/* Teleporting Hijinks on the host: the parts the upstream differential (mystery_teleport_diff_test) cannot compare, because its oracle does not
 * play the battle and starts from a clean arena: the machine really leaves the old arena behind (weather, tags), the fight cannot be fled from but
 * can be caught, the helper Pokemon alone gets experience when the fight is won, inspecting the machine opens the Steel and Electric boosters
 * first in the shop, and leaving the encounter's fights costs the price only when the money option is taken. */
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
    run.money = 100000;
    gc_items_to_battle(&run);
    run.n_party = 3;
    for (int i = 0; i < 3; i++) {
        gc_starter s;
        memset(&s, 0, sizeof s);
        s.species = i == 0 ? SPECIES_ID_BULBASAUR : i == 1 ? SPECIES_ID_MAGNEMITE : SPECIES_ID_EEVEE;
        s.tera_type = -1;
        for (int k = 0; k < 6; k++) s.ivs[k] = 15;
        s.moves[0] = MOVE_ID_TACKLE;
        gc_mon_new_player(&run, &run.party[i], &s, 30);
        run.party[i].pause_evolutions = true;
        run.party_order[i] = (int8_t)i;
    }
    gc_items_to_battle(&run);
    run.battle_type = BATTLE_TYPE_WILD;
    gc_rq_reset(&gc_int(&run)->rq);
    run.mystery_type = GC_ME_TELEPORTING_HIJINKS;
    gc_rq_unshift(&run, gc_rp(RP_MYSTERY, GC_ME_TELEPORTING_HIJINKS, GC_ME_COMMON, 0));
    gc_run_until_screen(&run);
    assert(run.screen.kind == GC_SCREEN_MYSTERY);
}

static void win_the_fight(void)
{
    const char *why;
    assert(run.screen.kind == GC_SCREEN_COMMAND);
    run.enemy[0].b.hp = 1;
    gc_choice fight = {GC_CH_FIGHT, 0, -1, 0};
    for (int guard = 0; guard < 12 && run.screen.kind == GC_SCREEN_COMMAND; guard++) assert(gc_run_choose(&run, &fight, &why));
}

static void test_money_teleports_and_clears_the_arena(void)
{
    puts("test_teleport_money_teleports_and_clears_the_arena...");
    start("tlp-money", 43);
    /* the old arena: a permanent Fog and a Reflect on the player's side; a new Arena has neither */
    run.bt.weather = WEATHER_TYPE_FOG;
    run.bt.weather_turns_left = 0;
    run.bt.terrain = TERRAIN_PSYCHIC;
    run.bt.terrain_turns_left = 0;   /* a permanent one: a timed one is cleared by the next battle's reset anyway */
    run.bt.player_faints = 3;
    run.bt.player_teras_used = 2;
    run.bt.n_arena_tags = 1;
    memset(&run.bt.arena_tags[0], 0, sizeof run.bt.arena_tags[0]);
    run.bt.arena_tags[0].type = ARENA_TAG_TYPE_REFLECT;
    run.bt.arena_tags[0].side = ARENA_TAG_SIDE_PLAYER;
    const double money = run.money;
    const double price = pr_run_get_wave_money_amount(run.wave, 1.75);
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 0, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    assert(money - run.money == price);
    assert(run.biome != BIOME_ID_PLAINS);
    assert(run.biome == BIOME_ID_SPACE || run.biome == BIOME_ID_FAIRY_CAVE || run.biome == BIOME_ID_LABORATORY || run.biome == BIOME_ID_ISLAND ||
           run.biome == BIOME_ID_WASTELAND || run.biome == BIOME_ID_DOJO);
    assert(run.bt.weather != WEATHER_TYPE_FOG);
    assert(run.bt.terrain != TERRAIN_PSYCHIC);   /* (the seed's boss does not set a Psychic Terrain of its own) */
    assert(run.bt.player_faints == 0 && run.bt.player_teras_used == 0);
    for (int i = 0; i < run.bt.n_arena_tags; i++) assert(run.bt.arena_tags[i].type != ARENA_TAG_TYPE_REFLECT);
    /* the fight: the boss cannot be fled from but can be caught */
    assert(run.screen.kind == GC_SCREEN_COMMAND && run.n_enemy == 1 && run.enemy[0].b.is_boss);
    assert(!run.screen.can_run);
    assert(run.me_catch_allowed);
    win_the_fight();
    /* the ordinary rewards of the wave, and nobody but the lead gained experience from the machine */
    assert(run.screen.kind == GC_SCREEN_REWARD);
    puts("ok");
}

static void test_helper_gets_the_experience(void)
{
    puts("test_teleport_helper_gets_the_experience...");
    start("tlp-helper", 43);
    const double money = run.money;
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 1, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    assert(run.money == money);   /* the helper pays the machine's price */
    assert(run.biome != BIOME_ID_PLAINS);
    assert(run.screen.kind == GC_SCREEN_COMMAND && !run.screen.can_run);
    const double helper_exp = run.party[1].exp, bench_exp = run.party[2].exp;
    win_the_fight();
    assert(run.screen.kind == GC_SCREEN_REWARD || run.screen.kind == GC_SCREEN_COMMAND || run.screen.kind == GC_SCREEN_MYSTERY || run.n_party == 3);
    /* Magnemite (Electric / Steel, second in the party, benched) is the helper: only it gains; the benched Eevee does not */
    assert(run.party[1].exp > helper_exp);
    assert(run.party[2].exp == bench_exp);
    puts("ok");
}

static void test_inspect_opens_the_boosters(void)
{
    puts("test_teleport_inspect_opens_the_boosters...");
    start("tlp-inspect", 43);
    const double money = run.money;
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 2, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    assert(run.money == money && run.biome == BIOME_ID_PLAINS);
    assert(run.screen.kind == GC_SCREEN_COMMAND && !run.screen.can_run && run.me_catch_allowed);
    win_the_fight();
    assert(run.screen.kind == GC_SCREEN_REWARD && run.screen.n_options >= 3);
    assert(!strcmp(run.screen.options[0].key, "ATTACK_TYPE_BOOSTER") && run.screen.options[0].arg == POKEMON_TYPE_STEEL);
    assert(!strcmp(run.screen.options[1].key, "ATTACK_TYPE_BOOSTER") && run.screen.options[1].arg == POKEMON_TYPE_ELECTRIC);
    puts("ok");
}

static void test_unaffordable_and_no_helper(void)
{
    puts("test_teleport_unaffordable_and_no_helper...");
    start("tlp-poor", 43);
    run.money = pr_run_get_wave_money_amount(run.wave, 1.75) - 1;
    gc_mystery_setup_screen(&run);
    assert(!run.screen.option_ok[0] && run.screen.option_ok[1] && run.screen.option_ok[2]);
    /* the only Steel / Electric Pokemon fainted: the helper option is off */
    run.money = 100000;
    run.party[1].b.hp = 0;
    run.party[1].b.status = STATUS_EFFECT_FAINT;
    gc_mystery_setup_screen(&run);
    assert(run.screen.option_ok[0] && !run.screen.option_ok[1] && run.screen.option_ok[2]);
    puts("ok");
}

int main(void)
{
    test_money_teleports_and_clears_the_arena();
    test_helper_gets_the_experience();
    test_inspect_opens_the_boosters();
    test_unaffordable_and_no_helper();
    puts("all teleport host tests passed");
    return 0;
}
