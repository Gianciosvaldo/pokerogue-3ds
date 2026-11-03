/* Fiery Fallout on the host: the parts the upstream differential (mystery_fiery_diff_test) cannot compare, because its oracle does not play the
 * battle: the two Volcarona really fight as a double battle (they use Fire Spin before the first command, each raising Sp. Def and Spd as they
 * come out), the player cannot flee but can catch, a win gives the lead a booster and opens the ordinary shop, a lone Pokemon still gets the
 * double battle, hunkering down hurts the Pokemon that are no Fire type and leaves with the heal shop, and the calm option is on only for a Fire
 * type that is not fainted or a fire resistant ability. */
#include "../src/internal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static gc_run run;
static gc_system sys;

static void start(const char *seed, int wave, const int *species, int n, int level)
{
    memset(&run, 0, sizeof run);
    gc_run_new(&run, GAME_MODES_CLASSIC, seed, 7);
    gc_system_init_defaults(&sys);
    run.sys = &sys;
    run.wave = wave;
    run.biome = BIOME_ID_VOLCANO;
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
        run.party_order[i] = (int8_t)i;
    }
    gc_items_to_battle(&run);
    run.battle_type = BATTLE_TYPE_WILD;
    gc_rq_reset(&gc_int(&run)->rq);
    run.mystery_type = GC_ME_FIERY_FALLOUT;
    gc_rq_unshift(&run, gc_rp(RP_MYSTERY, GC_ME_FIERY_FALLOUT, GC_ME_COMMON, 0));
    gc_run_until_screen(&run);
    assert(run.screen.kind == GC_SCREEN_MYSTERY);
}

static int message_count(const char *key)
{
    gc_event e;
    int n = 0;
    while (gc_run_next_event(&run, &e)) if (e.kind == GC_EV_MESSAGE && e.msg.key && !strcmp(e.msg.key, key)) n++;
    return n;
}

static const int THREE[3] = {SPECIES_ID_BULBASAUR, SPECIES_ID_SQUIRTLE, SPECIES_ID_EEVEE};

static void test_double_battle_and_fire_spin(void)
{
    puts("test_fiery_double_battle_and_fire_spin...");
    start("fie-fight", 45, THREE, 3, 60);
    run.ask_switch = true;
    assert(run.bt.weather == WEATHER_TYPE_SUNNY && run.bt.weather_turns_left == 0);   /* onInit: Sunny, with no turn count */
    assert(run.screen.n_options == 3 && run.screen.option_ok[0] && run.screen.option_ok[1] && !run.screen.option_ok[2]);
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 0, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    assert(run.screen.kind == GC_SCREEN_COMMAND);
    assert(run.double_battle && run.n_enemy == 2);
    for (int i = 0; i < 2; i++) {
        const gc_mon *e = &run.enemy[i];
        assert(e->b.species == SPECIES_ID_VOLCARONA && !e->b.is_boss && e->b.boss_segments == 0);
        assert(e->b.gender == i);                         /* the first male, the second female */
        assert(e->b.stat_stages[STAT_SPDEF - 1] == 1 && e->b.stat_stages[STAT_SPD - 1] == 1);
        assert(e->b.stat_stages[STAT_ATK - 1] == 0);
    }
    /* Fire Spin from each at the Pokemon opposite, before the first command: someone was hurt, and no start effect is left to run */
    const gc_mon *a = &run.party[run.party_order[0]], *b = &run.party[run.party_order[1]];
    assert(a->b.hp < a->b.stats[0] || b->b.hp < b->b.stats[0]);
    assert(gc_int(&run)->n_me_start == 0);
    assert(!run.screen.can_run);
    assert(run.me_catch_allowed);
    assert(!run.eng.awaiting_confirm);   /* the player is not asked whether to switch (ask_switch was on) */
    gc_choice run_away = {GC_CH_RUN, 0, 0, 0};
    assert(!gc_run_choose(&run, &run_away, &why));
    puts("ok");
}

static void test_win_gives_the_lead_a_booster(void)
{
    puts("test_fiery_win_gives_the_lead_a_booster...");
    start("fie-win", 45, THREE, 3, 60);
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 0, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    assert(run.screen.kind == GC_SCREEN_COMMAND);
    const uint32_t lead = run.party[run.party_order[0]].b.id;
    int boosters = 0;
    for (int i = 0; i < run.n_items; i++) boosters += !strcmp(run.items[i].key, "ATTACK_TYPE_BOOSTER") && run.items[i].pokemon_id == lead;
    assert(boosters == 0);
    run.enemy[0].b.hp = 1;
    run.enemy[1].b.hp = 1;
    bool seen = false;
    for (int guard = 0; guard < 40 && (run.screen.kind == GC_SCREEN_COMMAND || run.screen.kind == GC_SCREEN_TARGET); guard++) {
        /* Tackle at whichever Volcarona still stands (both are down to 1 HP) */
        gc_choice fight = {run.screen.kind == GC_SCREEN_TARGET ? GC_CH_TARGET : GC_CH_FIGHT, 0, -1, 0};
        const int target = run.enemy[0].b.hp > 0 ? 2 : 3;
        if (fight.kind == GC_CH_TARGET) fight.a = (int16_t)target; else fight.b = (int16_t)target;
        assert(gc_run_choose(&run, &fight, &why));
        seen = seen || message_count("mysteryEncounters/fieryFallout:foundItem") > 0;
    }
    assert(run.screen.kind == GC_SCREEN_REWARD);
    for (int i = 0; i < run.n_items; i++) boosters += !strcmp(run.items[i].key, "ATTACK_TYPE_BOOSTER") && run.items[i].pokemon_id == lead;
    assert(boosters == 1);
    assert(seen || boosters == 1);
    puts("ok");
}

