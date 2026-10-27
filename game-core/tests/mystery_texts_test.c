/* The encounter screen's texts and picks (gc_screen.me_args, option_pick, gc_mystery_member_ok) for the encounters ported with their texts:
 * the tokens the locale's labels and tooltips name, with the values the port itself uses (the price is the requirement's money, the
 * primary Pokemon is the one the option's own run uses), and which options ask for a Pokemon first. */
#include "../src/internal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static gc_run run;
static gc_system sys;

static void start(int type, int wave, const char *seed, const int *species, const int *moves, int n, double money)
{
    memset(&run, 0, sizeof run);
    gc_run_new(&run, GAME_MODES_CLASSIC, seed, 7);
    gc_system_init_defaults(&sys);
    run.sys = &sys;
    run.wave = wave;
    run.biome = BIOME_ID_PLAINS;
    gc_items_to_battle(&run);
    run.n_party = n;
    for (int i = 0; i < n; i++) {
        gc_starter s;
        memset(&s, 0, sizeof s);
        s.species = (uint16_t)species[i];
        s.tera_type = -1;
        for (int k = 0; k < 6; k++) s.ivs[k] = 15;
        s.moves[0] = (uint16_t)moves[i];
        gc_mon_new_player(&run, &run.party[i], &s, 30);
        run.party[i].pause_evolutions = true;
        run.party_order[i] = (int8_t)i;
    }
    run.money = money;
    run.battle_type = BATTLE_TYPE_WILD;
    gc_rq_reset(&gc_int(&run)->rq);
    run.mystery_type = type;
    gc_rq_unshift(&run, gc_rp(RP_MYSTERY, type, GC_ME_COMMON, 0));
    gc_run_until_screen(&run);
    assert(run.screen.kind == GC_SCREEN_MYSTERY);
}

static const pr_msg_arg *arg(const char *name)
{
    for (int i = 0; i < run.screen.n_me_args; i++) if (!strcmp(run.screen.me_args[i].name, name)) return &run.screen.me_args[i];
    return NULL;
}

static double money_of(const pr_msg_arg *a)
{
    assert(a && a->kind == PR_ARG_MONEY_EN_US);
    uint64_t bits = (uint64_t)a->v | (uint64_t)a->fusion_v << 32;
    double d;
    memcpy(&d, &bits, sizeof d);
    return d;
}

static void test_shady(void)
{
    puts("test_texts_shady...");
    const int sp[2] = {SPECIES_ID_PIKACHU, SPECIES_ID_LAPRAS}, mv[2] = {MOVE_ID_TACKLE, MOVE_ID_TACKLE};
    start(GC_ME_SHADY_VITAMIN_DEALER, 45, "tx-shady", sp, mv, 2, 1e7);
    assert(run.screen.n_options == 3);
    assert(money_of(arg("option1Money")) == pr_run_get_wave_money_amount(45, 1.5));
    assert(money_of(arg("option2Money")) == pr_run_get_wave_money_amount(45, 5.0));
    assert(run.screen.option_pick[0] == GC_PICK_MEMBER && run.screen.option_pick[1] == GC_PICK_MEMBER && !run.screen.option_pick[2]);
    /* a Pokemon below half HP is not accepted by the cheap deal, any allowed one by the pricey deal */
    run.party[1].b.hp = 1;
    assert(gc_mystery_member_ok(&run, 0, 0) && !gc_mystery_member_ok(&run, 0, 1));
    assert(gc_mystery_member_ok(&run, 1, 0) && gc_mystery_member_ok(&run, 1, 1));
    assert(!gc_mystery_member_ok(&run, 2, 0) && !gc_mystery_member_ok(&run, 0, 5) && !gc_mystery_member_ok(&run, 0, -1));
    puts("ok");
}

