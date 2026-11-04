/* Uncommon Breed on the host: the parts the upstream differential (mystery_breed_diff_test) cannot compare, because its oracle does not play the battle or
 * the party-full prompt: the Pokemon really fights as a plain wild enemy (no bars, no fleeing, catchable) after its post-summon boost, the egg move starts the
 * battle, winning opens the ordinary shop, a full party is asked which member to release before the Pokemon joins (food and charm), and the options are on
 * only with four berry stacks (the Berry items alone) or a charming move. */
#include "../src/internal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static gc_run run;
static gc_system sys;

static const int FOUR[4] = {SPECIES_ID_BULBASAUR, SPECIES_ID_SQUIRTLE, SPECIES_ID_EEVEE, SPECIES_ID_PIKACHU};
static const int SIX[6] = {SPECIES_ID_BULBASAUR, SPECIES_ID_SQUIRTLE, SPECIES_ID_EEVEE, SPECIES_ID_PIKACHU, SPECIES_ID_RATTATA, SPECIES_ID_SNORLAX};

static void start(const char *seed, int wave, const int *species, int n, int level, int move0)
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
        s.moves[0] = i == 0 && move0 ? move0 : MOVE_ID_TACKLE;
        gc_mon_new_player(&run, &run.party[i], &s, level);
        run.party[i].pause_evolutions = true;
        run.party_order[i] = (int8_t)i;
    }
    gc_items_to_battle(&run);
    run.battle_type = BATTLE_TYPE_WILD;
    gc_rq_reset(&gc_int(&run)->rq);
    run.mystery_type = GC_ME_UNCOMMON_BREED;
    gc_rq_unshift(&run, gc_rp(RP_MYSTERY, GC_ME_UNCOMMON_BREED, GC_ME_COMMON, 0));
    gc_run_until_screen(&run);
    assert(run.screen.kind == GC_SCREEN_MYSTERY);
}

static void give_berries(int stacks)
{
    for (int k = 0; k < stacks; k++) gc_item_add(&run, "BERRY", (int16_t)(k % 4), run.party[0].b.id, 1);
    gc_items_to_battle(&run);
    gc_mystery_setup_screen(&run);
}

static int berry_stacks(void)
{
    int n = 0;
    for (int i = 0; i < run.n_items; i++) if (!strcmp(run.items[i].key, "BERRY")) n += run.items[i].stack;
    return n;
}

static void test_options_gates(void)
{
    puts("test_breed_options_gates...");
    start("brd-gates", 43, FOUR, 4, 40, 0);
    assert(run.screen.n_options == 3 && run.screen.option_ok[0]);
    assert(!run.screen.option_ok[1] && !run.screen.option_ok[2]);   /* no berries, no charming move */
    /* three stacks are too few, four are enough; a Leftovers is no berry */
    gc_item_add(&run, "LEFTOVERS", -1, run.party[0].b.id, 1);
    gc_item_add(&run, "BERRY_POUCH", -1, 0, 3);   /* nor is the Berry Pouch (PreserveBerryModifier, not a BerryModifier) */
    give_berries(3);
    gc_mystery_setup_screen(&run);
    assert(!run.screen.option_ok[1]);
    give_berries(1);
    gc_mystery_setup_screen(&run);
    assert(run.screen.option_ok[1]);
    /* a fainted Pokemon with a charming move does not count; a fit one does */
    gc_mon_set_move(&run.party[1], 1, MOVE_ID_CHARM);
    run.party[1].b.hp = 0;
    run.party[1].b.status = STATUS_EFFECT_FAINT;
    gc_mystery_setup_screen(&run);
    assert(!run.screen.option_ok[2]);
    run.party[1].b.hp = 5;
    run.party[1].b.status = STATUS_EFFECT_NONE;
    gc_mystery_setup_screen(&run);
    assert(run.screen.option_ok[2]);
    puts("ok");
}

static void test_the_fight(void)
{
    puts("test_breed_the_fight...");
    start("brd-fight", 43, FOUR, 4, 40, 0);
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 0, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    assert(run.screen.kind == GC_SCREEN_COMMAND);
    assert(run.n_enemy == 1 && !run.double_battle);
    const gc_mon *e = &run.enemy[0];
    assert(!e->b.is_boss && e->b.boss_segments == 0);
    assert(e->b.level == 38);   /* the party's highest level - 2 */
    assert(!run.screen.can_run && run.me_catch_allowed);
    /* below wave 50: Def, SpDef and Spd up (a status egg move may add to its own stages, nothing takes them away) */
    assert(e->b.stat_stages[STAT_DEF - 1] >= 1 && e->b.stat_stages[STAT_SPDEF - 1] >= 1 && e->b.stat_stages[STAT_SPD - 1] >= 1);
    assert(gc_int(&run)->n_me_start == 0);   /* the egg move's start effect has run */
    gc_choice run_away = {GC_CH_RUN, 0, 0, 0};
    assert(!gc_run_choose(&run, &run_away, &why));
    /* winning gives the ordinary shop */
    run.enemy[0].b.hp = 1;
    run.enemy[0].b.stats[1] = run.enemy[0].b.stats[3] = 1;   /* it cannot hurt the lead before the win */
    gc_choice fight = {GC_CH_FIGHT, 0, -1, 0};
    for (int guard = 0; guard < 20 && run.screen.kind == GC_SCREEN_COMMAND; guard++) assert(gc_run_choose(&run, &fight, &why));
    assert(run.screen.kind == GC_SCREEN_REWARD);
    puts("ok");
}

