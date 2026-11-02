/* What a Pokemon made with DNA Splicers can do afterwards, end to end from the splice itself (not a fused Pokemon built by hand): the
 * second member's evolution item evolves its half, the second member's TMs and level-up moves are the fused Pokemon's, and a friendship
 * gain feeds the candy bars of both species (Pokemon.addFriendship: the rate is halved for each). The numbers behind the evolution
 * and the moves (upstream, case by case) are fusion_item_evolution_host_test and the learnset tests; this one checks the
 * port's glue from a real splice. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_run r;
static gc_system sys;
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr, "fusion after line %d: %s\n", __LINE__, #x); exit(1); } } while (0)

/* two starters, storage 0 leads; the first is the primary of a later splice */
static void setup(int first, int second)
{
    const char *why;
    gc_system_init_defaults(&sys);
    gc_run_new(&r, GAME_MODES_CLASSIC, "fusion-after", 4242);
    gc_starter st[2];
    memset(st, 0, sizeof st);
    st[0].species = (uint16_t)first;
    st[1].species = (uint16_t)second;
    for (int i = 0; i < 2; i++) { st[i].tera_type = -1; memset(st[i].ivs, 15, 6); }
    CHECK(gc_run_set_starters(&r, st, 2, &why));
    r.sys = &sys;
    gc_event e;
    while (gc_run_next_event(&r, &e)) {}
    CHECK(r.n_party == 2 && r.party[0].b.species == first && r.party[1].b.species == second);
}

/* the splice of party[1] into party[0], taken as a free reward */
static void splice(void)
{
    gc_rqueue *q = &gc_int(&r)->rq;
    gc_rq_reset(q);
    q->current = gc_rp(RP_SELECT_MODIFIER, 0, 0, 0);
    q->waiting = true;
    gc_rq_push(&r, gc_rp(RP_COMMAND, 0, 0, 0));
    memset(&r.screen, 0, sizeof r.screen);
    r.screen.kind = GC_SCREEN_REWARD;
    r.screen.n_options = 1;
    r.screen.options[0] = (gc_offer){.key = "DNA_SPLICERS", .arg = 0, .cost = 0, .target = GC_TARGET_FUSE};
    const char *why = NULL;
    gc_choice c = {GC_CH_REWARD, 0, 0, 1};
    CHECK(gc_run_choose(&r, &c, &why));
    CHECK(r.n_party == 1 && r.party[0].b.fusion_species != 0);
}

static void tms_and_level_moves(void)
{
    /* Pikachu leads, Duraludon is spliced in: both species' TMs and level-up moves are the fused Pokemon's */
    setup(SPECIES_ID_PIKACHU, SPECIES_ID_DURALUDON);
    gc_mon pika = r.party[0], dura = r.party[1];
    gc_learn_row pika_rows[64], dura_rows[64];
    const int np = gc_level_moves(&pika, 1, false, false, false, pika_rows, 64);
    const int nd = gc_level_moves(&dura, 1, false, false, false, dura_rows, 64);
    CHECK(np > 0 && nd > 0);
    /* compatibility alone: a move a member already knows is not offered, so ask with no moves known */
    pika.b.n_moves = 0;
    dura.b.n_moves = 0;
    int only_dura_tm = -1, only_pika_tm = -1, both_tm = 0;
    bool pika_tm[PR_MOVE_COUNT] = {0}, dura_tm[PR_MOVE_COUNT] = {0};
    for (int mv = 1; mv < PR_MOVE_COUNT; mv++) {
        pika_tm[mv] = gc_mon_can_learn_tm(&pika, mv);
        dura_tm[mv] = gc_mon_can_learn_tm(&dura, mv);
        if (dura_tm[mv] && !pika_tm[mv] && only_dura_tm < 0) only_dura_tm = mv;
        if (pika_tm[mv] && !dura_tm[mv] && only_pika_tm < 0) only_pika_tm = mv;
        both_tm += pika_tm[mv] && dura_tm[mv];
    }
    CHECK(only_dura_tm > 0 && only_pika_tm > 0);
    splice();
    gc_mon *f = &r.party[0];
    for (int mv = 1; mv < PR_MOVE_COUNT; mv++) {
        bool known = false;
        for (int k = 0; k < f->b.n_moves; k++) known |= f->b.moves[k] == mv;
        if (known) continue;
        CHECK(gc_mon_can_learn_tm(f, mv) == (pika_tm[mv] || dura_tm[mv]));
    }
    (void)both_tm;
    /* the level-up list of the fusion holds every move of either species' list that the fusion does not know yet */
    gc_learn_row rows[128];
    const int n = gc_level_moves(f, 1, false, false, false, rows, 128);
    CHECK(n > 0);
    for (int pass = 0; pass < 2; pass++) {
        const gc_learn_row *src = pass ? dura_rows : pika_rows;
        const int ns = pass ? nd : np;
        for (int i = 0; i < ns; i++) {
            bool known = false;
            for (int k = 0; k < f->b.n_moves; k++) known |= f->b.moves[k] == src[i].move;
            if (known) continue;
            bool found = false;
            for (int j = 0; j < n; j++) found |= rows[j].move == src[i].move;
            CHECK(found);
        }
    }
}


