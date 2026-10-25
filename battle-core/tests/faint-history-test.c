/* Actual C FaintPhase and the actual private trainer-Tera consumer. */
#include "../../ai-core/src/ai.c"
#include <stdio.h>
#include "faint-history.native.inc"
static pr_battle battle;static pr_engine engine;static pr_pokemon mons[12];static pr_ai ai;static pr_ai_trainer trainer;
int main(void){pr_closures_init();int failures=0,observations=0;
 for(int row=0;row<72;row++){
  memset(&battle,0,sizeof battle);memset(&engine,0,sizeof engine);memset(mons,0,sizeof mons);memset(&ai,0,sizeof ai);memset(&trainer,0,sizeof trainer);
  battle.engine=&engine;battle.double_battle=true;battle.battle_type=BATTLE_TYPE_TRAINER;battle.wave_index=1;battle.game_mode=GAME_MODES_CLASSIC;battle.ignoring_effect_source=-1;
  battle.rng=faint_global_rng;battle.battle_rng=faint_battle_rng;battle.rng_live=battle.battle_rng_live=true;
  battle.max_exp_level=faint_max_exp_level;pr_seed_from_ascii(&battle.wave_seed,faint_wave_seed);
  ai.trainer=&trainer;trainer.tera_mode=PR_TERA_AI_INSTANT_TERA;trainer.n_instant_teras=6;
  for(int side=0;side<2;side++){
   battle.n_party[side]=6;
   for(int k=0;k<6;k++){pr_pokemon*p=&mons[side*6+k];p->id=side*6+k+10;p->species=side?SPECIES_ID_MAGIKARP:SPECIES_ID_MEW;p->level=30;p->is_player=!side;p->hp=100;p->ability_override=-1;p->tera_type=-1;p->has_trainer=side;
    for(int s=0;s<6;s++)p->stats[s]=100;p->stats[STAT_SPD]=10+k+side*6;battle.party[side][k]=p;
    if(k<2){p->on_field=true;battle.field[side*2+k]=p;}
    if(side){trainer.instant_teras[k]=k;pr_ai_mon_of(&ai,p)->initial_team_index=k;}
   }
  }
  int side=faint_cases[row].side;
  for(int i=0;i<faint_cases[row].length;i++){
   int who=faint_cases[row].events[i].index;pr_pokemon*p=&mons[side*6+who];battle.field[side*2]=p;battle.field[side*2+1]=&mons[side*6+(who?0:1)];
   for(int k=0;k<6;k++)mons[side*6+k].on_field=&mons[side*6+k]==battle.field[side*2]||&mons[side*6+k]==battle.field[side*2+1];
   p->hp=0;p->status=STATUS_EFFECT_NONE;pr_reset_turn_data(p);battle.turn=faint_cases[row].events[i].turn;battle.n_unported=0;
   memset(&engine,0,sizeof engine);engine.current=-1;engine.n_levels=1;int ph=pr_phase_new(&engine,PH_FAINT,side*2);engine.current=ph;engine.pool[ph].source=-1;engine.pool[ph].prevent_instant_revive=true;
   pr_rng before=battle.rng,local=battle.battle_rng;pr_phase_run(&battle,&engine,ph);
   if(battle.n_unported||engine.overflow||memcmp(&before,&battle.rng,sizeof before)||memcmp(&local,&battle.battle_rng,sizeof local)){if(failures<5){fprintf(stderr,"phase row%d i%d overflow%d rng%d battle%d",row,i,engine.overflow,!!memcmp(&before,&battle.rng,sizeof before),!!memcmp(&local,&battle.battle_rng,sizeof local));for(int f=0;f<battle.n_unported;f++)fprintf(stderr," %s",pr_unported_name(battle.unported[f]));fputc('\n',stderr);}failures++;}
   observations++;p->hp=100;p->status=STATUS_EFFECT_NONE;p->on_field=true;
  }
  if(battle.player_faints!=faint_cases[row].counts[0]||battle.enemy_faints!=faint_cases[row].counts[1]||battle.n_faint_history[side]!=faint_cases[row].nt)failures++;
  for(int i=0;i<faint_cases[row].nt;i++)if(battle.faint_history[side][i].id!=mons[faint_cases[row].tail[i].index].id||battle.faint_history[side][i].turn!=faint_cases[row].tail[i].turn)failures++;
  for(int d=0;d<4;d++){
   battle.turn=faint_cases[row].length+(int[]){-1,0,1,2}[d];
   for(int side=0;side<2;side++){double power=1;pr_apply_variable_power(&battle,&mons[side*6],&mons[(1-side)*6],MOVE_ID_RETALIATE,&power);if(power!=faint_cases[row].power[d][side])failures++;observations++;}
   for(int k=0;k<6;k++){if(should_tera(&battle,&ai,&mons[6+k])!=faint_cases[row].tera[d][k]){if(failures<8)fprintf(stderr,"faint history Tera row%d candidate%d mismatch\n",row,k);failures++;}observations++;}
  }
 }
 printf("Faint history: 72 native phase/caller cases, %d phase/consumer observations; %d mismatches/uncovered\n",observations,failures);return failures?1:0;}
