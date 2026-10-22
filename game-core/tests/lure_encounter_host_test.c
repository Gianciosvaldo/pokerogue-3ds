#include "../src/internal.h"
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
static gc_run run,before;static int checks,seen_count;static jmp_buf stop;static bool observe;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"encounter guard line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
int __real_pr_run_new_battle(pr_run *,int,const pr_new_battle_ctx *,pr_new_battle *);
int __wrap_pr_run_new_battle(pr_run *r,int w,const pr_new_battle_ctx *c,pr_new_battle *o){int result=__real_pr_run_new_battle(r,w,c,o);if(observe){seen_count=c->n_lures;longjmp(stop,1);}return result;}
static void observe_caller(void){observe=true;if(!setjmp(stop))gc_new_battle(&run);observe=false;}
static void setup(void){gc_run_new(&run,GAME_MODES_ENDLESS,"lure-encounter-host",777);run.n_items=1;run.items[0]=(gc_item){.key="LURE",.arg=-1,.stack=2,.battle_count=2};}
int main(void){
 for(int guard=0;guard<11;guard++){setup();switch(guard){case 0:run.items[0].battle_count=0;break;case 1:run.items[0].battle_count=11;break;case 2:run.items[0].stack=3;break;case 3:run.items[0].arg=1;break;case 4:run.items[0].pokemon_id=100;break;case 5:run.items[0].virtual_stack=1;break;case 6:run.items[0].detached=true;break;case 7:run.run.has_challenges=true;break;case 8:run.n_items=2;run.items[1]=(gc_item){.key="UNKNOWN",.stack=1};break;case 9:run.n_items=GC_MAX_ITEMS+1;break;case 10:run.n_items=2;run.items[1]=(gc_item){.key="MAX_LURE",.stack=1,.battle_count=0};break;}
  before=run;int count=99;CHECK(!gc_lure_count(&run,&count));CHECK(count==99&&!memcmp(&run,&before,sizeof run));if(guard==7||guard==9)continue;observe_caller();CHECK(seen_count==0&&run.stubs[GC_STUB_REWARD_INPUT]==1);CHECK(run.n_items==before.n_items&&!memcmp(run.items,before.items,sizeof run.items));
 }
 setup();run.items[0]=(gc_item){.key="UNKNOWN",.stack=1,.battle_count=13};before=run;int count=-1;CHECK(gc_lure_count(&run,&count)&&count==0&&!memcmp(&run,&before,sizeof run));
 setup();run.n_items=3;run.items[1]=(gc_item){.key="SUPER_LURE",.arg=0,.stack=1,.battle_count=14};run.items[2]=(gc_item){.key="MAX_LURE",.arg=-1,.stack=2,.battle_count=1};before=run;CHECK(gc_lure_count(&run,&count)&&count==3&&!memcmp(&run,&before,sizeof run));
 /* The real uninstrumented caller executes the forced wild double. */
 gc_new_battle(&run);CHECK(run.stubs[GC_STUB_DOUBLE_AS_SINGLE]==0&&run.n_enemy==2&&run.double_battle&&run.bt.double_battle);
 printf("Lure encounter host %d checks passed\n",checks);return 0;
}
