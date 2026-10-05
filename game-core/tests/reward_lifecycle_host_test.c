#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>

static gc_run run, loaded;
static gc_system sys;
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"lifecycle host line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void setup(int species)
{
    gc_system_init_defaults(&sys);
    gc_run_new(&run, GAME_MODES_CLASSIC, "lifecycle-host", 777);
    gc_starter s = {.species = SPECIES_ID_GROWLITHE, .tera_type = -1};
    for (int j = 0; j < 6; j++) s.ivs[j] = 15;
    const char *why;
    CHECK(gc_run_set_starters(&run, &s, 1, &why));
    run.sys = &sys;
    if (species != s.species) {
        s.species = species;
        gc_mon_new_player(&run, &run.party[0], &s, 50);
        run.bt.party[0][0] = &run.party[0].b;
        run.bt.field[0] = &run.party[0].b;
    }
    gc_rq_reset(&gc_int(&run)->rq);
    CHECK(gc_system_set_pokemon_caught(&sys,&run.party[0],true,false));
    run.screen.kind = GC_SCREEN_NONE;
}
static void reward_screen(gc_offer o)
{
    gc_rqueue *q = &gc_int(&run)->rq;
    q->current = gc_rp(RP_SELECT_MODIFIER,0,0,0); q->waiting = true;
    memset(&run.screen,0,sizeof run.screen);
    run.screen.kind = GC_SCREEN_REWARD;
    run.screen.options[0] = o; run.screen.n_options = 1;
}
int main(void)
{
    const char *why;
    gc_offer stone = {.key="EVOLUTION_ITEM", .arg=5, .target=GC_TARGET_POKEMON};
    setup(SPECIES_ID_GROWLITHE);
    run.party[0].pause_evolutions = true;
    reward_screen(stone);
    gc_choice pick = {GC_CH_REWARD,0,0,-1};
    CHECK(gc_run_choose(&run,&pick,&why));
    CHECK(run.screen.kind == GC_SCREEN_EVOLVE);
    CHECK(run.party[0].b.species == SPECIES_ID_GROWLITHE);
    CHECK(run.n_items == 0);
    gc_choice cancel = {GC_CH_CANCEL,1,0,0};
    CHECK(gc_run_choose(&run,&cancel,&why));
    CHECK(run.party[0].b.species == SPECIES_ID_GROWLITHE);
    CHECK(run.party[0].pause_evolutions);
    CHECK(sys.dex[SPECIES_ID_ARCANINE].caught_attr == 0);

    setup(SPECIES_ID_GROWLITHE);
    gc_mon *m = &run.party[0];
    m->b.n_moves = 4;
    const int moves[] = {MOVE_ID_TACKLE,MOVE_ID_GROWL,MOVE_ID_SCRATCH,MOVE_ID_LEER};
    for (int j = 0; j < 4; j++) gc_mon_set_move(m,j,moves[j]);
    reward_screen(stone);
    CHECK(gc_run_choose(&run,&pick,&why));
    gc_choice accept = {GC_CH_EVOLVE,0,0,0};
    CHECK(gc_run_choose(&run,&accept,&why));
    CHECK(m->b.species == SPECIES_ID_ARCANINE);
    CHECK(run.screen.kind == GC_SCREEN_LEARN_MOVE);
    CHECK(run.screen.move == MOVE_ID_EXTREME_SPEED);
    gc_choice learn = {GC_CH_FORGET,2,0,0};
    CHECK(gc_run_choose(&run,&learn,&why));
    CHECK(m->b.moves[2] == MOVE_ID_EXTREME_SPEED);
    CHECK(m->b.pp_up[2] == 0 && m->b.pp_used[2] == 0);
    CHECK(!m->pause_evolutions);
    CHECK(sys.dex[SPECIES_ID_ARCANINE].caught_attr != 0);

    gc_offer core = {.key="FORM_CHANGE_ITEM", .arg=FORM_CHANGE_ITEM_GRISEOUS_CORE, .target=GC_TARGET_POKEMON};
    setup(SPECIES_ID_GIRATINA);
    const uint32_t id = run.party[0].b.id;
    const int move0 = run.party[0].b.moves[0];
    reward_screen(core);
    CHECK(gc_run_choose(&run,&pick,&why));
    CHECK(!strcmp(gc_form(SPECIES_ID_GIRATINA,run.party[0].b.form)->form_key,"origin"));
    CHECK(run.n_items == 1 && run.items[0].pokemon_id == id && run.items[0].stack == 1);
    CHECK(run.party[0].b.moves[0] == move0);
    CHECK(sys.dex[SPECIES_ID_GIRATINA].caught_attr != 0);
    const int balls = run.pokeballs[0];
    CHECK(gc_reward_lifecycle_apply(&run,&core,0,&why) == GC_APPLY_DONE);
    CHECK(run.pokeballs[0] == balls + 5);
    CHECK(run.items[0].stack == 1);

    /* Persist at the existing supported command/save boundary. Pending evolution
     * and form phases themselves are deliberately not save points. */
    gc_rq_reset(&gc_int(&run)->rq);
    gc_int(&run)->rq.current = gc_rp(RP_COMMAND,0,0,0);
    gc_int(&run)->rq.waiting = true;
    gc_int(&run)->at_save_point = true;
    run.in_battle = true; run.screen.kind = GC_SCREEN_COMMAND;
    unsigned char *buf = malloc(GC_SAVE_MAX); CHECK(buf != NULL);
    CHECK(gc_run_can_save(&run));
    size_t n = gc_run_save(&run,buf,GC_SAVE_MAX); CHECK(n > 0);
    CHECK(gc_run_load(&loaded,buf,n,&why));
    CHECK(loaded.party[0].b.id == id && loaded.party[0].b.form == run.party[0].b.form);
    CHECK(loaded.party[0].b.moves[0] == move0);
    CHECK(!memcmp(loaded.party[0].b.stats,run.party[0].b.stats,sizeof run.party[0].b.stats));
    CHECK(loaded.n_items == 1 && loaded.items[0].pokemon_id == id && loaded.items[0].stack == 1);
    free(buf);

    setup(SPECIES_ID_GROWLITHE);
    reward_screen(stone);
    gc_choice invalid = {GC_CH_REWARD,0,4,-1};
    CHECK(!gc_run_choose(&run,&invalid,&why));
    CHECK(run.screen.kind == GC_SCREEN_REWARD && run.n_items == 0);
    gc_choice skip = {GC_CH_SKIP,0,0,0};
    CHECK(gc_run_choose(&run,&skip,&why));
    CHECK(run.party[0].b.species == SPECIES_ID_GROWLITHE);
    printf("reward lifecycle host: %u checks passed\n",checks);
    return 0;
}
