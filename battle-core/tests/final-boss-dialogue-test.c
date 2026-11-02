/* Actual FaintPhase.start through the ordinary-doFaint boundary. Source delays
 * its callback until dialogue completion; C emits a snapshot before doFaint and
 * the app plays that prompted event before the faint. No downstream-faint parity
 * claim is made by this intercepted native prefix. */
#include "pr/engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
static pr_battle bt;static pr_engine engine;static pr_pokemon mons[2];
static FILE *input;static char line[4096],*cursor,locale[4096];static int cid=-1,checks,called,side;
typedef struct {int hp,form,field,tags,nmoves,moves[4],nparts,parts[1],counts[4];pr_rng r,b;} State;
typedef struct {State state;int ndialogues,nunported,overflow,nlevels,len[PR_MAX_LEVELS];pr_event dialogue;pr_phase phases[2];int nphases;} Snapshot;
static Snapshot observed;
static void bad(void){fprintf(stderr,"dialogue capture invalid case%d: %s",cid,line);exit(2);}
static void record(const char *tag){if(!fgets(line,sizeof line,input))bad();size_t n=strlen(tag);if(strncmp(line,tag,n)||line[n]!=' ')bad();cursor=line+n+1;}
static double number(void){char *end;errno=0;double n=strtod(cursor,&end);if(end==cursor||errno||!isfinite(n))bad();cursor=end;return n;}
static int integer(void){double n=number();if(n!=floor(n)||n< -2147483648.||n>2147483647.)bad();return(int)n;}
static int bounded(int lo,int hi){int n=integer();if(n<lo||n>hi)bad();return n;}
static void end(void){if(strcmp(cursor,"\n"))bad();}
static void eq(double actual,double expected){checks++;if(actual!=expected){fprintf(stderr,"dialogue case%d check%d actual%.17g expected%.17g\n",cid,checks,actual,expected);exit(1);}}
static pr_rng rng(const char *tag){record(tag);pr_rng r={0};r.c=number();r.s0=number();r.s1=number();r.s2=number();end();if(r.c<0||r.c>2091639||r.c!=floor(r.c)||r.s0<0||r.s0>=1||r.s1<0||r.s1>=1||r.s2<0||r.s2>=1)bad();return r;}
static void check_rng(pr_rng a,pr_rng b){eq(a.c,b.c);eq(a.s0,b.s0);eq(a.s1,b.s1);eq(a.s2,b.s2);}
static State read_state(void){State s={0};record("S");s.hp=bounded(0,100);s.form=bounded(0,1);s.field=bounded(0,1);s.tags=bounded(0,1);s.nmoves=bounded(0,4);for(int k=0;k<s.nmoves;k++)s.moves[k]=bounded(0,PR_MOVE_COUNT-1);s.nparts=bounded(0,1);for(int k=0;k<s.nparts;k++)s.parts[k]=bounded(101,101);for(int k=0;k<4;k++)s.counts[k]=bounded(0,1);end();s.r=rng("R");s.b=rng("B");return s;}
static void text_record(const char *tag,char *out,size_t cap){record(tag);size_t n=strcspn(cursor,"\n");if(!n||n%2||n/2>=cap||cursor[n]!='\n'||cursor[n+1])bad();for(size_t k=0;k<n;k++){if(!strchr("0123456789abcdef",cursor[k]))bad();}for(size_t k=0;k<n/2;k++){unsigned v;if(sscanf(cursor+k*2,"%2x",&v)!=1||!v)bad();out[k]=(char)v;}out[n/2]=0;}
static State state(void){State s={0};pr_pokemon *p=&mons[side];s.hp=p->hp;s.form=p->form;s.field=p->on_field;s.tags=p->n_tags;for(int k=0;k<p->n_tags;k++)eq(p->tags[k].type,BATTLER_TAG_TYPE_CHARGING);s.nmoves=p->n_move_queue;for(int k=0;k<s.nmoves&&k<4;k++)s.moves[k]=p->move_queue[k].move;s.nparts=bt.n_participants;if(s.nparts)s.parts[0]=bt.participants[0];s.counts[0]=bt.player_faints;s.counts[1]=bt.enemy_faints;s.counts[2]=bt.n_faint_history[0];s.counts[3]=bt.n_faint_history[1];s.r=bt.rng;s.b=bt.battle_rng;return s;}
static void check_state(State a,State b){eq(a.hp,b.hp);eq(a.form,b.form);eq(a.field,b.field);eq(a.tags,b.tags);eq(a.nmoves,b.nmoves);for(int k=0;k<b.nmoves;k++)eq(a.moves[k],b.moves[k]);eq(a.nparts,b.nparts);for(int k=0;k<b.nparts;k++)eq(a.parts[k],b.parts[k]);for(int k=0;k<4;k++)eq(a.counts[k],b.counts[k]);check_rng(a.r,b.r);check_rng(a.b,b.b);}
static Snapshot snapshot(void){Snapshot s={0};s.state=state();s.nunported=bt.n_unported;s.overflow=engine.overflow;s.nlevels=engine.n_levels;for(int l=0;l<engine.n_levels;l++){s.len[l]=engine.level_len[l];for(int k=0;k<engine.level_len[l];k++){if(s.nphases>=2)exit(1);s.phases[s.nphases++]=engine.pool[engine.levels[l][k]];}}for(int k=0;k<engine.n_events;k++)if(engine.events[k].type==PR_EV_DIALOGUE){s.ndialogues++;s.dialogue=engine.events[k];}return s;}
static void observe(const pr_battle *b,const pr_faint_note *n,void *context){(void)b;(void)n;(void)context;++called;observed=snapshot();}
static const char *text_get(void *ctx,const char *file,int fallback,size_t *len){(void)ctx;if(fallback||strcmp(file,"dialogue-final-boss"))return NULL;*len=strlen(locale);return locale;}
int main(int argc,char **argv){
 if(argc!=3||!(input=fopen(argv[1],"r")))return 2;
 FILE *f=fopen(argv[2],"r");if(!f)return 2;size_t n=fread(locale,1,sizeof locale-1,f);if(!feof(f)){fclose(f);return 2;}fclose(f);locale[n]=0;
 record("FINAL_BOSS_DIALOGUE");if(integer()!=1||integer()!=640)bad();end();pr_closures_init();const int waves[]={199,200,201,5850};int total_dialogues=0;
 for(cid=0;cid<640;cid++){
  record("C");if(integer()!=cid)bad();int mode=bounded(0,4),wave=bounded(1,65535);side=bounded(0,1);int form=bounded(0,1),hp=bounded(0,1),count=bounded(0,4),tag=bounded(0,1);end();
  int basis=cid;if(tag!=basis%2)bad();basis/=2;if(count!=(basis%2?4:0))bad();basis/=2;if(hp!=basis%2)bad();basis/=2;if(form!=basis%2)bad();basis/=2;if(side!=basis%2)bad();basis/=2;if(wave!=waves[basis%4])bad();basis/=4;if(mode!=basis)bad();
  State before=read_state();record("D");int waiting=bounded(0,1);end();char key[128]="",name[128]="",text[256]="";State dialogue_before={0};
  if(waiting){text_record("K",key,sizeof key);text_record("N",name,sizeof name);text_record("T",text,sizeof text);dialogue_before=read_state();}
  record("CALLED");int expected_called=bounded(0,1);end();State after=read_state();record("Q");int levels=bounded(1,PR_MAX_LEVELS),lens[PR_MAX_LEVELS]={0},args[2][3]={{0}},nphase=0;for(int l=0;l<levels;l++){lens[l]=bounded(0,1);for(int k=0;k<lens[l];k++){if(nphase>=2)bad();args[nphase][0]=bounded(0,3);args[nphase][1]=bounded(0,100);args[nphase++][2]=bounded(0,255);}}end();
  memset(&bt,0,sizeof bt);memset(&engine,0,sizeof engine);memset(mons,0,sizeof mons);bt.engine=&engine;bt.game_mode=mode;bt.wave_index=wave;bt.ignoring_effect_source=-1;bt.rng_live=bt.battle_rng_live=true;bt.player_teras_used_known=true;bt.faint_observer=observe;bt.rng=before.r;bt.battle_rng=before.b;engine.n_levels=1;engine.current=-1;
  for(int i=0;i<2;i++){pr_pokemon *p=&mons[i];p->id=101+i;p->species=SPECIES_ID_ETERNATUS;p->level=30;p->hp=100;p->on_field=true;p->is_player=!i;p->status=STATUS_EFFECT_NONE;p->ability=p->base_ability=ABILITY_ID_BALL_FETCH;p->ability_override=p->custom_ability=p->custom_passive=-1;for(int k=0;k<6;k++){p->stats[k]=100;}bt.field[i?2:0]=p;bt.party[i][0]=p;bt.n_party[i]=1;}
  pr_pokemon *p=&mons[side];p->hp=hp;p->form=form;p->n_move_queue=count;for(int k=0;k<count;k++){p->move_queue[k]=(pr_turn_move){.move=MOVE_ID_DIG,.n_targets=1};p->move_queue[k].targets[0]=side?0:2;}if(tag){p->n_tags=1;p->tags[0]=(pr_battler_tag){.type=BATTLER_TAG_TYPE_CHARGING,.turn_count=1,.source_move=MOVE_ID_DIG,.source_id=p->id};}
  check_state(state(),before);called=0;int ph=pr_phase_new(&engine,PH_FAINT,side?2:0);engine.pool[ph].prevent_instant_revive=true;engine.pool[ph].source=-1;engine.current=ph;pr_phase_run(&bt,&engine,ph);
  Snapshot got=called?observed:snapshot();eq(called,expected_called);check_state(got.state,after);eq(got.nunported,0);eq(got.overflow,0);eq(got.ndialogues,waiting);total_dialogues+=got.ndialogues;
  if(waiting){pr_event *ev=&got.dialogue;eq(ev->battler,side?2:0);eq(ev->target,-1);eq(ev->id,SPECIES_ID_ETERNATUS);eq(ev->value,0);eq(ev->msg.n_args,0);if(!ev->msg.key||strcmp(ev->msg.key,key))fprintf(stderr,"key [%s] expected [%s]\n",ev->msg.key,key);eq(ev->msg.key&&strcmp(ev->msg.key,key)==0,1);const char *base_name=NULL;for(int z=0;z<PR_SPECIES_COUNT;z++)if(pr_species_table[z].id==ev->id)base_name=pr_species_table[z].name;eq(base_name&&strcmp(base_name,name)==0,1);char rendered[256];pr_text_source src={text_get,NULL,"en"};pr_msg_format(&ev->msg,&src,rendered,sizeof rendered);if(strcmp(rendered,text))fprintf(stderr,"dialogue text got [%s], expected [%s]\n",rendered,text);eq(strcmp(rendered,text)==0,1);check_state(got.state,dialogue_before);eq(ev->field[2].species,ev->id);eq(ev->field[2].form,dialogue_before.form);eq(ev->field[2].hp,dialogue_before.hp);eq(ev->field[2].visible,dialogue_before.field);eq(ev->field[2].pokemon_id,102);}
  eq(got.nlevels,levels);eq(got.nphases,nphase);for(int l=0;l<levels;l++)eq(got.len[l],lens[l]);for(int k=0;k<nphase;k++){eq(got.phases[k].type,PH_DAMAGE_ANIM);eq(got.phases[k].battler,args[k][0]);eq(got.phases[k].anim_amount,args[k][1]);eq(got.phases[k].anim_result,args[k][2]);}
 }
 record("COMPLETE");if(integer()!=640)bad();end();if(fgetc(input)!=EOF)bad();fclose(input);eq(total_dialogues,8);printf("Final boss dialogue: 640 native state/queue/RNG contexts, %d dialogue callbacks; %d checks passed\n",total_dialogues,checks);return 0;
}
