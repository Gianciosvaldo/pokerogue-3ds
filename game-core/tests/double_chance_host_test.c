/* Observe the real game encounter caller before its subsequent cleanup. Native
 * parity separately establishes the getter; this guards wiring, order and gates. */
#include "../src/internal.h"
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static gc_run run;
static jmp_buf stop;
static int checks,calls;
static double denominator;
static double (*original)(void *,int);
static void *original_ctx;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"double chance host line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static bool same_rng(pr_rng a,pr_rng b){return a.c==b.c&&a.s0==b.s0&&a.s1==b.s1&&a.s2==b.s2;}
static double observe_chance(void *ctx,int wave){
 (void)ctx;calls++;pr_rng before=run.run.rng,battle=run.bt.battle_rng;
 denominator=original(original_ctx,wave);
 CHECK(same_rng(before,run.run.rng)&&same_rng(battle,run.bt.battle_rng));
 CHECK(run.bt.engine==&run.eng&&run.bt.n_party[0]==run.n_party);
 for(int k=0;k<run.n_party;k++)CHECK(run.bt.party[0][k]==&run.party[run.party_order[k]].b);
 CHECK(!run.bt.n_unported&&!run.bt.n_deferred);return denominator;
}
int __real_pr_run_new_battle(pr_run *,int,const pr_new_battle_ctx *,pr_new_battle *);
int __wrap_pr_run_new_battle(pr_run *r,int w,const pr_new_battle_ctx *ctx,pr_new_battle *o){
 CHECK(ctx->double_chance!=NULL);original=ctx->double_chance;original_ctx=ctx->double_chance_ctx;
 pr_new_battle_ctx copy=*ctx;copy.double_chance=observe_chance;
 CHECK(__real_pr_run_new_battle(r,w,&copy,o)==0&&!r->unported);
 longjmp(stop,1);
}
int main(void){
 const int abilities[]={ABILITY_ID_ILLUMINATE,ABILITY_ID_ARENA_TRAP,ABILITY_ID_NO_GUARD,ABILITY_ID_COMMANDER};
 for(int dbl=0;dbl<2;dbl++)for(int order=0;order<4;order++)for(int shape=0;shape<8;shape++)for(int lures=0;lures<4;lures++)for(int wi=0;wi<4;wi++){
  int wave=(int[]){1,2,10,50}[wi];gc_run_new(&run,GAME_MODES_ENDLESS,"double-chance-host",777);
  run.n_party=4;run.wave=wave-1;run.in_battle=wave>1;run.bt.double_battle=dbl;
  for(int k=0;k<4;k++){
   gc_starter s={.species=SPECIES_ID_BULBASAUR,.tera_type=-1};memset(s.ivs,15,6);gc_mon_new_player(&run,&run.party[k],&s,5);
   run.party_order[k]=(k+order)%4;pr_pokemon *p=&run.party[k].b;
   p->custom_ability=(k==0&&shape!=1)?abilities[k]:ABILITY_ID_BALL_FETCH;pr_refresh_ability(p);
   p->passive_flag=k==0&&(shape==1||shape==2||shape==3);p->custom_passive=p->passive_flag?abilities[shape==3?1:0]:ABILITY_ID_NONE;
   p->ability_suppressed=k==0&&shape==4;p->switch_out_status=k==0&&shape==6;p->on_field=k<2&&shape!=7;
   if(k==0&&shape==5)p->hp=0;
  }
  run.n_items=lures;for(int k=0;k<lures;k++)run.items[k]=(gc_item){.key=(const char*[]){"LURE","SUPER_LURE","MAX_LURE"}[k],.stack=2,.arg=-1,.battle_count=1};
  calls=0;denominator=-1;if(!setjmp(stop))gc_new_battle(&run);
  CHECK(calls==(wave%50!=0));
  bool selected=order==0||(dbl&&order==3),applies=selected&&shape!=4&&shape!=5;
  if(calls){double expected=wave%10==0?32:8;for(int k=0;k<lures;k++)expected/=4;if(applies)expected/=4;if(applies&&shape==3)expected/=4;if(expected<1)expected=1;CHECK(denominator==expected);}
  CHECK(pr_wave_ability_applied(&run.party[0].b,abilities[0])==(calls&&applies));
  CHECK(pr_wave_ability_applied(&run.party[0].b,abilities[1])==(calls&&applies&&shape==3));
  for(int k=1;k<4;k++)CHECK(run.party[k].b.n_ab_applied_wave==0);
 }
 printf("Double chance real encounter caller: %d checks passed\n",checks);return 0;
}
