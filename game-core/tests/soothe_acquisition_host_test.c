/* Canonical storage/party order, atomic guards, real encounter saves and effects. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run run,loaded;static gc_system sys;static unsigned char saved[GC_SAVE_MAX];static int checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"Soothe acquisition host line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void setup(void){gc_run_new(&run,GAME_MODES_CLASSIC,"soothe-acquire",777);gc_system_init_defaults(&sys);gc_starter st[2]={{.species=SPECIES_ID_BULBASAUR,.tera_type=-1},{.species=SPECIES_ID_RATTATA,.tera_type=-1}};memset(st[0].ivs,15,6);memset(st[1].ivs,15,6);const char *why;CHECK(gc_run_set_starters(&run,st,1,&why));run.sys=&sys;run.party[1]=run.party[0];run.party[1].b.id++;run.party[1].b.on_field=false;run.n_party=2;run.party_order[1]=1;run.bt.party[0][1]=&run.party[1].b;run.bt.n_party[0]=2;gc_event ev;while(gc_run_next_event(&run,&ev)){};}
static gc_offer offer(int tier,bool paid){return(gc_offer){.key="SOOTHE_BELL",.arg=-1,.tier=tier,.target=GC_TARGET_POKEMON,.cost=paid?100:-1,.is_purchase=paid};}
static void acquisition(void){const char *why;for(int target=0;target<2;target++)for(int stack=0;stack<=3;stack++)for(int wildcard=0;wildcard<2;wildcard++)for(int paid=0;paid<2;paid++){
 setup();run.money=1000.25;int before=0;
 if(wildcard)run.items[before++]=(gc_item){.key="SOOTHE_BELL",.arg=-1,.stack=2};
 if(stack)run.items[before++]=(gc_item){.key="SOOTHE_BELL",.arg=-1,.stack=stack,.pokemon_id=run.party[target].b.id};
 run.n_items=before;gc_items_to_battle(&run);gc_item originals[GC_MAX_ITEMS];memcpy(originals,run.items,sizeof originals);gc_system profile=sys;pr_rng rng=run.run.rng;
 gc_offer o=offer(1,paid);int status=gc_reward_apply(&run,&o,target,-1,&why);
 CHECK(status==(stack==3||paid?GC_APPLY_REFUSED:GC_APPLY_DONE));CHECK(run.money==1000.25&&!memcmp(&sys,&profile,sizeof sys)&&!memcmp(&run.run.rng,&rng,sizeof rng));
 if(stack==3){CHECK(run.n_items==before&&!memcmp(run.items,originals,sizeof originals));CHECK(!strcmp(why,"modifierType:ModifierType.PokemonHeldItemModifierType.extra.tooMany"));}
 else{CHECK(run.n_items==1&&run.items[0].pokemon_id==run.party[target].b.id&&run.items[0].stack==stack+1);CHECK(run.items[0].arg==(stack?-1:0));}
 CHECK(gc_run_can_save(&run));size_t size=gc_run_save(&run,saved,sizeof saved);CHECK(size&&gc_run_load(&loaded,saved,size,&why));CHECK(loaded.n_items==run.n_items);
 for(int j=0;j<run.n_items;j++)CHECK(loaded.items[j].stack==run.items[j].stack&&loaded.items[j].pokemon_id==run.items[j].pokemon_id&&loaded.items[j].arg==run.items[j].arg);

 }
 for(int tier=0;tier<5;tier++){setup();run.items[0]=(gc_item){.key="SOOTHE_BELL",.arg=-1,.stack=3,.pokemon_id=run.party[0].b.id};run.items[1]=(gc_item){.key="SOOTHE_BELL",.arg=-1,.stack=2};run.n_items=2;gc_items_to_battle(&run);gc_offer o=offer(tier,true);gc_item old[2];memcpy(old,run.items,sizeof old);uint32_t balls[5];for(int j=0;j<5;j++)balls[j]=run.pokeballs[j];CHECK(gc_soothe_scene_apply(&run,&o,0,&why,false)==GC_APPLY_DONE);CHECK(!memcmp(old,run.items,sizeof old)&&run.n_items==2);int changed=0;for(int j=0;j<5;j++)changed+=run.pokeballs[j]>balls[j];if(changed!=1)fprintf(stderr,"fallback tier%d balls%u/%u/%u/%u/%u before%u/%u/%u/%u/%u\n",tier,run.pokeballs[0],run.pokeballs[1],run.pokeballs[2],run.pokeballs[3],run.pokeballs[4],balls[0],balls[1],balls[2],balls[3],balls[4]);CHECK(changed==1);}
 setup();run.items[0]=(gc_item){.key="SOOTHE_BELL",.arg=-1,.stack=3,.pokemon_id=run.party[0].b.id};run.items[1]=(gc_item){.key="SOOTHE_BELL",.arg=0,.stack=1,.pokemon_id=run.party[0].b.id};run.n_items=2;gc_item old[2];memcpy(old,run.items,sizeof old);CHECK(gc_soothe_add(&run,0)==PR_REWARD_SOOTHE_FULL&&!memcmp(old,run.items,sizeof old));
 setup();run.n_items=1;run.items[0]=(gc_item){.key="MULTI_LENS",.arg=-1,.stack=2,.pokemon_id=run.party[1].b.id};CHECK(gc_soothe_add(&run,0)==PR_REWARD_SOOTHE_ADDED&&run.items[0].arg==-1&&run.items[0].stack==2);CHECK(run.n_items==2&&run.items[1].pokemon_id==run.party[0].b.id);
}
static void refusal(void){for(int guard=0;guard<16;guard++){
 setup();run.n_items=1;run.items[0]=(gc_item){.key="SOOTHE_BELL",.arg=-1,.stack=1,.pokemon_id=run.party[0].b.id};
 switch(guard){case 0:run.n_items=-1;break;case 1:run.n_items=GC_MAX_ITEMS+1;break;case 2:run.n_party=7;break;case 3:run.party_order[1]=run.party_order[0];break;case 4:run.party[1].b.id=run.party[0].b.id;break;case 5:run.items[0].key="UNKNOWN";break;case 6:run.items[0].virtual_stack=1;break;case 7:run.items[0].detached=true;break;case 8:run.items[0].pokemon_id=123;break;case 9:run.items[0].arg=5;break;case 10:run.items[0].stack=4;break;case 11:run.items[0].stack=0;break;case 12:run.items[0].battle_count=1;break;case 13:run.run.has_challenges=true;break;case 14:run.party[0].b.id=0;break;case 15:run.n_items=PR_MAX_MODIFIERS;for(int j=0;j<run.n_items;j++)run.items[j]=(gc_item){.key="HEALING_CHARM",.stack=1};break;}
 gc_item before[GC_MAX_ITEMS];memcpy(before,run.items,sizeof before);int count=run.n_items;gc_mon party[6];memcpy(party,run.party,sizeof party);pr_rng rng=run.run.rng;double money=run.money;
 CHECK(gc_soothe_add(&run,0)==PR_REWARD_SOOTHE_UNSUPPORTED);CHECK(run.n_items==count&&!memcmp(before,run.items,sizeof before)&&!memcmp(party,run.party,sizeof party)&&!memcmp(&rng,&run.run.rng,sizeof rng)&&run.money==money);
 }
 setup();const char *why;gc_offer o=offer(0,false);
 for(int stack=-1;stack<=0;stack++){setup();run.n_items=1;run.items[0]=(gc_item){.key="HEALING_CHARM",.arg=-1,.stack=stack};gc_run before=run;CHECK(gc_soothe_scene_apply(&run,&o,0,&why,true)==GC_APPLY_UNHANDLED&&!memcmp(&run,&before,sizeof run));}
CHECK(gc_reward_apply(&run,&o,-1,-1,&why)==GC_APPLY_REFUSED&&!strcmp(why,"pick a Pokemon"));
 setup();run.n_items=1;run.items[0]=(gc_item){.key="SOOTHE_BELL",.arg=-1,.stack=1,.pokemon_id=run.party[0].b.id,.virtual_stack=1};CHECK(gc_reward_apply(&run,&o,0,-1,&why)==GC_APPLY_UNHANDLED);CHECK(gc_port_apply_reward(&run,&o,0,-1,&why)&&run.stubs[GC_STUB_REWARD_APPLY]==0&&run.items[0].virtual_stack==1);
}
int main(void){acquisition();refusal();printf("Soothe acquisition host: %d checks passed\n",checks);return 0;}
