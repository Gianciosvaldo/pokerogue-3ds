/* Real run->battle challenge inputs and the Relic Song owning phase. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static gc_run run;static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"Meloetta host line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void drain(void){gc_event e;while(gc_run_next_event(&run,&e)){}}
static void setup(bool mono)
{
 const char *why=NULL;gc_run_new(&run,GAME_MODES_CLASSIC,"meloetta-host",777);
 gc_starter s={.species=SPECIES_ID_MELOETTA,.tera_type=-1,.moves={MOVE_ID_RELIC_SONG}};memset(s.ivs,15,sizeof s.ivs);
 CHECK(gc_run_set_starters(&run,&s,1,&why));drain();
 run.run.mode=GAME_MODES_CHALLENGE;run.run.has_challenges=mono;run.run.challenge_flags=mono?PR_CHALLENGE_SINGLE_TYPE:0;run.run.challenge_type=mono?1:0;
 /* The return value means a new lead was sent out; a retained lead returns false. */
 gc_battle_setup_field(&run,false);
 CHECK(run.bt.challenge_flags==(mono?PR_CHALLENGE_SINGLE_TYPE:0));
 pr_pokemon *p=run.bt.field[0],*enemy=run.bt.field[2];CHECK(p&&enemy);p->level=50;pr_recalculate_stats(&run.bt,p);p->hp=pr_get_stat(p,STAT_HP,true);p->custom_ability=ABILITY_ID_BALL_FETCH;p->ability_override=-1;pr_refresh_ability(p);p->passive_flag=false;p->passive=0;p->custom_passive=0;
 enemy->hp=pr_get_stat(enemy,STAT_HP,true);enemy->custom_ability=ABILITY_ID_BALL_FETCH;enemy->ability_override=-1;pr_refresh_ability(enemy);enemy->n_moves=1;enemy->moves[0]=MOVE_ID_SPLASH;
}
int main(void)
{
 for(int mono=0;mono<2;mono++){
  setup(mono);pr_pokemon *p=run.bt.field[0];CHECK(p->form==0);
  const char *why=NULL;gc_choice c={GC_CH_FIGHT,0,-1,0};CHECK(gc_run_choose(&run,&c,&why));drain();CHECK(p->form==(mono?0:1));
 }
 printf("Meloetta run challenge/owning form: %u checks passed\n",checks);return 0;
}
