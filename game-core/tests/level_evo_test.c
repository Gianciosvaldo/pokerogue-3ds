#include "../src/internal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_single_level_up_and_move_learn(void)
{
    printf("test_single_level_up_and_move_learn...\n");
    gc_run run;
    gc_run_new(&run, GAME_MODES_CLASSIC, "leveltest1", 1001);
    gc_starter team[1];
    memset(&team[0], 0, sizeof team[0]);
    team[0].species = SPECIES_ID_BULBASAUR;
    team[0].tera_type = -1;
    for (int i = 0; i < 6; i++) team[0].ivs[i] = 15;

    const char *why;
    bool ok = gc_run_set_starters(&run, team, 1, &why);
    assert(ok);

    run.wave = 10;
    run.in_battle = true;

    gc_mon *m = &run.party[0];
    const pr_species *sp = gc_species(SPECIES_ID_BULBASAUR);
    int rate = sp->growth_rate;

    /* Level up to 7 (where Leech Seed is learned) */
    double exp_lv7 = gc_level_total_exp(7, rate);
    double req_exp = exp_lv7 - m->exp + 10;

    gc_rphase p = gc_rp(RP_EXP, 0, 1, 0);
    p.d = req_exp;
    gc_rq_unshift(&run, p);

    gc_internal *in = gc_int(&run);
    in->rq.waiting = false;

    gc_run_until_screen(&run);

    assert(m->b.level == 7);
    assert(m->b.n_moves == 4); /* Tackle, Growl, Vine Whip, Leech Seed */

    gc_event ev;
    bool got_exp = false, got_lvl = false, got_move = false;
    while (gc_run_next_event(&run, &ev)) {
        if (ev.kind == GC_EV_EXP) got_exp = true;
        if (ev.kind == GC_EV_LEVEL_UP) got_lvl = true;
        if (ev.kind == GC_EV_MOVE_LEARNED) {
            got_move = true;
            assert(ev.c == MOVE_ID_LEECH_SEED);
        }
    }
    assert(got_exp);
    assert(got_lvl);
    assert(got_move);
    printf("ok\n");
}

static void test_learn_move_replace(void)
{
    printf("test_learn_move_replace...\n");
    gc_run run;
    gc_run_new(&run, GAME_MODES_CLASSIC, "leveltest2", 1002);
    gc_starter team[1];
    memset(&team[0], 0, sizeof team[0]);
    team[0].species = SPECIES_ID_BULBASAUR;
    team[0].tera_type = -1;
    for (int i = 0; i < 6; i++) team[0].ivs[i] = 15;

    const char *why;
    bool ok = gc_run_set_starters(&run, team, 1, &why);
    assert(ok);

    run.wave = 15;
    run.in_battle = true;

    gc_mon *m = &run.party[0];
    const pr_species *sp = gc_species(SPECIES_ID_BULBASAUR);
    int rate = sp->growth_rate;

    /* Level up to 10 (gains Leech Seed at 7 and Growth at 10) */
    double exp_lv10 = gc_level_total_exp(10, rate);
    double req_exp = exp_lv10 - m->exp + 10;

    gc_rphase p = gc_rp(RP_EXP, 0, 1, 0);
    p.d = req_exp;
    gc_rq_unshift(&run, p);

    gc_internal *in = gc_int(&run);
    in->rq.waiting = false;

    gc_run_until_screen(&run);

    /* Stops at GC_SCREEN_LEARN_MOVE for Growth */
    const gc_screen *s = gc_run_screen(&run);
    assert(s->kind == GC_SCREEN_LEARN_MOVE);
    assert(s->party == 0);
    assert(s->move == MOVE_ID_GROWTH);

    /* Choose slot 0 (forget Tackle) */
    gc_choice c = {GC_CH_FORGET, 0, 0, 0};
    ok = gc_run_choose(&run, &c, &why);
    assert(ok);

    assert(m->b.moves[0] == MOVE_ID_GROWTH);
    printf("ok\n");
}

