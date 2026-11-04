/* Dancing Lessons on the host: the parts the upstream differential (mystery_dance_diff_test) cannot compare, because its oracle does not play the battle or the
 * prompts that follow: the Oricorio really fights as a boss with its post-summon stages and text, cannot be fled from and can be caught, winning gives the Baton
 * and the ordinary shop, the lesson comes before the heal shop (with a move forgotten when the member knows four), the show gives the Oricorio the shown move
 * and a full party is asked which member to release, and the party screens refuse what the filters refuse. */
#include "../src/internal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static gc_run run;
static gc_system sys;

static const int FOUR[4] = {SPECIES_ID_BULBASAUR, SPECIES_ID_SQUIRTLE, SPECIES_ID_EEVEE, SPECIES_ID_PIKACHU};
static const int TANKS[4] = {SPECIES_ID_SNORLAX, SPECIES_ID_SNORLAX, SPECIES_ID_SNORLAX, SPECIES_ID_SNORLAX};
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
    run.mystery_type = GC_ME_DANCING_LESSONS;
    gc_rq_unshift(&run, gc_rp(RP_MYSTERY, GC_ME_DANCING_LESSONS, GC_ME_COMMON, 0));
    gc_run_until_screen(&run);
    assert(run.screen.kind == GC_SCREEN_MYSTERY);
}

static int slot_of(const gc_mon *m, int move)
{
    for (int k = 0; k < m->b.n_moves; k++) if (m->b.moves[k] == move) return k;
    return -1;
}

static void test_options_gates(void)
{
    puts("test_dance_options_gates...");
    start("dnc-gates", 43, FOUR, 4, 40, 0);
    assert(run.screen.n_options == 3 && run.screen.option_ok[0] && run.screen.option_ok[1]);
    assert(!run.screen.option_ok[2]);   /* nobody knows a dance move */
    assert(run.screen.option_pick[1] == GC_PICK_MEMBER && run.screen.option_pick[2] == GC_PICK_MEMBER_MOVE);
    /* a fainted Pokemon with a dance move does not count; a fit one does */
    gc_mon_set_move(&run.party[1], 1, MOVE_ID_SWORDS_DANCE);
    run.party[1].b.hp = 0;
    run.party[1].b.status = STATUS_EFFECT_FAINT;
    gc_mystery_setup_screen(&run);
    assert(!run.screen.option_ok[2]);
    assert(run.screen.option_ok[1] && gc_mystery_member_ok(&run, 1, 1));    /* the lesson can be given to a fainted Pokemon */
    assert(!gc_mystery_member_ok(&run, 2, 1));                              /* the show cannot be asked of one */
    run.party[1].b.hp = 5;
    run.party[1].b.status = STATUS_EFFECT_NONE;
    gc_mystery_setup_screen(&run);
    assert(run.screen.option_ok[2]);
    assert(gc_mystery_member_ok(&run, 2, 1) && !gc_mystery_member_ok(&run, 2, 0));
    /* the second list holds the dance moves only */
    assert(gc_mystery_move_ok(&run, 2, 1, 1) && !gc_mystery_move_ok(&run, 2, 1, 0) && !gc_mystery_move_ok(&run, 2, 1, 3));
    /* every dance move of the group counts, and nothing else */
    static const int dances[12] = {MOVE_ID_AQUA_STEP, MOVE_ID_CLANGOROUS_SOUL, MOVE_ID_DRAGON_DANCE, MOVE_ID_FEATHER_DANCE, MOVE_ID_FIERY_DANCE, MOVE_ID_LUNAR_DANCE,
                                   MOVE_ID_PETAL_DANCE, MOVE_ID_REVELATION_DANCE, MOVE_ID_QUIVER_DANCE, MOVE_ID_SWORDS_DANCE, MOVE_ID_TEETER_DANCE, MOVE_ID_VICTORY_DANCE};
    for (int k = 0; k < 12; k++) assert(gc_is_dance_move(dances[k]));
    assert(!gc_is_dance_move(MOVE_ID_TACKLE) && !gc_is_dance_move(0));
    puts("ok");
}

