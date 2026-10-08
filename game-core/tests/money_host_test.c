/* Public shop/free behavior, complete preservation and legal money saves. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run run,loaded;
static gc_system sys;
static unsigned char saved[GC_SAVE_MAX];
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"money host line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void setup(void) {
 gc_run_new(&run,GAME_MODES_CLASSIC,"money-host",777);gc_system_init_defaults(&sys);
 gc_starter st={.species=SPECIES_ID_BULBASAUR,.nature=17,.tera_type=-1};memset(st.ivs,15,6);const char *why;
 CHECK(gc_run_set_starters(&run,&st,1,&why));run.sys=&sys;memset(run.party[0].b.ivs,3,6);gc_mon_recalc(&run,&run.party[0]);
 gc_event ev;while(gc_run_next_event(&run,&ev)){}
}
static gc_offer offer(const char *key,double cost){return(gc_offer){.key=key,.arg=0,.cost=cost,.target=GC_TARGET_NONE};}
static void screen(gc_offer o){gc_rqueue *q=&gc_int(&run)->rq;gc_rq_reset(q);q->current=gc_rp(RP_SELECT_MODIFIER,0,0,0);q->waiting=true;
 gc_rq_push(&run,gc_rp(RP_COMMAND,0,0,0));memset(&run.screen,0,sizeof run.screen);run.screen.kind=GC_SCREEN_REWARD;
 run.screen.n_shop=run.screen.n_options=1;run.screen.shop[0]=run.screen.options[0]=o;}
int main(void){const char *why;const char *keys[]={"NUGGET","BIG_NUGGET","RELIC_GOLD"};
 for(int k=0;k<3;k++)for(int cap=0;cap<2;cap++) {
  setup();run.money=cap?9007199254740991.0:1000.25;double before=run.money;
  gc_mon old=run.party[0];gc_system oldsys=sys;pr_rng rng=run.run.rng;gc_offer o=offer(keys[k],0);
  CHECK(gc_port_apply_reward(&run,&o,-1,-1,&why));CHECK(cap?run.money==before:run.money>before);
  CHECK(!memcmp(&old,&run.party[0],sizeof old)&&!memcmp(&oldsys,&sys,sizeof sys)&&!memcmp(&rng,&run.run.rng,sizeof rng));
  CHECK(run.n_items==0&&run.stubs[GC_STUB_REWARD_APPLY]==0);
  size_t n=gc_run_save(&run,saved,sizeof saved);CHECK(n&&gc_run_load(&loaded,saved,n,&why));CHECK(loaded.money==run.money&&loaded.n_items==0);
 }
 for(int price=0;price<=300;price+=300){setup();run.money=1000;screen(offer("NUGGET",price));gc_choice buy={GC_CH_SHOP,0,-1,-1};
  double amount=pr_run_get_wave_money_amount(run.wave,1);CHECK(gc_run_choose(&run,&buy,&why));CHECK(run.money==1000+amount-price&&run.screen.kind==GC_SCREEN_REWARD&&run.n_items==0);
  CHECK(gc_run_choose(&run,&buy,&why));CHECK(run.money==1000+2*(amount-price));
  if(price){run.money=100;CHECK(!gc_run_choose(&run,&buy,&why));CHECK(run.money==100&&strstr(why,"notEnoughMoney"));}
  gc_choice skip={GC_CH_SKIP,0,0,0};CHECK(gc_run_choose(&run,&skip,&why));CHECK(gc_run_can_save(&run));
 }
 setup();run.money=1000;screen(offer("RELIC_GOLD",0));gc_choice pick={GC_CH_REWARD,0,-1,-1};CHECK(gc_run_choose(&run,&pick,&why));CHECK(run.screen.kind==GC_SCREEN_COMMAND&&run.n_items==0);
 for(int fused=0;fused<2;fused++){setup();if(fused)run.party[0].b.fusion_species=SPECIES_ID_GIMMIGHOUL;else run.party[0].b.species=SPECIES_ID_GIMMIGHOUL;
  gc_offer o=offer("NUGGET",0);double before=run.money;CHECK(gc_reward_apply(&run,&o,-1,-1,&why)==GC_APPLY_UNHANDLED);CHECK(run.money==before);
  CHECK(gc_port_apply_reward(&run,&o,-1,-1,&why));CHECK(run.stubs[GC_STUB_REWARD_APPLY]==1&&run.money==before&&run.n_items==1);
 }
 setup();run.party[0].b.species=SPECIES_ID_GHOLDENGO;gc_offer o=offer("NUGGET",0);CHECK(gc_port_apply_reward(&run,&o,-1,-1,&why)&&run.n_items==0);
 for(int guard=0;guard<11;guard++){setup();double before=run.money;
  if(guard==0)run.wave=0;
  if(guard==1)run.wave=10001;
  if(guard==2)run.run.has_challenges=true;
  if(guard==3)gc_item_add(&run,"UNKNOWN_TEST",0,0,1);
  if(guard==4)gc_item_add(&run,"AMULET_COIN",0,run.party[0].b.id+1,1);
  if(guard==5)gc_item_add(&run,"AMULET_COIN",0,0,6);
  if(guard==6)gc_item_add(&run,"AMULET_COIN",1,0,1);
  if(guard==7)run.n_items=GC_MAX_ITEMS+1;
  if(guard==8)run.n_items=-1;
  if(guard==9)run.n_party=0;
  if(guard==10)run.n_party=7;
  CHECK(gc_reward_apply(&run,&o,-1,-1,&why)==GC_APPLY_UNHANDLED);CHECK(run.money==before);
 }
 setup();gc_item_add(&run,"AMULET_COIN",0,0,5);double before=run.money;CHECK(gc_port_apply_reward(&run,&o,-1,-1,&why));CHECK(run.money-before==2*pr_run_get_wave_money_amount(run.wave,1));CHECK(run.n_items==1&&run.items[0].stack==5);
 printf("money host: %u checks passed\n",checks);return 0;
}
