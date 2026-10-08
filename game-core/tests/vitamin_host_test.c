/* Public shop/free behavior, complete preservation and legal vitamin saves. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run run,loaded;
static gc_system sys;
static unsigned char saved[GC_SAVE_MAX];
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"vitamin host line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void setup(void) {
 gc_run_new(&run,GAME_MODES_CLASSIC,"vitamin-host",777);gc_system_init_defaults(&sys);
 gc_starter st={.species=SPECIES_ID_BULBASAUR,.nature=17,.tera_type=-1};memset(st.ivs,15,6);const char *why;
 CHECK(gc_run_set_starters(&run,&st,1,&why));run.sys=&sys;memset(run.party[0].b.ivs,3,6);gc_mon_recalc(&run,&run.party[0]);
 gc_event ev;while(gc_run_next_event(&run,&ev)){}
}
static gc_offer vitamin(int stat,double cost){return(gc_offer){.key="BASE_STAT_BOOSTER",.arg=stat,.cost=cost,.target=GC_TARGET_POKEMON};}
static void screen(gc_offer o){gc_rqueue *q=&gc_int(&run)->rq;gc_rq_reset(q);q->current=gc_rp(RP_SELECT_MODIFIER,0,0,0);q->waiting=true;
 gc_rq_push(&run,gc_rp(RP_COMMAND,0,0,0));memset(&run.screen,0,sizeof run.screen);run.screen.kind=GC_SCREEN_REWARD;
 run.screen.n_shop=run.screen.n_options=1;run.screen.shop[0]=run.screen.options[0]=o;run.money=1000;}
int main(void){const char *why;
 for(int stat=0;stat<6;stat++) {
  setup();gc_offer o=vitamin(stat,0);gc_mon before=run.party[0];gc_system oldsys=sys;pr_rng oldrng=run.run.rng;
  run.party[0].b.status=STATUS_EFFECT_TOXIC;run.party[0].b.status_toxic_turns=7;before=run.party[0];
  CHECK(gc_port_apply_reward(&run,&o,0,-1,&why));CHECK(run.n_items==1&&run.items[0].arg==stat&&run.items[0].stack==1);
  CHECK(run.items[0].pokemon_id==run.party[0].b.id&&run.stubs[GC_STUB_REWARD_APPLY]==0);
  before.b.hp=run.party[0].b.hp;memcpy(before.b.stats,run.party[0].b.stats,sizeof before.b.stats);
  CHECK(!memcmp(&before,&run.party[0],sizeof before));CHECK(!memcmp(&oldsys,&sys,sizeof sys));CHECK(!memcmp(&oldrng,&run.run.rng,sizeof oldrng));
  int32_t stats[6];memcpy(stats,run.party[0].b.stats,sizeof stats);gc_mon_recalc(&run,&run.party[0]);CHECK(!memcmp(stats,run.party[0].b.stats,sizeof stats));
  CHECK(gc_port_apply_reward(&run,&o,0,-1,&why));CHECK(gc_port_apply_reward(&run,&o,0,-1,&why));
  CHECK(run.items[0].stack==3);before=run.party[0];CHECK(!gc_port_apply_reward(&run,&o,0,-1,&why));
  CHECK(strstr(why,"tooMany")!=NULL&&run.items[0].stack==3&&!memcmp(&before,&run.party[0],sizeof before));
  size_t n=gc_run_save(&run,saved,sizeof saved);CHECK(n>0&&gc_run_load(&loaded,saved,n,&why));
  CHECK(loaded.items[0].stack==3&&loaded.items[0].arg==stat&&loaded.items[0].pokemon_id==loaded.party[0].b.id);
  CHECK(!memcmp(loaded.party[0].b.stats,run.party[0].b.stats,sizeof stats)&&loaded.party[0].b.status_toxic_turns==7);
  setup();run.party[0].b.ivs[stat]=0;gc_mon_recalc(&run,&run.party[0]);o=vitamin(stat,0);before=run.party[0];
  CHECK(!gc_port_apply_reward(&run,&o,0,-1,&why));CHECK(strstr(why,"inoperable")!=NULL&&run.n_items==0&&!memcmp(&before,&run.party[0],sizeof before));
 }
 for(int cost=0;cost<=300;cost+=300){setup();screen(vitamin(STAT_ATK,cost));gc_choice buy={GC_CH_SHOP,0,0,-1};
  CHECK(!gc_run_choose(&run,&buy,&why));CHECK(why&&!*why);CHECK(run.money==1000&&run.screen.kind==GC_SCREEN_REWARD&&run.items[0].stack==1);
  CHECK(!gc_run_choose(&run,&buy,&why));CHECK(run.money==1000&&run.items[0].stack==2);
  run.money=cost?100:0;if(cost){CHECK(!gc_run_choose(&run,&buy,&why));CHECK(run.items[0].stack==2&&strstr(why,"notEnoughMoney"));}
  gc_choice skip={GC_CH_SKIP,0,0,0};CHECK(gc_run_choose(&run,&skip,&why));CHECK(gc_run_can_save(&run));
  size_t n=gc_run_save(&run,saved,sizeof saved);CHECK(n&&gc_run_load(&loaded,saved,n,&why));CHECK(loaded.items[0].stack==2);
 }
 setup();screen(vitamin(STAT_HP,0));gc_choice pick={GC_CH_REWARD,0,0,-1};CHECK(gc_run_choose(&run,&pick,&why));CHECK(run.screen.kind==GC_SCREEN_COMMAND&&run.money==1000);
 setup();run.party[0].b.hp=0;gc_offer o=vitamin(STAT_HP,0);CHECK(gc_port_apply_reward(&run,&o,0,-1,&why));CHECK(run.party[0].b.hp==0);
 setup();gc_offer mint={.key="MINT",.arg=1,.cost=0,.target=GC_TARGET_POKEMON};CHECK(gc_port_apply_reward(&run,&mint,0,-1,&why));
 o=vitamin(STAT_ATK,0);CHECK(gc_port_apply_reward(&run,&o,0,-1,&why));CHECK(run.party[0].has_custom_nature&&run.party[0].base_nature==17&&run.party[0].b.nature==1);
 setup();o=vitamin(STAT_ATK,0);CHECK(!gc_port_apply_reward(&run,&o,-1,-1,&why));CHECK(!strcmp(why,"pick a Pokemon"));
 const char *unsupported[]={"SOUL_DEW","MYSTERY_ENCOUNTER_SHUCKLE_JUICE","MYSTERY_ENCOUNTER_OLD_GATEAU","MYSTERY_ENCOUNTER_MACHO_BRACE","UNSUPPORTED_TEST"};
 for(unsigned i=0;i<sizeof unsupported/sizeof *unsupported;i++){setup();gc_item_add(&run,unsupported[i],0,run.party[0].b.id,1);gc_mon old=run.party[0];
  CHECK(gc_reward_apply(&run,&o,0,-1,&why)==GC_APPLY_UNHANDLED);CHECK(!memcmp(&old,&run.party[0],sizeof old));
  CHECK(gc_port_apply_reward(&run,&o,0,-1,&why));CHECK(run.stubs[GC_STUB_REWARD_APPLY]==1&&run.n_items==2);
 }
 setup();gc_item_add(&run,"LEFTOVERS",0,run.party[0].b.id+1,1);CHECK(gc_reward_apply(&run,&o,0,-1,&why)==GC_APPLY_UNHANDLED);
 setup();gc_item_add(&run,"BASE_STAT_BOOSTER",STAT_ATK,0,1);CHECK(gc_reward_apply(&run,&o,0,-1,&why)==GC_APPLY_UNHANDLED);
 setup();run.run.has_challenges=true;CHECK(gc_reward_apply(&run,&o,0,-1,&why)==GC_APPLY_UNHANDLED);
 setup();run.n_items=PR_MAX_MODIFIERS;
 for(int i=0;i<run.n_items;i++)run.items[i]=(gc_item){"HEALING_CHARM",0,1,0};
 gc_mon before=run.party[0];CHECK(gc_reward_apply(&run,&o,0,-1,&why)==GC_APPLY_UNHANDLED);
 CHECK(!memcmp(&before,&run.party[0],sizeof before));
 run.items[0]=(gc_item){"BASE_STAT_BOOSTER",STAT_ATK,1,run.party[0].b.id};
 CHECK(gc_port_apply_reward(&run,&o,0,-1,&why));CHECK(run.n_items==PR_MAX_MODIFIERS&&run.items[0].stack==2);
 setup();run.n_items=PR_MAX_MODIFIERS+1;
 for(int i=0;i<run.n_items;i++)run.items[i]=(gc_item){"HEALING_CHARM",0,1,0};
 CHECK(gc_reward_apply(&run,&o,0,-1,&why)==GC_APPLY_UNHANDLED);
 printf("vitamin host: %u checks passed\n",checks);return 0;
}
