/* Native MoveLearned scene dispatch and actual queued phase execution. */
#define main frame_replay_main
#include "wide-substitute-frame.inc"
#undef main
static pr_battle bt;static pr_engine e;static pr_pokemon mon[8];
static int diagnostic_count;
static bool frame_compare(const pr_pokemon *p,const double *want,int c,int b,char stage){
 bool ok=compare(p,want,c,b,stage);
 if(!ok&&c>=200&&diagnostic_count++<8){double got[WIDTH];capture(p,got);for(int j=0;j<WIDTH;j++)if(got[j]!=want[j])fprintf(stderr,"case%d %c%d field%d C%.17g native%.17g\n",c,stage,b,j,got[j],want[j]);}
 return ok;
}
static int actor(const pr_pokemon *p){for(int k=0;k<8;k++)if(p==&mon[k])return k;return -1;}
/* Render individual constructor names with the production argument formatter.
 * This test-only template namespace never replaces pinned locale messages. */
static const char *constructor_text_get(void *ctx,const char *file,int fallback,size_t *len){
 if(!strcmp(file,"constructor-proof")){const char *json="{\"pokemon\":\"{{pokemonName}}\",\"ability\":\"{{abilityName}}\"}";*len=strlen(json);return json;}
 return locale_get(ctx,file,fallback,len);
}
static void text_hex(const pr_msg *m,char *out){char s[1024];pr_text_source src={constructor_text_get,NULL,"en"};size_t n=pr_msg_format(m,&src,s,sizeof s);if(n>=sizeof s)bad();if(!n){strcpy(out,"-");return;}for(size_t j=0;j<n;j++)sprintf(out+2*j,"%02x",(unsigned char)s[j]);}
static bool queues(void){
 double v[4];read_vector("Q ",v,4);int n=integer(v[0],0,32),levels=integer(v[1],1,PR_MAX_LEVELS),current=integer(v[2],0,levels-1),defer=integer(v[3],0,1),count=0;bool ok=e.n_levels==levels&&e.current_level==current&&e.deferred_active==defer;
 for(int l=0;l<e.n_levels;l++)count+=e.level_len[l];ok&=count==n;
 int last_l=-1,last_j=-1;
 for(int k=0;k<n;k++){
  int l,j,who,target,prompt;char name[64],text[2048],extra;
  if(sscanf(line_read("P "),"%d %d %63s %d %d %2047s %d %c",&l,&j,name,&who,&target,text,&prompt,&extra)!=7||l<0||l>=PR_MAX_LEVELS||j<0||j>=32||who< -1||who>7||target< -1||target>7)bad();
  if(strcmp(name,"FormChangePhase")&&strcmp(name,"QuietFormChangePhase")&&strcmp(name,"MessagePhase")&&strcmp(name,"EndEvolutionPhase")&&strcmp(name,"ShowAbilityPhase")&&strcmp(name,"HideAbilityPhase"))bad();
  if(l<last_l||(l==last_l&&j!=last_j+1)||(l>last_l&&j!=0))bad();last_l=l;last_j=j;
  if(l>=e.n_levels||j>=e.level_len[l]){ok=false;continue;}
  const pr_phase *p=&e.pool[e.levels[l][j]];const char *actual=pr_phase_name[p->type];int cw=-1,ct=-1;char got[2048]="-";
  if(!strcmp(actual,"FormChangePhase")||p->type==PH_QUIET_FORM_CHANGE){cw=actor(p->pokemon);ct=p->move_id;}
  int cp=-1;
  if(p->type==PH_MESSAGE){text_hex(&p->msg,got);cp=p->message_prompt;}
  if(p->type==PH_SHOW_ABILITY){cw=p->battler;ct=p->anim_result;char pn[1024],an[1024];
   if(p->battler<0||p->battler>=4||p->anim<0){strcpy(pn,"-");strcpy(an,"-");}
   else{pr_msg a=pr_msg1("constructorProof:pokemon",pr_arg_pokemon(&bt,"pokemonName",bt.field[p->battler]));text_hex(&a,pn);a=pr_msg1("constructorProof:ability",pr_arg_ability("abilityName",p->anim));text_hex(&a,an);}
   snprintf(got,sizeof got,"%s.%s",pn,an);
  }
  bool same=!strcmp(name,actual)&&who==cw&&target==ct&&!strcmp(text,got)&&prompt==cp;
  if(!same&&diagnostic_count++<8)fprintf(stderr,"queue native %s %d %d %s %d C %s %d %d %s %d\n",name,who,target,text,prompt,actual,cw,ct,got,cp);
  ok&=same;
 }
 for(int t=0;t<PH_COUNT_;t++)if(e.n_dyn[t])ok=false;return ok;
}
int main(int argc,char **argv){
 if(argc!=2||!getenv("TURNTEST_LOCALES")||!(input=fopen(argv[1],"r")))return 2;pr_closures_init();double h[3];
#ifdef MIMICRY_PROOF
 read_vector("MIMICRY ",h,3);if(h[0]!=1)bad();int n=integer(h[1],16384,100000),failed=0;
#else
 read_vector("KELDEO ",h,3);int version=integer(h[0],4,5);int n=integer(h[1],version==5?33792:32768,100000),failed=0;
#endif
integer(h[2],0,INT32_MAX);
 for(int i=0;i<n;i++){
  memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);bt.engine=&e;e.n_levels=1;e.current=-1;bt.wave_index=201;bt.turn=1;bt.double_battle=true;bt.ignoring_effect_source=-1;bt.last_used_pokeball=-1;
  
#ifdef MIMICRY_PROOF
 double c[7];read_vector("C ",c,7);bt.terrain=integer(c[6],0,TERRAIN_PSYCHIC);
#else
 double c[9]={0};c[8]=-1;read_vector("C ",c,version==5?9:6);
 if(version==5){int direct=integer(c[6],0,1),modal=integer(c[7],0,1),target=integer(c[8],-1,1);if((!direct&&(modal||target!=-1))||(direct&&target<0))bad();}
#endif
if(c[0]!=i)bad();int u=integer(c[1],0,7),mode=integer(c[2],0,1);bt.game_mode=integer(c[3],0,GAME_MODES_CHALLENGE);bool delayed=integer(c[4],0,1),ok=true;bt.challenge_flags=(uint32_t)integer(c[5],0,INT32_MAX);bt.challenge_flags_known=true;char seed[512],extra;
  if(sscanf(line_read("W "),"%511s %c",seed,&extra)!=1)bad();pr_seed_from_ascii(&bt.wave_seed,seed);rng_read("R ",&bt.rng);rng_read("B ",&bt.battle_rng);bt.rng_live=bt.battle_rng_live=true;
  for(int k=0;k<8;k++){double v[WIDTH+1];read_vector("I ",v,WIDTH+1);if(v[0]!=k)bad();load(&mon[k],v+1,k<4?k:k<6?0:2);ok&=frame_compare(&mon[k],v+1,i,k,'I');}
  double tags[8],weights[8];read_vector("IT ",tags,8);read_vector("IW ",weights,8);
  for(int k=0;k<8;k++){int count=integer(tags[k],0,3);if(count){pr_battler_tag *t=&mon[k].tags[mon[k].n_tags++];*t=(pr_battler_tag){.type=BATTLER_TAG_TYPE_AUTOTOMIZED,.turn_count=1,.source_move=MOVE_ID_AUTOTOMIZE,.source_id=-1,.autotomize_count=count};}ok&=pr_get_weight(&bt,&mon[k])==weights[k];}
  int slots[2][4]={{0,1,4,5},{2,3,6,7}};for(int side=0;side<2;side++){bt.n_party[side]=4;for(int k=0;k<4;k++)bt.party[side][k]=&mon[slots[side][k]];}for(int k=0;k<4;k++)bt.field[k]=&mon[k];
  pr_queue_msg_deferred(&bt,&e,pr_msg0("anchor"));int current=pr_phase_new(&e,PH_MOVE,0);e.current=current;e.pool[current].move.id=MOVE_ID_SPLASH;
#ifdef MIMICRY_PROOF
  pr_ab_params params={.pokemon=&mon[u],.move=-1,.simulated=mode!=0};pr_apply_ab_attrs(&bt,PR_AC_TerrainEventTypeChangeAbAttr,&params);
  double v[1];read_vector("RESULT ",v,1);if(v[0]!=0)bad();int form=-1;ok&=queues();
#else
  if(version==5&&integer(c[6],0,1)){
   if(!mon[u].is_player)bad();int modal=integer(c[7],0,1),target=integer(c[8],0,1);
   int ph=pr_phase_new(&e,PH_FORM_CHANGE,pr_battler_index(&bt,&mon[u]));if(ph<0)bad();e.pool[ph].pokemon=&mon[u];e.pool[ph].move_id=target;e.pool[ph].sw_modal=modal;
   if(delayed)pr_phase_push(&bt,&e,ph);else pr_phase_unshift(&bt,&e,ph);
  }else if(delayed)pr_form_change_delayed(&bt,&mon[u],PR_FCT_SpeciesFormChangeMoveLearnedTrigger);else pr_form_change(&bt,&mon[u],PR_FCT_SpeciesFormChangeMoveLearnedTrigger);
  double v[1];read_vector("RESULT ",v,1);integer(v[0],0,1);int form=-1;for(int l=0;l<e.n_levels;l++)for(int j=0;j<e.level_len[l];j++){int p=e.levels[l][j];if(e.pool[p].type!=PH_MESSAGE)form=p;}ok&=(form>=0)==(bool)v[0];ok&=queues();
#endif
  if(mode==1&&form>=0){e.current=form;pr_phase_run(&bt,&e,form);}
  for(int k=0;k<8;k++){double frame[WIDTH+1];read_vector("O ",frame,WIDTH+1);if(frame[0]!=k)bad();ok&=frame_compare(&mon[k],frame+1,i,k,'O');}
  read_vector("OT ",tags,8);read_vector("OW ",weights,8);for(int k=0;k<8;k++){const pr_battler_tag *t=pr_get_battler_tag(&mon[k],BATTLER_TAG_TYPE_AUTOTOMIZED);ok&=(t?t->autotomize_count:0)==tags[k];ok&=pr_get_weight(&bt,&mon[k])==weights[k];}ok&=queues();
  read_vector("T ",v,1);int nt=integer(v[0],0,4),seen=0;for(int k=0;k<e.n_events;k++){if(e.events[k].type!=PR_EV_TEXT){ok=false;continue;}if(seen++>=nt){ok=false;continue;}char expected[2048],actual[2048];if(sscanf(line_read("TEXT "),"%2047s %c",expected,&extra)!=1)bad();text_hex(&e.events[k].msg,actual);ok&=!strcmp(expected,actual);}while(seen<nt){line_read("TEXT ");seen++;ok=false;}ok&=seen==nt;
  pr_rng out;rng_read("RO ",&out);ok&=rng_equal(out,bt.rng);rng_read("BO ",&out);ok&=rng_equal(out,bt.battle_rng);if(strcmp(line_read("E"),"\n"))bad();if(bt.n_unported||bt.n_deferred||e.overflow||e.events_overflow)ok=false;
  if(!ok){if(failed<10)fprintf(stderr,"Keldeo case%d actor%d mode%d unsupported%d %s failed\n",i,u,mode,bt.n_unported,bt.n_unported?pr_unported_name(bt.unported[0]):"none");failed++;}
 }
 double end[1];
#ifdef MIMICRY_PROOF
 read_vector("MIMICRY_COMPLETE ",end,1);
#else
 read_vector("KELDEO_COMPLETE ",end,1);
#endif
if(end[0]!=n||fgetc(input)!=EOF)bad();printf("Keldeo: %d complete, %d exact, %d failures\n",n,n-failed,failed);return failed?1:0;
}
