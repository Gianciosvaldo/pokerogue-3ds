/* Mysterious Challengers (src/gc_pillar.c, gc_mc_execute): the three options are trainer battles of the encounter with the teams game-core
 * generates (easy: the normal template; hard: one STRONGER and min(ceil(wave / 20), 5) AVERAGE members; brutal: the Elite Four's six), levels rising with
 * the difficulty, no trainer money, and the victory gives the encounter's rewards. The screen carries the three trainers for the silhouettes. */
#include "../src/internal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static gc_run run;
static gc_system sys;

static void start(const char *seed, int wave)
{
    memset(&run, 0, sizeof run);
    gc_run_new(&run, GAME_MODES_CLASSIC, seed, 7);
    gc_system_init_defaults(&sys);
    run.sys = &sys;
    run.wave = wave;
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
    run.mystery_type = GC_ME_MYSTERIOUS_CHALLENGERS;
    gc_rq_unshift(&run, gc_rp(RP_MYSTERY, GC_ME_MYSTERIOUS_CHALLENGERS, GC_ME_GREAT, 0));
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

static void test_fights(void)
{
    for (int wave = 25; wave <= 145; wave += 40) {
        int levels[3] = {0, 0, 0}, sizes[3],hard_type=-1;
        for (int option = 0; option < 3; option++) {
            start("mc-fight", wave);
            assert(run.screen.n_options == 3);
            for (int i = 0; i < 3; i++) assert(run.screen.mc_trainer[i] >= 0);
            const char *why = "";
            bool ok = false;
            assert(gc_mystery_execute(&run, option, 0, 0, &why, &ok) && ok);
            assert(run.battle_type == BATTLE_TYPE_TRAINER && run.me_battle && !run.me_catch_allowed);
            assert(run.double_battle == (run.screen.mc_variant[option] == TRAINER_VARIANT_DOUBLE));
            if(option==1)hard_type=run.trainer_type;
            /* the fought trainer is the one the screen showed */
            assert(gc_int(&run)->pillar.active && gc_int(&run)->pillar.mc && gc_int(&run)->pillar.option == option);
            assert(run.trainer_type == run.screen.mc_trainer[option]);
            assert(run.n_enemy >= 1 && run.n_enemy <= 6);
            sizes[option] = run.n_enemy;
            for (int i = 0; i < run.n_enemy; i++) if (run.enemy[i].b.level > levels[option]) levels[option] = run.enemy[i].b.level;
            assert(run.in_battle && gc_rq_has(&run, RP_TURN_INIT));
        }
        assert(sizes[2] == 6);
        const int avg = (wave + 19) / 20 > 5 ? 5 : (wave + 19) / 20;
        /* Source clone preserves partyTemplateFunc on the hard trainer.
         * The native boundary corpus separately verifies its exact template. */
        const pr_trainer_info *hard=gc_trainer_info_of(hard_type);
        assert(hard);
        if(hard->has_template_func){const pr_party_template *t=gc_party_template_of(&run,hard_type,0,wave);assert(t&&sizes[1]==t->size);}
        else assert(sizes[1] == 1 + avg);
        assert(levels[0] <= levels[1] && levels[1] < levels[2]);
        printf("ok fights wave %d (sizes %d/%d/%d, top levels %d/%d/%d)\n", wave, sizes[0], sizes[1], sizes[2], levels[0], levels[1], levels[2]);
    }
}

static void test_victory(void)
{
    const int expect_tiers[3][4] = {{0, 0, 0, 0}, {2, 2, 1, 1}, {3, 3, 2, 1}};
    for (int option = 0; option < 3; option++) {
        start("mc-win", 45);
        const char *why = "";
        bool ok = false;
        assert(gc_mystery_execute(&run, option, 0, 0, &why, &ok) && ok);
        const double money = run.money;
        gc_rq_clear(&run);
        gc_pillar_victory(&run);
        const pr_reward_custom *c = &gc_int(&run)->reward.me_custom;
        assert(c->fill_remaining);
        if (option == 0) {
            assert(c->n_funcs == 3 && c->n_tiers == 0);
        } else {
            assert(c->n_tiers == 4 && c->n_funcs == 0);
            for (int i = 0; i < 4; i++) assert(c->tiers[i] == expect_tiers[option][i]);
        }
        assert(queue_has(RP_BATTLE_END, 1) && queue_has(RP_SELECT_MODIFIER, 2) && queue_has(RP_NEW_BATTLE, -1));
        assert(!queue_has(RP_TRAINER_VICTORY, -1) && !queue_has(RP_MONEY_REWARD, -1) && run.money == money);
    }
    puts("ok victory rewards");
}

int main(void)
{
    test_fights();
    test_victory();
    puts("mystery_challengers_test: all passed");
    return 0;
}
