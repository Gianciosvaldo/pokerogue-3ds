/* Real weather commands, legal save/load and biome recall form ordering. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run run,loaded;static unsigned char saved[GC_SAVE_MAX];static int checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"weather host line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void drain(void){gc_event ev;while(gc_run_next_event(&run,&ev)){} }
static void fight(int slot){const char *why=NULL;gc_choice c={GC_CH_FIGHT,slot,-1,0};CHECK(gc_run_choose(&run,&c,&why));drain();}
static void roundtrip(void){const char *why=NULL;CHECK(gc_run_can_save(&run));size_t n=gc_run_save(&run,saved,sizeof saved);CHECK(n>0);CHECK(gc_run_load(&loaded,saved,n,&why));pr_pokemon *p=run.bt.field[0],*q=loaded.bt.field[0];CHECK(p&&q&&p->form==q->form&&p->species==q->species&&p->hp==q->hp&&!memcmp(p->stats,q->stats,sizeof p->stats));CHECK(run.bt.weather==loaded.bt.weather&&run.bt.weather_turns_left==loaded.bt.weather_turns_left);}
int main(void)
{
 const char *why=NULL;
 for(int k=0;k<4;k++){
  gc_run_new(&run,GAME_MODES_CLASSIC,"weather-command-save",777);gc_starter s={.species=SPECIES_ID_BULBASAUR,.tera_type=-1,.moves={MOVE_ID_SUNNY_DAY,MOVE_ID_RAIN_DANCE,MOVE_ID_HAIL,MOVE_ID_TACKLE}};memset(s.ivs,15,sizeof s.ivs);CHECK(gc_run_set_starters(&run,&s,1,&why));drain();
  pr_pokemon *p=run.bt.field[0],*foe=run.bt.field[2];CHECK(p&&foe);p->species=k%2?SPECIES_ID_CHERRIM:SPECIES_ID_CASTFORM;p->form=0;p->gender=-1;p->level=22;p->custom_ability=k%2?ABILITY_ID_FLOWER_GIFT:ABILITY_ID_FORECAST;p->ability_override=-1;pr_refresh_ability(p);pr_recalculate_stats(&run.bt,p);p->hp=p->stats[STAT_HP];run.party[run.party_order[0]].exp=gc_level_total_exp(22,gc_species(p->species)->growth_rate);
  foe->n_moves=1;foe->moves[0]=MOVE_ID_SPLASH;foe->pp_used[0]=foe->pp_up[0]=0;foe->max_pp_override[0]=-1;foe->custom_ability=ABILITY_ID_BALL_FETCH;foe->ability_override=-1;pr_refresh_ability(foe);foe->level=70;pr_recalculate_stats(&run.bt,foe);foe->hp=foe->stats[STAT_HP];run.enemy[0].exp=gc_level_total_exp(70,gc_species(foe->species)->growth_rate);
  fight(0);CHECK(p->form==1);fight(1);CHECK(p->form==(k%2?0:2));fight(2);CHECK(p->form==(k%2?0:3));fight(0);CHECK(p->form==1);CHECK(!run.bt.n_unported&&!run.eng.overflow);
  foe->hp=1;fight(3);
  for(int step=0;step<25&&!gc_run_can_save(&run);step++){
   gc_choice c={0};switch(run.screen.kind){case GC_SCREEN_REWARD:c.kind=GC_CH_SKIP;break;case GC_SCREEN_LEARN_MOVE:c.kind=GC_CH_FORGET;c.a=-1;break;case GC_SCREEN_EVOLVE:c.kind=GC_CH_CANCEL;c.a=1;break;case GC_SCREEN_BIOME:c.kind=GC_CH_BIOME;c.a=0;break;default:CHECK(false);}
   CHECK(gc_run_choose(&run,&c,&why));drain();
  }
  roundtrip();p=run.bt.field[0];int old_form=p->form;
  if(k>=2){p->custom_ability=ABILITY_ID_BALL_FETCH;pr_refresh_ability(p);}
  run.wave=10;run.n_battle_events=0;gc_new_battle(&run);drain();CHECK(run.wave==11);CHECK(run.bt.field[0]->form==0);CHECK(!run.bt.n_unported&&!run.eng.overflow);int recalls=0;for(int i=0;i<run.n_battle_events;i++)if(run.battle_events[i].type==PR_EV_SWITCH_OUT){CHECK(run.battle_events[i].field[0].form==old_form);recalls++;}CHECK(recalls==1);roundtrip();
 }
 printf("Weather commands/save/recall: %d checks passed\n",checks);return 0;
}
