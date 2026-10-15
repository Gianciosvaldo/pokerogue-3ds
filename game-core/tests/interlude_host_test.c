/* Native command/tail/save integration; this is not a legal Classic-clear proof. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run run,loaded;static unsigned char saved[GC_SAVE_MAX];static int checks,weather_damage_messages;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"interlude host line%d: %s (wave%d screen%d)\n",__LINE__,#x,run.wave,run.screen.kind);exit(1);}}while(0)
static void drain(void){gc_event e;while(gc_run_next_event(&run,&e)){if(e.kind!=GC_EV_BATTLE)continue;CHECK(e.a>=0&&e.b>=0&&e.a+e.b<=run.n_battle_events);for(int i=e.a;i<e.a+e.b;i++){const pr_event *b=&run.battle_events[i];if(b->msg.key&&!strcmp(b->msg.key,pr_weather_key(WEATHER_TYPE_HAIL,PR_WEATHER_DAMAGE)))weather_damage_messages++;}}}
int main(void){
 const char *why=NULL;
 for(int mode=0;mode<2;mode++)for(int interlude=0;interlude<2;interlude++){
  gc_run_new(&run,mode?GAME_MODES_ENDLESS:GAME_MODES_CLASSIC,"interlude-tail",777);gc_starter s={.species=SPECIES_ID_BULBASAUR,.tera_type=-1,.moves={MOVE_ID_TACKLE}};memset(s.ivs,15,sizeof s.ivs);CHECK(gc_run_set_starters(&run,&s,1,&why));drain();CHECK(gc_run_can_save(&run));
  pr_pokemon *p=run.bt.field[0],*enemy=run.bt.field[2];CHECK(p&&enemy);run.wave=run.bt.wave_index=interlude?10:9;p->custom_ability=ABILITY_ID_BALL_FETCH;p->ability_override=-1;pr_refresh_ability(p);enemy->custom_ability=ABILITY_ID_BALL_FETCH;enemy->ability_override=-1;pr_refresh_ability(enemy);enemy->n_moves=1;enemy->moves[0]=MOVE_ID_SPLASH;enemy->pp_used[0]=enemy->pp_up[0]=0;enemy->max_pp_override[0]=-1;enemy->hp=1;p->temp_turn_count=2;p->temp_wave_turn_count=3;
  run.bt.weather=WEATHER_TYPE_HAIL;run.bt.weather_turns_left=run.bt.weather_max_duration=3;unsigned before=run.unported[PR_FEAT_PHASE_0+PH_CHECK_INTERLUDE];
  weather_damage_messages=0;gc_choice fight={GC_CH_FIGHT,0,-1,0};CHECK(gc_run_choose(&run,&fight,&why));drain();CHECK(run.unported[PR_FEAT_PHASE_0+PH_CHECK_INTERLUDE]==before);
  CHECK(weather_damage_messages==(interlude?0:1));
  for(int step=0;step<30&&!gc_run_can_save(&run);step++){gc_choice c={0};switch(run.screen.kind){case GC_SCREEN_REWARD:c.kind=GC_CH_SKIP;break;case GC_SCREEN_LEARN_MOVE:c.kind=GC_CH_FORGET;c.a=-1;break;case GC_SCREEN_EVOLVE:c.kind=GC_CH_CANCEL;c.a=1;break;case GC_SCREEN_BIOME:c.kind=GC_CH_BIOME;c.a=0;break;default:CHECK(false);}CHECK(gc_run_choose(&run,&c,&why));drain();}
  CHECK(gc_run_can_save(&run));size_t n=gc_run_save(&run,saved,sizeof saved);CHECK(n>0);CHECK(gc_run_load(&loaded,saved,n,&why));CHECK(!loaded.load_dropped);CHECK(run.wave==loaded.wave&&run.bt.turn==loaded.bt.turn);CHECK(loaded.bt.field[0]&&loaded.bt.field[0]->hp==run.bt.field[0]->hp);CHECK(!run.eng.overflow);
 }
 printf("Interlude commands/tail/save: %d checks passed\n",checks);return 0;
}
