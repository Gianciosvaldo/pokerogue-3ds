/* Boundary guards; independent source comparisons live in soothe_battle_test. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run run,before;static gc_system sys;static int checks;
#define CHECK(x) do {checks++;if(!(x)){fprintf(stderr,"guard line%d failed\n",__LINE__);exit(1);}}while(0)
static void setup(void){gc_run_new(&run,GAME_MODES_CLASSIC,"soothe-battle-host",1);gc_system_init_defaults(&sys);run.sys=&sys;run.n_party=2;run.party_order[0]=1;run.party_order[1]=0;for(int j=0;j<2;j++){run.party[j].b.id=100+j;run.party[j].b.species=1;run.party[j].b.friendship=255;}run.n_items=1;run.items[0]=(gc_item){.key="SOOTHE_BELL",.stack=3,.pokemon_id=101};}
static void refused(void){before=run;CHECK(!gc_soothe_battle_gain(&run,1));CHECK(!memcmp(&run,&before,sizeof run));}
int main(void){
 setup();CHECK(gc_soothe_battle_gain(&run,1));CHECK(run.party[1].b.friendship==255);CHECK(sys.starters[1].friendship==21);CHECK(run.items[0].stack==3);
 setup();run.run.mode=GAME_MODES_ENDLESS;CHECK(gc_soothe_battle_gain(&run,1));CHECK(sys.starters[1].friendship==7);
 for(int v=0;v<10;v++){setup();switch(v){case 0:run.items[0].virtual_stack=1;break;case 1:run.items[0].detached=true;break;case 2:run.items[0].stack=4;break;case 3:run.items[0].arg=1;break;case 4:run.items[0].battle_count=1;break;case 5:run.items[0].pokemon_id=999;break;case 6:run.party[0].b.id=101;break;case 7:run.party_order[1]=1;break;case 8:run.party[0].b.fusion_species=4;break;case 9:run.run.has_challenges=true;break;}refused();}
 setup();run.n_items=0;run.party[1].b.friendship=199;CHECK(gc_soothe_battle_gain(&run,1));CHECK(run.party[1].b.friendship==202);CHECK(sys.starters[1].friendship==9);
 setup();run.party[1].b.friendship=199;gc_system_add_friendship(&sys,&run,&run.party[1],6,true);CHECK(run.party[1].b.friendship==200);CHECK(sys.starters[1].friendship==18); /* unchanged capped API: Bell intentionally not applied */
 setup();gc_system_add_friendship(&sys,&run,&run.party[1],-1,false);CHECK(run.party[1].b.friendship==254);CHECK(sys.starters[1].friendship==0);
 printf("Soothe battle host guards %d passed\n",checks);return 0;
}
