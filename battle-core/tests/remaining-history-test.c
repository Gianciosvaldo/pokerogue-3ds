/* Include the actual owning phase to exercise its private accuracy check. */
#include "../src/phases.c"
#include <stdio.h>
#include "remaining-history.native.inc"
static pr_battle battle;static pr_engine engine;static pr_pokemon mons[4];
#ifndef RH_OBSERVE
#define RH_OBSERVE(row,stage,subject) ((void)0)
#endif
int main(void){pr_closures_init();int failures=0,observations=0;
 for(int row=0;row<192;row++){
  memset(&battle,0,sizeof battle);memset(&engine,0,sizeof engine);memset(mons,0,sizeof mons);battle.engine=&engine;battle.double_battle=true;battle.ignoring_effect_source=-1;
  for(int k=0;k<4;k++){pr_pokemon*p=&mons[k];p->id=k+1;p->species=SPECIES_ID_MEW;p->level=50;p->hp=p->stats[STAT_HP]=100;p->is_player=k<2;p->on_field=true;p->ability_override=-1;p->added_type=-1;battle.field[k]=p;}
  pr_pokemon*u=&mons[0],*t=&mons[2];int length=remaining_cases[row].length,move=remaining_cases[row].move,mode=remaining_cases[row].mode;
  int8_t targets[]={3,2};int shared=pr_targets_new(&engine,targets,2);
  for(int i=0;i<length;i++){pr_turn_move tm={.move=i==0?move:MOVE_ID_NONE,.use_mode=mode,.result=i%5,.n_targets=i==0?2:1,.targets={i==0?3:2,2}};if(i==0)pr_push_history_shared(&battle,&engine,u,tm,shared);else pr_push_move_history(u,tm);}
  int fresh=-1;
  for(int stage=0;stage<10;stage++){
   if(stage==1)pr_add_tag(&battle,&engine,u,BATTLER_TAG_TYPE_IGNORE_ACCURACY,2,MOVE_ID_LOCK_ON,u->id);
   // The first target list is shared with its original history entry.
   if(stage==2&&length){engine.target_lists[shared].v[0]=1;pr_targets_changed(&battle,&engine,shared);}
   if(stage==3)pr_push_move_history(u,(pr_turn_move){.move=MOVE_ID_LOCK_ON,.use_mode=mode,.result=2,.n_targets=2,.targets={0,3}});
   if(stage==4)for(int i=0;i<9;i++)pr_push_move_history(u,(pr_turn_move){.move=MOVE_ID_NONE,.use_mode=mode,.result=4,.n_targets=1,.targets={2}});
   if(stage==5)pr_push_move_history(u,(pr_turn_move){.move=MOVE_ID_MIND_READER,.use_mode=mode,.result=3,.n_targets=1,.targets={1}});
   if(stage==6)pr_reset_summon_data(u);
   if(stage==7){int8_t next[]={2};fresh=pr_targets_new(&engine,next,1);pr_push_history_shared(&battle,&engine,u,(pr_turn_move){.move=MOVE_ID_MIND_READER,.use_mode=mode,.result=2},fresh);for(int i=0;i<9;i++)pr_push_move_history(u,(pr_turn_move){.move=MOVE_ID_NONE,.use_mode=mode,.result=4,.n_targets=1,.targets={2}});}
   if(stage==8){pr_add_tag(&battle,&engine,u,BATTLER_TAG_TYPE_IGNORE_ACCURACY,2,MOVE_ID_MIND_READER,u->id);engine.target_lists[shared].v[0]=0;pr_targets_changed(&battle,&engine,shared);}
   if(stage==9){engine.target_lists[fresh].v[0]=3;pr_targets_changed(&battle,&engine,fresh);}
   RH_OBSERVE(row,stage,u);
   for(int q=0;q<5;q++){battle.n_unported=0;pr_rng global=battle.rng,local=battle.battle_rng;
    int got=q==0?pr_move_apply_conditions(&battle,MOVE_ID_CONVERSION_2,t,u,4):check_bypass_acc_and_invuln(&battle,u,&mons[q-1],MOVE_ID_TACKLE);
    int want=remaining_cases[row].expected[stage][q]>0;
    if(got!=want||battle.n_unported||memcmp(&global,&battle.rng,sizeof global)||memcmp(&local,&battle.battle_rng,sizeof local)){if(failures<8)fprintf(stderr,"remaining history row%d length%d move%d mode%d stage%d query%d got%d want%d unsupported%d\n",row,length,move,mode,stage,q,got,want,battle.n_unported);failures++;}observations++;
   }
  }
 }
 printf("Remaining history: %d native caller observations; %d mismatches/uncovered\n",observations,failures);return failures?1:0;}
