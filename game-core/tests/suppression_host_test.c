/* Augmented singles command/save consistency, not a natural starter/run proof. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run run, loaded;
static unsigned char saved[GC_SAVE_MAX];
static int checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"suppression host line%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static const char *why;
static pr_pokemon *setup(int mode)
{
    gc_run_new(&run, GAME_MODES_CLASSIC, "suppression-command-save", 777);
    gc_starter starter = {.species=SPECIES_ID_BULBASAUR, .tera_type=-1,
        .moves={mode ? MOVE_ID_CORE_ENFORCER : MOVE_ID_GASTRO_ACID}};
    memset(starter.ivs, 15, sizeof starter.ivs);
    CHECK(gc_run_set_starters(&run, &starter, 1, &why));
    gc_event ev; while (gc_run_next_event(&run, &ev)) {}
    pr_enemy_gen gen = {.id=run.enemy[0].b.id, .gender=-1, .fusion_species=-1};
    gc_mon_new_enemy(&run, &run.enemy[0], SPECIES_ID_CHANSEY, 20, &gen, TRAINER_SLOT_NONE);
    pr_pokemon *enemy = &run.enemy[0].b;
    run.bt.field[2] = run.bt.party[1][0] = enemy; enemy->on_field = true;
    enemy->n_moves = 1; pr_set_original_move(enemy, 0, MOVE_ID_SPLASH);
    enemy->custom_ability = ABILITY_ID_LEVITATE; pr_refresh_ability(enemy);
    pr_calculate_stats(&run.bt, enemy); enemy->hp = enemy->stats[STAT_HP];
    CHECK(pr_can_apply_ability(&run.bt, enemy, ABILITY_ID_LEVITATE));
    CHECK(gc_run_can_save(&run));
    return enemy;
}
int main(void)
{
    for (int mode = 0; mode < 2; mode++) {
        pr_pokemon *enemy = setup(mode);
        gc_choice fight = {GC_CH_FIGHT, 0, -1, 0};
        CHECK(gc_run_choose(&run, &fight, &why));
        CHECK(enemy->hp > 0 && enemy->ability_suppressed);
        /* The next command boundary has reset acted. Observe actual event order. */
        int enemy_anim = -1, player_anim = -1;
        for (int i = 0; i < run.n_battle_events; i++) {
            const pr_event *event = &run.battle_events[i];
            if (event->type != PR_EV_MOVE_ANIM) continue;
            if (event->battler == 2 && event->id == MOVE_ID_SPLASH) enemy_anim = i;
            if (event->battler == 0 && event->id == (mode ? MOVE_ID_CORE_ENFORCER : MOVE_ID_GASTRO_ACID)) player_anim = i;
        }
        CHECK(enemy_anim >= 0 && player_anim > enemy_anim);
        CHECK(!enemy->acted);
        CHECK(!pr_can_apply_ability(&run.bt, enemy, ABILITY_ID_LEVITATE));
        gc_event ev; while (gc_run_next_event(&run, &ev)) {}
        /* The app only permits saves before the first turn, never here. */
        CHECK(!gc_run_can_save(&run)); CHECK(!gc_run_save(&run, saved, sizeof saved));

        /* Separate augmented projection: apply the real public battle callback
         * at the existing first-command save point, using a scratch phase queue. */
        enemy = setup(mode);
        pr_engine scratch = {0}; scratch.current = -1; scratch.n_levels = 1;
        pr_engine *previous = run.bt.engine; run.bt.engine = &scratch;
        int attr = -1;
        const pr_move *move = &pr_moves[MOVE_ID_GASTRO_ACID];
        for (int j = 0; j < move->nattr; j++) if (pr_attrs[move->attr0+j].cls == PR_AC_SuppressAbilitiesAttr) attr = move->attr0+j;
        CHECK(attr >= 0);
        int ph = pr_phase_new(&scratch, PH_MOVE_EFFECT, 0); scratch.pool[ph].move_id = MOVE_ID_GASTRO_ACID;
        pr_apply_move_effect(&run.bt, &scratch, ph, attr, run.bt.field[0], enemy);
        run.bt.engine = previous;
        /* Commit the global RNG as the ordinary game battle caller does. */
        run.run.rng = run.bt.rng;
        CHECK(!scratch.overflow && !run.bt.n_unported && enemy->ability_suppressed);
        CHECK(gc_run_can_save(&run));
        size_t n = gc_run_save(&run, saved, sizeof saved);
        CHECK(n > 0); CHECK(gc_run_load(&loaded, saved, n, &why)); CHECK(!loaded.load_dropped);
        CHECK(loaded.enemy[0].b.ability_suppressed);
        CHECK(!pr_can_apply_ability(&loaded.bt, &loaded.enemy[0].b, ABILITY_ID_LEVITATE));
        CHECK(gc_run_choose(&run, &fight, &why)); CHECK(gc_run_choose(&loaded, &fight, &why));
        CHECK(enemy->hp > 0 && loaded.enemy[0].b.hp == enemy->hp);
        CHECK(enemy->ability_suppressed && loaded.enemy[0].b.ability_suppressed);
        CHECK(!memcmp(&run.bt.rng, &loaded.bt.rng, sizeof run.bt.rng));
        CHECK(!memcmp(&run.bt.battle_rng, &loaded.bt.battle_rng, sizeof run.bt.battle_rng));
        CHECK(!memcmp(&run.run.rng, &loaded.run.rng, sizeof run.run.rng));
    }
    printf("Suppression command/save: %d checks passed\n", checks);
    return 0;
}
