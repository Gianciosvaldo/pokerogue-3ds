/* Native form commands, legal saves and callback-before-recall integration. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run run,loaded;
static unsigned char saved[GC_SAVE_MAX];
static int checks;
#define CHECK(x) do { checks++; if(!(x)){fprintf(stderr,"native form host line%d: %s\n",__LINE__,#x);exit(1);} } while(0)
static void drain(void){gc_event ev;while(gc_run_next_event(&run,&ev)){} }
static void fight(int slot){const char *why=NULL;gc_choice c={GC_CH_FIGHT,slot,-1,0};CHECK(gc_run_choose(&run,&c,&why));drain();}
static void continue_to_save(void){
 const char *why=NULL;
 for(int step=0;step<25&&!gc_run_can_save(&run);step++){
  gc_choice c={0};
  switch(run.screen.kind){
  case GC_SCREEN_REWARD:c.kind=GC_CH_SKIP;break;
  case GC_SCREEN_LEARN_MOVE:c.kind=GC_CH_FORGET;c.a=-1;break;
  case GC_SCREEN_EVOLVE:c.kind=GC_CH_CANCEL;c.a=1;break;
  case GC_SCREEN_BIOME:c.kind=GC_CH_BIOME;c.a=0;break;
  default:CHECK(false);
  }
  CHECK(gc_run_choose(&run,&c,&why));drain();
 }
 CHECK(gc_run_can_save(&run));
}
static void roundtrip(void){
 const char *why=NULL;CHECK(gc_run_can_save(&run));
 size_t n=gc_run_save(&run,saved,sizeof saved);CHECK(n>0);CHECK(gc_run_load(&loaded,saved,n,&why));
 pr_pokemon *p=run.bt.field[0],*q=loaded.bt.field[0];CHECK(p&&q);
 CHECK(q->species==p->species&&q->form==p->form&&q->ability==p->ability&&q->ability_index==p->ability_index);
 CHECK(q->hp==p->hp&&!memcmp(q->stats,p->stats,sizeof p->stats));
 CHECK(q->custom_ability==p->custom_ability&&gc_run_can_save(&loaded));
}
int main(void){
 const int species[]={SPECIES_ID_DARMANITAN,SPECIES_ID_GALAR_DARMANITAN,SPECIES_ID_MINIOR,SPECIES_ID_WISHIWASHI,SPECIES_ID_MORPEKO,SPECIES_ID_EISCUE,SPECIES_ID_MIMIKYU,SPECIES_ID_ZYGARDE};
 const int abilities[]={ABILITY_ID_ZEN_MODE,ABILITY_ID_ZEN_MODE,ABILITY_ID_SHIELDS_DOWN,ABILITY_ID_SCHOOLING,ABILITY_ID_HUNGER_SWITCH,ABILITY_ID_ICE_FACE,ABILITY_ID_DISGUISE,ABILITY_ID_POWER_CONSTRUCT};
 const int start[]={0,0,0,0,0,0,1,2},after[]={1,1,7,1,1,0,1,4};
 const char *why=NULL;
 for(int k=0;k<8;k++){
  char seed[32];snprintf(seed,sizeof seed,"native-form-save-%d",k);gc_run_new(&run,GAME_MODES_CLASSIC,seed,777);
  gc_starter s={.species=SPECIES_ID_BULBASAUR,.tera_type=-1,.moves={MOVE_ID_SPLASH,MOVE_ID_TACKLE}};memset(s.ivs,15,sizeof s.ivs);
  CHECK(gc_run_set_starters(&run,&s,1,&why));drain();CHECK(gc_run_can_save(&run));
  pr_pokemon *p=run.bt.field[0],*foe=run.bt.field[2];CHECK(p&&foe);
  p->species=species[k];p->form=start[k];p->level=22;p->custom_ability=abilities[k];p->ability_override=-1;pr_refresh_ability(p);pr_recalculate_stats(&run.bt,p);
  run.party[run.party_order[0]].exp=gc_level_total_exp(22,gc_species(species[k])->growth_rate);
  p->hp=k==3?p->stats[STAT_HP]:p->stats[STAT_HP]/2;
  foe->n_moves=1;foe->moves[0]=MOVE_ID_SPLASH;foe->pp_used[0]=foe->pp_up[0]=0;foe->max_pp_override[0]=-1;foe->custom_ability=ABILITY_ID_BALL_FETCH;foe->ability_override=-1;pr_refresh_ability(foe);
  fight(0);CHECK(p->form==after[k]&&p->ability==abilities[k]);CHECK(!run.bt.n_unported&&!run.eng.overflow);
  foe->species=SPECIES_ID_RATTATA;foe->form=0;pr_recalculate_stats(&run.bt,foe);foe->hp=1;fight(1);continue_to_save();roundtrip();
  p=run.bt.field[0];int recalled_form=k==7?2:k==4?p->form:0;
  run.wave=10;run.n_battle_events=0;gc_new_battle(&run);CHECK(run.wave==11);
  int recalls=0;
  for(int i=0;i<run.n_battle_events;i++)if(run.battle_events[i].type==PR_EV_SWITCH_OUT){CHECK(run.battle_events[i].field[0].form==recalled_form);recalls++;}
  CHECK(recalls==1);drain();CHECK(gc_run_can_save(&run));roundtrip();
 }
 printf("native form command/save/cleanup: %d checks passed\n",checks);return 0;
}
