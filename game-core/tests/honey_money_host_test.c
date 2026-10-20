/* Real BattleEndPhase wiring, duplicate-victory folding, reserve/passive owners and wallet saves. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run run,loaded;
static unsigned char saved[GC_SAVE_MAX];
static int checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"honey/money host line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void drain(void){gc_event event;while(gc_run_next_event(&run,&event)){};}
static void setup(void){
 gc_run_new(&run,GAME_MODES_ENDLESS,"honey-money-host",777);
 gc_starter team[3]={{.species=SPECIES_ID_BULBASAUR,.tera_type=-1,.moves={MOVE_ID_SPLASH}},{.species=SPECIES_ID_CHARMANDER,.tera_type=-1,.moves={MOVE_ID_SPLASH}},{.species=SPECIES_ID_SQUIRTLE,.tera_type=-1,.moves={MOVE_ID_SPLASH}}};
 for(int i=0;i<3;i++)memset(team[i].ivs,15,6);const char *why=NULL;CHECK(gc_run_set_starters(&run,team,3,&why));drain();CHECK(gc_run_can_save(&run));
}
int main(void){
 for(int ci=0;ci<64;ci++){
  setup();run.wave=3000;run.money=0;run.bt.money_scattered=100;run.n_items=0;run.n_battle_events=0;
  gc_rq_clear(&run);gc_int(&run)->rq.waiting=false;
  for(int k=0;k<3;k++){pr_pokemon *p=&run.party[k].b;p->custom_ability=k==1?ABILITY_ID_BALL_FETCH:ABILITY_ID_HONEY_GATHER;pr_refresh_ability(p);p->passive_flag=k>0;p->custom_passive=p->passive_flag?ABILITY_ID_HONEY_GATHER:ABILITY_ID_NONE;p->n_ab_applied_wave=p->n_ab_applied_summon=0;}
  if(ci&1){run.party_order[0]=2;run.party_order[2]=0;}
  bool suppressed=!!(ci&2),fainted=!!(ci&4),victory=!!(ci&8),duplicate=!!(ci&16);run.party[1].b.ability_suppressed=suppressed;if(fainted)run.party[2].b.hp=0;
  run.party[0].b.switch_out_status=!!(ci&32);
  if(duplicate)gc_rq_push(&run,gc_rp(RP_BATTLE_END,1,0,0));
  pr_rng before=run.run.rng;
  gc_rphase phase=gc_rp(RP_BATTLE_END,victory,0,0);CHECK(gc_phase_start(&run,&phase));
  int count=(victory||duplicate)?3-fainted:0;double want=100;for(int k=0;k<count;k++)want+=pr_wave_money_amount(3000,.2);
  if(run.money!=want)fprintf(stderr,"ci%d money%.17g want%.17g count%d histories%d/%d/%d suppressed%d faint%d victory%d duplicate%d\n",ci,run.money,want,count,run.party[0].b.n_ab_applied_wave,run.party[1].b.n_ab_applied_wave,run.party[2].b.n_ab_applied_wave,suppressed,fainted,victory,duplicate);
  CHECK(run.money==want&&run.bt.money_scattered==0);CHECK(!gc_rq_has(&run,RP_BATTLE_END));CHECK(!memcmp(&before,&run.run.rng,sizeof before));
  CHECK(run.party[0].b.n_ab_applied_wave==(victory||duplicate));CHECK(run.party[1].b.n_ab_applied_wave==(victory||duplicate));CHECK(run.party[2].b.n_ab_applied_wave==((victory||duplicate)&&!fainted));
  int messages=0,money=0,shows=0;gc_event event;while(gc_run_next_event(&run,&event)){
   if(event.kind==GC_EV_MONEY){money++;CHECK(event.money_change==want&&event.money_after==want);}
   if(event.kind==GC_EV_MESSAGE&&event.msg.key&&!strcmp(event.msg.key,"battle:moneyPickedUp")){messages++;CHECK(event.prompt&&event.msg.args[0].kind==PR_ARG_MONEY_EN_US);}
   if(event.kind==GC_EV_BATTLE)for(int j=event.a;j<event.a+event.b;j++)shows+=run.battle_events[j].type==PR_EV_ABILITY_SHOW;
  }
  CHECK(money==1&&messages==1);CHECK(shows==(victory||duplicate)); /* only the on-field lead gets a banner */
  CHECK(run.stubs[GC_STUB_POST_BATTLE]==0);
 }
 setup();run.money=9007199254740991.;size_t n=gc_run_save(&run,saved,sizeof saved);const char *why=NULL;CHECK(n&&gc_run_load(&loaded,saved,n,&why));CHECK(loaded.money==run.money);
 /* Number payloads remain exact even where the legacy int fields cannot represent them. */
 drain();gc_ev_money(&run,1e18+128);gc_event event;CHECK(gc_run_next_event(&run,&event));CHECK(event.money_change==1e18+128&&event.money_after==run.money&&event.a==0&&event.b==0);
 printf("Honey Gather real BattleEnd/wallet/save: %d checks passed\n",checks);return 0;
}
