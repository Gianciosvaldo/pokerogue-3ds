/* Played integration, not a claim of native full-party generation parity. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include "challengers.native.inc"
static gc_run run;
static unsigned index,checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"Challenger flow case%u line%d %s screen%d\n",index,__LINE__,#x,run.screen.kind);exit(1);}}while(0)
int main(void){
 const unsigned cases[]={0,1,2,96,97,98};
 unsigned turns=0,doubles=0;
 for(unsigned k=0;k<sizeof cases/sizeof *cases;k++){
  index=cases[k];const challenger_case*c=&challenger_native[index];char seed[48],wave[48];
  snprintf(seed,sizeof seed,"challengers-run-%u",index);snprintf(wave,sizeof wave,"challengers-wave-%u",index);
  gc_run_new(&run,GAME_MODES_CLASSIC,seed,1);run.wave=c->wave;run.biome=c->biome;run.wave_cycle_offset=5;
  pr_seed_from_ascii(&run.run.wave_seed,wave);run.n_party=3;
  /* Strong healthy fixture party makes the test about battle/reward plumbing,
   * not AI difficulty or the native opponent moveset comparison. */
  for(int j=0;j<3;j++){gc_starter s={.species=SPECIES_ID_MEW,.tera_type=-1,.moves={MOVE_ID_MOONBLAST}};
   memset(s.ivs,31,6);gc_mon_new_player(&run,&run.party[j],&s,1000);run.party[j].pause_evolutions=true;run.party_order[j]=j;}
  gc_items_to_battle(&run);gc_rq_reset(&gc_int(&run)->rq);
  run.battle_type=BATTLE_TYPE_MYSTERY_ENCOUNTER;run.mystery_type=GC_ME_MYSTERIOUS_CHALLENGERS;
  gc_rq_push(&run,gc_rp(RP_MYSTERY,GC_ME_MYSTERIOUS_CHALLENGERS,GC_ME_ULTRA,0));gc_run_until_screen(&run);
  CHECK(run.screen.kind==GC_SCREEN_MYSTERY);const char*why=NULL;
  CHECK(gc_run_choose(&run,&(gc_choice){.kind=GC_CH_MYSTERY,.a=c->option},&why));
  CHECK(run.screen.kind==GC_SCREEN_COMMAND);CHECK(run.n_enemy==c->n_levels);doubles+=run.double_battle;
  CHECK(!gc_run_choose(&run,&(gc_choice){.kind=GC_CH_RUN},&why));
  unsigned steps=0;
  while(run.screen.kind!=GC_SCREEN_REWARD&&steps++<80){
   if(run.screen.kind==GC_SCREEN_COMMAND){turns++;CHECK(gc_run_choose(&run,&(gc_choice){.kind=GC_CH_FIGHT,.a=0,.b=-1},&why));}
   else if(run.screen.kind==GC_SCREEN_SWITCH){int pick=-1;for(int j=0;j<run.n_party;j++)if(run.screen.pick_ok[j]){pick=j;break;}
    CHECK(pick>=0);CHECK(gc_run_choose(&run,&(gc_choice){.kind=GC_CH_PICK,.a=pick},&why));}
   else if(run.screen.kind==GC_SCREEN_TARGET){int target=-1;for(int j=2;j<4;j++)if(run.screen.target_ok[j]){target=j;break;}
    CHECK(target>=0);CHECK(gc_run_choose(&run,&(gc_choice){.kind=GC_CH_TARGET,.a=target},&why));}
   else if(run.screen.kind==GC_SCREEN_LEARN_MOVE)CHECK(gc_run_choose(&run,&(gc_choice){.kind=GC_CH_FORGET,.a=-1},&why));
   else CHECK(false);
  }
  CHECK(run.screen.kind==GC_SCREEN_REWARD);CHECK(!gc_int(&run)->pillar.active);
  CHECK(run.screen.n_options==(c->option==0?3:4));
  /* Native reward configuration was applied by actual victory, not a direct
   * call to its owner. The source-specific easy functions are visible. */
  if(c->option==0){int memory=0,tm=0;for(int j=0;j<run.screen.n_options;j++){
    memory+=!strcmp(run.screen.options[j].key,"MEMORY_MUSHROOM");tm+=!strncmp(run.screen.options[j].key,"TM_",3);}
   CHECK(memory==1&&tm>=2);}
  CHECK(gc_run_choose(&run,&(gc_choice){.kind=GC_CH_SKIP},&why));
  if(run.screen.kind==GC_SCREEN_BIOME)CHECK(gc_run_choose(&run,&(gc_choice){.kind=GC_CH_BIOME,.a=0},&why));
  CHECK(run.wave==c->wave+1);CHECK(!gc_int(&run)->pillar.active);CHECK(gc_int(&run)->reward.me_custom.n_funcs==0);
 }
 CHECK(doubles>0);printf("Challengers played flow: 6 battles, %u double battles, %u commands, %u checks passed\n",doubles,turns,checks);return 0;
}
