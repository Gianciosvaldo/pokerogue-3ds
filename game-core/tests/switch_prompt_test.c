/* The two switch prompts game-core builds itself, checked on their own. Both used to be covered only by
 * soak seeds (Task 87's seed 849, Task 59's seed 782); real rewards changed what every seed plays, so the
 * bugs they caught are pinned here by hand-built states:
 *  - the replacement prompt after a faint (gc_phase_choose, RP_SWITCH with a = 1) must clear
 *    switch_out_status on a Pokemon that fainted earlier and was revived, or the battle never uses it;
 *  - a move's switch-out (Teleport) must not offer the Pokemon that is leaving (gc_battle_switch_screen). */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_run run;
static gc_system sys;
static int checks;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "switch prompt FAIL line %d: %s\n", __LINE__, #x); exit(1); } checks++; } while (0)

static void setup(const char *seed)
{
    memset(&run, 0, sizeof run);
    gc_system_init_defaults(&sys);
    gc_run_new(&run, GAME_MODES_CLASSIC, seed, 7);
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
    for (int i = 0; i < run.n_party; i++) {
        run.party[i].b.level = 50;
        gc_mon_recalc(&run, &run.party[i]);
        run.party[i].b.hp = run.party[i].b.stats[0];
    }
    CHECK(run.screen.kind == GC_SCREEN_COMMAND);
}

static void drain(void) { gc_event e; while (gc_run_next_event(&run, &e)) {} }

/* a fainted lead and a revived bench member that still carries the flag its own faint set */
static void replacement_prompt_clears_the_flag(void)
{
    setup("switch-prompt-revived");
    const int lead = run.party_order[0], bench = run.party_order[1];
    run.party[bench].b.switch_out_status = true;
    run.party[lead].b.hp = 0;
    gc_rq_clear(&run);
    gc_int(&run)->rq.waiting = false;
    gc_int(&run)->rq.current.type = RP_NONE;
    memset(&run.screen, 0, sizeof run.screen);
    gc_run_until_screen(&run);
    CHECK(run.screen.kind == GC_SCREEN_SWITCH);
    CHECK(run.screen.pick_ok[bench]);
    CHECK(!run.screen.pick_ok[lead]);

    const char *why = "";
    gc_choice pick = {GC_CH_PICK, (int16_t)bench, 0, 0};
    CHECK(gc_run_choose(&run, &pick, &why));
    CHECK(run.bt.field[0] == &run.party[bench].b);
    CHECK(run.party[bench].b.on_field);
    CHECK(!run.party[bench].b.switch_out_status);
    drain();

    /* and it takes its turn: the move it uses is spent */
    CHECK(run.screen.kind == GC_SCREEN_COMMAND);
    CHECK(run.party[bench].b.pp_used[0] == 0);
    gc_choice fight = {GC_CH_FIGHT, 0, -1, 0};
    CHECK(gc_run_choose(&run, &fight, &why));
    drain();
    CHECK(run.party[bench].b.pp_used[0] == 1);
}

/* the lead uses Teleport: the prompt lists the bench, not the Pokemon that is leaving */
static void teleport_prompt_leaves_out_the_leaver(void)
{
    setup("switch-prompt-teleport");
    const int lead = run.party_order[0], b1 = run.party_order[1], b2 = run.party_order[2];
    run.party[lead].b.moves[0] = MOVE_ID_TELEPORT;
    run.party[lead].b.n_moves = 1;
    run.party[lead].b.pp_used[0] = 0;
    gc_choice fight = {GC_CH_FIGHT, 0, -1, 0};
    const char *why = "";
    CHECK(gc_run_choose(&run, &fight, &why));
    CHECK(run.screen.kind == GC_SCREEN_SWITCH);
    CHECK(run.screen.slot == 0);
    CHECK(!run.screen.pick_ok[lead]);
    CHECK(run.screen.pick_ok[b1] && run.screen.pick_ok[b2]);
    gc_choice pick_leaver = {GC_CH_PICK, (int16_t)lead, 0, 0};
    CHECK(!gc_run_choose(&run, &pick_leaver, &why));
    gc_choice pick = {GC_CH_PICK, (int16_t)b1, 0, 0};
    CHECK(gc_run_choose(&run, &pick, &why));
    drain();
    CHECK(run.bt.field[0] == &run.party[b1].b);
    CHECK(!run.party[b1].b.switch_out_status);
    CHECK(!run.party[lead].b.on_field);
}

int main(void)
{
    replacement_prompt_clears_the_flag();
    teleport_prompt_leaves_out_the_leaver();
    printf("switch prompts: %d checks, 0 failures\n", checks);
    return 0;
}
