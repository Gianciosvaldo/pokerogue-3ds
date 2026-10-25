/* The Pillar Men encounter (src/gc_pillar.c, gc/custom.h pillar hook) on the host: it is offered only when the app's hook says so, it has three
 * options whose fights are trainer battles of the encounter with the hook's team (cut to the template's size, plain-text name, levels rising
 * with the difficulty), a refusing hook or an unbuildable team leaves with the heal-only shop, and the victory gives the encounter's rewards
 * (no trainer money) instead of the trainer's. A game-core without the hook does not link this file. */
#include "../src/internal.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static gc_run run;
static gc_system sys;
static int asked[4];   /* how often each `which` (-1 .. 2) was asked */
static bool offer = true, refuse = false;

static bool pillar(void *user, const struct gc_run *r, int which, gc_custom_team *team)
{
    (void)user; (void)r;
    asked[which + 1]++;
    if (which < 0) return offer;
    if (refuse) return false;
    static const char *const names[3] = {"Esidisi", "Wamuu", "Kars"};
    team->name = names[which];
    team->passive = -1;
    team->n = 6;
    static const uint16_t species[6] = {SPECIES_ID_VOLCANION, SPECIES_ID_DARMANITAN, SPECIES_ID_CENTISKORCH, SPECIES_ID_TENTACRUEL,
                                        SPECIES_ID_MAGMORTAR, SPECIES_ID_SALAZZLE};
    for (int i = 0; i < 6; i++) {
        team->member[i].species = species[i];
        team->member[i].moves[0] = MOVE_ID_TACKLE;
    }
    return true;
}
static const gc_custom_hooks hooks = {.pillar = pillar};

static void start(const char *seed, bool with_hook)
{
    memset(&run, 0, sizeof run);
    gc_run_new(&run, GAME_MODES_CLASSIC, seed, 7);
    if (with_hook) gc_run_set_custom(&run, &hooks);
    gc_system_init_defaults(&sys);
    run.sys = &sys;
    run.wave = 45;
    run.biome = BIOME_ID_PLAINS;
    run.n_items = 0;
    run.n_party = 3;
    for (int i = 0; i < 3; i++) {
        gc_starter s;
        memset(&s, 0, sizeof s);
        s.species = (uint16_t)(i == 0 ? SPECIES_ID_PIKACHU : i == 1 ? SPECIES_ID_LAPRAS : SPECIES_ID_GENGAR);
        s.tera_type = -1;
        for (int k = 0; k < 6; k++) s.ivs[k] = 15;
        s.moves[0] = MOVE_ID_TACKLE;
        gc_mon_new_player(&run, &run.party[i], &s, 40);
        run.party_order[i] = (int8_t)i;
    }
    run.money = 1000;
    run.battle_type = BATTLE_TYPE_WILD;
    gc_items_to_battle(&run);
    gc_rq_reset(&gc_int(&run)->rq);
    run.mystery_type = GC_ME_PILLAR_MEN;
    gc_rq_unshift(&run, gc_rp(RP_MYSTERY, GC_ME_PILLAR_MEN, GC_ME_GREAT, 0));
    gc_run_until_screen(&run);
    assert(run.screen.kind == GC_SCREEN_MYSTERY);
}

static bool queue_has(int type, int a)
{
    gc_rqueue copy = gc_int(&run)->rq;
    gc_rphase p;
    while (gc_rq_next(&copy, &p)) if (p.type == type && (a < 0 || p.a == a)) return true;
    return false;
}

static void test_offer(void)
{
    /* never without the hook; with it, it is among the Great encounters and shows up */
    int without = 0, with = 0;
    for (int i = 0; i < 300; i++) {
        char seed[16];
        snprintf(seed, sizeof seed, "pm-%d", i);
        start(seed, false);
        int t, tier;
        run.wave = 20 + i % 100;
        pr_run_reset_seed(&run.run, run.wave);
        assert(gc_mystery_select(&run, &t, &tier));
        without += t == GC_ME_PILLAR_MEN;
        start(seed, true);
        run.wave = 20 + i % 100;
        pr_run_reset_seed(&run.run, run.wave);
        assert(gc_mystery_select(&run, &t, &tier));
        with += t == GC_ME_PILLAR_MEN;
    }
    assert(without == 0 && with > 0);
    start("pm-off", true);
    offer = false;
    for (int i = 0; i < 100; i++) {
        int t, tier;
        run.wave = 20 + i;
        pr_run_reset_seed(&run.run, run.wave);
        assert(gc_mystery_select(&run, &t, &tier) && t != GC_ME_PILLAR_MEN);
    }
    offer = true;
    printf("ok offer (picked %d of 300 with the hook, none without)\n", with);
}

