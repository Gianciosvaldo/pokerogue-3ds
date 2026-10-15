/* Constructor/move/save metadata fixture; no legal final-boss/doubles-clear claim. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run run,loaded;static unsigned char saved[GC_SAVE_MAX];static int checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"boss PP line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
int main(void){
 const char *why=NULL;
 for(int mode=0;mode<2;mode++){
  gc_run_new(&run,mode?GAME_MODES_ENDLESS:GAME_MODES_CLASSIC,"boss-pp",777);gc_starter s={.species=SPECIES_ID_BULBASAUR,.tera_type=-1,.moves={MOVE_ID_TACKLE}};memset(s.ivs,15,sizeof s.ivs);CHECK(gc_run_set_starters(&run,&s,1,&why));gc_event event;while(gc_run_next_event(&run,&event)){}CHECK(gc_run_can_save(&run));
  pr_enemy_gen g={.id=123,.form=1,.gender=-1,.friendship=0,.fusion_species=-1,.boss_segments=4};gc_mon_new_enemy(&run,&run.enemy[0],SPECIES_ID_ETERNATUS,200,&g,TRAINER_SLOT_NONE);run.n_enemy=1;run.enemy[0].b.on_field=true;run.bt.field[2]=run.bt.party[1][0]=&run.enemy[0].b;run.bt.n_party[1]=1;
  pr_rng before=run.run.rng;CHECK(gc_enemy_moveset(&run,&run.enemy[0],&g));CHECK(!memcmp(&before,&run.run.rng,sizeof before));pr_pokemon *p=&run.enemy[0].b;CHECK(p->n_moves==4&&p->moves[3]==MOVE_ID_RECOVER&&p->pp_up[3]==-4&&p->max_pp_override[3]==-1);pr_use_moveset_pp(p,3,false,8);CHECK(p->pp_used[3]==1);
  size_t n=gc_run_save(&run,saved,sizeof saved);CHECK(n>0);CHECK(gc_run_load(&loaded,saved,n,&why));CHECK(!loaded.load_dropped);CHECK(loaded.enemy[0].b.pp_up[3]==-4&&loaded.enemy[0].b.pp_used[3]==1&&loaded.enemy[0].b.moves[3]==MOVE_ID_RECOVER&&loaded.enemy[0].b.max_pp_override[3]==-1);
  const int bad[]={-128,-3,-2,-1,4,127};for(unsigned k=0;k<sizeof bad/sizeof *bad;k++){p->pp_up[3]=(int8_t)bad[k];n=gc_run_save(&run,saved,sizeof saved);CHECK(n>0);CHECK(!gc_run_load(&loaded,saved,n,&why));}p->pp_up[3]=-4;
 }
 printf("Boss signed PP constructor/use/save: %d checks passed\n",checks);return 0;
}
