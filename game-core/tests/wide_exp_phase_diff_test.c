/* Native owning phase replay: wide award, exact text and pre-award display state. */
#include "../src/internal.h"
#include "../../game/source/run_exp.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
static FILE *input;static char line[4096],*cursor;static int cid=-1;static gc_run run;
static void bad(const char *why){fprintf(stderr,"wide EXP case%d: %s (%s)\n",cid,why,line);exit(2);}
static void mismatch(const char *why){fprintf(stderr,"wide EXP case%d: %s\n",cid,why);exit(1);}
static void record(const char *kind){if(!fgets(line,sizeof line,input))bad("missing record");size_t n=strlen(kind);if(strncmp(line,kind,n)||(line[n]!=' '&&line[n]!='\n'))bad("wrong record");cursor=line+n;}
static double num(void){char *end;errno=0;double v=strtod(cursor,&end);if(end==cursor||errno||!isfinite(v))bad("bad number");cursor=end;return v;}
static int integer(void){double v=num();if(v<INT32_MIN||v>INT32_MAX||v!=trunc(v))bad("bad integer");return(int)v;}
static void end(void){while(*cursor==' '||*cursor=='\r')cursor++;if(strcmp(cursor,"\n"))bad("extra fields");}
static pr_rng rng(const char *kind){record(kind);pr_rng r={0};r.c=num();r.s0=num();r.s1=num();r.s2=num();end();if(r.c<0||r.c>2091639||r.c!=trunc(r.c)||r.s0<0||r.s0>=1||r.s1<0||r.s1>=1||r.s2<0||r.s2>=1)bad("RNG domain");return r;}
static int eq(pr_rng a,pr_rng b){return a.c==b.c&&a.s0==b.s0&&a.s1==b.s1&&a.s2==b.s2;}
static const char *locale(void *unused,const char *space,int unused_lang,size_t *length){static char data[8][400000];static char names[8][96];static size_t sizes[8];static int n;(void)unused;(void)unused_lang;for(int i=0;i<n;i++)if(!strcmp(names[i],space)){*length=sizes[i];return data[i];}if(n==8)bad("locale cache");char path[1024];snprintf(path,sizeof path,"%s/%s.json",getenv("TURNTEST_LOCALES"),space);FILE *f=fopen(path,"rb");if(!f)bad("locale missing");sizes[n]=fread(data[n],1,sizeof data[n]-1,f);if(fgetc(f)!=EOF)bad("locale size");fclose(f);strcpy(names[n],space);*length=sizes[n];return data[n++];}
static void hex(const char *s,char *out){if(!*s){strcpy(out,"-");return;}for(size_t i=0;i<strlen(s);i++)sprintf(out+2*i,"%02x",(unsigned char)s[i]);}
static const char *test_locale(void *ctx,const char *space,int fallback,size_t *size){
 (void)ctx;(void)space;if(fallback)return NULL;const char *json="{\"amount\":\"{{value}}\"}";*size=strlen(json);return json;
}
static int selftest(void){
 const int64_t values[]={0,-1,2147483648LL,4294967296LL,9007199254740991LL,INT64_MAX,INT64_MIN};
 const char *expected[]={"0","-1","2147483648","4294967296","9007199254740991","9223372036854775807","-9223372036854775808"};
 pr_text_source src={test_locale,NULL,"en"};int checks=0;
 for(size_t i=0;i<sizeof values/sizeof *values;i++){char text[64];pr_msg m=pr_msg1("test:amount",pr_arg_int64("value",values[i]));pr_msg_format(&m,&src,text,sizeof text);if(strcmp(text,expected[i]))mismatch("64-bit integer text");checks++;}
 const double invalid[]={-1,INFINITY,NAN,9007199254740992.0};
 for(size_t i=0;i<sizeof invalid/sizeof *invalid;i++){
  gc_run_new(&run,GAME_MODES_CLASSIC,"wide-exp-guards",777);gc_starter starter={.species=SPECIES_ID_BULBASAUR,.tera_type=-1,.moves={MOVE_ID_SPLASH}};memset(starter.ivs,15,6);const char *why;if(!gc_run_set_starters(&run,&starter,1,&why))mismatch("guard setup");gc_event ev;while(gc_run_next_event(&run,&ev)){}gc_rq_clear(&run);run.n_items=0;run.run.unported=0;
  gc_mon *m=&run.party[run.party_order[0]];double before=m->exp;int level=m->b.level;gc_rphase p=gc_rp(RP_EXP,run.party_order[0],1,0);p.d=invalid[i];gc_phase_start(&run,&p);
  if(!(run.run.unported&PR_REWARD_UNPORTED_LEVEL)||m->exp!=before||m->b.level!=level||gc_run_next_event(&run,&ev)||gc_rq_next(&gc_int(&run)->rq,&p))mismatch("unsupported award must remain explicit and unmodified");checks++;
 }
 printf("Wide EXP transport guards: %d checks passed\n",checks);return 0;
}
int main(int argc,char **argv){if(argc==2&&!strcmp(argv[1],"--selftest"))return selftest();if(argc!=2||!getenv("TURNTEST_LOCALES")||!(input=fopen(argv[1],"r")))return 2;
 record("WIDE_EXP_PHASE");int version=integer();if(version<1||version>3)bad("version");int n=integer();integer();end();if(n<8192||n>100000)bad("minimum/count");int ops[2]={0},growth[6]={0},wide=0,capped=0,changed=0,fractional=0;
 for(cid=0;cid<n;cid++){
  record("C");if(integer()!=cid)bad("case sequence");int op=integer(),mode=integer(),wave=integer();double id=num();int species=integer(),rate=integer(),level=integer();double exp=num(),amount=num();int cap=integer(),stack[3],order[3],mask=0;for(int j=0;j<3;j++)stack[j]=integer();for(int j=0;j<3;j++){order[j]=integer();if(order[j]<0||order[j]>2||(mask&(1<<order[j])))bad("order");mask|=1<<order[j];}end();if(op!=cid%2||mode<0||mode>2||wave<1||id<0||id>UINT32_MAX||id!=trunc(id)||rate<0||rate>5||level<1||exp<0||amount<0||stack[0]<0||stack[0]>99||stack[1]<0||stack[1]>30||stack[2]<0||stack[2]>10)bad("input domain");
  pr_rng r=rng("R"),b=rng("B");gc_run_new(&run,mode,"wide-exp-phase",777);run.wave=wave;const char *why;gc_starter starter={.species=SPECIES_ID_BULBASAUR,.tera_type=-1,.moves={MOVE_ID_SPLASH}};memset(starter.ivs,15,6);if(!gc_run_set_starters(&run,&starter,1,&why))bad("starter");gc_event ev;while(gc_run_next_event(&run,&ev)){}gc_rq_clear(&run);run.n_items=0;run.wave=wave;
  int storage=run.party_order[0];starter.species=species;gc_mon *m=&run.party[storage];gc_mon_new_player(&run,m,&starter,level);m->b.id=id;m->b.base_ability=m->b.ability=ABILITY_ID_BALL_FETCH;m->b.on_field=true;m->exp=exp;
  if(!gc_species(species)||gc_species(species)->growth_rate!=rate)bad("growth");if(pr_run_get_max_exp_level(&run.run,wave,false)!=cap)mismatch("cap");
  const char *keys[]={"EXP_CHARM","SUPER_EXP_CHARM","GOLDEN_EXP_CHARM"};for(int j=0;j<3;j++)if(stack[order[j]]){gc_item *it=&run.items[run.n_items++];memset(it,0,sizeof *it);it->key=keys[order[j]];it->stack=stack[order[j]];}
  run.run.rng=run.bt.rng=r;run.bt.battle_rng=b;run.bt.battle_rng_live=true;run.run.unported=0;run.bt.n_unported=run.bt.n_deferred=0;memset(run.stubs,0,sizeof run.stubs);
  gc_rphase p=gc_rp(RP_EXP,storage,!op,0);p.d=amount;if(!gc_phase_start(&run,&p))mismatch("phase waits");
  record("A");int wanted_level=integer();double wanted_exp=num(),award=num();end();if(award<0||award>9007199254740991.0||award!=floor(award))bad("award domain");if(m->b.level!=wanted_level||m->exp!=wanted_exp){fprintf(stderr,"got level%d exp%.17g award%.17g expected level%d exp%.17g award%.17g\n",m->b.level,m->exp,gc_apply_exp_boosters(&run,amount),wanted_level,wanted_exp,award);mismatch("native level/EXP");}
  char text[1024]="",encoded[2048];int messages=0,events=0;while(gc_run_next_event(&run,&ev)){if(ev.kind==GC_EV_MESSAGE){pr_text_source src={locale,NULL,"en"};if(++messages>1||pr_msg_format(&ev.msg,&src,text,sizeof text)>=sizeof text)mismatch("message count/size");}else if(ev.kind==GC_EV_EXP){if(++events>1||ev.a!=storage||ev.c!=level||ev.exp_amount!=award||ev.exp_before!=exp||ev.b!=(award<=INT32_MAX?(int32_t)award:0))mismatch("event amount/prior state");
    gc_event pending[3]={ev,ev,ev};pending[0].a=storage+1;pending[2].exp_before=123;pending[2].c=1;
    gc_mon before=rs_exp_before_pending(m,storage,pending,3);if(before.exp!=exp||before.b.level!=level)mismatch("app pending display state");
    before=rs_exp_before_pending(m,storage,pending,1);if(before.exp!=m->exp||before.b.level!=m->b.level)mismatch("app no pending EXP");
   }else mismatch("extra event");}
  if(events!=1||messages!=!op||run.ev_overflow)mismatch("event transport");record("T");while(*cursor==' ')cursor++;cursor[strcspn(cursor,"\n")]=0;if(strcmp(cursor,"-")&&(strlen(cursor)%2||strspn(cursor,"0123456789abcdef")!=strlen(cursor)))bad("text hex");hex(text,encoded);if(strcmp(cursor,encoded))mismatch("English award text");
  record("L");int nq=integer(),old=integer(),next=integer();end();if(nq<0||nq>1)bad("queue domain");gc_rphase q;if(nq){if(!gc_rq_next(&gc_int(&run)->rq,&q)||q.type!=RP_LEVEL_UP||q.a!=storage||q.b!=old||q.c!=next)mismatch("native level request");}else if(old||next)bad("empty level request");if(gc_rq_next(&gc_int(&run)->rq,&q))mismatch("extra level request");
  pr_rng ro=rng("RO"),bo=rng("BO");if(!eq(run.run.rng,ro)||!eq(run.bt.rng,ro)||!eq(run.bt.battle_rng,bo))mismatch("RNG");record("E");end();if(run.run.unported||run.bt.n_unported||run.bt.n_deferred)mismatch("unsupported");for(int j=0;j<GC_STUB_COUNT_;j++)if(run.stubs[j])mismatch("stub");ops[op]++;growth[rate]++;fractional+=!stack[0]&&!stack[1]&&!stack[2]&&amount!=floor(amount);wide+=award>INT32_MAX;capped+=wanted_exp-exp<award;changed+=wanted_level!=level;
 }
 record("WIDE_EXP_PHASE_COMPLETE");if(integer()!=n)bad("footer");end();if(fgetc(input)!=EOF)bad("tail");if(version>=3&&fractional<128)bad("fractional controls");if(ops[0]<4096||ops[1]<4096||wide<1000||!capped||!changed)bad("coverage");for(int j=0;j<6;j++)if(!growth[j])bad("growth coverage");printf("Wide EXP phases: %d ALL EXACT; ops %d/%d wide %d capped %d changed %d\n",n,ops[0],ops[1],wide,capped,changed);return 0;
}