/* the second member's evolution item (Duraludon's Metal Alloy) evolves its half and leaves the primary alone; the level-up list when the
 * fused Pokemon gains a level holds the second species' move of that level */
static void evolution_and_level_up(void)
{
    setup(SPECIES_ID_PIKACHU, SPECIES_ID_DURALUDON);
    int item = -1, evo_index = -1;
    for (int i = 0; i < 544; i++)
        if (gc_evo_data[i].species == SPECIES_ID_ARCHALUDON && gc_evo_data[i].item) { item = gc_evo_data[i].item; evo_index = i; }
    CHECK(item > 0 && evo_index >= 0);
    bool fusion = false;
    CHECK(gc_mon_item_evolution_target(&r, &r.party[0], item, &fusion) < 0);   /* nothing to evolve before the splice */
    gc_learn_row dura_rows[64];
    gc_mon dura = r.party[1];
    dura.b.level = 100;   /* the whole list, not just what a level 5 starter has reached */
    const int nd = gc_level_moves(&dura, 1, false, false, false, dura_rows, 64);
    splice();
    gc_mon *f = &r.party[0];
    CHECK(gc_mon_item_evolution_target(&r, f, item, &fusion) == evo_index && fusion);
    gc_offer offer = {.key = item > 50 ? "RARE_EVOLUTION_ITEM" : "EVOLUTION_ITEM", .arg = item, .target = GC_TARGET_POKEMON};
    const char *why = NULL;
    CHECK(gc_reward_lifecycle_apply(&r, &offer, 0, &why) == GC_APPLY_DONE);
    gc_rphase request;
    CHECK(gc_rq_next(&gc_int(&r)->rq, &request));
    CHECK(request.type == RP_EVOLUTION && request.d == 1 && gc_evo_data[request.b].species == SPECIES_ID_ARCHALUDON);

    /* a level of the second species' list: the move is on the fused Pokemon's level-up list */
    setup(SPECIES_ID_PIKACHU, SPECIES_ID_DURALUDON);
    splice();
    f = &r.party[0];
    int level = -1, move = -1;
    for (int i = 0; i < nd; i++) {
        bool known = false;
        for (int k = 0; k < f->b.n_moves; k++) known |= f->b.moves[k] == dura_rows[i].move;
        bool pika_has = false;
        gc_mon pika_only = f[0];
        pika_only.b.fusion_species = 0;
        pika_only.b.level = 100;
        gc_learn_row pr[64];
        const int np = gc_level_moves(&pika_only, 1, false, false, false, pr, 64);
        for (int j = 0; j < np; j++) pika_has |= pr[j].move == dura_rows[i].move;
        if (!known && !pika_has && dura_rows[i].level > 1 && dura_rows[i].level <= 100) { level = dura_rows[i].level; move = dura_rows[i].move; break; }
    }
    CHECK(level > 1 && move > 0);
    f->b.level = (uint8_t)level;   /* the level the Pokemon has just reached */
    uint16_t mv[32];
    const int n = gc_level_up_moves(f, level, mv, 32);
    bool found = false;
    for (int i = 0; i < n; i++) found |= mv[i] == move;
    CHECK(found);
}

/* the candy bar of a starter after a gain, from the table of caps (upstream getStarterValueFriendshipCap): the bar, and how many
 * candies the bar has paid out */
static int bar_cap(int species)
{
    const int cost = gc_starter_cost(gc_species_root_id((uint16_t)species));
    return getStarterValueFriendshipCap(cost < 1 ? 1 : cost);
}

