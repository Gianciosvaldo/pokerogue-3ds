/* Finish actual run-level terminal queues: canceled rewards, Honey Gather, and the win screen. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run run;
static int checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"terminal host line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
int main(void){
 for(int i=0;i<32;i++){
  gc_run_new(&run,i&1?GAME_MODES_SPLICED_ENDLESS:GAME_MODES_ENDLESS,"terminal-game-caller",777);
  gc_starter starter={.species=SPECIES_ID_BULBASAUR,.tera_type=-1,.moves={MOVE_ID_SPLASH}};memset(starter.ivs,15,6);const char *why=NULL;CHECK(gc_run_set_starters(&run,&starter,1,&why));gc_event event;while(gc_run_next_event(&run,&event)){}
  run.wave=i&2?5851:5850;run.money=0;run.n_items=0;run.bt.money_scattered=0;run.bt.enemy_faints=!!(i&4);run.bt.successful_run=!!(i&8);
  run.party[0].b.custom_ability=ABILITY_ID_HONEY_GATHER;pr_refresh_ability(&run.party[0].b);run.party[0].b.n_ab_applied_wave=run.party[0].b.n_ab_applied_summon=0;
  gc_rq_clear(&run);gc_int(&run)->rq.waiting=false;gc_int(&run)->rq.current.type=RP_NONE;gc_rq_push(&run,gc_rp(RP_NEW_BATTLE,0,0,0));gc_rphase reward=gc_rp(RP_MODIFIER_REWARD,0,0,0);reward.key="EXP_CHARM";gc_rq_push(&run,reward);
  pr_rng before=run.run.rng;gc_rphase phase=gc_rp(RP_BATTLE_END,!!(i&16),0,0);CHECK(gc_phase_start(&run,&phase));CHECK(gc_rq_has(&run,RP_GAME_OVER)&&!gc_rq_has(&run,RP_NEW_BATTLE)&&!gc_rq_has(&run,RP_MODIFIER_REWARD));
  CHECK(run.money==(i&16?9007199254740991.:0));CHECK(run.party[0].b.n_ab_applied_wave==!!(i&16));CHECK(run.bt.money_scattered==0);
  gc_run_until_screen(&run);CHECK(run.finished&&run.screen.kind==GC_SCREEN_CLEAR);CHECK(!gc_run_can_save(&run));CHECK(run.wave==(i&2?5851:5850));CHECK(!memcmp(&run.run.rng,&before,sizeof before));
 }
 printf("Endless terminal real caller: %d checks passed\n",checks);return 0;
}
