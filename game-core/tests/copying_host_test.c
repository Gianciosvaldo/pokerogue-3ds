/* Actual commands, shared PP, legal save/load and summon reset for copying moves. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run run,loaded;static uint8_t saved[GC_SAVE_MAX],mutated[GC_SAVE_MAX];static size_t saved_n;static int checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"copying host line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static uint32_t hashname(const char *s){uint32_t h=2166136261u;for(;*s;s++)h=(h^(uint8_t)*s)*16777619u;return h;}
static uint32_t checksum(const uint8_t *p,size_t n){uint32_t c=~0u;for(size_t i=0;i<n;i++){c^=p[i];for(int k=0;k<8;k++)c=c>>1^(0xedb88320u&(0u-(c&1)));}return ~c;}
static int hide_field(const char *name){uint32_t from=hashname(name),to=hashname("missing-copying-field");int hits=0;for(size_t i=20;i+4<=saved_n;i++)if(!memcmp(mutated+i,&from,4)){memcpy(mutated+i,&to,4);hits++;}uint32_t crc=checksum(mutated+20,saved_n-20);memcpy(mutated+16,&crc,4);return hits;}
static void drain(gc_run *r){gc_event ev;while(gc_run_next_event(r,&ev)){};}
static void fight(gc_run *r,int slot){const char *why=NULL;gc_choice c={GC_CH_FIGHT,slot,-1,0};CHECK(gc_run_choose(r,&c,&why));drain(r);}
static void next_save_point(void){
 const char *why=NULL;pr_pokemon *p=run.bt.field[0],*t=run.bt.field[2];CHECK(p&&t);
 t->species=SPECIES_ID_RATTATA;t->form=0;t->custom_ability=ABILITY_ID_BALL_FETCH;t->ability_override=-1;pr_refresh_ability(t);pr_recalculate_stats(&run.bt,t);t->hp=1;
 pr_set_move(&run.bt,&run.eng,p,1,MOVE_ID_TACKLE);
 fight(&run,1);
 for(int step=0;step<25&&!gc_run_can_save(&run);step++){
  gc_choice pick={0};
  if(run.screen.kind==GC_SCREEN_REWARD)pick.kind=GC_CH_SKIP;
  else if(run.screen.kind==GC_SCREEN_LEARN_MOVE){pick.kind=GC_CH_FORGET;pick.a=-1;}
  else if(run.screen.kind==GC_SCREEN_EVOLVE){pick.kind=GC_CH_CANCEL;pick.a=1;}
  else if(run.screen.kind==GC_SCREEN_BIOME){pick.kind=GC_CH_BIOME;pick.a=0;}
  else {fprintf(stderr,"unexpected copying continuation screen%d\n",run.screen.kind);exit(1);}
  CHECK(gc_run_choose(&run,&pick,&why));drain(&run);
 }
 CHECK(gc_run_can_save(&run));
}
static void roundtrip(void){const char *why=NULL;next_save_point();size_t n=gc_run_save(&run,saved,sizeof saved);CHECK(n>0);saved_n=n;CHECK(gc_run_load(&loaded,saved,n,&why));CHECK(gc_run_can_save(&loaded));}
int main(void){
 const char *why=NULL;gc_run_new(&run,GAME_MODES_CLASSIC,"copying-save",777);
 gc_starter starter={.species=SPECIES_ID_BULBASAUR,.tera_type=-1,.moves={MOVE_ID_TACKLE}};memset(starter.ivs,15,sizeof starter.ivs);
 CHECK(gc_run_set_starters(&run,&starter,1,&why));drain(&run);CHECK(gc_run_can_save(&run));
 gc_mon *user=&run.party[run.party_order[0]],*enemy=&run.enemy[0];
 user->b.n_moves=0;gc_mon_set_move(user,0,MOVE_ID_MIMIC);gc_mon_set_move(user,1,MOVE_ID_SPLASH);gc_mon_set_move(user,2,MOVE_ID_SKETCH);
 enemy->b.n_moves=0;gc_mon_set_move(enemy,0,MOVE_ID_SPLASH);enemy->b.custom_ability=ABILITY_ID_BALL_FETCH;enemy->b.ability_override=-1;pr_refresh_ability(&enemy->b);
 user->b.stat_stages[STAT_SPD-1]=-6;fight(&run,0);
 CHECK(user->b.summon_moves_active&&!user->b.transformed&&user->b.summon_moves_alias_mask==6);
 CHECK(user->b.moves[0]==MOVE_ID_MIMIC&&user->b.pp_used[0]==1&&PR_MOVES(&user->b)[0]==MOVE_ID_SPLASH&&PR_PP_USED(&user->b)[0]==0);
 roundtrip();pr_pokemon *back=loaded.bt.field[0];CHECK(back&&back->summon_moves_active&&!back->transformed&&back->summon_moves_alias_mask==6);
 pr_use_moveset_pp(back,1,true,1);CHECK(back->pp_used[1]==2&&back->transform_pp_used[1]==2);
 fight(&loaded,0);CHECK(back->pp_used[0]==1&&back->transform_pp_used[0]==1);
 pr_reset_summon_data(back);CHECK(!PR_HAS_SUMMON_MOVES(back)&&!back->summon_moves_alias_mask&&back->moves[0]==MOVE_ID_MIMIC&&back->pp_used[1]==2);
 enemy->b.n_moves=0;gc_mon_set_move(enemy,0,MOVE_ID_GROWL);user->b.stat_stages[STAT_SPD-1]=-6;
 fight(&run,2);CHECK(user->b.moves[2]==MOVE_ID_GROWL&&user->b.transform_moves[2]==MOVE_ID_GROWL&&user->b.pp_used[2]==0);
 fight(&run,2);CHECK(user->b.pp_used[2]==1&&user->b.transform_pp_used[2]==1);
 roundtrip();CHECK(loaded.bt.field[0]->moves[2]==MOVE_ID_GROWL&&loaded.bt.field[0]->transform_pp_used[2]==1);
 /* Now follow the real Transform -> Sketch -> copied-move path. */
 pr_reset_summon_data(&user->b);gc_mon_set_move(user,0,MOVE_ID_TRANSFORM);gc_mon_set_move(user,2,MOVE_ID_SKETCH);
 enemy->b.n_moves=0;gc_mon_set_move(enemy,0,MOVE_ID_SPLASH);gc_mon_set_move(enemy,1,MOVE_ID_MIMIC);gc_mon_set_move(enemy,2,MOVE_ID_SKETCH);enemy->b.pp_used[1]=pr_moves[MOVE_ID_MIMIC].pp;enemy->b.pp_used[2]=pr_moves[MOVE_ID_SKETCH].pp;
 fight(&run,0);CHECK(user->b.transformed&&user->b.summon_moves_active&&user->b.summon_moves_alias_mask==0);
 enemy->b.n_moves=0;gc_mon_set_move(enemy,0,MOVE_ID_GROWL);user->b.stat_stages[STAT_SPD-1]=-6;
 fight(&run,2);CHECK(user->b.transformed&&user->b.moves[2]==MOVE_ID_GROWL&&user->b.transform_moves[2]==MOVE_ID_GROWL&&user->b.summon_moves_alias_mask==4);
 fight(&run,2);CHECK(user->b.pp_used[2]==1&&user->b.transform_pp_used[2]==1);
 roundtrip();back=loaded.bt.field[0];CHECK(back&&back->transformed&&back->summon_moves_active&&back->summon_moves_alias_mask==6&&back->pp_used[2]==1&&back->transform_pp_used[2]==1);
 /* Corrupt copying state through the real writer, preserving legal save points
  * and valid checksums, then require the reader to reject it. */
 pr_pokemon valid=*run.bt.field[0];
 for(int fault=0;fault<8;fault++){
  pr_pokemon *p=run.bt.field[0];*p=valid;
  if(fault==0){uint8_t byte=2;memcpy(&p->summon_moves_active,&byte,1);}
  else if(fault==1)p->summon_moves_alias_mask=16;
  else if(fault==2)p->summon_moves_alias_mask|=8;
  else if(fault==3)p->transform_moves[2]=MOVE_ID_LEER;
  else if(fault==4)p->transform_pp_used[2]++;
  else if(fault==5)p->transform_pp_up[2]=1;
  else if(fault==6)p->transform_max_pp_override[2]=5;
  else p->transform_n_moves=5;
  size_t n=gc_run_save(&run,mutated,sizeof mutated);CHECK(n>0);CHECK(!gc_run_load(&loaded,mutated,n,&why));
 }
 *run.bt.field[0]=valid;
 const char *fields[]={"summon_moves_active","summon_moves_alias_mask"};
 for(int i=0;i<2;i++){memcpy(mutated,saved,saved_n);CHECK(hide_field(fields[i])>0);CHECK(!gc_run_load(&loaded,mutated,saved_n,&why));}
 CHECK(gc_run_load(&loaded,saved,saved_n,&why));back=loaded.bt.field[0];
 pr_reset_summon_data(back);CHECK(!PR_HAS_SUMMON_MOVES(back)&&back->moves[2]==MOVE_ID_GROWL&&back->pp_used[2]==1);
 /* Historical normal actors can omit both metadata fields. */
 pr_reset_summon_data(run.bt.field[0]);saved_n=gc_run_save(&run,saved,sizeof saved);CHECK(saved_n>0);
 memcpy(mutated,saved,saved_n);CHECK(hide_field(fields[0])>0);CHECK(hide_field(fields[1])>0);
 CHECK(gc_run_load(&loaded,mutated,saved_n,&why));CHECK(!PR_HAS_SUMMON_MOVES(loaded.bt.field[0]));
 printf("copying commands/PP/save/reset: %d checks passed\n",checks);return 0;
}
