#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run run,before;static int checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"lapse guard line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void setup(void){gc_run_new(&run,GAME_MODES_CLASSIC,"lure-lapse-host",777);run.n_items=1;run.items[0]=(gc_item){.key="LURE",.arg=-1,.stack=2,.battle_count=2};}
int main(void){
 for(int guard=0;guard<10;guard++){setup();switch(guard){case 0:run.items[0].battle_count=0;break;case 1:run.items[0].battle_count=11;break;case 2:run.items[0].stack=3;break;case 3:run.items[0].arg=1;break;case 4:run.items[0].pokemon_id=100;break;case 5:run.items[0].virtual_stack=1;break;case 6:run.items[0].detached=true;break;case 7:run.run.has_challenges=true;break;case 8:run.n_items=2;run.items[1]=(gc_item){.key="UNKNOWN",.stack=1};break;case 9:run.n_items=GC_MAX_ITEMS+1;break;}before=run;int expired=99;CHECK(!gc_lure_lapse(&run,&expired));CHECK(expired==99&&!memcmp(&run,&before,sizeof run));}
 setup();run.items[0]=(gc_item){.key="UNKNOWN",.stack=1,.battle_count=13};before=run;int expired=-1;CHECK(gc_lure_lapse(&run,&expired)&&expired==0&&!memcmp(&run,&before,sizeof run));
 setup();run.n_items=4;run.items[0].battle_count=1;run.items[1]=(gc_item){.key="DIRE_HIT",.stack=2,.battle_count=1};run.items[2]=(gc_item){.key="SUPER_LURE",.stack=1,.battle_count=2};run.items[3]=run.items[0];CHECK(gc_lure_lapse(&run,&expired)&&expired==2&&run.n_items==2);CHECK(!strcmp(run.items[0].key,"DIRE_HIT")&&run.items[0].battle_count==1);CHECK(!strcmp(run.items[1].key,"SUPER_LURE")&&run.items[1].battle_count==1);CHECK(gc_lure_lapse(&run,&expired)&&expired==1&&run.n_items==1&&run.items[0].battle_count==1); /* separate family clock never touched */
 printf("Lure lapse host %d checks passed\n",checks);return 0;
}
