#define main retained_doubles_main
#include "doubles_host_test.c"
#undef main
static void probe(int move){
 setup(1);int wave=r.wave;uint32_t id=r.party[r.party_order[0]].b.id;
 for(int k=0;k<4;k++){pr_pokemon *p=r.bt.field[k];p->hp=p->stats[0]=k<2?100000:1;p->n_moves=1;p->moves[0]=k?MOVE_ID_SPLASH:move;p->stats[1]=p->stats[3]=100000;p->stat_stages[5]=k==0?6:0;p->ability=p->base_ability=p->custom_ability=ABILITY_ID_NONE;p->passive=0;}
 for(int n=0;n<50&&r.wave==wave;n++){
  const gc_screen *s=gc_run_screen(&r);gc_choice c={0};
  switch(s->kind){
  case GC_SCREEN_COMMAND:c=(gc_choice){GC_CH_FIGHT,0,-1,0};break;
  case GC_SCREEN_TARGET:c=(gc_choice){GC_CH_TARGET,s->target_ok[2]?2:3,0,0};break;
  case GC_SCREEN_REWARD:c.kind=GC_CH_SKIP;break;
  case GC_SCREEN_LEARN_MOVE:c=(gc_choice){GC_CH_FORGET,-1,0,0};break;
  case GC_SCREEN_EVOLVE:c.kind=GC_CH_CANCEL;break;
  case GC_SCREEN_CHECK_SWITCH:c.kind=GC_CH_CONFIRM;break;
  default:fprintf(stderr,"unexpected screen%d wave%d\n",s->kind,r.wave);CHECK(false);
  }choose(&r,c);
 }
 pr_pokemon *p=r.bt.field[0];CHECK(p&&p->id==id&&r.wave==wave+1);
 CHECK(p->n_move_history>0);
 pr_pokemon copy=*p;pr_push_move_history(&copy,(pr_turn_move){.move=move,.result=MOVE_RESULT_SUCCESS});
 double power=pr_moves[move].power;pr_apply_variable_power(&r.bt,&copy,r.bt.field[2],move,&power);
 CHECK(power>pr_moves[move].power);
 const char *why=NULL;size_t n=gc_run_save(&r,save,sizeof save);CHECK(n&&gc_run_load(&back,save,n,&why));
 CHECK(back.bt.field[0]->n_move_history==p->n_move_history);
 copy=*back.bt.field[0];pr_push_move_history(&copy,(pr_turn_move){.move=move,.result=MOVE_RESULT_SUCCESS});
 double restored=pr_moves[move].power;pr_apply_variable_power(&back.bt,&copy,back.bt.field[2],move,&restored);CHECK(restored==power);
 pr_leave_field(&r.bt,&r.eng,p,true);CHECK(!p->on_field&&p->n_move_history==0&&p->n_move_queue==0);
 copy=*p;pr_push_move_history(&copy,(pr_turn_move){.move=move,.result=MOVE_RESULT_SUCCESS});
 power=pr_moves[move].power;pr_apply_variable_power(&r.bt,&copy,r.bt.field[2],move,&power);CHECK(power==pr_moves[move].power);
}
static void native_cases(const char *file){
 FILE *f=fopen(file,"r");CHECK(f);char tag[32];int ver,total;CHECK(fscanf(f,"%31s%d%d",tag,&ver,&total)==3&&!strcmp(tag,"CHAIN_POWER")&&ver==1&&total==256);
 setup(1);pr_pokemon *p=r.bt.field[0];
 for(int i=0;i<total;i++){
  int move,n,mode;double expected;CHECK(fscanf(f,"%31s%d%d%d%lf",tag,&move,&n,&mode,&expected)==5&&!strcmp(tag,"C"));
  pr_reset_summon_data(p);
  for(int k=0;k<n;k++)pr_push_move_history(p,(pr_turn_move){.move=mode==3&&k==n-1?MOVE_ID_TACKLE:mode==4&&k==0?MOVE_ID_DEFENSE_CURL:move,.result=(mode==1&&k==n-1)||(mode==2&&k==0)?MOVE_RESULT_FAIL:MOVE_RESULT_SUCCESS});
  pr_push_move_history(p,(pr_turn_move){.move=move,.result=MOVE_RESULT_SUCCESS});
  if(mode==5)pr_reset_wave_data(p);
  if(mode==6)pr_reset_battle_data(p);
  if(mode==7){pr_reset_summon_data(p);pr_push_move_history(p,(pr_turn_move){.move=move,.result=MOVE_RESULT_SUCCESS});}
  pr_rng rng=r.bt.rng,battle=r.bt.battle_rng;pr_pokemon before=*p;
  double power=pr_moves[move].power;pr_apply_variable_power(&r.bt,p,r.bt.field[2],move,&power);
  CHECK(power==expected&&!memcmp(&before,p,sizeof before)&&rngsame(rng,r.bt.rng)&&rngsame(battle,r.bt.battle_rng));
 }
 CHECK(fscanf(f,"%31s%d",tag,&total)==2&&!strcmp(tag,"COMPLETE")&&total==256&&fscanf(f,"%31s",tag)==EOF);fclose(f);
}
int main(int argc,char **argv){CHECK(argc==2);native_cases(argv[1]);probe(MOVE_ID_FURY_CUTTER);probe(MOVE_ID_ROLLOUT);probe(MOVE_ID_ECHOED_VOICE);printf("Chain power: 256 native histories plus wave/save/leave-field ownership; %d checks passed\n",checks);return 0;}
