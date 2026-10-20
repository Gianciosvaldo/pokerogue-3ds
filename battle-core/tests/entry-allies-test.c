/* Actual native PostSummon dispatcher and gain caller; strict full frames/queues. */
#define main frame_replay_main
#include "wide-substitute-frame.inc"
#undef main
static pr_battle bt;
static pr_engine e;
static pr_pokemon mon[8];
static const char *constructor_text_get(void *ctx,const char *file,int fallback,size_t *len){
 if(!strcmp(file,"constructor-proof")){const char *s="{\"pokemon\":\"{{pokemonName}}\",\"ability\":\"{{abilityName}}\"}";*len=strlen(s);return s;}
 return locale_get(ctx,file,fallback,len);
}
static void text_hex(const pr_msg *m,char *out){
 char s[1024];pr_text_source src={constructor_text_get,NULL,"en"};size_t n=pr_msg_format(m,&src,s,sizeof s);if(n>=sizeof s)bad();
 if(!n){strcpy(out,"-");return;}for(size_t j=0;j<n;j++)sprintf(out+2*j,"%02x",(unsigned char)s[j]);
}
static bool queues(void){
 double v[4];read_vector("Q ",v,4);int n=integer(v[0],0,32),levels=integer(v[1],1,PR_MAX_LEVELS),current=integer(v[2],0,levels-1),defer=integer(v[3],0,1),count=0;
 bool ok=e.n_levels==levels&&e.current_level==current&&e.deferred_active==defer;
 for(int l=0;l<e.n_levels;l++)count+=e.level_len[l];ok&=count==n;
 int last_l=-1,last_j=-1;
 for(int k=0;k<n;k++){
  int l,j,who,passive,prompt,flags;double hp;char name[64],text[2048],extra;
  if(sscanf(line_read("P "),"%d %d %63s %d %d %d %2047s %lf %d %c",&l,&j,name,&who,&passive,&prompt,text,&hp,&flags,&extra)!=9||!isfinite(hp)||hp<0||flags<0||flags>127||l<0||l>=PR_MAX_LEVELS||j<0||j>=32||who< -1||who>3||passive< -1||passive>1||prompt< -1||prompt>1)bad();
  if(strcmp(name,"PokemonHealPhase")&&strcmp(name,"MessagePhase")&&strcmp(name,"ShowAbilityPhase")&&strcmp(name,"HideAbilityPhase"))bad();
  if(l<last_l||(l==last_l&&j!=last_j+1)||(l>last_l&&j!=0))bad();last_l=l;last_j=j;
  if(l>=e.n_levels||j>=e.level_len[l]){ok=false;continue;}
  const pr_phase *p=&e.pool[e.levels[l][j]];int cw=-1,cp=-1,ca=-1,cf=0;double ch=0;char got[2048]="-";
  if(p->type==PH_MESSAGE){text_hex(&p->msg,got);cp=p->message_prompt;}
  else if(p->type==PH_POKEMON_HEAL){cw=p->battler;ch=p->heal_hp;cf=p->heal_flags;text_hex(&p->msg,got);}
  else if(p->type==PH_SHOW_ABILITY){cw=p->battler;ca=p->anim_result;char pn[1024],an[1024];
   if(cw<0||cw>=4||p->anim<0){strcpy(pn,"-");strcpy(an,"-");}
   else{pr_msg a=pr_msg1("constructorProof:pokemon",pr_arg_pokemon(&bt,"pokemonName",bt.field[cw]));text_hex(&a,pn);a=pr_msg1("constructorProof:ability",pr_arg_ability("abilityName",p->anim));text_hex(&a,an);}snprintf(got,sizeof got,"%s.%s",pn,an);
  }else if(p->type!=PH_HIDE_ABILITY)ok=false;
  bool same=!strcmp(name,pr_phase_name[p->type])&&who==cw&&passive==ca&&prompt==cp&&!strcmp(text,got)&&hp==ch&&flags==cf;
  if(!same)fprintf(stderr,"queue native %s %d %d %d %s %.17g %d C %s %d %d %d %s %.17g %d\n",name,who,passive,prompt,text,hp,flags,pr_phase_name[p->type],cw,ca,cp,got,ch,cf);ok&=same;
 }
 for(int t=0;t<PH_COUNT_;t++)if(e.n_dyn[t])ok=false;return ok;
}
int main(int argc,char **argv){
 if(argc!=2||!getenv("TURNTEST_LOCALES")||!(input=fopen(argv[1],"r")))return 2;
 pr_closures_init();double h[3];read_vector("ENTRY_ALLIES ",h,3);int version=integer(h[0],1,2);int n=integer(h[1],version==2?49152:32768,100000),failed=0;integer(h[2],0,INT32_MAX);
 for(int i=0;i<n;i++){
  memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);bt.engine=&e;e.n_levels=1;e.current=-1;bt.wave_index=1;bt.game_mode=GAME_MODES_CLASSIC;bt.turn=1;bt.ignoring_effect_source=-1;bt.last_used_pokeball=-1;
  double c[5];read_vector("C ",c,5);if(c[0]!=i)bad();int u=integer(c[1],0,7);bool simulated=integer(c[2],0,1),gain=integer(c[3],0,1);bt.double_battle=!integer(c[4],0,1);
  char seed[512],extra;if(sscanf(line_read("W "),"%511s %c",seed,&extra)!=1)bad();pr_seed_from_ascii(&bt.wave_seed,seed);rng_read("R ",&bt.rng);rng_read("B ",&bt.battle_rng);bt.rng_live=bt.battle_rng_live=true;
  bool ok=true;for(int k=0;k<8;k++){double v[WIDTH+1];read_vector("I ",v,WIDTH+1);if(v[0]!=k)bad();load(&mon[k],v+1,k<4?k:k<6?0:2);ok&=compare(&mon[k],v+1,i,k,'I');}
  int slots[2][4]={{0,1,4,5},{2,3,6,7}};for(int side=0;side<2;side++){bt.n_party[side]=4;for(int k=0;k<4;k++)bt.party[side][k]=&mon[slots[side][k]];bt.field[side*2]=&mon[side*2];if(bt.double_battle)bt.field[side*2+1]=&mon[side*2+1];}
  pr_queue_msg_deferred(&bt,&e,pr_msg0("anchor"));int current=pr_phase_new(&e,PH_MOVE,0);e.current=current;e.pool[current].move.id=MOVE_ID_SPLASH;
  pr_ab_params pp={.pokemon=&mon[u],.move=-1,.simulated=simulated,.on_gain=gain,.only=gain?1:0};pr_apply_ab_attrs(&bt,PR_AC_PostSummonAbAttr,&pp);
  for(int k=0;k<8;k++){double v[WIDTH+1];read_vector("O ",v,WIDTH+1);if(v[0]!=k)bad();ok&=compare(&mon[k],v+1,i,k,'O');}ok&=queues();
  pr_rng out;rng_read("RO ",&out);ok&=rng_equal(out,bt.rng);rng_read("BO ",&out);ok&=rng_equal(out,bt.battle_rng);if(strcmp(line_read("E"),"\n"))bad();if(bt.n_unported||bt.n_deferred||e.overflow||e.n_events)ok=false;
  if(!ok){if(failed<12)fprintf(stderr,"Entry allies case%d actor%d sim%d gain%d unsupported%d %s failed\n",i,u,simulated,gain,bt.n_unported,bt.n_unported?pr_unported_name(bt.unported[0]):"none");failed++;}
 }
 double end[1];read_vector("ENTRY_ALLIES_COMPLETE ",end,1);if(end[0]!=n||fgetc(input)!=EOF)bad();printf("Entry allies: %d compared, %d exact, %d failures\n",n,n-failed,failed);return failed?1:0;
}