static void test_the_fight(void)
{
    puts("test_dance_the_fight...");
    start("dnc-fight", 63, TANKS, 4, 100, 0);   /* sturdy enough to survive the Revelation Dance it opens with */
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 0, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    assert(run.screen.kind == GC_SCREEN_COMMAND);
    assert(run.n_enemy == 1 && !run.double_battle);
    const gc_mon *e = &run.enemy[0];
    assert(e->b.species == SPECIES_ID_ORICORIO && e->b.is_boss && e->b.boss_segments >= 2);
    assert(slot_of(e, MOVE_ID_REVELATION_DANCE) >= 0);
    assert(!run.screen.can_run && run.me_catch_allowed);
    /* Atk, Def, SpAtk and SpDef up one stage, Spd untouched (Simple / Contrary aside) */
    const int ability = pr_get_ability(&e->b, false);
    if (ability != ABILITY_ID_SIMPLE && ability != ABILITY_ID_CONTRARY) {
        for (int s = 0; s < 4; s++) assert(e->b.stat_stages[s] == 1);
        assert(e->b.stat_stages[4] == 0);
    }
    gc_choice run_away = {GC_CH_RUN, 0, 0, 0};
    assert(!gc_run_choose(&run, &run_away, &why));
    /* winning gives the Baton in the ordinary shop */
    gc_internal *in = gc_int(&run);
    assert(in->me_reward.active && in->me_reward.func == gc_reward_item_index("BATON"));
    run.enemy[0].b.boss_segments = 0;
    run.enemy[0].b.hp = 1;
    run.enemy[0].b.stats[1] = run.enemy[0].b.stats[3] = 1;   /* it cannot hurt the lead before the win */
    gc_choice fight = {GC_CH_FIGHT, 0, -1, 0};
    for (int guard = 0; guard < 20 && run.screen.kind == GC_SCREEN_COMMAND; guard++) assert(gc_run_choose(&run, &fight, &why));
    assert(run.screen.kind == GC_SCREEN_REWARD);
    puts("ok");
}

static void test_lesson_with_room(void)
{
    puts("test_dance_lesson_with_room...");
    start("dnc-lesson", 33, FOUR, 4, 40, 0);
    assert(slot_of(&run.party[2], MOVE_ID_REVELATION_DANCE) < 0);
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 1, 2, 0};
    assert(gc_run_choose(&run, &ch, &why));
    /* the move is learned (one move known: room), then the heal shop; no battle, no catch */
    assert(slot_of(&run.party[2], MOVE_ID_REVELATION_DANCE) >= 0);
    assert(run.screen.kind == GC_SCREEN_REWARD && !run.in_battle);
    assert(run.n_party == 4);
    puts("ok");
}

static void test_lesson_asks_what_to_forget(void)
{
    puts("test_dance_lesson_forget...");
    start("dnc-forget", 33, FOUR, 4, 40, 0);
    gc_mon *m = &run.party[1];
    gc_mon_set_move(m, 1, MOVE_ID_GROWL); gc_mon_set_move(m, 2, MOVE_ID_LEER); gc_mon_set_move(m, 3, MOVE_ID_SCRATCH);
    assert(m->b.n_moves == 4);
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 1, 1, 0};
    assert(gc_run_choose(&run, &ch, &why));
    assert(run.screen.kind == GC_SCREEN_LEARN_MOVE && run.screen.move == MOVE_ID_REVELATION_DANCE);
    const int old = m->b.moves[2];
    gc_choice forget = {GC_CH_FORGET, 2, 0, 0};
    assert(gc_run_choose(&run, &forget, &why));
    assert(m->b.moves[2] == MOVE_ID_REVELATION_DANCE && old != MOVE_ID_REVELATION_DANCE);
    assert(run.screen.kind == GC_SCREEN_REWARD);   /* the heal shop after the lesson */
    puts("ok");
}

static void test_lesson_for_a_fainted_member(void)
{
    puts("test_dance_lesson_fainted...");
    start("dnc-fainted", 33, FOUR, 4, 40, 0);
    run.party[3].b.hp = 0;
    run.party[3].b.status = STATUS_EFFECT_FAINT;
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 1, 3, 0};
    assert(gc_run_choose(&run, &ch, &why));
    assert(slot_of(&run.party[3], MOVE_ID_REVELATION_DANCE) >= 0);
    puts("ok");
}

