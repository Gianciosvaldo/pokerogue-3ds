/* Exact registered ability dispatch, inventory, phase payload and repeated-use proof. */
#define main gas_replay_main
#include "wide-substitute-frame.inc"
#undef main
static pr_battle bt;static pr_engine e;static pr_pokemon mon[4];
static void hex_text(const pr_msg *m,char *out)
{
 char s[1024];pr_text_source src={locale_get,NULL,"en"};size_t n=pr_msg_format(m,&src,s,sizeof s);if(n>=sizeof s)bad();if(!n){strcpy(out,"-");return;}for(size_t j=0;j<n;j++)sprintf(out+2*j,"%02x",(unsigned char)s[j]);
}
static bool compare_queue(void)
{
 double q[4];read_vector("Q ",q,4);int count=integer(q[0],0,64),levels=integer(q[1],1,PR_MAX_LEVELS),deferred=integer(q[2],0,1),current=integer(q[3],0,PR_MAX_LEVELS-1);if(current>=levels)bad();bool ok=e.n_levels==levels&&e.deferred_active==deferred&&e.current_level==current;int n=0;
 for(int l=0;l<e.n_levels;l++)for(int j=0;j<e.level_len[l];j++){
  if(n++>=count){ok=false;continue;}
  char name[64],text[2048],extra;int lv,order,b,a,id;
  if(sscanf(line_read("P "),"%d %d %63s %d %d %d %2047s %c",&lv,&order,name,&b,&a,&id,text,&extra)!=7||lv<0||lv>=levels||order<0||order>=64||b<-1||b>3)bad();
  if(strcmp(name,"ShowAbilityPhase")&&strcmp(name,"HideAbilityPhase")&&strcmp(name,"MessagePhase"))bad();
  if(!strcmp(name,"ShowAbilityPhase")){if(a<0||a>1||id<0||id>=PR_ABILITY_COUNT||strcmp(text,"-"))bad();}else if(a!=-1||id!=-1)bad();
  if(strcmp(name,"ShowAbilityPhase")&&b!=-1)bad();
  if(!strcmp(name,"HideAbilityPhase")&&strcmp(text,"-"))bad();
  if(strcmp(text,"-")&&(strlen(text)%2||strspn(text,"0123456789abcdef")!=strlen(text)))bad();
  pr_phase *p=&e.pool[e.levels[l][j]];char actual[2048]="-";int ca=-1,ci=-1;if(p->type==PH_MESSAGE)hex_text(&p->msg,actual);if(p->type==PH_SHOW_ABILITY){ca=p->anim_result;ci=p->anim;}
  ok&=lv==l&&order==j&&!strcmp(name,pr_phase_name[p->type])&&b==p->battler&&a==ca&&id==ci&&!strcmp(text,actual);
 }
 while(n<count){line_read("P ");n++;ok=false;}
 for(int t=0;t<PH_COUNT_;t++)ok&=e.n_dyn[t]==0;
 return ok;
}
static bool inventory(const char *prefix,bool set)
{
 double inv[9];read_vector(prefix,inv,9);int last=integer(inv[0],-1,5);bool ok=true;
 if(set){bt.last_used_pokeball=last;bt.pokeball_counts_known=true;}else ok&=last==bt.last_used_pokeball;
 for(int k=0;k<8;k++){if(!isfinite(inv[k+1])||inv[k+1]<0||inv[k+1]>9007199254740992.0||inv[k+1]!=floor(inv[k+1]))bad();if(set)bt.pokeball_counts[k]=inv[k+1];else ok&=bt.pokeball_counts[k]==inv[k+1];}return ok;
}
int main(int argc,char **argv)
{
 if(argc!=2||!getenv("TURNTEST_LOCALES")||!(input=fopen(argv[1],"r")))return 2;pr_closures_init();double h[3];read_vector("BALLFETCH ",h,3);if(h[0]!=1)bad();int n=integer(h[1],16384,100000),failed=0,positive=0;integer(h[2],0,INT32_MAX);
 for(int i=0;i<n;i++){
  memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);bt.engine=&e;e.n_levels=1;e.current=-1;bt.game_mode=GAME_MODES_ENDLESS;bt.wave_index=201;bt.turn=1;bt.double_battle=true;bt.ignoring_effect_source=-1;
  double c[5];read_vector("C ",c,5);if(c[0]!=i)bad();int user=integer(c[1],0,3),only=integer(c[2],0,2),sim=integer(c[3],0,1),buddy=integer(c[4],0,1);bool ok=true;char seed[512],extra;if(sscanf(line_read("W "),"%511s %c",seed,&extra)!=1)bad();pr_seed_from_ascii(&bt.wave_seed,seed);rng_read("R ",&bt.rng);rng_read("B ",&bt.battle_rng);bt.rng_live=bt.battle_rng_live=true;inventory("INV ",true);
  for(int k=0;k<4;k++){double v[WIDTH+1];read_vector("I ",v,WIDTH+1);if(v[0]!=k)bad();load(&mon[k],v+1,k);bt.field[k]=bt.party[k/2][k%2]=&mon[k];ok&=compare(&mon[k],v+1,i,k,'I');}bt.n_party[0]=bt.n_party[1]=2;
  e.current=pr_phase_new(&e,PH_TURN_END,-1);
  for(int step=0;step<2;step++){
   int before=bt.last_used_pokeball;pr_ab_params pp={.pokemon=&mon[step&&buddy?(user^1):user],.move=-1,.simulated=sim,.only=only};pr_apply_ab_attrs(&bt,PR_AC_FetchBallAbAttr,&pp);positive+=before>=0&&bt.last_used_pokeball<0;
   double st[1];read_vector("STEP ",st,1);if(st[0]!=step)bad();ok&=inventory("OUTINV ",false);
   for(int k=0;k<4;k++){double v[WIDTH+1];read_vector("O ",v,WIDTH+1);if(v[0]!=k)bad();ok&=compare(&mon[k],v+1,i,k,'O');}ok&=compare_queue();pr_rng want;rng_read("RO ",&want);ok&=rng_equal(want,bt.rng);rng_read("BO ",&want);ok&=rng_equal(want,bt.battle_rng);
  }
  if(strcmp(line_read("E"),"\n"))bad();if(bt.n_unported||bt.n_deferred||e.overflow)ok=false;if(!ok){if(failed<20)fprintf(stderr,"Ball Fetch case%d user%d only%d sim%d unported%d failed\n",i,user,only,sim,bt.n_unported);failed++;}
 }
 double foot[1];read_vector("BALLFETCH_COMPLETE ",foot,1);if(foot[0]!=n||fgetc(input)!=EOF)bad();if(!failed&&positive<100)bad();printf("Ball Fetch: %d complete, %d exact, %d failures, positive=%d\n",n,n-failed,failed,positive);return failed?1:0;
}
