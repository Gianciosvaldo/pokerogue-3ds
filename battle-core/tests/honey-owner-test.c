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
#ifndef HONEY_OWNER_COMMON_ONLY
int main(int argc,char **argv){
 if(argc!=2||!getenv("TURNTEST_LOCALES")||!(input=fopen(argv[1],"r")))return 2;pr_closures_init();double h[3];read_vector("HONEY_OWNER ",h,3);integer(h[0],1,1);int n=integer(h[1],1024,1024),failed=0;integer(h[2],0,INT32_MAX);
 for(int i=0;i<n;i++){
  memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);bt.engine=&e;e.n_levels=1;e.current=-1;bt.wave_index=1;bt.game_mode=GAME_MODES_CLASSIC;bt.turn=1;bt.double_battle=true;bt.ignoring_effect_source=-1;bt.last_used_pokeball=-1;
  double c[5];read_vector("C ",c,5);if(c[0]!=i)bad();int victory=integer(c[1],0,1),wave=integer(c[2],1,5849),reverse=integer(c[4],0,1);if(c[3]!=1234)bad();bt.game_mode=GAME_MODES_ENDLESS;bt.wave_index=wave;bt.money_scattered=c[3];bool ok=true;
  char seed[512],extra;if(sscanf(line_read("W "),"%511s %c",seed,&extra)!=1)bad();pr_seed_from_ascii(&bt.wave_seed,seed);rng_read("R ",&bt.rng);rng_read("B ",&bt.battle_rng);bt.rng_live=bt.battle_rng_live=true;
  for(int k=0;k<8;k++){double v[WIDTH+1];read_vector("I ",v,WIDTH+1);if(v[0]!=k)bad();load(&mon[k],v+1,k<4?k:k<6?0:2);ok&=compare(&mon[k],v+1,i,k,'I');}
  int slots[2][4]={{0,1,4,5},{2,3,6,7}};for(int side=0;side<2;side++){bt.n_party[side]=4;for(int j=0;j<4;j++)bt.party[side][j]=&mon[slots[side][j]];}for(int k=0;k<4;k++)bt.field[k]=&mon[k];
  if(reverse){bt.party[0][0]=&mon[5];bt.party[0][1]=&mon[4];bt.party[0][2]=&mon[1];bt.party[0][3]=&mon[0];bt.field[0]=&mon[5];bt.field[1]=&mon[4];}
  pr_prepare_post_battle_money(&bt,&e,victory);
  double vout[1];read_vector("V ",vout,1);ok&=bt.money_scattered==vout[0];
  for(int k=0;k<8;k++){double v[WIDTH+1];read_vector("O ",v,WIDTH+1);if(v[0]!=k)bad();ok&=compare(&mon[k],v+1,i,k,'O');}ok&=queues();
  pr_rng out;rng_read("RO ",&out);ok&=rng_equal(out,bt.rng);rng_read("BO ",&out);ok&=rng_equal(out,bt.battle_rng);if(strcmp(line_read("E"),"\n"))bad();if(bt.n_unported||bt.n_deferred||e.overflow||e.n_events)ok=false;
  if(!ok){if(failed<12)fprintf(stderr,"Honey owner case%d user%d sim%d owner%d unsupported%d %s failed\n",i,0,0,1,bt.n_unported,bt.n_unported?pr_unported_name(bt.unported[0]):"none");failed++;}
 }
 double end[1];read_vector("HONEY_OWNER_COMPLETE ",end,1);if(end[0]!=n||fgetc(input)!=EOF)bad();printf("Honey owner: %d compared, %d exact, %d failures\n",n,n-failed,failed);return failed?1:0;
}

#endif
