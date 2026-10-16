/* Real start-controller integration; source owning-phase answers are separate. */
#include "pr/engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"summon entry host line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
int main(void){pr_closures_init();
 for(int dbl=0;dbl<2;dbl++)for(int side=0;side<2;side++){
  static pr_battle bt;static pr_engine e;memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);pr_pokemon actors[6]={0};bt.battle_type=BATTLE_TYPE_TRAINER;bt.double_battle=dbl;bt.wave_index=1;bt.ignoring_effect_source=-1;bt.last_used_pokeball=-1;bt.trainer_names[0]=bt.trainer_names[1]="Aday";bt.turn=1;pr_seed_from_ascii(&bt.battle_seed,"summon-entry-host");bt.wave_seed=bt.battle_seed;pr_rng_sow(&bt.rng,&bt.battle_seed);bt.rng_live=true;
  for(int k=0;k<6;k++){pr_pokemon *p=&actors[k];p->id=k+1;p->species=k<3?SPECIES_ID_BULBASAUR:SPECIES_ID_MAGIKARP;p->is_player=k<3;p->level=5;p->custom_ability=ABILITY_ID_NONE;p->custom_passive=ABILITY_ID_NONE;p->ability_override=-1;p->added_type=-1;memset(p->ivs,15,sizeof p->ivs);bt.party[k/3][k%3]=p;bt.n_party[k/3]++;pr_calculate_stats(&bt,p);p->hp=p->stats[STAT_HP];}
  /* A fresh start with a fainted second member must find a living reserve. */
  if(dbl)actors[side*3+1].hp=0;
  for(int s=0;s<2;s++)for(int k=0;k<(dbl?2:1);k++)bt.field[s*2+k]=bt.party[s][k];
  pr_start_battle(&bt,&e);CHECK(!e.overflow&&!e.events_overflow);for(int k=0;k<bt.n_unported;k++)fprintf(stderr,"controller double%d side%d: %s\n",dbl,side,pr_unported_name(bt.unported[k]));CHECK(bt.n_unported==0);CHECK(e.stop==PH_TURN_INIT);
  for(int s=0;s<2;s++){int count=dbl?2:1;for(int k=0;k<count;k++){pr_pokemon *p=bt.field[s*2+k];CHECK(p&&p==bt.party[s][k]&&p->on_field&&!p->switch_out_status&&p->hp>0);}if(dbl)CHECK(bt.field[s*2]!=bt.field[s*2+1]);}
  if(dbl)CHECK(bt.party[side][1]==&actors[side*3+2]&&bt.party[side][2]==&actors[side*3+1]);
 }
 printf("Summon entry controller: %d checks passed\n",checks);return 0;
}