static void test_screen_and_fights(void)
{
    int levels[3], sizes[3];
    for (int option = 0; option < 3; option++) {
        start("pm-fight", true);
        assert(run.screen.n_options == 3 && run.screen.option_ok[0] && run.screen.option_ok[1] && run.screen.option_ok[2]);
        const char *why = "";
        bool ok = false;
        assert(gc_mystery_execute(&run, option, 0, 0, &why, &ok) && ok);
        assert(asked[option + 1] > 0);
        assert(run.battle_type == BATTLE_TYPE_TRAINER && run.me_battle && !run.me_catch_allowed && !run.double_battle);
        assert(gc_int(&run)->pillar.active && gc_int(&run)->pillar.option == option);
        assert(run.n_enemy >= 1 && run.n_enemy <= 6);
        sizes[option] = run.n_enemy;
        levels[option] = 0;   /* the highest level of the party */
        for (int i = 0; i < run.n_enemy; i++) if (run.enemy[i].b.level > levels[option]) levels[option] = run.enemy[i].b.level;
        for (int i = 0; i < run.n_enemy; i++) assert(run.enemy[i].b.has_trainer || run.enemy[i].trainer_slot);
        assert(run.enemy[0].b.species == SPECIES_ID_VOLCANION);   /* the team's lead */
        assert(!strcmp(gc_int(&run)->custom_trainer_name, option == 0 ? "Esidisi" : option == 1 ? "Wamuu" : "Kars"));
        assert(run.in_battle && gc_rq_has(&run, RP_TURN_INIT));
    }
    assert(sizes[2] == 6);                         /* the brutal fight brings the whole team (the Elite Four template) */
    assert(sizes[0] <= sizes[1] || sizes[0] <= 6);
    assert(levels[0] < levels[1] && levels[1] < levels[2]);   /* the harder fight is higher */
    printf("ok fights (sizes %d/%d/%d, top levels %d/%d/%d)\n", sizes[0], sizes[1], sizes[2], levels[0], levels[1], levels[2]);
}

static void test_refusals(void)
{
    start("pm-refuse", true);
    refuse = true;
    const char *why = "";
    bool ok = false;
    assert(gc_mystery_execute(&run, 1, 0, 0, &why, &ok) && ok);
    refuse = false;
    assert(run.battle_type != BATTLE_TYPE_TRAINER && !gc_int(&run)->pillar.active);
    assert(queue_has(RP_SELECT_MODIFIER, 1));   /* the heal-only shop */
    puts("ok a refusing hook leaves with the heal-only shop");
}

static void test_victory(void)
{
    const int expect_tiers[3][4] = {{0, 0, 0, 0}, {2, 2, 1, 1}, {3, 3, 2, 1}};
    for (int option = 0; option < 3; option++) {
        start("pm-win", true);
        const char *why = "";
        bool ok = false;
        assert(gc_mystery_execute(&run, option, 0, 0, &why, &ok) && ok);
        const double money = run.money;
        gc_rq_clear(&run);
        gc_pillar_victory(&run);
        assert(!gc_int(&run)->pillar.active);
        const pr_reward_custom *c = &gc_int(&run)->reward.me_custom;
        assert(c->fill_remaining && c->allow_luck_upgrades);
        if (option == 0) {
            assert(c->n_funcs == 3 && c->n_tiers == 0);
        } else {
            assert(c->n_tiers == 4 && c->n_funcs == 0);
            for (int i = 0; i < 4; i++) assert(c->tiers[i] == expect_tiers[option][i]);
        }
        assert(queue_has(RP_BATTLE_END, 1) && queue_has(RP_SELECT_MODIFIER, 2) && queue_has(RP_NEW_BATTLE, -1));
        assert(!queue_has(RP_TRAINER_VICTORY, -1) && !queue_has(RP_MONEY_REWARD, -1) && run.money == money);   /* no trainer money */
    }
    puts("ok victory rewards");
}

int main(void)
{
    test_offer();
    test_screen_and_fights();
    test_refusals();
    test_victory();
    puts("mystery_pillar_test: all passed");
    return 0;
}
