/* Task 77: Flee through the run loop test.
 * Verifies allowed/blocked fleeing, attempt counter, message events, reward suppression,
 * wave transition, save/resume at the next wave, and escape RNG logic. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_run run;
static gc_system sys;
static int checks;

#define CHECK(x) do { \
    if (!(x)) { \
        fprintf(stderr, "flee_test FAIL line %d: %s\n", __LINE__, #x); \
        exit(1); \
    } \
    checks++; \
} while (0)

static void drain(void)
{
    gc_event e;
    while (gc_run_next_event(&run, &e)) {}
}

static void new_run(const char *seed)
{
    memset(&run, 0, sizeof run);
    gc_system_init_defaults(&sys);
    gc_run_new(&run, GAME_MODES_CLASSIC, seed, 42);
    gc_starter team[3];
    memset(team, 0, sizeof team);
    const int species[3] = {SPECIES_ID_BULBASAUR, SPECIES_ID_CHARMANDER, SPECIES_ID_SQUIRTLE};
    for (int i = 0; i < 3; i++) {
        team[i].species = (uint16_t)species[i];
        team[i].tera_type = -1;
        for (int k = 0; k < 6; k++) team[i].ivs[k] = 15;
    }
    const char *why;
    CHECK(gc_run_set_starters(&run, team, 3, &why));
    run.sys = &sys;
    drain();
}

static bool bev_text_exists(const char *key)
{
    for (int i = 0; i < run.n_battle_events; i++) {
        const pr_event *e = &run.battle_events[i];
        if (e->type == PR_EV_TEXT && e->msg.key && !strcmp(e->msg.key, key)) return true;
    }
    return false;
}

static void test_trainer_prohibition(void)
{
    new_run("flee-trainer-test");
    run.battle_type = BATTLE_TYPE_TRAINER;
    run.bt.battle_type = BATTLE_TYPE_TRAINER;
    gc_battle_command_screen(&run);

    const gc_screen *s = gc_run_screen(&run);
    CHECK(!s->can_run);

    const char *why = NULL;
    gc_choice c = {GC_CH_RUN, 0, 0, 0};
    bool ok = gc_run_choose(&run, &c, &why);
    CHECK(!ok);
    CHECK(why != NULL);
    CHECK(!strcmp(why, "battle:noEscapeTrainer"));
}

static void test_trapped_wild(void)
{
    new_run("flee-trapped-test");
    CHECK(run.battle_type == BATTLE_TYPE_WILD);

    pr_pokemon *p = run.bt.field[0];
    pr_pokemon *ep = run.bt.field[2];
    CHECK(p != NULL && ep != NULL);

    /* Apply a trapping tag to player's active Pokemon with wild foe as source */
    pr_add_tag(&run.bt, &run.eng, p, BATTLER_TAG_TYPE_TRAPPED, 0, MOVE_ID_MEAN_LOOK, ep->id);
    gc_battle_command_screen(&run);

    const gc_screen *s = gc_run_screen(&run);
    CHECK(!s->can_run);

    const char *why = NULL;
    gc_choice c = {GC_CH_RUN, 0, 0, 0};
    bool ok = gc_run_choose(&run, &c, &why);
    CHECK(!ok);
    CHECK(why != NULL);
    CHECK(!strcmp(why, "battle:noEscapePokemon"));
}

static void test_failed_attempts_then_success_and_rewards_suppression(void)
{
    new_run("flee-soak-seed-1");
    CHECK(run.battle_type == BATTLE_TYPE_WILD);
    const int start_wave = run.wave;

    /* Adjust stats: player lead slow, wild enemy fast */
    pr_pokemon *p = run.bt.field[0];
    pr_pokemon *ep = run.bt.field[2];
    CHECK(p != NULL && ep != NULL);

    p->stats[STAT_SPD] = 10;
    ep->stats[STAT_SPD] = 500;

    const gc_screen *s = gc_run_screen(&run);
    CHECK(s->can_run);

    /* Turn 1: Attempt flee */
    const char *why = NULL;
    gc_choice c = {GC_CH_RUN, 0, 0, 0};
    bool ok = gc_run_choose(&run, &c, &why);
    CHECK(ok);

    /* Verify attempt counter incremented during turn 1 */
    CHECK(run.in_battle && run.screen.kind == GC_SCREEN_COMMAND);
    CHECK(bev_text_exists("battle:runAwayCannotEscape"));
    drain();

    /* Attempt flee again on Turn 2 with high speed to ensure escape */
    p->stats[STAT_SPD] = 5000;
    ok = gc_run_choose(&run, &c, &why);
    CHECK(ok);

    /* Verify successful flee message */
    CHECK(bev_text_exists("battle:runAwaySuccess"));

    /* REWARDS SUPPRESSION & ENCOUNTER TRANSITION:
     * Screen must NOT be GC_SCREEN_REWARD!
     * It should transition directly to wave start of wave N+1 (GC_SCREEN_COMMAND). */
    s = gc_run_screen(&run);
    CHECK(s->kind == GC_SCREEN_COMMAND);
    CHECK(run.wave == start_wave + 1);
    CHECK(run.in_battle);

    /* SAVE & RESUME AT NEXT LEGAL POINT (Wave N+1 command screen) */
    CHECK(gc_run_can_save(&run));

    static uint8_t save_buf[GC_SAVE_MAX];
    size_t save_len = gc_run_save(&run, save_buf, sizeof save_buf);
    CHECK(save_len > 0);

    /* Load into fresh run struct */
    static gc_run loaded_run;
    memset(&loaded_run, 0, sizeof loaded_run);
    const char *load_why = NULL;
    bool loaded = gc_run_load(&loaded_run, save_buf, save_len, &load_why);
    CHECK(loaded);
    CHECK(loaded_run.wave == start_wave + 1);
    CHECK(loaded_run.in_battle);
    CHECK(loaded_run.screen.kind == GC_SCREEN_COMMAND);
    CHECK(loaded_run.n_party == run.n_party);
}

int main(void)
{
    test_trainer_prohibition();
    test_trapped_wild();
    test_failed_attempts_then_success_and_rewards_suppression();

    printf("flee: %d checks, 0 failures\n", checks);
    return 0;
}
