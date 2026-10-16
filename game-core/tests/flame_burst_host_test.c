/* Augmented raw callback/save continuity; full doubles execution is proven separately. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run run,loaded;static unsigned char saved[GC_SAVE_MAX];static int checks;
#define CHECK(x) do { checks++; if(!(x)){fprintf(stderr,"Flame Burst host line%d: %s\n",__LINE__,#x);exit(1);} } while(0)
int main(void){
 const char *why=NULL;int damage[2]={0};
 for(int guard=0;guard<2;guard++){
  gc_run_new(&run,GAME_MODES_CLASSIC,"flame-burst-command-save",777);
  gc_starter starters[2]={{.species=SPECIES_ID_BULBASAUR,.tera_type=-1,.moves={MOVE_ID_SPLASH}},{.species=SPECIES_ID_CHARMANDER,.tera_type=-1,.moves={MOVE_ID_SPLASH}}};
  for(int k=0;k<2;k++)memset(starters[k].ivs,15,sizeof starters[k].ivs);
  CHECK(gc_run_set_starters(&run,starters,2,&why));gc_event ev;while(gc_run_next_event(&run,&ev)){}
  CHECK(gc_run_can_save(&run));pr_pokemon *p=&run.party[0].b,*target=&run.party[1].b,*user=&run.enemy[0].b;
  p->custom_ability=guard?ABILITY_ID_MAGIC_GUARD:ABILITY_ID_NONE;pr_refresh_ability(p);
  user->n_moves=1;pr_set_original_move(user,0,MOVE_ID_SPLASH);user->custom_ability=ABILITY_ID_NONE;pr_refresh_ability(user);
  int before=p->hp;pr_engine scratch={0};scratch.n_levels=1;scratch.current=-1;pr_engine *old=run.bt.engine;run.bt.engine=&scratch;
  const pr_move *move=&pr_moves[MOVE_ID_FLAME_BURST];int attr=-1;for(int j=0;j<move->nattr;j++)if(pr_attrs[move->attr0+j].cls==PR_AC_FlameBurstAttr)attr=move->attr0+j;CHECK(attr>=0);
  int ph=pr_phase_new(&scratch,PH_MOVE_EFFECT,2);scratch.pool[ph].move_id=MOVE_ID_FLAME_BURST;
  /* Raw source getAlly on a singles reserve returns the side's first member. */
  CHECK(pr_get_ally(&run.bt,target)==p);pr_apply_move_effect(&run.bt,&scratch,ph,attr,user,target);run.bt.engine=old;
  CHECK(!run.bt.n_unported&&!scratch.overflow);damage[guard]=before-p->hp;CHECK(p->hp>0);
  run.run.rng=run.bt.rng;
  CHECK(gc_run_can_save(&run));size_t n=gc_run_save(&run,saved,sizeof saved);CHECK(n>0);
  CHECK(gc_run_load(&loaded,saved,n,&why));CHECK(!loaded.load_dropped);CHECK(loaded.party[0].b.hp==p->hp);CHECK(loaded.party[0].b.custom_ability==p->custom_ability);
  gc_choice fight={GC_CH_FIGHT,0,-1,0};CHECK(gc_run_choose(&run,&fight,&why));CHECK(gc_run_choose(&loaded,&fight,&why));
  CHECK(run.screen.kind==GC_SCREEN_COMMAND&&loaded.screen.kind==GC_SCREEN_COMMAND);
  CHECK(run.party[0].b.hp==loaded.party[0].b.hp&&run.enemy[0].b.hp==loaded.enemy[0].b.hp);
  CHECK(!memcmp(&run.run.rng,&loaded.run.rng,sizeof(pr_rng)));CHECK(!memcmp(&run.bt.rng,&loaded.bt.rng,sizeof(pr_rng)));CHECK(!memcmp(&run.bt.battle_rng,&loaded.bt.battle_rng,sizeof(pr_rng)));
 }
 CHECK(damage[0]>0&&damage[1]==0);printf("Flame Burst callback/save/command: %d checks passed\n",checks);return 0;
}
