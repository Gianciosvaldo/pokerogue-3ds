#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run run,loaded,before;static unsigned char saved[GC_SAVE_MAX];static int checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"Lure host line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void setup(void){gc_run_new(&run,GAME_MODES_CLASSIC,"lure-host",777);gc_starter st={.species=SPECIES_ID_BULBASAUR,.tera_type=-1};memset(st.ivs,15,6);const char *why;CHECK(gc_run_set_starters(&run,&st,1,&why));gc_event ev;while(gc_run_next_event(&run,&ev)){};}
int main(void){const char *why;int ids[]={PR_ITEM_LURE,PR_ITEM_SUPER_LURE,PR_ITEM_MAX_LURE},max[]={10,15,30};
 for(int k=0;k<3;k++)for(int timer=1;timer<=max[k];timer++)for(int stack=1;stack<=2;stack++){
  setup();run.n_items=3;run.items[0]=(gc_item){.key="DIRE_HIT",.stack=1,.battle_count=4};run.items[1]=(gc_item){.key=pr_item_keys[ids[k]],.arg=-1,.stack=stack,.battle_count=timer};run.items[2]=run.items[1];run.items[2].arg=0;gc_items_to_battle(&run);gc_item old[3];memcpy(old,run.items,sizeof old);
  CHECK(gc_run_can_save(&run));size_t size=gc_run_save(&run,saved,sizeof saved);CHECK(size&&gc_run_load(&loaded,saved,size,&why));CHECK(loaded.items[1].battle_count==timer&&loaded.items[1].stack==stack&&loaded.items[1].arg==-1);
  int result=gc_lure_add(&loaded,ids[k]);CHECK(result==(timer==max[k]?PR_REWARD_LURE_FULL:PR_REWARD_LURE_REFRESHED));CHECK(loaded.items[1].battle_count==max[k]&&loaded.items[1].stack==stack&&loaded.items[1].arg==-1);CHECK(!strcmp(loaded.items[0].key,old[0].key)&&loaded.items[0].arg==old[0].arg&&loaded.items[0].stack==old[0].stack&&loaded.items[0].battle_count==old[0].battle_count);CHECK(loaded.items[2].battle_count==timer&&loaded.items[2].stack==stack);
  CHECK(gc_run_can_save(&loaded));size=gc_run_save(&loaded,saved,sizeof saved);CHECK(size&&gc_run_load(&run,saved,size,&why));CHECK(run.items[1].battle_count==max[k]&&run.items[2].battle_count==timer);
 }
 for(int guard=0;guard<10;guard++){setup();run.n_items=1;run.items[0]=(gc_item){.key="LURE",.stack=1,.battle_count=3};switch(guard){case 0:run.items[0].battle_count=0;break;case 1:run.items[0].battle_count=11;break;case 2:run.items[0].stack=3;break;case 3:run.items[0].arg=1;break;case 4:run.items[0].pokemon_id=run.party[0].b.id;break;case 5:run.items[0].virtual_stack=1;break;case 6:run.items[0].detached=true;break;case 7:run.items[0].key="UNKNOWN";break;case 8:run.run.has_challenges=true;break;case 9:run.n_items=GC_MAX_ITEMS+1;break;}before=run;CHECK(gc_lure_add(&run,PR_ITEM_LURE)==PR_REWARD_LURE_UNSUPPORTED);CHECK(!memcmp(&run,&before,sizeof run));}
 setup();run.n_items=PR_MAX_MODIFIERS;for(int j=0;j<run.n_items;j++)run.items[j]=(gc_item){.key="HEALING_CHARM",.stack=1};before=run;CHECK(gc_lure_add(&run,PR_ITEM_LURE)==PR_REWARD_LURE_UNSUPPORTED&&!memcmp(&run,&before,sizeof run));
 setup();run.n_items=1;run.items[0]=(gc_item){.key="LURE",.arg=-1,.stack=2,.battle_count=0};gc_items_to_battle(&run);size_t size=gc_run_save(&run,saved,sizeof saved);CHECK(size&&gc_run_load(&loaded,saved,size,&why));CHECK(loaded.items[0].battle_count==0);before=loaded;CHECK(gc_lure_add(&loaded,PR_ITEM_LURE)==PR_REWARD_LURE_UNSUPPORTED&&!memcmp(&loaded,&before,sizeof loaded));
 printf("Lure acquisition host %d checks passed\n",checks);return 0;
}
