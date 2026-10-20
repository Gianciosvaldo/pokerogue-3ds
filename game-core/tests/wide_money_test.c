/* Real pickup and MoneyRewardPhase owners, wide Number values and frozen text/prompt. */
#include "../src/internal.h"
#define main money_frame_main
#include "wide-substitute-frame.inc"
#undef main
static gc_run run;
int main(int argc,char **argv){
 if(argc!=2||!getenv("TURNTEST_LOCALES")||!(input=fopen(argv[1],"r")))return 2;
 double h[3];read_vector("WIDE_MONEY ",h,3);integer(h[0],1,1);int n=integer(h[1],23400,23400),failures=0;integer(h[2],0,INT32_MAX);
 for(int i=0;i<n;i++){
  memset(&run,0,sizeof run);double c[10];read_vector("C ",c,10);if(c[0]!=i)bad();run.wave=integer(c[1],1,5850);int reward=integer(c[2],0,1),hh=integer(c[3],0,1),nc=integer(c[4],0,2);if(run.wave!=i%5850+1||reward!=(i>=11700))bad();
  for(int j=0;j<2;j++){int stack=integer(c[5+j],0,5);if(j<nc)run.items[run.n_items++]=(gc_item){.key="AMULET_COIN",.stack=stack,.arg=-1};else if(stack)bad();}
  if(c[7]!=.2&&c[7]!=1&&c[7]!=2.5&&c[7]!=5)bad();if(c[8]<0||c[9]<0||c[9]>9007199254740991.)bad();run.bt.money_scattered=c[8];run.money=c[9];
  if(hh){run.bt.n_arena_tags=1;run.bt.arena_tags[0]=(pr_arena_tag){.type=ARENA_TAG_TYPE_HAPPY_HOUR,.side=ARENA_TAG_SIDE_BOTH,.turn_count=5,.max_duration=5};}
  rng_read("R ",&run.bt.rng);rng_read("B ",&run.bt.battle_rng);run.run.rng=run.bt.rng;
  if(reward){gc_rphase phase=gc_rp(RP_MONEY_REWARD,0,0,0);phase.d=c[7];gc_phase_start(&run,&phase);}else gc_pick_up_scattered_money(&run);
  double wallet,scattered,credit;int prompt;char expected[2048],extra;
  if(sscanf(line_read("O "),"%lf %lf %lf %d %2047s %c",&wallet,&scattered,&credit,&prompt,expected,&extra)!=5||!isfinite(wallet)||wallet<0||wallet>9007199254740991.||!isfinite(scattered)||scattered<0||isnan(credit)||credit<0||prompt!=1)bad();
  bool ok=run.money==wallet&&run.bt.money_scattered==scattered&&run.ev_count==2;
  gc_event event;ok&=gc_run_next_event(&run,&event)&&event.kind==GC_EV_MONEY&&event.money_change==credit&&event.money_after==wallet;
  ok&=event.a==(credit<=INT32_MAX?(int32_t)credit:0)&&event.b==(wallet<=INT32_MAX?(int32_t)wallet:0);
  ok&=gc_run_next_event(&run,&event)&&event.kind==GC_EV_MESSAGE&&event.prompt==prompt;
  char text[1024],hex[2048];pr_text_source src={locale_get,NULL,"en"};size_t length=pr_msg_format(&event.msg,&src,text,sizeof text);if(length>=sizeof text)bad();for(size_t j=0;j<length;j++)sprintf(hex+2*j,"%02x",(unsigned char)text[j]);hex[2*length]=0;ok&=!strcmp(hex,expected);
  ok&=event.msg.n_args==1&&event.msg.args[0].kind==PR_ARG_MONEY_EN_US&&!run.ev_count;
  pr_rng want;rng_read("RO ",&want);ok&=rng_equal(want,run.bt.rng)&&rng_equal(want,run.run.rng);rng_read("BO ",&want);ok&=rng_equal(want,run.bt.battle_rng);
  if(strcmp(line_read("E"),"\n"))bad();if(!ok){if(failures<8)fprintf(stderr,"wide money case%d got wallet%.17g scattered%.17g text%s / want%s\n",i,run.money,run.bt.money_scattered,hex,expected);failures++;}
 }
 double end[1];read_vector("WIDE_MONEY_COMPLETE ",end,1);if(end[0]!=n||fgetc(input)!=EOF)bad();printf("Wide pickup/reward: %d compared, %d exact, %d failures\n",n,n-failures,failures);return failures?1:0;
}
