/* Include actual private phase owners: Focus Punch and counter redirection. */
#include "../src/phases.c"
#include <stdio.h>
#include "received-history.native.inc"
static pr_battle battle;static pr_engine engine;static pr_pokemon mons[6];
static int observations,failures,row_index,stage_index;
static void check(double got,double expected,const char*what){observations++;if(got!=expected||battle.n_unported||engine.overflow){if(failures<12)fprintf(stderr,"received row%d stage%d %s got%.17g expected%.17g flags%d overflow%d\n",row_index,stage_index,what,got,expected,battle.n_unported,engine.overflow);if(failures<12)for(int k=0;k<battle.n_unported;k++)fprintf(stderr,"  %s\n",pr_unported_name(battle.unported[k]));failures++;}battle.n_unported=0;engine.overflow=false;}
static void clear_engine(void){memset(&engine,0,sizeof engine);engine.current=-1;engine.n_levels=1;}
int main(void){pr_closures_init();const int counters[]={MOVE_ID_COUNTER,MOVE_ID_MIRROR_COAT,MOVE_ID_METAL_BURST,MOVE_ID_COMEUPPANCE};
 for(row_index=0;row_index<384;row_index++){
  const int subject=received_cases[row_index].subject,context=received_cases[row_index].context,length=received_cases[row_index].length;
  memset(&battle,0,sizeof battle);memset(mons,0,sizeof mons);clear_engine();battle.engine=&engine;battle.double_battle=true;battle.game_mode=GAME_MODES_CLASSIC;battle.wave_index=1;battle.turn=1;pr_seed_from_ascii(&battle.wave_seed,"received-history-native");battle.ignoring_effect_source=-1;
  for(int k=0;k<6;k++){pr_pokemon*p=&mons[k];p->id=received_ids[k];p->species=(k==0||k==1||k==4)?SPECIES_ID_MEW:SPECIES_ID_MAGIKARP;p->is_player=k==0||k==1||k==4;p->level=50;p->hp=100;p->ability_override=ABILITY_ID_NONE;p->added_type=-1;p->tera_type=-1;p->n_moves=1;p->moves[0]=MOVE_ID_FOCUS_PUNCH;p->max_pp_override[0]=-1;for(int s=0;s<6;s++)p->stats[s]=100;}
  int party[2][3]={{0,1,4},{2,3,5}};int side=subject/2;int u_order[3];if(context==0){u_order[0]=subject;u_order[1]=subject+1;u_order[2]=subject==0?4:5;}else if(context==1){u_order[0]=subject+1;u_order[1]=subject;u_order[2]=subject==0?4:5;}else{u_order[0]=subject+1;u_order[1]=subject==0?4:5;u_order[2]=subject;}
  memcpy(party[side],u_order,sizeof u_order);for(int s=0;s<2;s++){battle.n_party[s]=3;for(int k=0;k<3;k++){battle.party[s][k]=&mons[party[s][k]];if(k<2){battle.field[s*2+k]=battle.party[s][k];battle.field[s*2+k]->on_field=true;}}}
  pr_pokemon*u=&mons[subject],*other=&mons[subject==0?2:0];pr_add_tag(&battle,&engine,u,BATTLER_TAG_TYPE_RAGE,1,MOVE_ID_RAGE,u->id);
  for(int n=0;n<length;n++)pr_unshift_attack_received(u,n==0?&received_cases[row_index].first:&received_cases[row_index].filler);
  for(stage_index=0;stage_index<5;stage_index++){
   if(stage_index==1)pr_unshift_attack_received(u,&received_cases[row_index].incoming);
   if(stage_index==2){pr_attack_record zero=received_cases[row_index].filler;zero.move=MOVE_ID_GROWL;zero.damage=0;zero.result=HIT_RESULT_EFFECTIVE;for(int i=0;i<33;i++)pr_unshift_attack_received(u,&zero);}
   if(stage_index==3)pr_reset_turn_data(u);
   if(stage_index==4)pr_unshift_attack_received(u,&received_cases[row_index].fresh);
   battle.rng=received_cases[row_index].stage[stage_index].rng[0];battle.battle_rng=received_cases[row_index].stage[stage_index].rng[1];battle.rng_live=battle.battle_rng_live=true;
   for(int c=0;c<4;c++){
    clear_engine();check(pr_apply_fixed_damage(&battle,u,other,counters[c]),received_cases[row_index].stage[stage_index].damage[c],"counter damage");
    int ph=pr_phase_new(&engine,PH_MOVE,pr_battler_index(&battle,u));engine.pool[ph].pokemon=u;engine.pool[ph].move.id=counters[c];engine.pool[ph].targets=pr_targets_new(&engine,(int8_t[]){BATTLER_INDEX_ATTACKER},1);resolve_counter_attack_target(&battle,&engine,ph);
    int expected=received_cases[row_index].stage[stage_index].target[c];check(tlist(&engine,engine.pool[ph].targets)->v[0],expected==-2?-1:expected,"counter redirect phase");
    check(pr_move_apply_conditions(&battle,counters[c],u,other,3),received_cases[row_index].stage[stage_index].condition[c],"counter condition");
   }
   clear_engine();for(int k=0;k<6;k++){double power=1;pr_apply_variable_power(&battle,u,&mons[k],MOVE_ID_REVENGE,&power);check(power,received_cases[row_index].stage[stage_index].power[k],"Revenge power");}
   pr_modifier berry={.cls=PR_MOD_BERRY,.pokemon_id=u->id,.stack_count=1,.berry_type=BERRY_TYPE_ENIGMA};check(pr_berry_should_apply(&battle,u,&berry),received_cases[row_index].stage[stage_index].enigma,"Enigma");
   clear_engine();int ph=pr_phase_new(&engine,PH_MOVE,pr_battler_index(&battle,u));engine.current=ph;engine.pool[ph].pokemon=u;engine.pool[ph].move=(pr_move_ref){.id=MOVE_ID_FOCUS_PUNCH,.slot=0,.max_pp_override=-1};engine.pool[ph].targets=pr_targets_new(&engine,(int8_t[]){pr_battler_index(&battle,other)},1);
   check(first_failure_check(&battle,&engine,ph),received_cases[row_index].stage[stage_index].focus,"Focus interrupt owner");check(engine.pool[ph].cancelled,received_cases[row_index].stage[stage_index].cancelled,"Focus cancelled");
   clear_engine();check(pr_lapse_tag(&battle,&engine,u,BATTLER_TAG_TYPE_RAGE,BATTLER_TAG_LAPSE_TYPE_AFTER_HIT),1,"Rage lapse");int queued=0,battler=-2,payload=0,total=0;for(int q=0;q<PR_MAX_PHASES;q++)if(engine.used[q])total++;for(int q=0;q<PR_MAX_PHASES;q++)if(engine.used[q]&&engine.pool[q].type==PH_STAT_STAGE_CHANGE){pr_phase*p=&engine.pool[q];queued++;battler=p->battler;payload=p->source_p==u&&p->n_ssc==1&&p->ssc_stat[0]==STAT_ATK&&p->ssc_stages[0]==1;}check(queued,received_cases[row_index].stage[stage_index].rage_count,"Rage queue");check(total,received_cases[row_index].stage[stage_index].rage_total,"complete Rage queue");int markers=0,marker_payload=1;for(int q=0;q<PR_MAX_PHASES;q++)if(engine.used[q]&&engine.pool[q].type==PH_MARKER){markers++;marker_payload&=engine.pool[q].marker_of==PH_STAT_STAGE_CHANGE;}check(markers,received_cases[row_index].stage[stage_index].rage_markers,"Rage markers");check(marker_payload,1,"Rage marker type");check(engine.n_levels,received_cases[row_index].stage[stage_index].rage_levels,"Rage tree levels");check(engine.n_dyn[PH_STAT_STAGE_CHANGE],received_cases[row_index].stage[stage_index].rage_count,"Rage dynamic queue");check(battler,received_cases[row_index].stage[stage_index].rage_battler,"Rage battler");check(payload,received_cases[row_index].stage[stage_index].rage_count,"Rage payload");
   check(memcmp(&battle.rng,&received_cases[row_index].stage[stage_index].rng[0],sizeof(pr_rng))||memcmp(&battle.battle_rng,&received_cases[row_index].stage[stage_index].rng[1],sizeof(pr_rng)),0,"query RNG");
  }
 }
 printf("Received history: 384 native cases, 1920 stages, %d observations; %d mismatches/uncovered\n",observations,failures);return failures?1:0;
}
