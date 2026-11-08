/* A frenzy (Petal Dance / Outrage / Thrash) that the encounter's last foe interrupts must not follow the Pokemon into
 * the next wave: the next turn is queued with no target, the next wave's first turn cancels it in silence, and the
 * frenzy then ends with no turns and no confusion ("copies Hyper Beam"). The battle's end clears it like an unfinished
 * charge. Singles and doubles: the first use KOs the last foe of the wave, then the next wave starts.
 * Exit 0 only when no seed carries the frenzy or its queued turn over. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_run run;
static gc_system sys;

static int one(int move, int nstart, int sd, int *killed_first_use)
{
    char seed[32];
    snprintf(seed, sizeof seed, "fe%d", sd);
    memset(&run, 0, sizeof run);
    gc_system_init_defaults(&sys);
    gc_run_new(&run, GAME_MODES_CLASSIC, seed, sd);
    gc_starter team[2];
    memset(team, 0, sizeof team);
    for (int i = 0; i < nstart; i++) {
        team[i].species = i ? SPECIES_ID_CHARMANDER : SPECIES_ID_BULBASAUR;
        team[i].tera_type = -1;
        for (int k = 0; k < 6; k++) team[i].ivs[k] = 15;
    }
    const char *why;
    if (!gc_run_set_starters(&run, team, nstart, &why)) { printf("starters: %s\n", why); exit(2); }
    run.sys = &sys;
    for (int i = 0; i < nstart; i++) {
        run.party[i].b.level = 100;
        gc_mon_recalc(&run, &run.party[i]);
        gc_mon_set_move(&run.party[i], 0, move);
    }
    int bad = 0, in_first_battle = 1;
    for (int step = 0; step < 4000 && in_first_battle; step++) {
        gc_event e;
        while (gc_run_next_event(&run, &e)) {}
        const gc_screen *s = gc_run_screen(&run);
        gc_choice c;
        memset(&c, 0, sizeof c);
        switch (s->kind) {
        case GC_SCREEN_COMMAND:
            for (int k = 2; k < 4; k++) if (run.bt.field[k]) run.bt.field[k]->hp = 1;   /* the first use KOs every foe */
            for (int i = 0; i < nstart; i++) { run.party[i].b.hp = run.party[i].b.stats[0]; }
            c.kind = GC_CH_FIGHT; c.b = -1; c.a = 0; break;
        case GC_SCREEN_TARGET: c.kind = GC_CH_TARGET; c.a = -1; for (int k = 2; k < 4; k++) if (s->target_ok[k]) { c.a = k; break; } break;
        default: in_first_battle = 0; continue;
        }
        if (!gc_run_choose(&run, &c, &why)) return bad;
        if (!run.in_battle && run.wave == 1) {
            /* the battle ended: nothing of the frenzy may be left on any party member */
            for (int i = 0; i < run.n_party; i++) {
                const pr_pokemon *p = &run.party[i].b;
                if (pr_get_battler_tag(p, BATTLER_TAG_TYPE_FRENZY) || p->n_move_queue) bad++;
            }
            (*killed_first_use)++;
            in_first_battle = 0;
        }
    }
    return bad;
}

int main(void)
{
    int bad = 0, covered = 0;
    for (int nstart = 1; nstart <= 2; nstart++)
        for (int m = 0; m < 2; m++)
            for (int sd = 1; sd <= 12; sd++)
                bad += one(m ? MOVE_ID_OUTRAGE : MOVE_ID_PETAL_DANCE, nstart, sd, &covered);
    printf("frenzy end test: %d battles ended on the first use, %d frenzy/queue leftovers\n", covered, bad);
    if (covered < 12) { printf("too few covered battles\n"); return 1; }
    return bad ? 1 : 0;
}
