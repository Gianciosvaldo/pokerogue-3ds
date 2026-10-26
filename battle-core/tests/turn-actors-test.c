/* The actual dynamic pop owner is used for the observed post-input appends. */
#include "../src/engine.c"
#include <stdio.h>
#include "turn-actors.native.inc"
static pr_battle bt;static pr_engine e;static pr_pokemon mons[12];static int failures,observations;
static void pop_actor(int actor){int ph=pr_phase_new(&e,PH_MOVE,actor<2?actor:actor==6?2:3);e.pool[ph].pokemon=&mons[actor];e.pool[ph].move.id=MOVE_ID_SPLASH;e.dyn[PH_MOVE][e.n_dyn[PH_MOVE]++]=ph;int got=dyn_pop(&bt,&e,PH_MOVE);if(got!=ph)e.overflow=true;phase_free(&e,ph);}
int main(void){pr_closures_init();for(int row=0;row<336;row++){
 memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);memset(mons,0,sizeof mons);bt.engine=&e;bt.game_mode=GAME_MODES_CLASSIC;bt.wave_index=bt.turn=1;bt.double_battle=true;bt.ignoring_effect_source=-1;bt.rng=order_rng[0];bt.battle_rng=order_rng[1];bt.rng_live=bt.battle_rng_live=true;pr_seed_from_ascii(&bt.wave_seed,"turn-order-api-wave");bt.n_party[0]=bt.n_party[1]=6;
 for(int k=0;k<12;k++){pr_pokemon*p=&mons[k];p->id=order_cases[row].ids[k];p->species=k<6?SPECIES_ID_MEW:SPECIES_ID_MAGIKARP;p->level=50;p->hp=100;p->ability_override=ABILITY_ID_NONE;p->added_type=p->tera_type=-1;p->is_player=k<6;p->on_field=k<2||(k>=6&&k<8);for(int s=0;s<6;s++)p->stats[s]=100;p->n_moves=1;p->moves[0]=MOVE_ID_SPLASH;p->max_pp_override[0]=-1;bt.party[k/6][k%6]=p;pr_push_move_history(p,order_cases[row].initial[k]);}
 bt.field[0]=&mons[0];bt.field[1]=&mons[1];bt.field[2]=&mons[6];bt.field[3]=&mons[7];
 for(int i=0;i<order_cases[row].length;i++){int actor=order_cases[row].period[i%order_cases[row].period_n];for(int q=0;q<order_cases[row].patch_n;q++)if(order_cases[row].patch[q].at==i)actor=order_cases[row].patch[q].value;pop_actor(actor);}
 for(int stage=0;stage<5;stage++){
  if(stage==1)pr_push_move_history(&mons[order_cases[row].selected],order_cases[row].update);
  if(stage==2)for(int k=0;k<12;k++)mons[k].id=order_cases[row].ids[k]^0x80000000u;
  if(stage==3){int actor=order_cases[row].selected,side=actor/6;for(int k=actor%6;k<5;k++)bt.party[side][k]=bt.party[side][k+1];bt.n_party[side]--;}
  if(stage==4)pr_clear_turn_order(&bt);
  for(int c=0;c<2;c++){double power=1;bt.n_unported=0;pr_apply_variable_power(&bt,&mons[0],&mons[6],c?MOVE_ID_FUSION_BOLT:MOVE_ID_FUSION_FLARE,&power);observations++;if(power!=order_cases[row].power[stage][c]||bt.n_unported||e.overflow||memcmp(&bt.rng,&order_rng[0],sizeof(pr_rng))||memcmp(&bt.battle_rng,&order_rng[1],sizeof(pr_rng))){if(failures<8)fprintf(stderr,"turn-order row%d stage%d caller%d got%.17g expected%d flags%d overflow%d\n",row,stage,c,power,order_cases[row].power[stage][c],bt.n_unported,e.overflow);failures++;}}
 }
 }printf("Turn actors: 336 native cases, 3360 caller/RNG observations; %d mismatches/uncovered\n",failures);return failures?1:0;}
