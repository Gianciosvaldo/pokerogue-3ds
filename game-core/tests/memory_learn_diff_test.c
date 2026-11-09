/* Memory Mushroom and TM learning against upstream at 8555c08: replays tests/fixtures/memory-learn.native.txt, written by
 * tests/memory-learn.producer.test.ts (tools/memorylearndiff.sh), through game-core.
 *
 *   R <TM|MEMORY|LEARN_MOVE> <cost> <money before> <money after> <copy queued after 0|1> <TMs used>
 *       the real LearnMovePhase.learnMove with a SelectModifierPhase copy in the queue: what a learned move does to the copy and the
 *       money. Here: a LEARN phase of that type, with the copy queued, started and answered "forget the first move".
 *   Q <name> <cost> <money before> <money after> <phases queued in order, END when the phase ended>
 *       the real SelectModifierPhase.applyModifier: the phases a TM or a Memory Mushroom queues, and what it charges at the pick. Here:
 *       the reward screen's choice (a free reward, or a shop purchase) through gc_phase_choose, then the queue read back.
 * Usage: memory_learn_diff_test <fixture>; exit 0 only with 0 mismatches. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_run run;
static gc_system sys;
static long checks, bad;

#define EXPECT(x, ...) do { checks++; if (!(x)) { bad++; fprintf(stderr, "MISMATCH line %d: %s\n  ", line_no, #x); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)
static int line_no;

static void setup(void)
{
    memset(&run, 0, sizeof run);
    gc_system_init_defaults(&sys);
    gc_run_new(&run, GAME_MODES_CLASSIC, "memory-learn", 7);
    gc_starter team[1];
    memset(team, 0, sizeof team);
    for (int i = 0; i < gc_starter_cost_count; i++)
        if (gc_starter_costs[i].cost == 1) { team[0].species = gc_starter_costs[i].id; break; }
    team[0].tera_type = -1;
    for (int j = 0; j < 6; j++) team[0].ivs[j] = 15;
    const char *why;
    if (!gc_run_set_starters(&run, team, 1, &why)) { fputs("setup failed\n", stderr); exit(2); }
    run.sys = &sys;
    pr_pokemon *b = &run.party[0].b;
    b->level = 50;
    gc_mon_recalc(&run, &run.party[0]);
    /* the producer's Pokemon: Splash, Tackle, Pound, Growl */
    b->n_moves = 4;
    b->moves[0] = MOVE_ID_SPLASH; b->moves[1] = MOVE_ID_TACKLE; b->moves[2] = MOVE_ID_POUND; b->moves[3] = MOVE_ID_GROWL;
    b->species = SPECIES_ID_MEW;
    gc_mon_recalc(&run, &run.party[0]);
    run.party[0].n_used_tms = 0;
    gc_rq_clear(&run);   /* the run's own opening phases are not part of the case */
}

static void queue_copy(void)
{
    gc_int(&run)->reward.copy_pending = true;
    gc_rq_unshift(&run, gc_rp(RP_SELECT_MODIFIER, 0, 1, 0));
}

static void replay_r(const char *type, int cost, int before, int after, int copy, int tms)
{
    setup();
    run.money = before;
    gc_rphase p;
    if (!strcmp(type, "TM")) p = gc_rp(RP_LEARN_MOVE, 0, MOVE_ID_FLAMETHROWER, 1);
    else if (!strcmp(type, "MEMORY")) { p = gc_rp(RP_LEARN_MOVE, 0, MOVE_ID_FLAMETHROWER, 2); p.d = cost; }
    else p = gc_rp(RP_LEARN_MOVE, 0, MOVE_ID_FLAMETHROWER, 0);
    queue_copy();
    EXPECT(!gc_phase_start(&run, &p), "the full moveset asks which move to forget");
    const char *why = NULL;
    bool chosen = false;
    gc_choice c = {GC_CH_FORGET, 0, 0, 0};
    EXPECT(gc_phase_choose(&run, &p, &c, &why, &chosen) && chosen, "forget answer refused");
    EXPECT((int)run.money == after, "%s %d: money %d, upstream %d", type, cost, (int)run.money, after);
    EXPECT(gc_rq_has(&run, RP_SELECT_MODIFIER) == (copy != 0), "%s %d: copy queued %d, upstream %d", type, cost, gc_rq_has(&run, RP_SELECT_MODIFIER), copy);
    EXPECT(run.party[0].n_used_tms == tms, "%s %d: used TMs %d, upstream %d", type, cost, run.party[0].n_used_tms, tms);
}

static void replay_q(const char *name, int cost, int before, int after, const char *queued)
{
    setup();
    run.money = before;
    gc_screen *s = &run.screen;
    memset(s, 0, sizeof *s);
    s->kind = GC_SCREEN_REWARD;
    gc_offer o;
    memset(&o, 0, sizeof o);
    o.key = name;
    o.arg = !strncmp(name, "TM_", 3) ? MOVE_ID_FLAMETHROWER : -1;
    o.target = gc_reward_target(name);
    o.cost = cost > 0 ? cost : 0;
    gc_choice c;
    memset(&c, 0, sizeof c);
    c.b = 0;
    c.c = 0;   /* the Memory Mushroom's first move; ignored by a TM */
    if (cost > 0) { s->n_shop = 1; s->shop[0] = o; c.kind = GC_CH_SHOP; }
    else { s->n_options = 1; s->options[0] = o; c.kind = GC_CH_REWARD; }
    gc_rphase p = gc_rp(RP_SELECT_MODIFIER, 0, 0, 0);
    const char *why = NULL;
    bool chosen = false;
    const bool ended = gc_phase_choose(&run, &p, &c, &why, &chosen);
    EXPECT(chosen, "%s %d refused: %s", name, cost, why ? why : "?");
    char got[128] = "";
    gc_rphase q;
    while (gc_rq_next(&gc_int(&run)->rq, &q)) {
        if (got[0]) strcat(got, ",");
        strcat(got, q.type == RP_LEARN_MOVE ? "LearnMovePhase" : q.type == RP_SELECT_MODIFIER ? "SelectModifierPhase" : "?");
    }
    if (ended) strcat(got, got[0] ? ",END" : "END");
    EXPECT((int)run.money == after, "%s %d: money after the pick %d, upstream %d", name, cost, (int)run.money, after);
    EXPECT(!strcmp(got, queued), "%s %d: queued %s, upstream %s", name, cost, got, queued);
}

int main(int argc, char **argv)
{
    if (argc < 2) { fputs("usage: memory_learn_diff_test <fixture>\n", stderr); return 2; }
    FILE *f = fopen(argv[1], "r");
    if (!f) { perror(argv[1]); return 2; }
    char buf[256];
    int r = 0, q = 0;
    while (fgets(buf, sizeof buf, f)) {
        line_no++;
        char tag[4] = "", name[48] = "", queued[128] = "";
        int cost, before, after, copy, tms;
        if (buf[0] == 'V') continue;
        if (sscanf(buf, "R %47s %d %d %d %d %d", name, &cost, &before, &after, &copy, &tms) == 6) { replay_r(name, cost, before, after, copy, tms); r++; continue; }
        if (sscanf(buf, "Q %47s %d %d %d %127s", name, &cost, &before, &after, queued) == 5) { replay_q(name, cost, before, after, queued); q++; continue; }
        (void)tag;
        fprintf(stderr, "unreadable line %d: %s", line_no, buf);
        return 2;
    }
    fclose(f);
    printf("memory learn: %d learn cases, %d pick cases, %ld checks, %ld mismatches\n", r, q, checks, bad);
    return bad || r != 4 || q != 4;
}