/* Pokemon.addFriendship: a Pokemon made of two species feeds the candy bar of each root species, and the rate of each is halved
 * (floor(gain * 3 / 2) in Classic, floor(gain / 2) elsewhere); a Pokemon that is not fused feeds its own root alone, at the full
 * rate. The gains and the bars are worked out by hand from the rule, not by calling the function being tested twice. */
static void candy(void)
{
    const int pikachu_root = gc_species_root_id(SPECIES_ID_PIKACHU), dura_root = gc_species_root_id(SPECIES_ID_DURALUDON);
    CHECK(pikachu_root != dura_root);
    const int cap_p = bar_cap(SPECIES_ID_PIKACHU), cap_d = bar_cap(SPECIES_ID_DURALUDON);
    CHECK(cap_p > 12 && cap_d > 12);

    /* before the splice: Classic, 4 friendship is 12 points on the bar of the Pokemon's own root, none on the other */
    setup(SPECIES_ID_PIKACHU, SPECIES_ID_DURALUDON);
    gc_system_add_friendship(&sys, &r, &r.party[0], 4, false);
    CHECK(sys.starters[pikachu_root].friendship == 12 && sys.starters[dura_root].friendship == 0);
    CHECK(r.party[0].b.friendship >= 4);

    /* after it: both bars gain half of that rate, 4 * 3 / 2 = 6 each; the Pokemon's own friendship still gains the 4 */
    splice();
    const int before_friendship = r.party[0].b.friendship;
    gc_system_add_friendship(&sys, &r, &r.party[0], 4, false);
    CHECK(sys.starters[pikachu_root].friendship == 12 + 6 && sys.starters[dura_root].friendship == 6);
    CHECK(r.party[0].b.friendship == before_friendship + 4);
    CHECK(sys.starters[pikachu_root].candy_count == 0 && sys.starters[dura_root].candy_count == 0);

    /* an odd product is floored: 5 * 3 / 2 = 7 */
    gc_system_add_friendship(&sys, &r, &r.party[0], 5, false);
    CHECK(sys.starters[pikachu_root].friendship == 12 + 6 + 7 && sys.starters[dura_root].friendship == 6 + 7);

    /* a bar that crosses its cap pays out a candy on that species alone and keeps the rest: the second species' bar is 3 short of
     * its cap, the first has room */
    sys.starters[pikachu_root].friendship = 0;
    sys.starters[dura_root].friendship = (uint16_t)(cap_d - 3);
    gc_system_add_friendship(&sys, &r, &r.party[0], 4, false);
    CHECK(sys.starters[dura_root].candy_count == 1 && sys.starters[dura_root].friendship == 3);
    CHECK(sys.starters[pikachu_root].candy_count == 0 && sys.starters[pikachu_root].friendship == 6);

    /* outside Classic the rate is 1, halved: 7 / 2 = 3 each; a loss of friendship touches no bar */
    r.run.mode = GAME_MODES_ENDLESS;
    sys.starters[pikachu_root].friendship = 0;
    sys.starters[dura_root].friendship = 0;
    gc_system_add_friendship(&sys, &r, &r.party[0], 7, false);
    CHECK(sys.starters[pikachu_root].friendship == 3 && sys.starters[dura_root].friendship == 3);
    gc_system_add_friendship(&sys, &r, &r.party[0], -5, false);
    CHECK(sys.starters[pikachu_root].friendship == 3 && sys.starters[dura_root].friendship == 3);

    /* a Pokemon that is not fused, same mode: its root at the full rate, 7 points */
    gc_mon alone = r.party[0];
    alone.b.fusion_species = 0;
    sys.starters[pikachu_root].friendship = 0;
    sys.starters[dura_root].friendship = 0;
    gc_system_add_friendship(&sys, &r, &alone, 7, false);
    CHECK(sys.starters[pikachu_root].friendship == 7 && sys.starters[dura_root].friendship == 0);

    /* two members of one line fused: that one bar is fed twice, upstream's forEach adds to the same entry once for each */
    setup(SPECIES_ID_PICHU, SPECIES_ID_PIKACHU);
    splice();
    sys.starters[pikachu_root].friendship = 0;
    gc_system_add_friendship(&sys, &r, &r.party[0], 4, false);
    CHECK(sys.starters[pikachu_root].friendship == 12);
}

int main(void)
{
    tms_and_level_moves();
    evolution_and_level_up();
    candy();
    printf("fusion after the splice: %u checks\n", checks);
    return 0;
}
