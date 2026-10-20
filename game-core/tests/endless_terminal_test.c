/* Actual BattleEndPhase queue, observed callback victory, and terminal/reward conditions. */
#include "../src/internal.h"
#define main terminal_frame_main
#include "wide-substitute-frame.inc"
#undef main
static gc_run run;
static int actual_victory=-1;
static gc_rqueue callback_queue;
void __real_pr_prepare_post_battle_money(pr_battle *,pr_engine *,bool);
void __wrap_pr_prepare_post_battle_money(pr_battle *bt,pr_engine *e,bool victory){actual_victory=victory;callback_queue=gc_int(&run)->rq;__real_pr_prepare_post_battle_money(bt,e,victory);}
static const char *name(int type){switch(type){case RP_NEW_BATTLE:return "NewBattlePhase";case RP_TRAINER_VICTORY:return "TrainerVictoryPhase";case RP_MODIFIER_REWARD:return "ModifierRewardPhase";case RP_SELECT_BIOME:return "SelectBiomePhase";case RP_SELECT_MODIFIER:return "SelectModifierPhase";case RP_GAME_OVER:return "GameOverPhase";default:return "UNKNOWN";}}
static bool queue_snapshot(const char *key,const char *row_prefix,gc_rqueue *q){
 double o[4];read_vector(key,o,4);integer(o[0],0,1);int levels=integer(o[1],1,GC_RQ_LEVELS),current=integer(o[2],0,levels-1),count=integer(o[3],0,32);bool ok=actual_victory==o[0]&&q->n_levels==levels&&q->current_level==current;int seen=0;
  for(int l=0;l<levels;l++)for(int k=0;k<(l<q->n_levels?q->n[l]:0);k++){
   int el,ek,flag;char phase_name[80],extra;if(seen++>=count){ok=false;continue;}if(sscanf(line_read(row_prefix),"%d %d %79s %d %c",&el,&ek,phase_name,&flag,&extra)!=4||el<0||el>=levels||ek<0||ek>=32||flag<0||flag>1)bad();if(strcmp(phase_name,"NewBattlePhase")&&strcmp(phase_name,"TrainerVictoryPhase")&&strcmp(phase_name,"ModifierRewardPhase")&&strcmp(phase_name,"SelectBiomePhase")&&strcmp(phase_name,"SelectModifierPhase")&&strcmp(phase_name,"GameOverPhase"))bad();gc_rphase *p=&q->q[l][k];int actual_flag=(p->type==RP_GAME_OVER||p->type==RP_SELECT_MODIFIER)?p->a:0;ok&=el==l&&ek==k&&!strcmp(phase_name,name(p->type))&&actual_flag==flag;
  }
  while(seen<count){line_read(row_prefix);seen++;ok=false;}ok&=seen==count;return ok;
}
int main(int argc,char **argv){
 if(argc!=2||!(input=fopen(argv[1],"r")))return 2;double h[3];read_vector("ENDLESS_TERMINAL ",h,3);int version=integer(h[0],1,2);int n=integer(h[1],2560,2560),failed=0;integer(h[2],0,INT32_MAX);
 for(int i=0;i<n;i++){
  memset(&run,0,sizeof run);gc_rq_reset(&gc_int(&run)->rq);double c[9];read_vector("C ",c,9);if(c[0]!=i)bad();run.run.mode=integer(c[1],0,4);run.wave=integer(c[2],200,5851);int victory=integer(c[3],0,1);run.bt.successful_run=integer(c[4],0,1);run.bt.enemy_faints=integer(c[5],0,1);int dup=integer(c[6],0,3),preselect=integer(c[7],0,1),depth=integer(c[8],0,1);run.n_enemy=1;
  int ordinal=i;int expected_mode=ordinal%5;ordinal/=5;int expected_wave=(int[]){200,5849,5850,5851}[ordinal%4];ordinal/=4;int expected_victory=ordinal%2;ordinal/=2;int expected_success=ordinal%2;ordinal/=2;int expected_faints=ordinal%2;ordinal/=2;int expected_dup=ordinal%4;ordinal/=4;int expected_select=ordinal%2;ordinal/=2;int expected_depth=ordinal%2;
  if(run.run.mode!=expected_mode||run.wave!=expected_wave||victory!=expected_victory||run.bt.successful_run!=expected_success||run.bt.enemy_faints!=expected_faints||dup!=expected_dup||preselect!=expected_select||depth!=expected_depth)bad();
  gc_rq_push(&run,gc_rp(RP_NEW_BATTLE,0,0,0));gc_rq_push(&run,gc_rp(RP_TRAINER_VICTORY,0,0,0));gc_rq_push(&run,gc_rp(RP_MODIFIER_REWARD,0,0,0));if(dup)gc_rq_push(&run,gc_rp(RP_BATTLE_END,0,0,0));
  gc_rq_unshift(&run,gc_rp(RP_SELECT_BIOME,0,0,0));if(dup==2)gc_rq_unshift(&run,gc_rp(RP_BATTLE_END,1,0,0));if(dup==3){gc_int(&run)->rq.current_level=1;gc_rq_unshift(&run,gc_rp(RP_BATTLE_END,1,0,0));gc_int(&run)->rq.current_level=0;}if(preselect)gc_rq_push(&run,gc_rp(RP_SELECT_MODIFIER,0,0,0));gc_int(&run)->rq.current_level=depth;
  rng_read("R ",&run.bt.rng);run.run.rng=run.bt.rng;rng_read("B ",&run.bt.battle_rng);run.bt.rng_live=run.bt.battle_rng_live=true;
  actual_victory=-1;gc_rphase phase=gc_rp(RP_BATTLE_END,victory,0,0);bool ok=gc_phase_start(&run,&phase);
  if(version==2)ok&=queue_snapshot("A ","AP ",&callback_queue);
  ok&=queue_snapshot("O ","P ",&gc_int(&run)->rq);
  pr_rng want;rng_read("RO ",&want);ok&=rng_equal(want,run.run.rng)&&rng_equal(want,run.bt.rng);rng_read("BO ",&want);ok&=rng_equal(want,run.bt.battle_rng);if(strcmp(line_read("E"),"\n"))bad();
  if(!ok){if(failed<12)fprintf(stderr,"terminal case%d mode%d wave%d victory%d/%g levels%d/%d current%d/%d count%d/%d\n",i,run.run.mode,run.wave,actual_victory,(double)actual_victory,gc_int(&run)->rq.n_levels,0,gc_int(&run)->rq.current_level,0,0,0);failed++;}
 }
 double end[1];read_vector("ENDLESS_TERMINAL_COMPLETE ",end,1);if(end[0]!=n||fgetc(input)!=EOF)bad();printf("BattleEnd terminal queue: %d compared, %d exact, %d failures\n",n,n-failed,failed);return failed?1:0;
}
