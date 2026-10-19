/* Real legal commands: ingestion, queued forms, retaliation and save continuation. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static gc_run run,loaded;static unsigned checks;static unsigned char bytes[GC_SAVE_MAX];
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"Gulp host line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void drain(void){gc_event e;while(gc_run_next_event(&run,&e)){}}
static bool choose(uint8_t kind,int a){const char *why=NULL;gc_choice c={kind,a,-1,0};bool ok=gc_run_choose(&run,&c,&why);if(!ok)fprintf(stderr,"choice%d/%d: %s\n",kind,a,why?why:"?");drain();return ok;}
int main(void)
{
 for(int dive=0;dive<2;dive++)for(int low=0;low<2;low++){
  const char *why=NULL;gc_run_new(&run,GAME_MODES_CLASSIC,"gulp-host",777+dive*2+low);
  gc_starter starter={.species=SPECIES_ID_CRAMORANT,.tera_type=-1,.moves={MOVE_ID_SURF,MOVE_ID_SPLASH,MOVE_ID_DIVE,MOVE_ID_TACKLE}};memset(starter.ivs,15,sizeof starter.ivs);
  CHECK(gc_run_set_starters(&run,&starter,1,&why));drain();CHECK(run.screen.kind==GC_SCREEN_COMMAND);
  pr_pokemon *p=run.bt.field[0],*enemy=run.bt.field[2];CHECK(p&&enemy);
  p->level=50;p->custom_ability=ABILITY_ID_GULP_MISSILE;p->ability_override=-1;pr_refresh_ability(p);p->passive_flag=false;pr_recalculate_stats(&run.bt,p);p->hp=low?pr_get_stat(p,STAT_HP,true)/3:pr_get_stat(p,STAT_HP,true);p->stat_stages[STAT_SPD-1]=6;
  enemy->species=SPECIES_ID_BLISSEY;enemy->form=0;enemy->level=100;enemy->custom_ability=ABILITY_ID_NONE;enemy->ability_override=-1;pr_refresh_ability(enemy);pr_recalculate_stats(&run.bt,enemy);enemy->hp=pr_get_stat(enemy,STAT_HP,true);enemy->moves[0]=MOVE_ID_SPLASH;enemy->n_moves=1;
  CHECK(choose(GC_CH_FIGHT,dive?2:0));CHECK(run.screen.kind==GC_SCREEN_COMMAND);
  /* The game host automatically continues a forced second-turn command. */
  CHECK(run.bt.turn==(dive?3:2));
  CHECK(p->form==(low?2:1));CHECK(pr_get_battler_tag(p,low?BATTLER_TAG_TYPE_GULP_MISSILE_PIKACHU:BATTLER_TAG_TYPE_GULP_MISSILE_ARROKUDA));
  enemy->moves[0]=MOVE_ID_TACKLE;double hp=enemy->hp;CHECK(choose(GC_CH_FIGHT,1));CHECK(p->form==0);CHECK(enemy->hp<hp);CHECK(!pr_get_battler_tag(p,BATTLER_TAG_TYPE_GULP_MISSILE_ARROKUDA)&&!pr_get_battler_tag(p,BATTLER_TAG_TYPE_GULP_MISSILE_PIKACHU));CHECK(low?enemy->status==STATUS_EFFECT_PARALYSIS:enemy->stat_stages[STAT_DEF-1]==-1);
  p->level=100;pr_recalculate_stats(&run.bt,p);p->hp=pr_get_stat(p,STAT_HP,true);enemy->hp=1;enemy->moves[0]=MOVE_ID_SPLASH;CHECK(choose(GC_CH_FIGHT,3));
  for(int i=0;i<30&&!gc_run_can_save(&run);i++){
   if(run.screen.kind==GC_SCREEN_REWARD)CHECK(choose(GC_CH_SKIP,0));else if(run.screen.kind==GC_SCREEN_LEARN_MOVE)CHECK(choose(GC_CH_FORGET,-1));else if(run.screen.kind==GC_SCREEN_EVOLVE)CHECK(choose(GC_CH_CANCEL,1));else if(run.screen.kind==GC_SCREEN_BIOME)CHECK(choose(GC_CH_BIOME,0));else {fprintf(stderr,"unexpected continuation%d\n",run.screen.kind);return 1;}
  }
  CHECK(gc_run_can_save(&run));size_t n=gc_run_save(&run,bytes,sizeof bytes);CHECK(n>0&&gc_run_load(&loaded,bytes,n,&why));CHECK(loaded.bt.field[0]&&loaded.bt.field[0]->form==0);
 }
 printf("Gulp Missile legal Surf/Dive/retaliation/save: %u checks passed\n",checks);return 0;
}
