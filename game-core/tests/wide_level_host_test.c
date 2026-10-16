/* Augmented high-level constructor and first-command save continuity. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run run, loaded;static gc_mon mon;
static unsigned char saved[GC_SAVE_MAX];static int checks;
#define CHECK(x) do { checks++; if (!(x)) {fprintf(stderr,"wide-level host line%d: %s\n",__LINE__,#x);exit(1);} } while(0)
static gc_starter starter={.species=SPECIES_ID_BULBASAUR,.tera_type=-1,.moves={MOVE_ID_SPLASH}};
static const char *why;
static void drain(gc_run *r){gc_event ev;while(gc_run_next_event(r,&ev)){}}
static void legacy(void){
 FILE *f=fopen("tests/fixtures/byte-level-v2.hex","r");CHECK(f);size_t n=0;unsigned byte;
 while(fscanf(f,"%2x",&byte)==1){if(n>=sizeof saved)CHECK(false);saved[n++]=(unsigned char)byte;}CHECK(feof(f));fclose(f);CHECK(n==8582);
 CHECK(gc_run_load(&loaded,saved,n,&why));CHECK(!loaded.load_dropped);
 CHECK(loaded.party[0].b.level==255&&loaded.party[0].met_level==255);
 CHECK(loaded.party[1].b.level==127&&loaded.party[1].met_level==127);
 CHECK(loaded.enemy[0].b.level>=1&&loaded.enemy[0].met_level==loaded.enemy[0].b.level);
 size_t wide=gc_run_save(&loaded,saved,sizeof saved);CHECK(wide>n);
 CHECK(gc_run_load(&run,saved,wide,&why));CHECK(!run.load_dropped);
 CHECK(run.party[0].b.level==255&&run.party[1].met_level==127);
}
int main(void){
 legacy();memset(starter.ivs,15,sizeof starter.ivs);
 const int levels[]={1,199,200,201,254,255,256,257,511,1000,65535,65536,100000,1000000};
 for(int i=0;i<14;i++){
  int level=levels[i];gc_run_new(&run,i%2?GAME_MODES_ENDLESS:GAME_MODES_SPLICED_ENDLESS,"wide-level-save",777);
  CHECK(gc_run_set_starters(&run,&starter,1,&why));drain(&run);CHECK(gc_run_can_save(&run));
  gc_mon_new_player(&run,&mon,&starter,level);CHECK(mon.b.level==level&&mon.met_level==level);CHECK(mon.b.stats[STAT_HP]>0);CHECK(mon.exp==gc_level_total_exp(level,gc_species(mon.b.species)->growth_rate));
  /* Preserve the installed actor's identity and pointers at a permitted save point. */
  run.party[0].b.level=level;run.party[0].met_level=level;run.party[0].exp=mon.exp;pr_calculate_stats(&run.bt,&run.party[0].b);
  pr_enemy_gen g={.id=run.enemy[0].b.id,.gender=-1,.fusion_species=-1};memset(g.ivs,0,sizeof g.ivs);
  gc_mon_new_enemy(&run,&run.enemy[0],SPECIES_ID_MAGIKARP,level,&g,TRAINER_SLOT_NONE);
  CHECK(run.enemy[0].b.level==level&&run.enemy[0].met_level==level);
  run.enemy[0].b.on_field=true;gc_mon_set_move(&run.enemy[0],0,MOVE_ID_SPLASH);pr_calculate_stats(&run.bt,&run.enemy[0].b);
  /* Commit the construction RNG at the same boundary as the normal battle bridge. */
  run.bt.rng=run.run.rng;
  CHECK(gc_run_can_save(&run));size_t n=gc_run_save(&run,saved,sizeof saved);CHECK(n>0);
  CHECK(gc_run_load(&loaded,saved,n,&why));CHECK(!loaded.load_dropped);
  CHECK(loaded.party[0].b.level==level&&loaded.party[0].met_level==level);
  CHECK(loaded.enemy[0].b.level==level&&loaded.enemy[0].met_level==level);
  CHECK(loaded.party[0].exp==run.party[0].exp);
  CHECK(!memcmp(loaded.party[0].b.stats,run.party[0].b.stats,sizeof mon.b.stats));
  CHECK(!memcmp(loaded.enemy[0].b.stats,run.enemy[0].b.stats,sizeof mon.b.stats));
  gc_choice fight={GC_CH_FIGHT,0,-1,0};
  CHECK(gc_run_choose(&run,&fight,&why));CHECK(gc_run_choose(&loaded,&fight,&why));
  CHECK(run.screen.kind==loaded.screen.kind&&run.screen.kind==GC_SCREEN_COMMAND);
  CHECK(run.party[0].b.hp==loaded.party[0].b.hp&&run.enemy[0].b.hp==loaded.enemy[0].b.hp);
  CHECK(run.party[0].b.level==level&&loaded.party[0].b.level==level);
  CHECK(!memcmp(&run.run.rng,&loaded.run.rng,sizeof(pr_rng)));
  CHECK(!memcmp(&run.bt.rng,&loaded.bt.rng,sizeof(pr_rng)));
  CHECK(run.bt.battle_rng_live==loaded.bt.battle_rng_live&&!memcmp(&run.bt.battle_rng,&loaded.bt.battle_rng,sizeof(pr_rng)));
 }
 /* Signed widening must not admit negative runtime levels or met metadata. */
 drain(&run);gc_run_new(&run,GAME_MODES_ENDLESS,"invalid-wide-level",777);CHECK(gc_run_set_starters(&run,&starter,1,&why));drain(&run);
 run.party[0].b.level=-1;size_t n=gc_run_save(&run,saved,sizeof saved);CHECK(n>0);CHECK(!gc_run_load(&loaded,saved,n,&why));
 run.party[0].b.level=1;run.party[0].met_level=-1;n=gc_run_save(&run,saved,sizeof saved);CHECK(n>0);CHECK(!gc_run_load(&loaded,saved,n,&why));
 run.party[0].met_level=0;n=gc_run_save(&run,saved,sizeof saved);CHECK(n>0);CHECK(gc_run_load(&loaded,saved,n,&why));
 printf("Wide-level constructors/save/command: %d checks passed\n",checks);return 0;
}
