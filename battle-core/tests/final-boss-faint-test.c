/* Compare the native FaintPhase.start prefix; ordinary doFaint is intercepted
 * in the producer and observed at the same boundary here, not signed off by it. */
#include "pr/engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
static pr_battle bt;static pr_engine engine;static pr_pokemon mons[2];
static FILE *input;static char line[2048],*cursor;static int cid=-1,checks,called,expected_called,side;
static void bad(void){fprintf(stderr,"faint capture invalid at case%d: %s",cid,line);exit(2);}
static void record(const char *tag){if(!fgets(line,sizeof line,input))bad();size_t n=strlen(tag);if(strncmp(line,tag,n)||line[n]!=' ')bad();cursor=line+n+1;}
static double number(void){char *end;errno=0;double n=strtod(cursor,&end);if(end==cursor||errno||!isfinite(n))bad();cursor=end;return n;}
static int integer(void){double n=number();if(n!=floor(n)||n< -2147483648.||n>2147483647.)bad();return(int)n;}
static int bounded(int low,int high){int n=integer();if(n<low||n>high)bad();return n;}
static void end(void){if(strcmp(cursor,"\n"))bad();}
static void eq(double actual,double expected){checks++;if(actual!=expected){fprintf(stderr,"faint case%d check%d actual%.17g expected%.17g\n",cid,checks,actual,expected);exit(1);}}
static pr_rng rng(const char *tag){record(tag);pr_rng r={0};r.c=number();r.s0=number();r.s1=number();r.s2=number();end();if(r.c<0||r.c>2091639||r.c!=floor(r.c)||r.s0<0||r.s0>=1||r.s1<0||r.s1>=1||r.s2<0||r.s2>=1)bad();return r;}
static void check_rng(pr_rng a,pr_rng b){eq(a.c,b.c);eq(a.s0,b.s0);eq(a.s1,b.s1);eq(a.s2,b.s2);}
static void state(bool seed)
{
 pr_pokemon *p=&mons[side];record("S");eq(p->hp,bounded(0,100));int tags=bounded(0,1);eq(p->n_tags,tags);if(tags)eq(p->tags[0].type,BATTLER_TAG_TYPE_CHARGING);
 int moves=bounded(0,4);eq(p->n_move_queue,moves);for(int i=0;i<moves;i++)eq(p->move_queue[i].move,bounded(0,PR_MOVE_COUNT-1));
 int participants=bounded(0,1);eq(bt.n_participants,participants);for(int i=0;i<participants;i++)eq(bt.participants[i],bounded(101,101));end();
 pr_rng global=rng("R"),battle=rng("B");if(seed){bt.rng=global;bt.battle_rng=battle;}else{check_rng(bt.rng,global);check_rng(bt.battle_rng,battle);}
 record("Q");int levels=bounded(1,PR_MAX_LEVELS);eq(engine.n_levels,levels);
 for(int l=0;l<levels;l++){int n=bounded(0,PR_LEVEL_CAP);eq(engine.level_len[l],n);for(int k=0;k<n;k++){pr_phase *q=&engine.pool[engine.levels[l][k]];eq(q->type,PH_DAMAGE_ANIM);eq(q->battler,bounded(0,3));eq(q->anim_amount,bounded(0,100));eq(q->anim_result,bounded(0,255));}}end();
 eq(bt.n_unported,0);eq(engine.overflow,0);
}
static void observe(const pr_battle *battle,const pr_faint_note *note,void *context)
{
 (void)battle;(void)note;(void)context;++called;eq(expected_called,1);state(false);
}
int main(int argc,char **argv)
{
 if(argc!=2||!(input=fopen(argv[1],"r")))return 2;
 record("FINAL_BOSS_FAINT");if(integer()!=1||integer()!=480)bad();end();pr_closures_init();
 const int waves[]={199,200,201,5850},counts[]={0,1,4};
 for(cid=0;cid<480;cid++){
  record("C");if(integer()!=cid)bad();int mode=bounded(0,4),wave=bounded(1,65535);side=bounded(0,1);int hp=bounded(0,1),count=bounded(0,4),tag=bounded(0,1),classic=bounded(0,1);end();
  int basis=cid;if(tag!=basis%2)bad();basis/=2;if(count!=counts[basis%3])bad();basis/=3;if(hp!=basis%2)bad();basis/=2;if(side!=basis%2)bad();basis/=2;if(wave!=waves[basis%4])bad();basis/=4;if(mode!=basis||classic!=((mode==0||mode==4)&&wave==200))bad();
  memset(&bt,0,sizeof bt);memset(&engine,0,sizeof engine);memset(mons,0,sizeof mons);bt.engine=&engine;bt.game_mode=mode;bt.wave_index=wave;bt.ignoring_effect_source=-1;bt.rng_live=bt.battle_rng_live=true;bt.player_teras_used_known=true;bt.faint_observer=observe;
  engine.n_levels=1;engine.current=-1;
  for(int i=0;i<2;i++){pr_pokemon *p=&mons[i];p->id=101+i;p->species=i?SPECIES_ID_ETERNATUS:SPECIES_ID_MEW;p->level=30;p->hp=100;p->on_field=true;p->is_player=!i;p->status=STATUS_EFFECT_NONE;p->ability=p->base_ability=ABILITY_ID_BALL_FETCH;p->ability_override=p->custom_ability=p->custom_passive=-1;
   for(int k=0;k<6;k++){p->stats[k]=100;}
   bt.field[i?2:0]=p;bt.party[i][0]=p;bt.n_party[i]=1;}
  pr_pokemon *p=&mons[side];p->hp=hp;p->n_move_queue=count;for(int k=0;k<count;k++){p->move_queue[k]=(pr_turn_move){.move=MOVE_ID_DIG,.n_targets=1};p->move_queue[k].targets[0]=side?0:2;}
  if(tag){p->n_tags=1;p->tags[0]=(pr_battler_tag){.type=BATTLER_TAG_TYPE_CHARGING,.turn_count=1,.source_move=MOVE_ID_DIG,.source_id=p->id};}
  state(true);record("CALLED");expected_called=bounded(0,1);end();called=0;
  int ph=pr_phase_new(&engine,PH_FAINT,side?2:0);engine.pool[ph].prevent_instant_revive=true;engine.pool[ph].source=-1;engine.current=ph;pr_phase_run(&bt,&engine,ph);
  eq(called,expected_called);if(!called){state(false);eq(bt.player_faints,0);eq(bt.enemy_faints,0);eq(bt.n_faint_history[0],0);eq(bt.n_faint_history[1],0);}
 }
 record("COMPLETE");if(integer()!=480)bad();end();if(fgetc(input)!=EOF)bad();fclose(input);printf("Final boss faint prefix: 480 native mode/wave/state/queue/RNG contexts; %d checks passed\n",checks);return 0;
}
