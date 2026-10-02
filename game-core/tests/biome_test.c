#include "../src/internal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_is_new_biome_function(void)
{
    printf("test_is_new_biome_function...\n");
    gc_run run;
    gc_run_new(&run, GAME_MODES_CLASSIC, "biometest0", 2000);

    assert(gc_is_new_biome(&run, 10) == true);
    assert(gc_is_new_biome(&run, 20) == true);
    assert(gc_is_new_biome(&run, 50) == true);

    assert(gc_is_new_biome(&run, 1) == false);
    assert(gc_is_new_biome(&run, 9) == false);
    assert(gc_is_new_biome(&run, 11) == false);
    printf("ok\n");
}

static void test_biome_transition_without_map(void)
{
    printf("test_biome_transition_without_map...\n");
    gc_run run;
    gc_run_new(&run, GAME_MODES_CLASSIC, "biometest1", 2001);
    gc_starter team[1];
    memset(&team[0], 0, sizeof team[0]);
    team[0].species = SPECIES_ID_BULBASAUR;
    team[0].tera_type = -1;
    for (int i = 0; i < 6; i++) team[0].ivs[i] = 15;

    const char *why;
    bool ok = gc_run_set_starters(&run, team, 1, &why);
    assert(ok);

    run.wave = 10;
    run.biome = BIOME_ID_PLAINS;

    assert(gc_item_stack(&run, "MAP") == 0);

    /* Clear existing wave 1 queue state */
    gc_internal *in = gc_int(&run);
    gc_rq_clear(&run);
    in->rq.waiting = false;

    /* Push RP_SELECT_BIOME then RP_NEW_BATTLE as victory() does */
    gc_rq_push(&run, gc_rp(RP_SELECT_BIOME, 0, 0, 0));
    gc_rq_push(&run, gc_rp(RP_NEW_BATTLE, 0, 0, 0));
    gc_run_until_screen(&run);

    /* Without Map, RP_SELECT_BIOME auto-selects a biome via RNG without showing GC_SCREEN_BIOME,
     * so it proceeds straight to the COMMAND screen of the next wave. */
    const gc_screen *s = gc_run_screen(&run);
    assert(s->kind == GC_SCREEN_COMMAND);

    /* Check that biome changed from PLAINS and wave advanced to 11 */
    assert(run.biome != BIOME_ID_PLAINS);
    assert(run.wave == 11);

    /* Verify GC_EV_BIOME event was recorded */
    gc_event ev;
    bool got_biome_event = false;
    while (gc_run_next_event(&run, &ev)) {
        if (ev.kind == GC_EV_BIOME) {
            got_biome_event = true;
            assert(ev.a == run.biome);
        }
    }
    assert(got_biome_event);
    printf("ok\n");
}

static void test_biome_transition_with_map(void)
{
    printf("test_biome_transition_with_map...\n");
    gc_run run;
    gc_run_new(&run, GAME_MODES_CLASSIC, "biometest2", 2002);
    gc_starter team[1];
    memset(&team[0], 0, sizeof team[0]);
    team[0].species = SPECIES_ID_BULBASAUR;
    team[0].tera_type = -1;
    for (int i = 0; i < 6; i++) team[0].ivs[i] = 15;

    const char *why;
    bool ok = gc_run_set_starters(&run, team, 1, &why);
    assert(ok);

    run.wave = 10;
    run.biome = BIOME_ID_PLAINS;

    /* Give player the MAP item */
    gc_item_add(&run, "MAP", -1, 0, 1);
    assert(gc_item_stack(&run, "MAP") == 1);

    /* Clear existing wave 1 queue state */
    gc_internal *in = gc_int(&run);
    gc_rq_clear(&run);
    in->rq.waiting = false;

    /* Push RP_SELECT_BIOME then RP_NEW_BATTLE */
    gc_rq_push(&run, gc_rp(RP_SELECT_BIOME, 0, 0, 0));
    gc_rq_push(&run, gc_rp(RP_NEW_BATTLE, 0, 0, 0));
    gc_run_until_screen(&run);

    /* Run loop MUST halt on GC_SCREEN_BIOME since PLAINS has 3 links */
    const gc_screen *s = gc_run_screen(&run);
    assert(s->kind == GC_SCREEN_BIOME);
    assert(s->n_biomes == 3);

    /* Reject invalid choice index */
    gc_choice c_bad = {GC_CH_BIOME, 99, 0, 0};
    ok = gc_run_choose(&run, &c_bad, &why);
    assert(!ok);
    assert(strcmp(why, "no such biome") == 0);

    /* Make valid choice (index 0) */
    uint8_t chosen_biome = s->biomes[0];
    gc_choice c_good = {GC_CH_BIOME, 0, 0, 0};
    ok = gc_run_choose(&run, &c_good, &why);
    assert(ok);

    /* Biome should now be updated to chosen_biome, wave to 11, and screen to COMMAND */
    assert(run.biome == chosen_biome);
    assert(run.wave == 11);
    assert(gc_run_screen(&run)->kind == GC_SCREEN_COMMAND);

    /* Verify GC_EV_BIOME event was queued/produced */
    gc_event ev;
    bool got_biome_event = false;
    while (gc_run_next_event(&run, &ev)) {
        if (ev.kind == GC_EV_BIOME) {
            got_biome_event = true;
            assert(ev.a == chosen_biome);
        }
    }
    assert(got_biome_event);
    printf("ok\n");
}

static void test_biome_transition_party_heal(void)
{
    printf("test_biome_transition_party_heal...\n");
    gc_run run;
    gc_run_new(&run, GAME_MODES_CLASSIC, "biometest3", 2003);
    gc_starter team[1];
    memset(&team[0], 0, sizeof team[0]);
    team[0].species = SPECIES_ID_BULBASAUR;
    team[0].tera_type = -1;
    for (int i = 0; i < 6; i++) team[0].ivs[i] = 15;

    const char *why;
    bool ok = gc_run_set_starters(&run, team, 1, &why);
    assert(ok);

    run.wave = 10;
    run.biome = BIOME_ID_PLAINS;

    /* Damage and poison the party member */
    run.party[0].b.hp = 1;
    run.party[0].b.status = STATUS_EFFECT_POISON;
    run.party[0].b.pp_used[0] = 5;

    /* Give MAP item */
    gc_item_add(&run, "MAP", -1, 0, 1);

    /* Clear existing wave 1 queue state */
    gc_internal *in = gc_int(&run);
    gc_rq_clear(&run);
    in->rq.waiting = false;

    gc_rq_push(&run, gc_rp(RP_SELECT_BIOME, 0, 0, 0));
    gc_rq_push(&run, gc_rp(RP_NEW_BATTLE, 0, 0, 0));
    gc_run_until_screen(&run);

    assert(gc_run_screen(&run)->kind == GC_SCREEN_BIOME);

    /* Choose route */
    gc_choice c = {GC_CH_BIOME, 0, 0, 0};
    ok = gc_run_choose(&run, &c, &why);
    assert(ok);

    /* Verify party was fully healed at wave 10 transition */
    assert(run.party[0].b.hp == run.party[0].b.stats[0]);
    assert(run.party[0].b.status == STATUS_EFFECT_NONE);
    assert(run.party[0].b.pp_used[0] == 0);
    assert(gc_run_screen(&run)->kind == GC_SCREEN_COMMAND);
    printf("ok\n");
}

int main(void)
{
    test_is_new_biome_function();
    test_biome_transition_without_map();
    test_biome_transition_with_map();
    test_biome_transition_party_heal();
    printf("All biome transition and map routing tests passed successfully!\n");
    return 0;
}
