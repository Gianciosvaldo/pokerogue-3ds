/* Real battle victory, egg lapse, profile rewards and save/reload. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run run,resumed;static gc_system sys,loaded;
static unsigned char saved[524288];
static int checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"hatch host line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static int messages,reveals;
static void drain(void){gc_event ev;while(gc_run_next_event(&run,&ev)){if(ev.kind==GC_EV_EGG_HATCH){reveals++;CHECK(ev.egg_nature<25);for(int k=0;k<6;k++)CHECK(ev.egg_ivs[k]<=31);CHECK(ev.a==SPECIES_ID_BULBASAUR&&ev.b==0&&ev.egg_variant==0&&!ev.egg_shiny);}if(ev.kind==GC_EV_MESSAGE&&ev.msg.key&&!strcmp(ev.msg.key,"egg:hatchFromTheEgg"))messages++;}}
int main(void){
 const char *why=NULL;
 for(int n=2;n<=99;n+=97){
  gc_system_init_defaults(&sys);gc_run_new(&run,GAME_MODES_CLASSIC,"egg-host",1234);run.sys=&sys;
  gc_starter starter={.species=SPECIES_ID_BULBASAUR,.tera_type=-1,.nature=gc_default_starter_natures[0]};
  memset(starter.ivs,15,sizeof starter.ivs);
  CHECK(gc_run_set_starters(&run,&starter,1,&why));drain();
  CHECK(run.screen.kind==GC_SCREEN_COMMAND);
  run.bt.field[0]->moves[0]=MOVE_ID_WATER_GUN;run.bt.field[0]->n_moves=1;
  run.bt.field[0]->max_pp_override[0]=-1;run.bt.field[0]->pp_used[0]=0;
  pr_pokemon *enemy=run.bt.field[2];CHECK(enemy);enemy->hp=1;enemy->moves[0]=MOVE_ID_SPLASH;enemy->n_moves=1;
  enemy->pp_used[0]=0;enemy->max_pp_override[0]=-1;
  sys.n_eggs=n;messages=reveals=0;
  for(int i=0;i<n;i++)sys.eggs[i]=(gc_egg){.id=2654435761u+i,.hatch_waves=i==0?0:1,.species=SPECIES_ID_BULBASAUR,.egg_move_index=i%4,.source_type=i%3};
  uint32_t old_hatches=sys.dex[SPECIES_ID_BULBASAUR].hatched_count;
  gc_choice choice={GC_CH_FIGHT,0,-1,0};CHECK(gc_run_choose(&run,&choice,&why));drain();
  CHECK(sys.n_eggs==0&&messages==0&&reveals==n&&!run.ev_overflow);
  CHECK(sys.dex[SPECIES_ID_BULBASAUR].hatched_count==old_hatches+n);
  CHECK(sys.starters[SPECIES_ID_BULBASAUR].candy_count>=2*n);
  CHECK(sys.starters[SPECIES_ID_BULBASAUR].egg_moves==(n==2?3:15));
  CHECK(sys.stats.total_hatches==n);
  size_t count=gc_system_save(&sys,saved,sizeof saved);CHECK(count>0);
  CHECK(gc_system_load(&loaded,saved,count,&why));
  CHECK(loaded.n_eggs==0&&loaded.stats.total_hatches==n);
  run.sys=&loaded;CHECK(gc_system_hatch_ready_eggs(&run)==0);CHECK(loaded.stats.total_hatches==n);
  if(run.screen.kind==GC_SCREEN_REWARD){choice=(gc_choice){GC_CH_SKIP,0,0,0};CHECK(gc_run_choose(&run,&choice,&why));drain();}
  CHECK(gc_run_can_save(&run));
  count=gc_run_save(&run,saved,sizeof saved);CHECK(count>0);
  CHECK(gc_run_load(&resumed,saved,count,&why)&&!resumed.load_dropped);
  resumed.sys=&loaded;
  CHECK(gc_system_hatch_ready_eggs(&resumed)==0&&loaded.stats.total_hatches==n);

 }
 /* Future eggs keep order/countdown; failed legacy/invalid eggs stay recoverable. */
 sys.n_eggs=3;run.sys=&sys;
 sys.eggs[0]=(gc_egg){.id=3,.hatch_waves=2,.species=SPECIES_ID_BULBASAUR};
 sys.eggs[1]=(gc_egg){.id=4,.hatch_waves=0,.species=0};
 sys.eggs[2]=(gc_egg){.id=5,.hatch_waves=1,.species=SPECIES_ID_PICHU};
 gc_system_increment_egg_steps(&sys);CHECK(gc_system_hatch_ready_eggs(&run)==1);
 CHECK(sys.n_eggs==2&&sys.eggs[0].id==3&&sys.eggs[0].hatch_waves==1&&sys.eggs[1].id==4);
 CHECK(gc_system_hatch_ready_eggs(&run)==0);
 gc_system_increment_egg_steps(&sys);CHECK(gc_system_hatch_ready_eggs(&run)==1);
 CHECK(sys.n_eggs==1&&sys.eggs[0].id==4);
 printf("Egg hatch owner/profile/save: %d checks passed\n",checks);return 0;
}
