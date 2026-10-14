/* Actual Transform phase then native dispatch, including NoTransform eligibility. */
#define main gas_replay_main
#include "gas-lifecycle-test.c"
#undef main
int main(int argc,char **argv)
{
 if(argc!=2)return 2;input=fopen(argv[1],"r");if(!input||!getenv("TURNTEST_LOCALES"))return 2;pr_closures_init();double h[3];read_vector("FORMINTER ",h,3);if(h[0]!=1)bad();int n=integer(h[1],512,100000),failures=0;integer(h[2],0,INT32_MAX);
 for(int i=0;i<n;i++){
  static pr_battle bt;static pr_engine e;memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);e.n_levels=1;e.current=-1;bt.engine=&e;bt.game_mode=GAME_MODES_CLASSIC;bt.ignoring_effect_source=-1;bt.last_used_pokeball=-1;pr_seed_from_ascii(&bt.wave_seed,"form-interaction");
  double c[3];read_vector("C ",c,3);if(c[0]!=i)bad();int b=integer(c[1],0,3),sim=integer(c[2],0,1);if(b!=(i%2?2:0))bad();rng_read("R ",&bt.rng);rng_read("B ",&bt.battle_rng);bt.rng_live=bt.battle_rng_live=true;
  pr_pokemon p[2];bool ok=true;
  for(int j=0;j<2;j++){double v[WIDTH+1];read_vector("I ",v,WIDTH+1);if(v[0]!=j*2)bad();load(&p[j],v+1,j*2);bt.field[j*2]=bt.party[j][0]=&p[j];bt.n_party[j]=1;ok&=compare(&p[j],v+1,i,j*2,'I');}
  int phase=pr_phase_new(&e,PH_TRANSFORM,b);e.pool[phase].transform_target=2-b;e.pool[phase].transform_sound=false;e.current=phase;pr_phase_run(&bt,&e,phase);
  for(int j=0;j<2;j++){double v[WIDTH+1];read_vector("A ",v,WIDTH+1);if(v[0]!=j*2)bad();ok&=compare(&p[j],v+1,i,j*2,'A');}
  double ac[4];read_vector("AC ",ac,4);for(int j=0;j<2;j++){
   bool primary=pr_can_apply_ability(&bt,&p[j],p[j].ability),passive=p[j].passive_flag&&pr_can_apply_ability(&bt,&p[j],p[j].passive);
   if(integer(ac[j*2],0,1)!=primary||integer(ac[j*2+1],0,1)!=passive){if(failures<20)fprintf(stderr,"case%d actor%d canApply C%d/%d JS%.0f/%.0f\n",i,j*2,primary,passive,ac[j*2],ac[j*2+1]);ok=false;}
  }
  pr_ab_params pp={.pokemon=&p[b/2],.move=-1,.simulated=sim};pr_apply_ab_attrs(&bt,PR_AC_PostTurnAbAttr,&pp);
  for(int j=0;j<2;j++){double v[WIDTH+1];read_vector("O ",v,WIDTH+1);if(v[0]!=j*2)bad();ok&=compare(&p[j],v+1,i,j*2,'O');}
  pr_rng want;rng_read("RO ",&want);ok&=rng_equal(want,bt.rng);rng_read("BO ",&want);ok&=rng_equal(want,bt.battle_rng);
  double q[4];read_vector("Q ",q,4);int count=integer(q[0],0,128),seen=0;if(integer(q[1],1,PR_MAX_LEVELS)!=e.n_levels||integer(q[2],0,PR_MAX_LEVELS-1)!=e.current_level||integer(q[3],0,1)!=e.deferred_active)ok=false;
  for(int l=0;l<e.n_levels;l++)for(int k=0;k<e.level_len[l];k++){
   pr_phase *ph=&e.pool[e.levels[l][k]];if(seen++>=count){ok=false;continue;}char *row=line_read("P "),name[64],hex[2048],extra;int level,order;if(sscanf(row,"%d %d %63s %2047s %c",&level,&order,name,hex,&extra)!=4||level<0||level>=PR_MAX_LEVELS||order<0||order>=PR_MAX_PHASES)bad();bool known=false;for(int t=0;t<PH_COUNT_;t++)if(pr_phase_name[t]&&!strcmp(pr_phase_name[t],name))known=true;if(!known)bad();if(strcmp(hex,"-")&&(strlen(hex)%2||strspn(hex,"0123456789abcdef")!=strlen(hex)))bad();
   char text[2048]="-";if(ph->type==PH_MESSAGE){char rendered[1024];pr_text_source src={locale_get,NULL,"en"};size_t len=pr_msg_format(&ph->msg,&src,rendered,sizeof rendered);if(len>=sizeof rendered)bad();for(size_t z=0;z<len;z++)snprintf(text+2*z,3,"%02x",(unsigned char)rendered[z]);text[2*len]=0;}
   if(level!=l||order!=k||strcmp(name,pr_phase_name[ph->type])||strcmp(hex,text))ok=false;
  }
  while(seen<count){line_read("P ");seen++;ok=false;}if(strcmp(line_read("E"),"\n"))bad();if(bt.n_unported||e.overflow)ok=false;if(!ok)failures++;
 }
 double f[1];read_vector("FORMINTER_COMPLETE ",f,1);if(f[0]!=n||fgetc(input)!=EOF)bad();printf("Transform/native interactions: %d complete, %d exact, %d failures\n",n,n-failures,failures);return failures?1:0;
}
