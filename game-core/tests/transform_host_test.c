/* Real command save/load and move-learning boundaries while transformed. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static gc_run run, loaded;
static unsigned char saved[GC_SAVE_MAX], mutated[GC_SAVE_MAX];
static int checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"Transform save line%d: %s\n",__LINE__,#x);exit(1); } } while(0)
static uint32_t hashname(const char *s){uint32_t h=2166136261u;for(;*s;s++)h=(h^(unsigned char)*s)*16777619u;return h;}
static uint32_t crc32(const unsigned char *p,size_t n){uint32_t c=~0u;for(size_t i=0;i<n;i++){c^=p[i];for(int k=0;k<8;k++)c=c>>1^(0xedb88320u&(0u-(c&1)));}return ~c;}
static int hide_field(size_t n,const char *name)
{
    memcpy(mutated,saved,n);uint32_t hash=hashname(name),unknown=hashname("missing-transform-field");int hits=0;
    for(size_t i=20;i+4<=n;i++)if(!memcmp(mutated+i,&hash,4)){memcpy(mutated+i,&unknown,4);hits++;}
    uint32_t crc=crc32(mutated+20,n-20);memcpy(mutated+16,&crc,4);return hits;
}
int main(void)
{
    const char *why=NULL;
    gc_run_new(&run,GAME_MODES_CLASSIC,"transform-save-test",777);
    gc_starter starter={.species=SPECIES_ID_DITTO,.tera_type=-1,.moves={MOVE_ID_TRANSFORM}};
    memset(starter.ivs,15,sizeof starter.ivs);
    CHECK(gc_run_set_starters(&run,&starter,1,&why));
    gc_event event;while(gc_run_next_event(&run,&event)){}
    CHECK(run.screen.kind==GC_SCREEN_COMMAND&&gc_run_can_save(&run));
    pr_pokemon *user=run.bt.field[0],*target=run.bt.field[2];CHECK(user&&target);
    run.party[0].variant=user->variant=2;user->shiny=true;target->variant=1;
    PR_PP_USED(user)[0]=3;PR_PP_UP(user)[0]=2;
    gc_mon_set_move(&run.enemy[0],0,MOVE_ID_SKETCH);
    gc_mon_set_move(&run.enemy[0],1,MOVE_ID_TACKLE);
    target->n_moves=2;target->pp_used[0]=1;target->pp_used[1]=4;target->pp_up[1]=3;
    target->gender=1;target->summon_stats[STAT_ATK]=77.5;target->stat_stages[STAT_ACC-1]=4;
    int hp=user->hp,max=user->stats[STAT_HP];
    int ph=pr_phase_new(&run.eng,PH_TRANSFORM,0);CHECK(ph>=0);run.eng.pool[ph].transform_target=2;
    pr_phase_run(&run.bt,&run.eng,ph);run.eng.used[ph]=false;
    CHECK(user->transformed&&user->species==SPECIES_ID_DITTO&&user->hp==hp&&user->stats[STAT_HP]==max);
    CHECK(PR_MOVES(user)[0]==MOVE_ID_SKETCH&&gc_move_max_pp(user,0)==1&&PR_PP_USED(user)[0]==0);
    CHECK(PR_MOVES(user)[1]==MOVE_ID_TACKLE&&gc_move_max_pp(user,1)==5&&PR_PP_UP(user)[1]==0);
    CHECK(pr_get_stat(user,STAT_ATK,false)==77.5&&pr_get_gender(user)==1&&user->stat_stages[STAT_ACC-1]==4);
    CHECK(user->moves[0]==MOVE_ID_TRANSFORM&&user->pp_used[0]==3&&user->pp_up[0]==2);
    /* Real learning writes the permanent moves, leaving the copied move usable. */
    gc_mon_set_move(&run.party[0],1,MOVE_ID_GROWL);
    CHECK(PR_MOVES(user)[1]==MOVE_ID_TACKLE&&user->moves[1]==MOVE_ID_GROWL);
    PR_PP_USED(user)[1]=2;
    size_t n=gc_run_save(&run,saved,sizeof saved);CHECK(n>0);
    CHECK(gc_run_load(&loaded,saved,n,&why));
    pr_pokemon *copy=loaded.bt.field[0];CHECK(copy&&copy->transformed);
    CHECK(copy->species==SPECIES_ID_DITTO&&PR_MOVES(copy)[0]==MOVE_ID_SKETCH&&PR_PP_USED(copy)[1]==2);
    CHECK(copy->hp==hp&&copy->stats[STAT_HP]==max&&copy->summon_stats[STAT_ATK]==77.5);
    CHECK(copy->shiny&&copy->variant==2&&loaded.party[0].variant==2);
    CHECK(copy->moves[0]==MOVE_ID_TRANSFORM&&copy->moves[1]==MOVE_ID_GROWL&&copy->pp_used[0]==3);
    const char *fields[]={"transformed","transform_species","transform_form","transform_gender","transform_moves",
        "transform_n_moves","transform_pp_used","transform_pp_up","transform_max_pp_override"};
    for(unsigned i=0;i<sizeof fields/sizeof *fields;i++){
        CHECK(hide_field(n,fields[i])>0);CHECK(!gc_run_load(&loaded,mutated,n,&why));
    }
    CHECK(gc_run_load(&loaded,saved,n,&why));copy=loaded.bt.field[0];
    pr_reset_summon_data(copy);
    CHECK(!copy->transformed&&PR_MOVES(copy)[0]==MOVE_ID_TRANSFORM&&PR_MOVES(copy)[1]==MOVE_ID_GROWL);
    CHECK(PR_PP_USED(copy)[0]==3&&PR_PP_UP(copy)[0]==2&&copy->max_pp_override[0]==-1&&copy->n_moves==2);
    CHECK(pr_pokemon_form(copy)->species==SPECIES_ID_DITTO&&copy->ability==copy->base_ability);
    CHECK(copy->summon_stats[STAT_ATK]==0&&copy->hp==hp);
    CHECK(copy->shiny&&copy->variant==2);
    /* Older normal saves may omit the new fields; they keep their real moves. */
    pr_reset_summon_data(user);n=gc_run_save(&run,saved,sizeof saved);CHECK(n>0);
    for(unsigned i=0;i<sizeof fields/sizeof *fields;i++){
        CHECK(hide_field(n,fields[i])>0);CHECK(gc_run_load(&loaded,mutated,n,&why));
        CHECK(!loaded.party[0].b.transformed&&loaded.party[0].b.moves[0]==MOVE_ID_TRANSFORM);
    }
    printf("Transform command/save/learn/reset: %d checks passed\n",checks);return 0;
}