static void test_lost_at_sea(void)
{
    puts("test_texts_lost_at_sea...");
    const int sp[2] = {SPECIES_ID_PIKACHU, SPECIES_ID_LAPRAS}, mv[2] = {MOVE_ID_TACKLE, MOVE_ID_TACKLE};
    start(GC_ME_LOST_AT_SEA, 45, "tx-lost", sp, mv, 2, 1000);
    const pr_msg_arg *n1 = arg("option1PrimaryName");
    const int p1 = gc_mystery_primary_pokemon(&run, GC_ME_LOST_AT_SEA, 0);   /* the first that can Surf */
    assert(p1 >= 0 && n1 && n1->kind == PR_ARG_POKEMON_NAME && (n1->v >> 16) == run.party[p1].b.species);
    const pr_msg_arg *m1 = arg("option1RequiredMove");
    assert(m1 && m1->kind == PR_ARG_MOVE && (int)m1->v == MOVE_ID_SURF);
    const pr_msg_arg *m2 = arg("option2RequiredMove");
    assert(m2 && (int)m2->v == MOVE_ID_FLY);
    const pr_msg_arg *d = arg("damagePercentage");
    assert(d && d->kind == PR_ARG_INT && d->v == 25);
    for (int o = 0; o < 3; o++) assert(run.screen.option_pick[o] == GC_PICK_NONE);
    /* an option nobody can take has no primary name (the app words the disabled label instead) */
    assert(run.screen.option_ok[0] == (n1 != NULL));
    assert(!!arg("option2PrimaryName") == run.screen.option_ok[1]);
    puts("ok");
}

static void test_part_timer_and_games(void)
{
    puts("test_texts_part_timer_and_games...");
    const int sp[2] = {SPECIES_ID_PIKACHU, SPECIES_ID_LAPRAS}, mv[2] = {MOVE_ID_CHARM, MOVE_ID_TACKLE};
    start(GC_ME_PART_TIMER, 45, "tx-timer", sp, mv, 2, 1000);
    const pr_msg_arg *pm = arg("option3PrimaryMove");
    assert(pm && (int)pm->v == MOVE_ID_CHARM);
    const pr_msg_arg *pn = arg("option3PrimaryName");
    assert(pn && (pn->v >> 16) == SPECIES_ID_PIKACHU);
    assert(run.screen.option_pick[0] == GC_PICK_MEMBER && run.screen.option_pick[1] == GC_PICK_MEMBER && !run.screen.option_pick[2]);
    assert(gc_mystery_member_ok(&run, 0, 0) && gc_mystery_member_ok(&run, 0, 1));

    start(GC_ME_FUN_AND_GAMES, 45, "tx-games", sp, mv, 2, 1e6);
    assert(money_of(arg("option1Money")) == pr_run_get_wave_money_amount(45, 1.5));
    assert(run.screen.option_pick[0] == GC_PICK_MEMBER && !run.screen.option_pick[1]);

    start(GC_ME_FIELD_TRIP, 45, "tx-trip", sp, mv, 2, 1000);
    for (int o = 0; o < 3; o++) {
        assert(run.screen.option_pick[o] == GC_PICK_MEMBER_MOVE);
        assert(gc_mystery_member_ok(&run, o, 0) && gc_mystery_member_ok(&run, o, 1) && !gc_mystery_member_ok(&run, o, 2));
    }
    assert(run.screen.n_me_args == 0);

    /* an encounter without ported texts carries no tokens and asks for no pick, and the arguments do not pile up between screens */
    start(GC_ME_SAFARI_ZONE, 45, "tx-safari", sp, mv, 2, 1000);
    assert(run.screen.n_me_args == 0);
    for (int o = 0; o < 8; o++) assert(run.screen.option_pick[o] == GC_PICK_NONE);

    /* Slumbering Snorlax carries its dialogue token and, for the steal, the Pokemon and move (option 3) */
    const int thief[2] = {MOVE_ID_TACKLE, MOVE_ID_THIEF};
    start(GC_ME_SLUMBERING_SNORLAX, 45, "tx-snorlax", sp, thief, 2, 1000);
    assert(arg("snorlaxName"));
    { const pr_msg_arg *tm = arg("option3PrimaryMove"), *tn = arg("option3PrimaryName");
      assert(tm && (int)tm->v == MOVE_ID_THIEF);
      assert(tn && (tn->v >> 16) == SPECIES_ID_LAPRAS); }
    for (int o = 0; o < 8; o++) assert(run.screen.option_pick[o] == GC_PICK_NONE);
    puts("ok");
}

int main(void)
{
    test_shady();
    test_lost_at_sea();
    test_part_timer_and_games();
    puts("mystery texts: all ok");
    return 0;
}
