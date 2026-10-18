/* Legal arena saves and the owning encounter's callback-before-recall order. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run run,loaded;
static unsigned char saved[GC_SAVE_MAX];
static int checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"arena cleanup host line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void drain(void){gc_event ev;while(gc_run_next_event(&run,&ev)){} }
int main(void)
{
 const char *why=NULL;
 for(int k=0;k<4;k++){
  gc_run_new(&run,GAME_MODES_CLASSIC,"arena-cleanup-save",777);
  gc_starter s={.species=SPECIES_ID_BULBASAUR,.tera_type=-1,.moves={MOVE_ID_SPLASH,MOVE_ID_TACKLE}};memset(s.ivs,15,sizeof s.ivs);
  CHECK(gc_run_set_starters(&run,&s,1,&why));drain();CHECK(gc_run_can_save(&run));
  pr_pokemon *p=run.bt.field[0],*foe=run.bt.field[2];CHECK(p&&foe);
  p->custom_ability=ABILITY_ID_DRIZZLE;p->ability_override=-1;pr_refresh_ability(p);
  run.bt.weather=WEATHER_TYPE_SUNNY;run.bt.weather_turns_left=run.bt.weather_max_duration=k%2?0:5;
  run.bt.terrain=TERRAIN_MISTY;run.bt.terrain_turns_left=run.bt.terrain_max_duration=k%2?0:5;
  run.bt.player_faints=3;
  run.bt.n_arena_tags=1;run.bt.arena_tags[0]=(pr_arena_tag){.type=ARENA_TAG_TYPE_NEUTRALIZING_GAS,.side=ARENA_TAG_SIDE_BOTH,.source_id=foe->id,.source_count=1,.layers=1};
  size_t n=gc_run_save(&run,saved,sizeof saved);CHECK(n>0);CHECK(gc_run_load(&loaded,saved,n,&why));CHECK(loaded.bt.n_arena_tags==1&&loaded.bt.arena_tags[0].source_count==1);CHECK(loaded.bt.arena_tags[0].source_id==foe->id);CHECK(loaded.bt.weather==WEATHER_TYPE_SUNNY&&loaded.bt.weather_turns_left==run.bt.weather_turns_left);
  run.wave=k<2?10:4;run.n_battle_events=0;
  if(k<2)gc_new_battle(&run);else{run.battle_type=BATTLE_TYPE_TRAINER;gc_battle_cleanup_players(&run);gc_arena_new_battle(&run,false,false);}
  drain();
  CHECK(run.wave==(k<2?11:4));CHECK(run.bt.n_arena_tags==0);CHECK(run.bt.player_faints==0);CHECK(!run.bt.n_unported&&!run.eng.overflow);
  int recalls=0;for(int i=0;i<run.n_battle_events;i++)if(run.battle_events[i].type==PR_EV_SWITCH_OUT)recalls++;CHECK(recalls==1);
  /* Trainer entry has no biome roll: Drizzle regained during removal persists. */
  if(k>=2){CHECK(run.bt.weather==WEATHER_TYPE_RAIN);CHECK(run.bt.weather_turns_left==5);CHECK(run.bt.terrain==(k%2?TERRAIN_MISTY:TERRAIN_NONE));}
  CHECK(gc_run_can_save(&run));n=gc_run_save(&run,saved,sizeof saved);CHECK(n>0);CHECK(gc_run_load(&loaded,saved,n,&why));CHECK(loaded.bt.n_arena_tags==0);CHECK(loaded.bt.weather==run.bt.weather&&loaded.bt.terrain==run.bt.terrain);
 }
 printf("arena cleanup legal-save/encounter: %d checks passed\n",checks);return 0;
}