static void test_show_with_room(void)
{
    puts("test_dance_show_with_room...");
    start("dnc-show", 33, FOUR, 4, 40, MOVE_ID_PETAL_DANCE);
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 2, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    assert(run.n_party == 5);                           /* the Oricorio joined, no ball thrown, no question asked */
    const gc_mon *g = &run.party[run.party_order[4]];
    assert(g->b.species == SPECIES_ID_ORICORIO && !g->b.is_boss);
    assert(slot_of(g, MOVE_ID_REVELATION_DANCE) >= 0 && slot_of(g, MOVE_ID_PETAL_DANCE) >= 0);
    assert(g->pokeball == POKEBALL_TYPE_POKEBALL);
    assert(run.screen.kind == GC_SCREEN_REWARD);        /* the heal shop */
    gc_event e;
    int obtained = 0;
    while (gc_run_next_event(&run, &e)) if (e.kind == GC_EV_MESSAGE && e.msg.key && !strcmp(e.msg.key, "battle:pokemonObtained")) obtained++;
    assert(obtained == 0);   /* catchPokemon(.., false): no "obtained" text */
    puts("ok");
}

static void test_show_with_a_full_party(void)
{
    puts("test_dance_show_full_party...");
    start("dnc-full", 33, SIX, 6, 40, MOVE_ID_SWORDS_DANCE);
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 2, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    assert(run.screen.kind == GC_SCREEN_SWITCH && run.screen.can_skip && run.n_party == 6);
    const uint32_t kept = run.party[3].b.id;
    gc_choice release = {GC_CH_PICK, 2, 0, 0};
    assert(gc_run_choose(&run, &release, &why));
    assert(run.n_party == 6 && run.party[2].b.species == SPECIES_ID_ORICORIO && run.party[3].b.id == kept);
    assert(slot_of(&run.party[2], MOVE_ID_SWORDS_DANCE) >= 0);
    assert(run.screen.kind == GC_SCREEN_REWARD);
    puts("ok");
}

static void test_refused_picks(void)
{
    puts("test_dance_refused_picks...");
    start("dnc-refuse", 33, FOUR, 4, 40, MOVE_ID_SWORDS_DANCE);
    const char *why;
    gc_choice no_dance = {GC_CH_MYSTERY, 2, 1, 0};     /* Squirtle knows no dance move */
    assert(!gc_run_choose(&run, &no_dance, &why));
    gc_choice not_a_dance = {GC_CH_MYSTERY, 2, 0, 1};  /* a slot that holds no move */
    assert(!gc_run_choose(&run, &not_a_dance, &why));
    /* a fainted Pokemon that knows a dance move cannot be shown to the Oricorio (its lesson would be allowed) */
    gc_mon_set_move(&run.party[2], 1, MOVE_ID_PETAL_DANCE);
    run.party[2].b.hp = 0;
    run.party[2].b.status = STATUS_EFFECT_FAINT;
    gc_choice fainted_show = {GC_CH_MYSTERY, 2, 2, 1};
    assert(!gc_run_choose(&run, &fainted_show, &why));
    gc_choice no_member = {GC_CH_MYSTERY, 1, 9, 0};
    assert(!gc_run_choose(&run, &no_member, &why));
    assert(run.screen.kind == GC_SCREEN_MYSTERY && run.n_party == 4);
    puts("ok");
}

/* a roll run-core cannot make (here: nobody in the party to read the level from): nobody dances, the encounter ends as if declined, and says so */
static void test_unported_roll_ends_the_encounter(void)
{
    puts("test_dance_unported_roll...");
    start("dnc-unported", 12, FOUR, 4, 30, 0);
    run.n_party = 0;
    const unsigned before = run.stubs[GC_STUB_UNPORTED_ENEMY];
    gc_dance_execute(&run, 0, 0, 0);
    assert(run.stubs[GC_STUB_UNPORTED_ENEMY] == before + 1);
    puts("ok");
}

int main(void)
{
    test_options_gates();
    test_the_fight();
    test_lesson_with_room();
    test_lesson_asks_what_to_forget();
    test_lesson_for_a_fainted_member();
    test_show_with_room();
    test_show_with_a_full_party();
    test_refused_picks();
    test_unported_roll_ends_the_encounter();
    puts("all dance host tests passed");
    return 0;
}