static void test_the_fight_high_waves(void)
{
    puts("test_breed_the_fight_high_waves...");
    start("brd-high", 83, FOUR, 4, 60, 0);
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 0, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    const gc_mon *e = &run.enemy[0];
    /* from wave 50: Atk, Def, SpAtk, SpDef and Spd up one stage each (Simple / Contrary / a self-targeted egg move aside) */
    const int ability = pr_get_ability(&e->b, false);
    if (ability != ABILITY_ID_SIMPLE && ability != ABILITY_ID_CONTRARY) {
        for (int s = 0; s < 5; s++) assert(e->b.stat_stages[s] >= 1);
    }
    puts("ok");
}

static void test_food_with_room(void)
{
    puts("test_breed_food_with_room...");
    start("brd-food", 33, FOUR, 4, 40, 0);
    give_berries(6);
    assert(berry_stacks() == 6);
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 1, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    assert(berry_stacks() == 2);                       /* four stacks left the party */
    assert(run.n_party == 5);                          /* the Pokemon joined, no ball thrown, no question asked */
    const gc_mon *g = &run.party[run.party_order[4]];
    assert(!g->b.is_boss && g->b.level == 38 && g->b.n_moves == 4);
    assert(g->pokeball == POKEBALL_TYPE_POKEBALL);
    assert(run.screen.kind == GC_SCREEN_REWARD);       /* the ordinary shop */
    /* no "obtained" text: catchPokemon(.., false) */
    gc_event e;
    int obtained = 0;
    while (gc_run_next_event(&run, &e)) if (e.kind == GC_EV_MESSAGE && e.msg.key && !strcmp(e.msg.key, "battle:pokemonObtained")) obtained++;
    assert(obtained == 0);
    puts("ok");
}

static void test_charm_with_a_full_party(void)
{
    puts("test_breed_charm_with_a_full_party...");
    start("brd-charm", 33, SIX, 6, 40, MOVE_ID_CHARM);
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 2, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    /* six in the party: which member is released? */
    assert(run.screen.kind == GC_SCREEN_SWITCH && run.screen.can_skip && run.n_party == 6);
    const uint32_t kept = run.party[3].b.id;
    gc_choice release = {GC_CH_PICK, 2, 0, 0};
    assert(gc_run_choose(&run, &release, &why));
    assert(run.n_party == 6);
    assert(run.party[2].b.id != 0 && run.party[2].b.level == 38 && !run.party[2].b.is_boss);   /* the new one took the released place */
    assert(run.party[3].b.id == kept);
    assert(run.screen.kind == GC_SCREEN_REWARD);
    puts("ok");
}

static void test_charm_let_it_go(void)
{
    puts("test_breed_charm_let_it_go...");
    start("brd-letgo", 33, SIX, 6, 40, MOVE_ID_CHARM);
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 2, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    assert(run.screen.kind == GC_SCREEN_SWITCH);
    const uint32_t first = run.party[0].b.id, last = run.party[5].b.id;
    gc_choice no = {GC_CH_PICK, -1, 0, 0};
    assert(gc_run_choose(&run, &no, &why));
    assert(run.n_party == 6 && run.party[0].b.id == first && run.party[5].b.id == last);
    puts("ok");
}

/* a roll run-core cannot make (here: nobody in the party to read the level from): nobody is there, the encounter ends as if declined, and says so */
static void test_unported_roll_ends_the_encounter(void)
{
    printf("test_breed_unported_roll...\n");
    start("breed-unported", 12, FOUR, 4, 30, 0);
    run.n_party = 0;
    const unsigned before = run.stubs[GC_STUB_UNPORTED_ENEMY];
    gc_breed_execute(&run, 0);
    assert(run.stubs[GC_STUB_UNPORTED_ENEMY] == before + 1);
    puts("ok");
}

int main(void)
{
    test_options_gates();
    test_the_fight();
    test_the_fight_high_waves();
    test_food_with_room();
    test_charm_with_a_full_party();
    test_charm_let_it_go();
    test_unported_roll_ends_the_encounter();
    puts("all breed host tests passed");
    return 0;
}