static void test_lone_pokemon(void)
{
    puts("test_fiery_lone_pokemon...");
    static const int one[1] = {SPECIES_ID_EEVEE};
    start("fie-lone", 45, one, 1, 60);
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 0, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    assert(run.screen.kind == GC_SCREEN_COMMAND && run.n_enemy == 2 && run.double_battle);
    puts("ok");
}

static void test_hunker_down(void)
{
    puts("test_fiery_hunker_down...");
    static const int mix[5] = {SPECIES_ID_BULBASAUR, SPECIES_ID_CHARMANDER, SPECIES_ID_SQUIRTLE, SPECIES_ID_EEVEE, SPECIES_ID_PIKACHU};
    start("fie-hunker", 45, mix, 5, 50);
    run.party[3].b.hp = 0;                      /* fainted: left alone */
    run.party[4].b.hp = 1;                      /* would faint: left at 1 HP */
    const int hp[5] = {run.party[0].b.stats[0], run.party[1].b.stats[0], run.party[2].b.stats[0], run.party[3].b.hp, run.party[4].b.hp};
    int max[5];
    for (int i = 0; i < 5; i++) max[i] = run.party[i].b.stats[0];
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 1, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    assert(run.party[0].b.hp == hp[0] - (int)(max[0] * 0.2));
    assert(run.party[1].b.hp == hp[1]);         /* a Fire type is spared */
    assert(run.party[2].b.hp == hp[2] - (int)(max[2] * 0.2));
    assert(run.party[3].b.hp == 0);
    assert(run.party[4].b.hp == 1);
    int burned = 0;
    for (int i = 0; i < 5; i++) {
        if (run.party[i].b.status != STATUS_EFFECT_BURN) continue;
        burned++;
        assert(i != 1 && i != 3);
        assert(pr_get_ability(&run.party[i].b, false) == ABILITY_ID_HEATPROOF);
    }
    assert(burned == 1);
    /* the heal shop, not the next wave, comes after it */
    assert(run.screen.kind == GC_SCREEN_REWARD);
    puts("ok");
}

static void test_hunker_down_nobody_to_burn(void)
{
    puts("test_fiery_hunker_down_nobody_to_burn...");
    static const int fire[2] = {SPECIES_ID_CHARMANDER, SPECIES_ID_VULPIX};
    start("fie-fire", 45, fire, 2, 50);
    const int hp0 = run.party[0].b.hp, hp1 = run.party[1].b.hp;
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 1, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    assert(run.party[0].b.hp == hp0 && run.party[1].b.hp == hp1);
    assert(run.party[0].b.status == STATUS_EFFECT_NONE && run.party[1].b.status == STATUS_EFFECT_NONE);
    puts("ok");
}

static void test_calm_option_requirements(void)
{
    puts("test_fiery_calm_option_requirements...");
    start("fie-calm", 45, THREE, 3, 50);
    assert(!run.screen.option_ok[2]);
    /* a Fire type that is fainted does not open it */
    run.party[1].b.hp = 0;
    run.party[1].b.species = SPECIES_ID_CHARMANDER;
    gc_mystery_setup_screen(&run);
    assert(!run.screen.option_ok[2]);
    /* ...but a fire resistant ability on a Pokemon that can fight does, and the primary is the first Fire type even if fainted
     * (the first of the whole party), else the first with the ability */
    run.party[2].b.custom_ability = ABILITY_ID_FLASH_FIRE;
    gc_mystery_setup_screen(&run);
    assert(run.screen.option_ok[2]);
    assert(gc_fiery_primary(&run) == 1);        /* the fainted Charmander wins over the Flash Fire Eevee */
    run.party[1].b.species = SPECIES_ID_SQUIRTLE;
    gc_mystery_setup_screen(&run);
    assert(gc_fiery_primary(&run) == 2);
    /* an unfainted Fire type opens it by itself */
    run.party[2].b.custom_ability = -1;
    run.party[0].b.species = SPECIES_ID_CHARMANDER;
    gc_mystery_setup_screen(&run);
    assert(run.screen.option_ok[2] && gc_fiery_primary(&run) == 0);
    puts("ok");
}

