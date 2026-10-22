/* Real failed throws, once-per-battle inventory return, next-wave reset and save. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static gc_run run,loaded;static unsigned checks;static unsigned char bytes[GC_SAVE_MAX];
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"Ball Fetch host line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void drain(void){gc_event e;while(gc_run_next_event(&run,&e)){}}
static bool choose(uint8_t kind,int a){const char *why=NULL;gc_choice c={kind,a,-1,0};bool ok=gc_run_choose(&run,&c,&why);if(!ok)fprintf(stderr,"choice%d/%d: %s\n",kind,a,why?why:"?");drain();if(ok&&kind==GC_CH_FIGHT&&run.screen.kind==GC_SCREEN_TARGET){gc_choice target={GC_CH_TARGET,2,0,0};ok=gc_run_choose(&run,&target,&why);drain();}return ok;}
int main(void)
{
 for(int ball=0;ball<4;ball++){
  const char *why=NULL;gc_run_new(&run,GAME_MODES_CLASSIC,"ball-fetch-host",777+ball);
  gc_starter starters[3]={{.species=SPECIES_ID_CHARMANDER,.tera_type=-1,.moves={MOVE_ID_SPLASH}},{.species=SPECIES_ID_BULBASAUR,.tera_type=-1,.moves={MOVE_ID_SPLASH}},{.species=SPECIES_ID_SQUIRTLE,.tera_type=-1,.moves={MOVE_ID_SPLASH}}};
  for(int i=0;i<3;i++)memset(starters[i].ivs,15,sizeof starters[i].ivs);
  CHECK(gc_run_set_starters(&run,starters,3,&why));drain();CHECK(run.screen.kind==GC_SCREEN_COMMAND);
  pr_pokemon *p=run.bt.field[0],*enemy=run.bt.field[2];CHECK(p&&enemy);
  /* Throws are legal only after a double encounter has one opponent left. */
  if(run.bt.field[3]){run.bt.field[3]->hp=0;run.bt.field[3]->status=STATUS_EFFECT_FAINT;run.bt.field[3]->on_field=false;}
  p->custom_ability=ABILITY_ID_BALL_FETCH;p->ability_override=-1;pr_refresh_ability(p);
  enemy->species=SPECIES_ID_ARCEUS;enemy->form=0;enemy->level=100;enemy->custom_ability=ABILITY_ID_BALL_FETCH;enemy->ability_override=-1;pr_refresh_ability(enemy);pr_recalculate_stats(&run.bt,enemy);enemy->hp=pr_get_stat(enemy,STAT_HP,true);enemy->moves[0]=MOVE_ID_SPLASH;enemy->n_moves=1;
  run.pokeballs[ball]=ball==3?255:99;
  int initial=run.pokeballs[ball];
  CHECK(choose(GC_CH_BALL,ball));CHECK(run.screen.kind==GC_SCREEN_COMMAND&&run.bt.turn==2);
  CHECK(run.pokeballs[ball]==initial&&run.bt.last_used_pokeball==-1&&pr_wave_ability_applied(p,ABILITY_ID_BALL_FETCH));
  CHECK(choose(GC_CH_BALL,ball));CHECK(run.screen.kind==GC_SCREEN_COMMAND&&run.bt.turn==3);
  CHECK(run.pokeballs[ball]==initial-1&&run.bt.last_used_pokeball==ball);
  /* A normal victory and next encounter reach the source-defined wave reset and save point. */
  p->level=100;pr_recalculate_stats(&run.bt,p);p->hp=pr_get_stat(p,STAT_HP,true);p->moves[0]=MOVE_ID_TACKLE;p->pp_used[0]=0;enemy->hp=1;
  CHECK(choose(GC_CH_FIGHT,0));
  for(int i=0;i<30&&!gc_run_can_save(&run);i++){
   if(run.screen.kind==GC_SCREEN_COMMAND)CHECK(choose(GC_CH_FIGHT,0));
   else if(run.screen.kind==GC_SCREEN_REWARD)CHECK(choose(GC_CH_SKIP,0));
   else if(run.screen.kind==GC_SCREEN_LEARN_MOVE)CHECK(choose(GC_CH_FORGET,-1));
   else if(run.screen.kind==GC_SCREEN_EVOLVE)CHECK(choose(GC_CH_CANCEL,1));
   else if(run.screen.kind==GC_SCREEN_BIOME)CHECK(choose(GC_CH_BIOME,0));
   else {fprintf(stderr,"unexpected continuation%d\n",run.screen.kind);return 1;}
  }
  CHECK(gc_run_can_save(&run));CHECK(!pr_wave_ability_applied(run.bt.field[0],ABILITY_ID_BALL_FETCH));
  int before_save=run.pokeballs[ball];size_t n=gc_run_save(&run,bytes,sizeof bytes);CHECK(n>0&&gc_run_load(&loaded,bytes,n,&why));
  CHECK(loaded.pokeballs[ball]==before_save&&loaded.bt.pokeball_counts_known&&loaded.bt.pokeball_counts[ball]==before_save);
 }
 printf("Ball Fetch failed throws/once/wave/save: %u checks passed\n",checks);return 0;
}
