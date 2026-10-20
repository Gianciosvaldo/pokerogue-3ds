/* Actual encounter caller and legal save boundaries, including scene-owned Mint state. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static gc_run run,loaded;
static unsigned char saved[GC_SAVE_MAX];
static int checks,ctor_nature,new_double,ctor_calls,singles,doubles;
static bool watching;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"sync nature host line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
int __real_pr_run_new_battle(pr_run *,int,const pr_new_battle_ctx *,pr_new_battle *);
int __wrap_pr_run_new_battle(pr_run *r,int w,const pr_new_battle_ctx *ctx,pr_new_battle *out){int rc=__real_pr_run_new_battle(r,w,ctx,out);if(watching){CHECK(rc==0);new_double=out->double_battle;}return rc;}
void __real_gc_mon_new_enemy(gc_run *,gc_mon *,int,int,const pr_enemy_gen *,int);
void __wrap_gc_mon_new_enemy(gc_run *r,gc_mon *m,int species,int level,const pr_enemy_gen *g,int slot){__real_gc_mon_new_enemy(r,m,species,level,g,slot);if(watching&&m==&run.enemy[0]){ctor_nature=m->b.nature;ctor_calls++;}}
static void drain(gc_run *r){gc_event ev;while(gc_run_next_event(r,&ev)){};}
static void setup(const char *seed){
 gc_run_new(&run,GAME_MODES_ENDLESS,seed,777);
 gc_starter starters[2]={{.species=SPECIES_ID_BULBASAUR,.tera_type=-1,.moves={MOVE_ID_SPLASH}},{.species=SPECIES_ID_CHARMANDER,.tera_type=-1,.moves={MOVE_ID_SPLASH}}};
 for(int k=0;k<2;k++)memset(starters[k].ivs,15,6);const char *why=NULL;CHECK(gc_run_set_starters(&run,starters,2,&why));drain(&run);CHECK(gc_run_can_save(&run));
}
int main(void){
 for(int ci=0;ci<512;ci++){
  char seed[48];snprintf(seed,sizeof seed,"sync-nature-encounter-%d",ci);setup(seed);int shape=ci%8;bool lure=!!(ci&8);
  pr_pokemon *p0=&run.party[0].b,*p1=&run.party[1].b;
  p0->custom_ability=(shape==1||shape==2)?ABILITY_ID_BALL_FETCH:ABILITY_ID_SYNCHRONIZE;pr_refresh_ability(p0);p1->custom_ability=ABILITY_ID_SYNCHRONIZE;pr_refresh_ability(p1);
  p0->passive_flag=shape==2||shape==3;p0->custom_passive=p0->passive_flag?ABILITY_ID_SYNCHRONIZE:ABILITY_ID_NONE;p0->nature=ci%25;p1->nature=(ci+11)%25;
  if(shape==7){run.party[0].has_custom_nature=true;run.party[0].base_nature=(ci+19)%25;}
  gc_mon_recalc(&run,&run.party[0]);gc_mon_recalc(&run,&run.party[1]);p0->ability_suppressed=shape==4;p0->switch_out_status=shape==6;if(shape==5)p0->hp=0;
  int first=p0->nature,second=p1->nature;bool applies0=shape!=1&&shape!=4&&shape!=5;
  if(ci&32){run.party_order[0]=1;run.party_order[1]=0;int temp=first;first=second;second=temp;}
  run.bt.double_battle=!!(ci&16);run.n_items=lure?3:0;for(int k=0;k<run.n_items;k++)run.items[k]=(gc_item){.key=(const char*[]){"LURE","SUPER_LURE","MAX_LURE"}[k],.stack=1,.arg=-1,.battle_count=2};
  ctor_nature=-1;ctor_calls=0;new_double=-1;watching=true;gc_rq_clear(&run);gc_int(&run)->rq.waiting=false;gc_int(&run)->rq.current.type=RP_NONE;memset(&run.screen,0,sizeof run.screen);gc_new_battle(&run);watching=false;CHECK(ctor_calls>=2&&new_double>=0);
  bool reversed=!!(ci&32);int expected=ctor_nature;if(new_double&&(reversed?applies0:true))expected=second;if(reversed||applies0)expected=first;
  CHECK(run.enemy[0].b.nature==expected);CHECK(!run.enemy[0].has_custom_nature);if(lure)CHECK(new_double);singles+=!new_double;doubles+=new_double;
  gc_run_until_screen(&run);drain(&run);CHECK(gc_run_can_save(&run));CHECK(run.enemy[0].b.nature==expected);
  if(ci%16==0){const char *why=NULL;size_t n=gc_run_save(&run,saved,sizeof saved);CHECK(n&&gc_run_load(&loaded,saved,n,&why));CHECK(loaded.enemy[0].b.nature==expected);}
 }
 CHECK(singles>100&&doubles>200);
 /* A simulated direct callback mutates the underlying nature of a minted
  * target while retaining its effective nature and stats, then saves both. */
 setup("sync-nature-minted-save");pr_pokemon *u=&run.party[0].b,*t=&run.party[1].b;u->custom_ability=ABILITY_ID_SYNCHRONIZE;pr_refresh_ability(u);u->nature=20;run.party[1].has_custom_nature=true;run.party[1].base_nature=3;t->nature=18;gc_mon_recalc(&run,&run.party[1]);int32_t stats[6];memcpy(stats,t->stats,sizeof stats);pr_rng before=run.bt.rng;int history=u->n_ab_applied_summon;
 pr_ab_params pp={.pokemon=u,.target=t,.move=-1,.simulated=true,.set_nature=gc_set_encounter_nature,.nature_ctx=&run};pr_apply_ab_attrs(&run.bt,PR_AC_SyncEncounterNatureAbAttr,&pp);
 CHECK(run.party[1].base_nature==20&&run.party[1].has_custom_nature&&t->nature==18);CHECK(!memcmp(stats,t->stats,sizeof stats)&&u->n_ab_applied_summon==history);CHECK(!memcmp(&before,&run.bt.rng,sizeof before));
 const char *why=NULL;size_t n=gc_run_save(&run,saved,sizeof saved);CHECK(n&&gc_run_load(&loaded,saved,n,&why));CHECK(loaded.party[1].base_nature==20&&loaded.party[1].has_custom_nature&&loaded.party[1].b.nature==18);CHECK(!memcmp(stats,loaded.party[1].b.stats,sizeof stats));
 /* The first Challenge encounter has not had setup_field's context rebuild
  * yet. An empty Challenge must not be reported as an unknown stat rule. */
 gc_run_new(&run,GAME_MODES_CHALLENGE,"sync-first-challenge",777);
 gc_starter abra={.species=SPECIES_ID_ABRA,.tera_type=-1,.nature=13,.moves={MOVE_ID_SPLASH}};memset(abra.ivs,15,6);
 CHECK(gc_run_set_starters(&run,&abra,1,&why));drain(&run);CHECK(run.party[0].b.ability==ABILITY_ID_SYNCHRONIZE);CHECK(run.enemy[0].b.nature==13);CHECK(run.unported[PR_FEAT_WAVE]==0);
 printf("Synchronize real encounter/save: %d checks, singles%d doubles%d passed\n",checks,singles,doubles);return 0;
}