static int booster_stack(uint32_t holder)
{
    int n = 0;
    for (int i = 0; i < run.n_items; i++) if (!strcmp(run.items[i].key, "ATTACK_TYPE_BOOSTER") && run.items[i].pokemon_id == holder) n += run.items[i].stack;
    return n;
}

/* a Pokemon at the maximum stack (99) of the drawn type is left as it is; the text still comes; the stack of another Pokemon does not count */
static void test_booster_at_maximum_stack(void)
{
    puts("test_fiery_booster_at_maximum_stack...");
    static const int calm[3] = {SPECIES_ID_BULBASAUR, SPECIES_ID_CHARMANDER, SPECIES_ID_EEVEE};
    start("fie-max", 45, calm, 3, 30);
    const uint32_t lead = run.party[run.party_order[0]].b.id, other = run.party[run.party_order[1]].b.id;
    for (int t = 0; t < 18; t++) gc_item_add(&run, "ATTACK_TYPE_BOOSTER", (int16_t)t, lead, 99);
    gc_items_to_battle(&run);
    assert(booster_stack(lead) == 18 * 99);
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 2, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    assert(booster_stack(lead) == 18 * 99);
    assert(message_count("mysteryEncounters/fieryFallout:foundItem") == 1);
    puts("ok");

    puts("test_fiery_booster_of_another_pokemon_does_not_count...");
    start("fie-max2", 45, calm, 3, 30);
    const uint32_t lead2 = run.party[run.party_order[0]].b.id, other2 = run.party[run.party_order[1]].b.id;
    for (int t = 0; t < 18; t++) gc_item_add(&run, "ATTACK_TYPE_BOOSTER", (int16_t)t, other2, 99);
    gc_items_to_battle(&run);
    assert(gc_run_choose(&run, &ch, &why));
    assert(booster_stack(lead2) == 1 && booster_stack(other2) == 18 * 99);
    (void)other;
    puts("ok");
}

static void test_calm_gives_experience_booster_and_shop(void)
{
    puts("test_fiery_calm_gives_experience_booster_and_shop...");
    static const int calm[3] = {SPECIES_ID_BULBASAUR, SPECIES_ID_CHARMANDER, SPECIES_ID_EEVEE};
    start("fie-calm2", 45, calm, 3, 30);
    assert(run.screen.option_ok[2]);
    const uint32_t lead = run.party[run.party_order[0]].b.id;
    const double exp_fire = run.party[1].exp, exp_other = run.party[2].exp;
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 2, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    int boosters = 0;
    for (int i = 0; i < run.n_items; i++) boosters += !strcmp(run.items[i].key, "ATTACK_TYPE_BOOSTER") && run.items[i].pokemon_id == lead;
    assert(boosters == 1);
    assert(run.party[1].exp > exp_fire);        /* Charmander, the primary Pokemon, gained experience */
    assert(run.party[2].exp == exp_other);      /* nobody else, without an Exp. Share */
    assert(run.n_enemy == 0);
    puts("ok");
}

static void test_sunny_weather_on_init(void)
{
    puts("test_fiery_sunny_weather_on_init...");
    /* a primal weather stays and is only said to continue */
    memset(&run, 0, sizeof run);
    start("fie-sun", 45, THREE, 3, 30);
    assert(run.bt.weather == WEATHER_TYPE_SUNNY);
    run.bt.weather = WEATHER_TYPE_HEAVY_RAIN;
    run.bt.weather_turns_left = 0;
    { gc_event e; while (gc_run_next_event(&run, &e)) {} }
    gc_fiery_init(&run);
    assert(run.bt.weather == WEATHER_TYPE_HEAVY_RAIN);
    assert(message_count(pr_weather_key(WEATHER_TYPE_HEAVY_RAIN, PR_WEATHER_CONTINUE)) == 1);
    /* Rain gives way to Sunny at once, and again does nothing */
    run.bt.weather = WEATHER_TYPE_RAIN;
    run.bt.weather_turns_left = 3;
    gc_fiery_init(&run);
    assert(run.bt.weather == WEATHER_TYPE_SUNNY && run.bt.weather_turns_left == 0);
    assert(message_count(pr_weather_key(WEATHER_TYPE_SUNNY, PR_WEATHER_START)) == 1);
    gc_fiery_init(&run);
    assert(message_count(pr_weather_key(WEATHER_TYPE_SUNNY, PR_WEATHER_START)) == 0);
    puts("ok");
}

int main(void)
{
    test_double_battle_and_fire_spin();
    test_win_gives_the_lead_a_booster();
    test_lone_pokemon();
    test_hunker_down();
    test_hunker_down_nobody_to_burn();
    test_calm_option_requirements();
    test_calm_gives_experience_booster_and_shop();
    test_booster_at_maximum_stack();
    test_sunny_weather_on_init();
    puts("all fiery host tests passed");
    return 0;
}
