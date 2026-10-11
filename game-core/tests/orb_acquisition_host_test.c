#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run run,loaded;static gc_system sys;static unsigned char saved[GC_SAVE_MAX];static int checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"Orb acquisition host line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void setup(void){gc_run_new(&run,GAME_MODES_CLASSIC,"orb-acquisition",777);gc_system_init_defaults(&sys);gc_starter st[2]={{.species=SPECIES_ID_BULBASAUR,.tera_type=-1},{.species=SPECIES_ID_RATTATA,.tera_type=-1}};memset(st[0].ivs,15,6);memset(st[1].ivs,15,6);const char *why;CHECK(gc_run_set_starters(&run,st,1,&why));run.sys=&sys;run.party[1]=run.party[0];run.party[1].b.id++;run.party[1].b.on_field=false;run.n_party=2;run.party_order[1]=1;run.bt.party[0][1]=&run.party[1].b;run.bt.n_party[0]=2;gc_event ev;while(gc_run_next_event(&run,&ev)){};}

static const char *keys[]={"TOXIC_ORB","FLAME_ORB"};
int main(void){const char *why=NULL;
 for(int family=0;family<2;family++)for(int owner=0;owner<2;owner++)for(int existing=0;existing<3;existing++)for(int wildcard=0;wildcard<2;wildcard++)for(int paid=0;paid<2;paid++){
  setup();run.money=1000.25;int n=0;
  if(wildcard)run.items[n++]=(gc_item){.key=keys[1-family],.arg=-1,.stack=1};
  if(existing)run.items[n++]=(gc_item){.key=keys[existing==1?family:1-family],.arg=-1,.stack=1,.pokemon_id=run.party[owner].b.id};
  run.n_items=n;gc_items_to_battle(&run);gc_item before[GC_MAX_ITEMS];memcpy(before,run.items,sizeof before);pr_rng rng=run.run.rng;gc_system profile=sys;
  gc_offer offer={.key=keys[family],.arg=-1,.tier=2,.target=GC_TARGET_POKEMON,.cost=paid?100:-1,.is_purchase=paid};
  CHECK(gc_reward_apply(&run,&offer,owner,-1,&why)==(existing||paid?GC_APPLY_REFUSED:GC_APPLY_DONE));
  CHECK(run.money==1000.25&&!memcmp(&rng,&run.run.rng,sizeof rng)&&!memcmp(&profile,&sys,sizeof sys));
  if(existing){CHECK(run.n_items==n&&!memcmp(before,run.items,sizeof before));CHECK(!strcmp(why,"modifierType:ModifierType.PokemonHeldItemModifierType.extra.tooMany"));}
  else CHECK(run.n_items==1&&!strcmp(run.items[0].key,keys[family])&&run.items[0].pokemon_id==run.party[owner].b.id&&run.items[0].stack==1);
  CHECK(gc_run_can_save(&run));size_t size=gc_run_save(&run,saved,sizeof saved);CHECK(size&&gc_run_load(&loaded,saved,size,&why));CHECK(loaded.n_items==run.n_items);
  for(int j=0;j<run.n_items;j++)CHECK(loaded.items[j].stack==run.items[j].stack&&loaded.items[j].pokemon_id==run.items[j].pokemon_id&&loaded.items[j].arg==run.items[j].arg&&!strcmp(loaded.items[j].key,run.items[j].key));
 }
 for(int guard=0;guard<17;guard++){
  setup();run.n_items=1;run.items[0]=(gc_item){.key="TOXIC_ORB",.arg=-1,.stack=1,.pokemon_id=run.party[0].b.id};
  switch(guard){case 0:run.n_items=-1;break;case 1:run.n_items=GC_MAX_ITEMS+1;break;case 2:run.n_party=7;break;case 3:run.party_order[1]=run.party_order[0];break;case 4:run.party[1].b.id=run.party[0].b.id;break;case 5:run.items[0].key="UNKNOWN";break;case 6:run.items[0].virtual_stack=1;break;case 7:run.items[0].detached=true;break;case 8:run.items[0].pokemon_id=123;break;case 9:run.items[0].arg=5;break;case 10:run.items[0].stack=2;break;case 11:run.items[0].stack=0;break;case 12:run.items[0].battle_count=1;break;case 13:run.run.has_challenges=true;break;case 14:run.party[0].b.id=0;break;case 15:run.n_items=PR_MAX_MODIFIERS;for(int j=0;j<run.n_items;j++)run.items[j]=(gc_item){.key="HEALING_CHARM",.stack=1};break;case 16:run.n_party=0;break;}
  gc_run before=run;CHECK(gc_orb_add(&run,0,PR_ITEM_FLAME_ORB)==PR_REWARD_HELD_UNSUPPORTED);CHECK(!memcmp(&run,&before,sizeof run));
 }
 printf("Orb acquisition host: %d checks passed\n",checks);return 0;
}