static void test_evolution_flow(void)
{
    printf("test_evolution_flow...\n");
    gc_run run;
    gc_run_new(&run, GAME_MODES_CLASSIC, "leveltest3", 1003);
    gc_starter team[1];
    memset(&team[0], 0, sizeof team[0]);
    team[0].species = SPECIES_ID_BULBASAUR;
    team[0].tera_type = -1;
    for (int i = 0; i < 6; i++) team[0].ivs[i] = 15;

    const char *why;
    bool ok = gc_run_set_starters(&run, team, 1, &why);
    assert(ok);

    run.wave = 30;
    run.in_battle = true;

    gc_mon *m = &run.party[0];
    m->b.level = 15;
    const pr_species *sp = gc_species(SPECIES_ID_BULBASAUR);
    m->exp = pr_run_get_level_total_exp(15, sp->growth_rate);

    /* Add exp to reach level 16 */
    double req_exp = pr_run_get_level_total_exp(16, sp->growth_rate) - m->exp + 10;
    gc_rphase p = gc_rp(RP_EXP, 0, 1, 0);
    p.d = req_exp;
    gc_rq_unshift(&run, p);

    gc_internal *in = gc_int(&run);
    in->rq.waiting = false;

    gc_run_until_screen(&run);

    /* Stops at GC_SCREEN_EVOLVE */
    const gc_screen *s = gc_run_screen(&run);
    assert(s->kind == GC_SCREEN_EVOLVE);
    assert(s->party == 0);
    assert(s->species == SPECIES_ID_IVYSAUR);

    /* Accept evolution */
    gc_choice c = {GC_CH_EVOLVE, 0, 0, 0};
    ok = gc_run_choose(&run, &c, &why);
    assert(ok);

    assert(m->b.species == SPECIES_ID_IVYSAUR);

    gc_event ev;
    bool got_evolved = false;
    while (gc_run_next_event(&run, &ev)) {
        if (ev.kind == GC_EV_EVOLVED) {
            got_evolved = true;
            assert(ev.a == 0);
            assert(ev.b == SPECIES_ID_BULBASAUR);
            assert(ev.c == SPECIES_ID_IVYSAUR);
        }
    }
    assert(got_evolved);
    printf("ok\n");
}

static void test_pause_evolution(void)
{
    printf("test_pause_evolution...\n");
    gc_run run;
    gc_run_new(&run, GAME_MODES_CLASSIC, "leveltest4", 1004);
    gc_starter team[1];
    memset(&team[0], 0, sizeof team[0]);
    team[0].species = SPECIES_ID_BULBASAUR;
    team[0].tera_type = -1;
    for (int i = 0; i < 6; i++) team[0].ivs[i] = 15;

    const char *why;
    bool ok = gc_run_set_starters(&run, team, 1, &why);
    assert(ok);

    run.wave = 30;
    run.in_battle = true;

    gc_mon *m = &run.party[0];
    m->b.level = 15;
    const pr_species *sp = gc_species(SPECIES_ID_BULBASAUR);
    m->exp = pr_run_get_level_total_exp(15, sp->growth_rate);

    /* Level up to 16 */
    double req_exp = pr_run_get_level_total_exp(16, sp->growth_rate) - m->exp + 10;
    gc_rphase p = gc_rp(RP_EXP, 0, 1, 0);
    p.d = req_exp;
    gc_rq_unshift(&run, p);

    gc_internal *in = gc_int(&run);
    in->rq.waiting = false;

    gc_run_until_screen(&run);

    assert(gc_run_screen(&run)->kind == GC_SCREEN_EVOLVE);

    /* Cancel and pause evolutions (c.a = 1) */
    gc_choice c = {GC_CH_CANCEL, 1, 0, 0};
    ok = gc_run_choose(&run, &c, &why);
    assert(ok);

    assert(m->b.species == SPECIES_ID_BULBASAUR);
    assert(m->pause_evolutions == true);

    /* Level up to 17 */
    req_exp = pr_run_get_level_total_exp(17, sp->growth_rate) - m->exp + 10;
    p = gc_rp(RP_EXP, 0, 1, 0);
    p.d = req_exp;
    gc_rq_unshift(&run, p);

    in->rq.waiting = false;
    gc_run_until_screen(&run);

    /* Should NOT prompt for evolution again */
    assert(gc_run_screen(&run)->kind != GC_SCREEN_EVOLVE);
    assert(m->b.level == 17);
    assert(m->b.species == SPECIES_ID_BULBASAUR);
    printf("ok\n");
}

static void test_multi_party_exp_queue(void)
{
    printf("test_multi_party_exp_queue...\n");
    gc_run run;
    gc_run_new(&run, GAME_MODES_CLASSIC, "leveltest5", 1005);
    gc_starter team[2];
    memset(team, 0, sizeof team);
    team[0].species = SPECIES_ID_BULBASAUR;
    team[0].tera_type = -1;
    team[1].species = SPECIES_ID_CHARMANDER;
    team[1].tera_type = -1;
    for (int k = 0; k < 2; k++)
        for (int i = 0; i < 6; i++) team[k].ivs[i] = 15;

    const char *why;
    bool ok = gc_run_set_starters(&run, team, 2, &why);
    assert(ok);

    run.wave = 10;
    run.in_battle = true;

    /* Unshift EXP for party 0 then party 1 */
    gc_rphase p0 = gc_rp(RP_EXP, 0, 1, 0);
    p0.d = 200;
    gc_rphase p1 = gc_rp(RP_EXP, 1, 1, 0);
    p1.d = 200;

    gc_rq_unshift(&run, p0);
    gc_rq_unshift(&run, p1);

    gc_internal *in = gc_int(&run);
    in->rq.waiting = false;

    gc_run_until_screen(&run);

    assert(run.party[0].b.level > 5);
    assert(run.party[1].b.level > 5);
    printf("ok\n");
}

int main(void)
{
    test_single_level_up_and_move_learn();
    test_learn_move_replace();
    test_evolution_flow();
    test_pause_evolution();
    test_multi_party_exp_queue();
    printf("All level/evolution queue tests passed successfully!\n");
    return 0;
}
