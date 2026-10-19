/* Actual Tera phase/end and scene triggers; strict native state/queue transport. */
#define main frame_replay_main
#include "wide-substitute-frame.inc"
#undef main
static pr_battle bt;static pr_engine e;static pr_pokemon mon[8];
static int checks[4],nchecks;
static void achievement(const pr_battle *b,int check,void *ctx){if(nchecks>=4)bad();checks[nchecks++]=check;}
static int actor(const pr_pokemon *p){for(int k=0;k<8;k++)if(p==&mon[k])return k;return -1;}
static int actor_id(uint32_t id){for(int k=0;k<8;k++)if(mon[k].id==id)return k;return -1;}
static void text_hex(const pr_msg *m,char *out){char s[1024];pr_text_source src={locale_get,NULL,"en"};size_t n=pr_msg_format(m,&src,s,sizeof s);if(n>=sizeof s)bad();if(!n){strcpy(out,"-");return;}for(size_t j=0;j<n;j++)sprintf(out+2*j,"%02x",(unsigned char)s[j]);}
static bool queues(void)
{
 double v[4];read_vector("Q ",v,4);int n=integer(v[0],0,32),levels=integer(v[1],1,PR_MAX_LEVELS),defer=integer(v[2],0,1),current=integer(v[3],0,levels-1);bool ok=e.n_levels==levels&&e.deferred_active==defer&&e.current_level==current;int count=0;
 for(int l=0;l<e.n_levels;l++)count+=e.level_len[l];
 if(count!=n)ok=false;
 int last_a=-1,last_b=-1;
 for(int j=0;j<n;j++){
  int a,b,who,target,prompt;char name[64],text[2048],extra;
  if(sscanf(line_read("P "),"%d %d %63s %d %d %d %2047s %c",&a,&b,name,&who,&target,&prompt,text,&extra)!=7||a<0||a>=PR_MAX_LEVELS||b<0||b>=32||who< -1||who>7||target< -1||target>7||prompt< -1||prompt>1)bad();
  if(strcmp(name,"QuietFormChangePhase")&&strcmp(name,"MessagePhase"))bad();
  if(a<last_a||(a==last_a&&b!=last_b+1)||(a>last_a&&b!=0))bad();last_a=a;last_b=b;
  if(a>=e.n_levels||b>=e.level_len[a]){ok=false;continue;}
  pr_phase *p=&e.pool[e.levels[a][b]];
  char got[2048]="-";int cw=-1,ct=-1,cp=-1;
  if(p->type==PH_QUIET_FORM_CHANGE){cw=actor(p->pokemon);ct=p->move_id;}
  else if(p->type==PH_MESSAGE){text_hex(&p->msg,got);cp=p->message_prompt;}
  else ok=false;
  ok&=!strcmp(name,pr_phase_name[p->type])&&who==cw&&target==ct&&prompt==cp&&!strcmp(text,got);
 }
 for(int t=0;t<PH_COUNT_;t++)if(e.n_dyn[t])ok=false;return ok;
}
static int selftest(void)
{
 memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);memset(mon,0,sizeof mon);
 bt.engine=&e;e.n_levels=1;e.current=-1;
 mon[0].species=SPECIES_ID_BULBASAUR;mon[0].is_player=true;
 pr_tera_end(&bt,&mon[0]);
 if(!mon[0].terastallized||bt.n_unported!=1)return 1;
 memset(&bt,0,sizeof bt);bt.engine=&e;
 int ph=pr_phase_new(&e,PH_TERA,-1);if(ph<0)return 1;
 pr_phase_run(&bt,&e,ph);
 if(bt.n_unported!=1||bt.player_teras_used!=0)return 1;
 puts("Tera missing counter/constructor contexts explicitly refused");return 0;
}
int main(int argc,char **argv)
{
 if(argc==2&&!strcmp(argv[1],"--selftest"))return selftest();
 if(argc!=2||!getenv("TURNTEST_LOCALES")||!(input=fopen(argv[1],"r")))return 2;pr_closures_init();double h[3];read_vector("TERA ",h,3);if(h[0]!=1)bad();int n=integer(h[1],16384,100000),failed=0;integer(h[2],0,INT32_MAX);
 for(int i=0;i<n;i++){
  memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);bt.engine=&e;e.n_levels=1;e.current=-1;bt.game_mode=GAME_MODES_ENDLESS;bt.wave_index=201;bt.turn=1;bt.double_battle=true;bt.ignoring_effect_source=-1;bt.last_used_pokeball=-1;bt.achievement_check=achievement;nchecks=0;
  double c[3];read_vector("C ",c,3);if(c[0]!=i)bad();int mode=integer(c[1],0,3),u=integer(c[2],0,7);char seed[512],extra;if(sscanf(line_read("W "),"%511s %c",seed,&extra)!=1)bad();pr_seed_from_ascii(&bt.wave_seed,seed);rng_read("R ",&bt.rng);rng_read("B ",&bt.battle_rng);bt.rng_live=bt.battle_rng_live=true;
  double a[1];read_vector("A ",a,1);if(!isfinite(a[0])||a[0]<0||a[0]>9007199254740991.0||floor(a[0])!=a[0])bad();bt.player_teras_used_known=true;bt.player_teras_used=a[0];bool ok=true;
  for(int k=0;k<8;k++){double v[WIDTH+1];read_vector("I ",v,WIDTH+1);if(v[0]!=k)bad();load(&mon[k],v+1,k<4?k:k<6?0:2);ok&=compare(&mon[k],v+1,i,k,'I');}
  int slots[2][4]={{0,1,4,5},{2,3,6,7}};for(int side=0;side<2;side++){bt.n_party[side]=4;for(int k=0;k<4;k++)bt.party[side][k]=&mon[slots[side][k]];}for(int k=0;k<4;k++)bt.field[k]=&mon[k];
  double iq[3];read_vector("IQ ",iq,3);e.n_levels=integer(iq[0],1,PR_MAX_LEVELS);e.deferred_active=integer(iq[1],0,1);e.current_level=integer(iq[2],0,e.n_levels-1);
  if(mode==0){int ph=pr_phase_new(&e,PH_TERA,pr_battler_index(&bt,&mon[u]));e.pool[ph].pokemon=&mon[u];e.current=ph;pr_phase_run(&bt,&e,ph);}
  else if(mode==1)pr_tera_end(&bt,&mon[u]);else pr_form_change(&bt,&mon[u],mode==2?PR_FCT_SpeciesFormChangeTeraTrigger:PR_FCT_SpeciesFormChangeLapseTeraTrigger);
  read_vector("AO ",a,1);if(!isfinite(a[0])||a[0]<0||a[0]>9007199254740992.0||floor(a[0])!=a[0])bad();ok&=bt.player_teras_used==a[0];
  for(int k=0;k<8;k++){double v[WIDTH+1];read_vector("O ",v,WIDTH+1);if(v[0]!=k)bad();ok&=compare(&mon[k],v+1,i,k,'O');}ok&=queues();
  double an[1];read_vector("ANIM ",an,1);int na=integer(an[0],0,1),seen=0;for(int k=0;k<e.n_events;k++){
   const pr_event *ev=&e.events[k];if(ev->type!=PR_EV_COMMON_ANIM){ok=false;continue;}if(seen++>=na){ok=false;continue;}double v[4];read_vector("AN ",v,4);integer(v[0],0,UINT16_MAX);integer(v[1],0,7);integer(v[2],0,7);if(v[3]!=0)bad();ok&=ev->id==v[0]&&ev->actor_ids_known&&actor_id(ev->actor_id)==v[1]&&actor_id(ev->target_id)==v[2];
  }if(seen!=na)bad();
  read_vector("ACH ",an,1);na=integer(an[0],0,2);ok&=nchecks==na;for(int k=0;k<na;k++){char name[64];if(sscanf(line_read("AC "),"%63s %c",name,&extra)!=1)bad();int kind=!strcmp(name,"TERASTALLIZE")?PR_ACHV_CHECK_TERASTALLIZE:!strcmp(name,"STELLAR_TERASTALLIZE")?PR_ACHV_CHECK_STELLAR_TERASTALLIZE:-1;if(kind<0)bad();ok&=k<nchecks&&checks[k]==kind;}
  pr_rng rng;rng_read("RO ",&rng);ok&=rng_equal(rng,bt.rng);rng_read("BO ",&rng);ok&=rng_equal(rng,bt.battle_rng);if(strcmp(line_read("E"),"\n"))bad();if(bt.n_unported||bt.n_deferred||e.overflow||e.events_overflow)ok=false;
  if(!ok){if(failed<12)fprintf(stderr,"Tera case%d mode%d actor%d unsupported%d failed\n",i,mode,u,bt.n_unported);failed++;}
 }
 double f[1];read_vector("TERA_COMPLETE ",f,1);if(f[0]!=n||fgetc(input)!=EOF)bad();printf("Tera: %d complete, %d exact, %d failures\n",n,n-failed,failed);return failed?1:0;
}
