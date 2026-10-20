/* Actual biome weather/terrain dispatch: complete field state, frames, queues and RNG. */
#define main frame_replay_main
#include "wide-substitute-frame.inc"
#undef main
static pr_battle bt;static pr_engine e;static pr_pokemon mon[8];
static const char *constructor_get(void *ctx,const char *file,int fallback,size_t *len){
 if(!strcmp(file,"constructor-proof")){const char *s="{\"pokemon\":\"{{pokemonName}}\",\"ability\":\"{{abilityName}}\"}";*len=strlen(s);return s;}
 return locale_get(ctx,file,fallback,len);
}
static void text_hex(const pr_msg *m,char *out){
 char s[1024];pr_text_source src={constructor_get,NULL,"en"};size_t n=pr_msg_format(m,&src,s,sizeof s);if(n>=sizeof s)bad();if(!n){strcpy(out,"-");return;}for(size_t j=0;j<n;j++)sprintf(out+2*j,"%02x",(unsigned char)s[j]);
}
static bool phase_row(const char *key,int level,int order,const pr_phase *p){
 int l,j,who,passive,prompt,anim,target;char name[64],text[2048],extra;
 if(sscanf(line_read(key),"%d %d %63s %d %d %d %2047s %d %d %c",&l,&j,name,&who,&passive,&prompt,text,&anim,&target,&extra)!=9||l<0||l>=PR_MAX_LEVELS||j<0||j>=32||who< -1||who>3||passive< -1||passive>1||prompt< -1||prompt>1||anim<0||anim>UINT16_MAX||target< -1||target>3)bad();
 if(strcmp(name,"DynamicPhaseMarker")&&strcmp(name,"CommonAnimPhase")&&strcmp(name,"MessagePhase")&&strcmp(name,"ShowAbilityPhase")&&strcmp(name,"HideAbilityPhase"))bad();
 if((!strcmp(name,"CommonAnimPhase") && !((anim>=COMMON_ANIM_SUNNY && anim<=COMMON_ANIM_SUNNY+8)||(anim>=COMMON_ANIM_MISTY_TERRAIN && anim<=COMMON_ANIM_MISTY_TERRAIN+3))) || (!strcmp(name,"DynamicPhaseMarker") && anim!=1) || (strcmp(name,"CommonAnimPhase") && strcmp(name,"DynamicPhaseMarker") && anim!=0))bad();
 if(!p)return false;
 int cw=-1,cp=-1,ca=-1,cm=0,ct=-1;char got[2048]="-";const char *actual=p->type==PH_MARKER?"DynamicPhaseMarker":pr_phase_name[p->type];
 switch(p->type){
 case PH_MARKER:cm=p->marker_of==PH_COMMON_ANIM?1:-1;break;
 case PH_MESSAGE:text_hex(&p->msg,got);cp=p->message_prompt;break;
 case PH_COMMON_ANIM:cw=p->battler;cm=p->anim;ct=p->anim_target;break;
 case PH_SHOW_ABILITY:{cw=p->battler;ca=p->anim_result;char pn[1024],an[1024];if(cw<0||cw>=4||p->anim<0){strcpy(pn,"-");strcpy(an,"-");}else{pr_msg a=pr_msg1("constructorProof:pokemon",pr_arg_pokemon(&bt,"pokemonName",bt.field[cw]));text_hex(&a,pn);a=pr_msg1("constructorProof:ability",pr_arg_ability("abilityName",p->anim));text_hex(&a,an);}snprintf(got,sizeof got,"%s.%s",pn,an);break;}
 case PH_HIDE_ABILITY:break;
 default:return false;
 }
 bool ok=l==level&&j==order&&!strcmp(actual,name)&&who==cw&&passive==ca&&prompt==cp&&!strcmp(got,text)&&anim==cm&&target==ct;
 if(!ok)fprintf(stderr,"queue native %s b%d a%d t%d / C %s b%d a%d t%d text %s/%s\n",name,who,anim,target,actual,cw,cm,ct,text,got);return ok;
}
static bool queues(void){
 double v[4];read_vector("Q ",v,4);int n=integer(v[0],0,32),levels=integer(v[1],1,PR_MAX_LEVELS),current=integer(v[2],0,levels-1),defer=integer(v[3],0,1),seen=0;bool ok=e.n_levels==levels&&e.current_level==current&&e.deferred_active==defer;
 for(int l=0;l<e.n_levels;l++)for(int j=0;j<e.level_len[l];j++){if(seen++>=n){ok=false;continue;}ok&=phase_row("P ",l,j,&e.pool[e.levels[l][j]]);}while(seen<n){phase_row("P ",0,seen,NULL);seen++;ok=false;}ok&=seen==n;
 read_vector("DQ ",v,1);n=integer(v[0],0,8);seen=0;for(int j=0;j<e.n_dyn[PH_COMMON_ANIM];j++){if(seen++>=n){ok=false;continue;}ok&=phase_row("D ",0,j,&e.pool[e.dyn[PH_COMMON_ANIM][j]]);}while(seen<n){phase_row("D ",0,seen,NULL);seen++;ok=false;}ok&=seen==n;
 for(int t=0;t<PH_COUNT_;t++)if(t!=PH_COMMON_ANIM&&e.n_dyn[t])ok=false;return ok;
}
static bool environment(const char *key,bool initial){
 double v[6];read_vector(key,v,6);int w=integer(v[0],0,WEATHER_TYPE_STRONG_WINDS),wt=integer(v[1],0,11),wm=integer(v[2],0,11),t=integer(v[3],0,TERRAIN_PSYCHIC),tt=integer(v[4],0,9),tm=integer(v[5],0,9);
 if((!w&&(wt||wm))||(!t&&(tt||tm))||wt>wm||tt>tm||(pr_weather_immutable(w)&&(wt||wm)))bad();
 if(initial){bt.weather=w;bt.weather_turns_left=wt;bt.weather_max_duration=wm;bt.terrain=t;bt.terrain_turns_left=tt;bt.terrain_max_duration=tm;}
 return bt.weather==w&&bt.weather_turns_left==wt&&bt.weather_max_duration==wm&&bt.terrain==t&&bt.terrain_turns_left==tt&&bt.terrain_max_duration==tm;
}
static bool battle_data(const char *key,bool initial){
 bool ok=true;for(int k=0;k<8;k++){
  double v[6];read_vector(key,v,6);if(v[0]!=k)bad();int eaten=integer(v[1],0,1),endured=integer(v[2],0,1),n=integer(v[3],0,2),b0=integer(v[4],0,1),b1=integer(v[5],0,1);if((!n&&(b0||b1))||(n==1&&b1))bad();
  pr_pokemon *p=&mon[k];if(initial){p->has_eaten_berry=eaten;p->wave_endured=endured;p->n_battle_berries=n;p->battle_berries[0]=b0;p->battle_berries[1]=b1;}
  ok&=p->has_eaten_berry==eaten&&p->wave_endured==endured&&p->n_battle_berries==n&&(n?p->battle_berries[0]:0)==b0&&(n>1?p->battle_berries[1]:0)==b1;
 }return ok;
}
int main(int argc,char **argv){
 if(argc!=2||!getenv("TURNTEST_LOCALES")||!(input=fopen(argv[1],"r")))return 2;pr_closures_init();double h[3];read_vector("BIOME_CALLBACK ",h,3);int version=integer(h[0],1,2);int n=integer(h[1],version==1?26880:6720,100000),failed=0;integer(h[2],0,INT32_MAX);
 for(int i=0;i<n;i++){
  memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);bt.engine=&e;e.n_levels=1;e.current=-1;bt.wave_index=1;bt.game_mode=GAME_MODES_CLASSIC;bt.turn=1;bt.double_battle=true;bt.ignoring_effect_source=-1;bt.last_used_pokeball=-1;
  double c[4];read_vector("C ",c,4);if(c[0]!=i)bad();int u=integer(c[1],0,7),sim=integer(c[2],0,1),gain=integer(c[3],version==2?2:0,version==2?2:1);if(version==2&&sim)bad();bool ok=environment("ENV ",true);
  char seed[512],extra;if(sscanf(line_read("W "),"%511s %c",seed,&extra)!=1)bad();pr_seed_from_ascii(&bt.wave_seed,seed);rng_read("R ",&bt.rng);rng_read("B ",&bt.battle_rng);bt.rng_live=bt.battle_rng_live=true;
  for(int k=0;k<8;k++){double v[WIDTH+1];read_vector("I ",v,WIDTH+1);if(v[0]!=k)bad();load(&mon[k],v+1,k<4?k:k<6?0:2);ok&=compare(&mon[k],v+1,i,k,'I');}
  if(version==2)ok&=battle_data("ID ",true);
  int slots[2][4]={{0,1,4,5},{2,3,6,7}};for(int side=0;side<2;side++){bt.n_party[side]=4;for(int j=0;j<4;j++)bt.party[side][j]=&mon[slots[side][j]];}for(int k=0;k<4;k++)bt.field[k]=&mon[k];
  pr_queue_msg_deferred(&bt,&e,pr_msg0("anchor"));if(version==1){int current=pr_phase_new(&e,PH_MOVE,0);e.current=current;e.pool[current].move.id=MOVE_ID_SPLASH;}
  pr_ab_params pp={.pokemon=&mon[u],.move=-1,.simulated=sim,.on_gain=gain,.only=gain?1:0};if(version==2)pr_new_biome_encounter_callbacks(&bt);else pr_apply_ab_attrs(&bt,gain?PR_AC_PostSummonAbAttr:PR_AC_PostBiomeChangeAbAttr,&pp);
  for(int k=0;k<8;k++){double v[WIDTH+1];read_vector("O ",v,WIDTH+1);if(v[0]!=k)bad();ok&=compare(&mon[k],v+1,i,k,'O');}if(version==2)ok&=battle_data("OD ",false);ok&=environment("ENVO ",false);ok&=queues();
  pr_rng out;rng_read("RO ",&out);ok&=rng_equal(out,bt.rng);rng_read("BO ",&out);ok&=rng_equal(out,bt.battle_rng);if(strcmp(line_read("E"),"\n"))bad();if(bt.n_unported||bt.n_deferred||e.overflow||e.n_events)ok=false;
  if(!ok){if(failed<12)fprintf(stderr,"Biome callback case%d user%d sim%d gain%d unsupported%d %s failed\n",i,u,sim,gain,bt.n_unported,bt.n_unported?pr_unported_name(bt.unported[0]):"none");failed++;}
 }
 double end[1];read_vector("BIOME_CALLBACK_COMPLETE ",end,1);if(end[0]!=n||fgetc(input)!=EOF)bad();printf("Biome callbacks: %d compared, %d exact, %d failures\n",n,n-failed,failed);return failed?1:0;
}
