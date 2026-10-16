/* Strict replay of actual native addExp/addModifier results at pinned 8555c08.
 * Level requests are compared before execution, in dynamic insertion order. */
#include "../src/internal.h"
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static FILE *input;static char line[4096],*cursor;static int cid=-1;static gc_run run;static gc_system sys;
static void fail(const char *why){fprintf(stderr,"wide progression case %d: %s (%s)\n",cid,why,line);exit(1);}
static void record(const char *kind){if(!fgets(line,sizeof line,input))fail("missing record");size_t n=strlen(kind);if(strncmp(line,kind,n)||(line[n]!=' '&&line[n]!='\n'))fail("wrong record");cursor=line+n;}
static double number(void){char *end;errno=0;double v=strtod(cursor,&end);if(end==cursor||errno||!isfinite(v))fail("bad number");cursor=end;return v;}
static int integer(void){double v=number();if(v<INT_MIN||v>INT_MAX||v!=trunc(v))fail("bad integer");return(int)v;}
static void end(void){while(*cursor==' '||*cursor=='\r')cursor++;if(strcmp(cursor,"\n"))fail("extra fields");}
static pr_rng read_rng(const char *kind){record(kind);pr_rng r={0};r.c=number();r.s0=number();r.s1=number();r.s2=number();end();if(r.c<0||r.c>2091639||r.c!=trunc(r.c)||r.s0<0||r.s0>=1||r.s1<0||r.s1>=1||r.s2<0||r.s2>=1)fail("invalid RNG");return r;}
static bool same_rng(pr_rng a,pr_rng b){return a.c==b.c&&a.s0==b.s0&&a.s1==b.s1&&a.s2==b.s2;}
typedef struct{uint32_t id;int species,growth,level,friendship,root,root_f,root_c,hp,stats[6];double exp;} actor;
static void setup(actor *a,int n,int rot,int mode,int wave,int jars,pr_rng global,pr_rng battle){
 memset(&run,0,sizeof run);gc_system_init_defaults(&sys);gc_run_new(&run,mode,"wide-progression",777);
 gc_starter team[6]={0};int k=0;for(int i=0;i<gc_starter_cost_count&&k<n;i++)if(gc_starter_costs[i].cost==1){team[k].species=gc_starter_costs[i].id;team[k].tera_type=-1;memset(team[k].ivs,15,6);k++;}
 const char *why;if(k!=n||!gc_run_set_starters(&run,team,n,&why))fail("party setup");run.sys=&sys;run.wave=wave;
 for(int i=0;i<n;i++){run.party_order[i]=(i+rot)%n;gc_starter s={.species=a[i].species,.tera_type=-1};memset(s.ivs,15,6);gc_mon *m=&run.party[run.party_order[i]];gc_mon_new_player(&run,m,&s,a[i].level);m->b.id=a[i].id;m->b.ability=ABILITY_ID_BALL_FETCH;m->b.passive=false;m->exp=a[i].exp;m->b.friendship=a[i].friendship;m->b.hp=a[i].hp;memcpy(m->b.stats,a[i].stats,sizeof a[i].stats);if(!gc_species(a[i].species)||gc_species(a[i].species)->growth_rate!=a[i].growth)fail("growth mismatch");sys.starters[a[i].root].friendship=a[i].root_f;sys.starters[a[i].root].candy_count=a[i].root_c;}
 run.n_items=0;if(jars)gc_item_add(&run,"CANDY_JAR",-1,0,jars);
 gc_rq_reset(&gc_int(&run)->rq);memset(run.stubs,0,sizeof run.stubs);run.run.unported=0;memset(run.bt.unported,0,sizeof run.bt.unported);run.bt.n_unported=0;run.bt.n_deferred=0;run.run.rng=global;run.bt.rng=global;run.bt.battle_rng=battle;run.bt.battle_rng_live=true;
}
int main(int argc,char **argv){if(argc!=2||!(input=fopen(argv[1],"r"))){fputs("usage: wide_progression_diff_test capture\n",stderr);return 2;}
 record("WIDE_PROGRESSION");if(integer()!=1)fail("version");int count=integer();(void)integer();end();if(count<12288)fail("coverage minimum");int ops[3]={0},growth[6]={0};long changed=0,wide=0,requests=0;
 for(cid=0;cid<count;cid++){
  record("C");if(integer()!=cid)fail("case sequence");int op=integer(),mode=integer(),wave=integer(),n=integer(),target=integer(),jars=integer(),cap=integer();double delta=number();end();
  if(op!=cid%3||mode<0||mode>2||wave<1||n<1||n>6||target<0||target>=n||jars<0||jars>99||delta<0)fail("case domain");
  pr_rng r=read_rng("R"),b=read_rng("B");actor a[6]={0};
  for(int i=0;i<n;i++){record("P");if(integer()!=i)fail("actor sequence");double id=number();if(id<0||id>UINT32_MAX||id!=trunc(id))fail("actor ID");a[i].id=id;a[i].species=integer();a[i].growth=integer();a[i].level=integer();a[i].exp=number();a[i].friendship=integer();a[i].root=integer();a[i].root_f=integer();a[i].root_c=integer();a[i].hp=integer();for(int j=0;j<6;j++)a[i].stats[j]=integer();end();if(a[i].growth<0||a[i].growth>5||a[i].level<1||a[i].root<1||a[i].root>GC_MAX_SPECIES_ID||a[i].friendship<0||a[i].friendship>255)fail("actor domain");growth[a[i].growth]++;wide+=a[i].level>65535;}
  setup(a,n,cid%n,mode,wave,jars,r,b);if(pr_run_get_max_exp_level(&run.run,wave,false)!=cap)fail("native cap mismatch");
  bool ok=true;if(op==0)gc_mon_add_exp(&run,&run.party[run.party_order[target]],delta);else{gc_offer o={0};o.key=op==1?"RARE_CANDY":"RARER_CANDY";o.arg=-1;o.target=gc_reward_target(o.key);const char *why;ok=gc_port_apply_reward(&run,&o,op==1?run.party_order[target]:-1,-1,&why);}
  record("RESULT");int filter=integer(),applied=integer();end();if((filter!=0&&filter!=1)||(applied!=0&&applied!=1)||ok!=(filter&&applied))fail("application mismatch");
  for(int i=0;i<n;i++){record("A");if(integer()!=i)fail("result actor sequence");int level=integer();double exp=number();int friendship=integer(),hp=integer(),maxhp=integer(),rootf=integer(),rootc=integer();end();gc_mon *m=&run.party[run.party_order[i]];gc_starter_entry *s=&sys.starters[a[i].root];if(m->b.id!=a[i].id||m->b.level!=level||m->exp!=exp||m->b.friendship!=friendship||m->b.hp!=hp||m->b.stats[0]!=maxhp||s->friendship!=rootf||s->candy_count!=rootc){fprintf(stderr,"got member%d level%d exp%.17g friendship%d hp%d/%d starter%d/%d\n",i,m->b.level,m->exp,m->b.friendship,m->b.hp,m->b.stats[0],s->friendship,s->candy_count);fail("actor result mismatch");}changed+=level!=a[i].level;}
  record("LEVELS");int nl=integer();end();if(nl<0||nl>n)fail("level request count");gc_rphase q={0};
  for(int i=0;i<nl;i++){record("L");int role=integer(),old=integer(),level=integer();end();if(role<0||role>=n||!gc_rq_next(&gc_int(&run)->rq,&q)||q.type!=RP_LEVEL_UP||q.a!=run.party_order[role]||q.b!=old||q.c!=level){fprintf(stderr,"got queue type%d actor%d old%d level%d want role%d storage%d old%d level%d\n",q.type,q.a,q.b,q.c,role,run.party_order[role],old,level);fail("level request mismatch");}requests++;}
  if(gc_rq_next(&gc_int(&run)->rq,&q))fail("extra level request");
  pr_rng ro=read_rng("RO"),bo=read_rng("BO");if(!same_rng(run.run.rng,ro)||!same_rng(run.bt.rng,ro)||!same_rng(run.bt.battle_rng,bo))fail("RNG mismatch");record("E");end();if(run.run.unported)fail("run unported runtime");if(run.bt.n_unported||run.bt.n_deferred)fail("battle unsupported runtime");for(int i=0;i<16;i++)if(run.stubs[i])fail("stub runtime");ops[op]++;
 }
 record("WIDE_PROGRESSION_COMPLETE");if(integer()!=count)fail("footer count");end();if(fgets(line,sizeof line,input))fail("trailing records");fclose(input);for(int i=0;i<3;i++)if(ops[i]<4096)fail("operation coverage");for(int i=0;i<6;i++)if(!growth[i])fail("growth coverage");if(!changed||!wide||!requests)fail("effect coverage");printf("Wide progression: %d ALL EXACT; ops=%d/%d/%d changed=%ld wide=%ld queued=%ld (requests before execution)\n",count,ops[0],ops[1],ops[2],changed,wide,requests);return 0;
}
