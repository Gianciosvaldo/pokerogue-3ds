/* Manage Items (the reward screen's held-item transfer and discard): gc_item_transfer / gc_item_discard against
 * BattleScene.tryTransferHeldItemModifier and tryDiscardHeldItemModifier, and GC_CH_TRANSFER outside the reward screen. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run run;
static int checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"manage items host line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void drain(void){gc_event e;while(gc_run_next_event(&run,&e)){}}
static int held(const char *key,int16_t arg,int p){int n=0;for(int i=0;i<run.n_items;i++)if(run.items[i].pokemon_id==run.party[p].b.id&&!strcmp(run.items[i].key,key)&&run.items[i].arg==arg)n+=run.items[i].stack;return n;}
static int find(const char *key,int16_t arg,int p){for(int i=0;i<run.n_items;i++)if(run.items[i].pokemon_id==run.party[p].b.id&&!strcmp(run.items[i].key,key)&&run.items[i].arg==arg)return i;return -1;}
static void give(const char *key,int16_t arg,int p,int n){gc_item_add(&run,key,arg,run.party[p].b.id,n);gc_items_to_battle(&run);for(int i=0;i<run.n_party;i++)pr_recalculate_stats(&run.bt,&run.party[i].b);}
int main(void){
 const char *why=NULL;
 gc_run_new(&run,GAME_MODES_CLASSIC,"manage-items",4242);
 gc_starter s[2]={{.species=SPECIES_ID_BULBASAUR,.tera_type=-1},{.species=SPECIES_ID_CHARMANDER,.tera_type=-1}};
 memset(s[0].ivs,20,6);memset(s[1].ivs,20,6);s[1].ivs[STAT_HP]=2;
 CHECK(gc_run_set_starters(&run,s,2,&why));drain();CHECK(run.n_party==2);
 run.n_items=0;
 const int lmax=gc_item_max_held(&run,"LEFTOVERS",-1,&run.party[1]);CHECK(lmax==4);
 /* to a Pokemon without the item: all asked for move, uncapped, and the new entry goes last */
 give("LEFTOVERS",-1,0,3);give("FOCUS_BAND",-1,0,1);
 CHECK(gc_item_transfer(&run,find("LEFTOVERS",-1,0),1,2)==2);
 CHECK(held("LEFTOVERS",-1,0)==1&&held("LEFTOVERS",-1,1)==2);
 CHECK(run.items[run.n_items-1].pokemon_id==run.party[1].b.id&&!strcmp(run.items[run.n_items-1].key,"LEFTOVERS"));
 /* onto a matching stack: capped at its getMaxStackCount, the old matching entry re-added last */
 give("LEFTOVERS",-1,0,3);CHECK(held("LEFTOVERS",-1,0)==4); /* gc_item_add stacks without a cap */
 run.items[find("LEFTOVERS",-1,0)].type_tier=3;run.items[find("LEFTOVERS",-1,1)].type_tier=5; /* newItemModifier = itemModifier.clone() */
 CHECK(gc_item_transfer(&run,find("LEFTOVERS",-1,0),1,4)==2);
 CHECK(run.items[find("LEFTOVERS",-1,1)].type_tier==3);
 CHECK(held("LEFTOVERS",-1,0)==2&&held("LEFTOVERS",-1,1)==4);
 CHECK(!strcmp(run.items[run.n_items-1].key,"LEFTOVERS")&&run.items[run.n_items-1].pokemon_id==run.party[1].b.id);
 /* a full target refuses and nothing changes */
 const int n=run.n_items;gc_item before[GC_MAX_ITEMS];memcpy(before,run.items,sizeof before);
 CHECK(gc_item_transfer(&run,find("LEFTOVERS",-1,0),1,1)==0);
 CHECK(run.n_items==n&&!memcmp(before,run.items,sizeof before));
 /* an emptied source is removed; to itself, a locked item, a bad index or 0 copies refuse */
 CHECK(gc_item_transfer(&run,find("FOCUS_BAND",-1,0),1,5)==1&&find("FOCUS_BAND",-1,0)<0&&held("FOCUS_BAND",-1,1)==1);
 CHECK(gc_item_transfer(&run,find("LEFTOVERS",-1,0),0,1)==0);
 CHECK(gc_item_transfer(&run,-1,1,1)==0&&gc_item_transfer(&run,run.n_items,1,1)==0&&gc_item_transfer(&run,find("LEFTOVERS",-1,0),1,0)==0);
 give("SHELL_BELL",-1,0,1);run.items[find("SHELL_BELL",-1,0)].non_transferable=true;
 CHECK(gc_item_transfer(&run,find("SHELL_BELL",-1,0),1,1)==0&&gc_item_discard(&run,find("SHELL_BELL",-1,0),1)==0);
 /* a vitamin: the cap is the holder's IV for its stat, and the stats follow at once */
 give("BASE_STAT_BOOSTER",STAT_HP,0,3);
 const int hp0=run.party[0].b.stats[STAT_HP],hp1=run.party[1].b.stats[STAT_HP];
 CHECK(gc_item_max_held(&run,"BASE_STAT_BOOSTER",STAT_HP,&run.party[1])==2);
 CHECK(gc_item_transfer(&run,find("BASE_STAT_BOOSTER",STAT_HP,0),1,3)==3); /* no stack there yet: no cap */
 CHECK(run.party[1].b.stats[STAT_HP]>hp1&&run.party[0].b.stats[STAT_HP]<hp0);
 give("BASE_STAT_BOOSTER",STAT_HP,0,1);
 CHECK(gc_item_transfer(&run,find("BASE_STAT_BOOSTER",STAT_HP,0),1,1)==0); /* 3 is over the IV of 2 */
 /* discard: the asked copies, the entry with the last one */
 CHECK(gc_item_discard(&run,find("LEFTOVERS",-1,1),3)==3&&held("LEFTOVERS",-1,1)==1);
 CHECK(gc_item_discard(&run,find("LEFTOVERS",-1,1),9)==1&&find("LEFTOVERS",-1,1)<0);
 /* the choices exist on the reward screen only */
 CHECK(run.screen.kind!=GC_SCREEN_REWARD);
 gc_choice c={.kind=GC_CH_TRANSFER,.a=1,.b=(int16_t)find("BASE_STAT_BOOSTER",STAT_HP,0),.c=1};
 CHECK(!gc_run_choose(&run,&c,&why));
 c=(gc_choice){.kind=GC_CH_DISCARD,.b=0,.c=1};CHECK(!gc_run_choose(&run,&c,&why));
 printf("manage_items_host_test: %d checks, 0 failed\n",checks);
 return 0;
}
