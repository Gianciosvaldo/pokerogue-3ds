#include "pr/engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static pr_battle bt;static pr_engine engine;static pr_pokemon party[4];static int checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"faint guard line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
int main(void){bool unknown;pr_closures_init();
 for(int side=0;side<2;side++){
  memset(&bt,0,sizeof bt);CHECK(!pr_faint_history_contains(&bt,side,0,&unknown)&&!unknown&&!bt.n_unported);
  for(int i=0;i<100;i++){pr_record_faint(&bt,side,i%6==0?UINT32_MAX:(uint32_t)(i%6-1),i);CHECK(!bt.n_unported&&bt.faint_history_total[side]==(uint32_t)i+1&&bt.n_faint_history[side]==(i<12?i+1:12));}
  CHECK(pr_faint_history_contains(&bt,side,0,&unknown)&&!unknown&&pr_faint_history_contains(&bt,side,UINT32_MAX,&unknown)&&!unknown);
  CHECK(!pr_faint_history_contains(&bt,side,100,&unknown)&&!unknown);
  for(int k=0;k<12;k++){CHECK(bt.faint_history[side][k].turn==88+k);uint32_t id=(88+k)%6==0?UINT32_MAX:(uint32_t)((88+k)%6-1);CHECK(bt.faint_history[side][k].id==id);}
  memset(&bt,0,sizeof bt);pr_record_faint(&bt,side,0,0);for(int i=1;i<50;i++)pr_record_faint(&bt,side,1,i);
  CHECK(pr_faint_history_contains(&bt,side,0,&unknown)&&!unknown&&!bt.n_unported);
  // Complete raw histories can be supplied without a cached identity summary.
  memset(&bt,0,sizeof bt);bt.n_faint_history[side]=12;for(int k=0;k<12;k++){bt.faint_history[side][k].id=k%3;bt.faint_history[side][k].turn=k;}
  CHECK(!pr_faint_history_contains(&bt,side,5,&unknown)&&!unknown);
  pr_record_faint(&bt,side,3,12);for(int i=13;i<50;i++)pr_record_faint(&bt,side,4,i);
  CHECK(pr_faint_history_contains(&bt,side,3,&unknown)&&!unknown&&!bt.n_unported&&bt.faint_history_total[side]==50);
  // A missing prefix cannot establish absence. A new match is still certain.
  memset(&bt,0,sizeof bt);bt.n_faint_history[side]=12;bt.faint_history_total[side]=100;for(int i=0;i<12;i++)bt.faint_history[side][i].id=1;
  CHECK(!pr_faint_history_contains(&bt,side,2,&unknown)&&unknown&&bt.n_unported);
  bt.n_unported=0;pr_record_faint(&bt,side,2,100);CHECK(pr_faint_history_contains(&bt,side,2,&unknown)&&!unknown&&!bt.n_unported);
  CHECK(!pr_faint_history_contains(&bt,side,3,&unknown)&&unknown&&bt.n_unported);
  // More identities than the current party contract retains remain reported.
  memset(&bt,0,sizeof bt);for(int i=0;i<7;i++)pr_record_faint(&bt,side,i,i);CHECK(bt.n_unported&&!bt.faint_ids_complete[side]);
  bt.n_unported=0;bt.n_fainted_ids[side]=255;CHECK(!pr_faint_history_contains(&bt,side,99,&unknown)&&unknown&&bt.n_unported);
  bt.n_unported=0;pr_record_faint(&bt,side,99,8);CHECK(bt.n_unported);
  memset(&bt,0,sizeof bt);bt.faint_history_total[side]=UINT32_MAX;pr_record_faint(&bt,side,0,100);CHECK(bt.faint_history_total[side]==UINT32_MAX&&bt.n_unported);
 }
 // Existing signed arena/battle counters must report their numeric limit without UB.
 for(int side=0;side<2;side++){
  memset(&bt,0,sizeof bt);memset(&engine,0,sizeof engine);memset(party,0,sizeof party);
  bt.engine=&engine;bt.wave_index=1;bt.game_mode=GAME_MODES_CLASSIC;bt.max_exp_level=100;bt.double_battle=true;pr_seed_from_ascii(&bt.wave_seed,"faint-counter-boundary");
  for(int k=0;k<4;k++){pr_pokemon*p=&party[k];p->id=k+1;p->species=SPECIES_ID_MEW;p->hp=p->stats[STAT_HP]=100;p->level=30;p->is_player=k<2;p->on_field=true;p->ability_override=-1;p->stats[STAT_SPD]=10+k;bt.field[k]=p;bt.party[k/2][k%2]=p;}
  bt.n_party[0]=bt.n_party[1]=2;party[side*2].hp=0;bt.player_faints=bt.enemy_faints=INT32_MAX;
  engine.current=-1;engine.n_levels=1;int phase=pr_phase_new(&engine,PH_FAINT,side*2);engine.current=phase;engine.pool[phase].prevent_instant_revive=true;engine.pool[phase].source=-1;pr_phase_run(&bt,&engine,phase);
  CHECK(bt.player_faints==INT32_MAX&&bt.enemy_faints==INT32_MAX&&bt.n_unported&&!engine.overflow);CHECK(bt.faint_history_total[side]==1&&bt.n_faint_history[side]==1);
 }
 memset(&bt,0,sizeof bt);CHECK(!pr_faint_history_contains(&bt,2,0,&unknown)&&unknown&&bt.n_unported);pr_record_faint(&bt,-1,0,0);
 printf("Faint history raw/missing-prefix/identity/overflow guards: %d checks passed\n",checks);return 0;}
