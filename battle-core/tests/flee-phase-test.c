/* Complete owning projection of actual AttemptRunPhase, not a whole turn. */
#include "pr/engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
static pr_battle bt;static pr_engine e;static pr_pokemon mons[4];
static FILE *fp;static char line[4096],actual[128][256];static int nq;
static void bad(void){fputs("invalid flee phase trace\n",stderr);exit(2);}
static char *record(const char *tag){if(!fgets(line,sizeof line,fp)||strncmp(line,tag,strlen(tag))||!strchr(line,'\n'))bad();return line+strlen(tag);}
static void vec(const char *tag,double *v,int n){char *p=record(tag),*end;for(int j=0;j<n;j++){v[j]=strtod(p,&end);if(end==p||!isfinite(v[j]))bad();p=end;}while(isspace((unsigned char)*p))p++;if(*p)bad();}
static int integer(double v,int lo,int hi){if(v<lo||v>hi||v!=floor(v))bad();return (int)v;}
static void emit(int lv,int j,const pr_phase *p){char token[24];if(p->type==PH_SHOW_ABILITY)snprintf(token,sizeof token,"I%d",p->battler);else strcpy(token,"A");if(nq>=128)bad();snprintf(actual[nq++],256,"Q L %d %d %s %s %d\n",lv,j,pr_phase_name[p->type],token,p->type==PH_SHOW_ABILITY?p->anim_result:-1);}
static int has(const uint16_t *v,int n,int id){for(int j=0;j<n;j++)if(v[j]==id)return 1;return 0;}
static const char *locale_get(void *ctx,const char *file,int fallback,size_t *len){
 if(fallback||strcmp(file,"battle"))return NULL;
 *len=strlen(ctx);return ctx;
}
static char *load_locale(void){
 const char *dir=getenv("TURNTEST_LOCALES");if(!dir)return NULL;char path[4096];snprintf(path,sizeof path,"%s/battle.json",dir);FILE *f=fopen(path,"rb");if(!f)return NULL;
 if(fseek(f,0,SEEK_END))bad();
 long n=ftell(f);if(n<1||n>1048576)bad();rewind(f);char *s=malloc(n+1);if(!s||fread(s,1,n,f)!=(size_t)n)bad();s[n]=0;fclose(f);return s;
}
int main(int argc,char **argv){
 if(argc!=2||!(fp=fopen(argv[1],"r")))return 2;
 char *locale=load_locale();if(!locale)return 2;pr_text_source text_source={locale_get,locale,"en"};
 double header[2];vec("FLEE_PHASE ",header,2);if(header[0]!=1)bad();int n=integer(header[1],120,100000);if(n%2)bad();int fails=0,success=0,doubles_count=0;
 for(int i=0;i<n;i++){
  int id,doubles,mode,wave,turn,attempts,trick;char bseed[64],wseed[64],extra;
  char *p=record("C ");if(sscanf(p,"%d %d %d %d %d %d %d %63s %63s %c",&id,&doubles,&mode,&wave,&turn,&attempts,&trick,bseed,wseed,&extra)!=9||id!=i||doubles<0||doubles>1||doubles!=(i>=n/2)||mode<0||mode>4||wave<1||turn<1||attempts<0||attempts>12||trick<0||trick>1)bad();
  memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);memset(mons,0,sizeof mons);e.n_levels=1;e.current=-1;bt.engine=&e;bt.game_mode=mode;bt.wave_index=wave;bt.turn=turn;bt.escape_attempts=attempts;bt.double_battle=doubles;bt.max_exp_level=100;doubles_count+=doubles;
  pr_seed_from_ascii(&bt.battle_seed,bseed);pr_seed_from_ascii(&bt.wave_seed,wseed);double g[4];vec("G ",g,4);bt.rng=(pr_rng){.c=g[0],.s0=g[1],.s1=g[2],.s2=g[3]};bt.rng_live=true;
  int slots=doubles?4:2;for(int j=0;j<slots;j++){
   double v[16];vec("P ",v,16);int b=integer(v[0],0,3);if(b!=(doubles?j:2*j))bad();pr_pokemon *m=&mons[b];
   if(v[1]<1||v[1]>4294967295.0||v[1]!=floor(v[1]))bad();
   m->id=v[1];m->species=integer(v[2],1,PR_SPECIES_COUNT-1);m->hp=integer(v[3],0,1000);m->stats[STAT_HP]=1000;m->stats[STAT_SPD]=integer(v[4],1,1200);m->stat_stages[STAT_SPD-1]=integer(v[5],-6,6);m->status=integer(v[6],0,8);m->status_toxic_turns=integer(v[7],0,3);m->status_sleep_turns=integer(v[8],0,2);m->status_freeze_turns=integer(v[9],-1,2);m->ability_override=integer(v[10],0,PR_ABILITY_COUNT-1);m->passive_flag=integer(v[11],0,1);m->passive=integer(v[12],0,PR_ABILITY_COUNT-1);m->ability_suppressed=integer(v[13],0,1);m->boss_segments=integer(v[14],0,5);m->is_boss=m->boss_segments!=0;m->switch_out_status=integer(v[15],0,1);m->on_field=true;m->is_player=b<2;m->base_ability=ABILITY_ID_BALL_FETCH;m->ability=m->ability_override;m->custom_passive=m->passive;m->level=5;
   bt.field[b]=m;bt.party[b<2?0:1][b%2]=m;bt.n_party[b<2?0:1]++;
   double meta[15];vec("META ",meta,15);if(meta[0]!=b)bad();m->level=integer(meta[1],1,255);m->nature=integer(meta[2],0,24);for(int t=0;t<6;t++){m->ivs[t]=integer(meta[3+t],0,31);m->stats[t]=integer(meta[9+t],1,999999);}
  }
  double mod[2];vec("M ",mod,2);if(mod[0]!=0&&mod[0]!=1)bad();if(mod[1]!=0&&mod[1]!=2)bad();bt.n_mods[0]=mod[0];bt.n_mods[1]=mod[1];if(mod[0])bt.mods[0][0]=(pr_modifier){.cls=PR_MOD_TURN_HEAL,.pokemon_id=mons[0].id,.stack_count=1,.mult=0.0625};if(mod[1]){bt.mods[1][0]=(pr_modifier){.cls=PR_MOD_TURN_HEAL,.pokemon_id=mons[2].id,.stack_count=1,.mult=0.0625};bt.mods[1][1]=(pr_modifier){.cls=PR_MOD_MONEY_MULTIPLIER,.stack_count=1};}
  for(int side=0;side<2;side++)for(int j=0;j<bt.n_mods[side];j++){double v[6];vec("J ",v,6);if(v[0]!=side||v[1]!=j)bad();pr_modifier *m=&bt.mods[side][j];m->cls=integer(v[2],0,1)?PR_MOD_MONEY_MULTIPLIER:PR_MOD_TURN_HEAL;if(v[3]<0||v[3]>4294967295.0||v[3]!=floor(v[3]))bad();m->pokemon_id=v[3];m->stack_count=integer(v[4],0,5);m->virtual_stack_count=integer(v[5],0,2);}
  if(trick){bt.n_arena_tags=1;bt.arena_tags[0].type=ARENA_TAG_TYPE_TRICK_ROOM;bt.arena_tags[0].turn_count=5;}
  double a[5];vec("A ",a,5);bool same=pr_calculate_escape_chance(&bt,attempts)==a[0];int ph=pr_phase_new(&e,PH_ATTEMPT_RUN,-1);pr_phase_run(&bt,&e,ph);
  if(bt.escape_attempts!=a[1]||bt.successful_run!=a[2]||bt.n_mods[0]!=a[3]||bt.n_mods[1]!=a[4]||e.overflow||bt.n_unported||bt.n_deferred)same=false;
  for(int side=0;side<2;side++)for(int j=0;j<(int)a[3+side];j++){double v[6];vec("K ",v,6);if(v[0]!=side||v[1]!=j)bad();if(j>=bt.n_mods[side])same=false;else{pr_modifier *m=&bt.mods[side][j];if(m->cls!=(v[2]?PR_MOD_MONEY_MULTIPLIER:PR_MOD_TURN_HEAL)||m->pokemon_id!=v[3]||m->stack_count!=v[4]||m->virtual_stack_count!=v[5])same=false;}}
  success+=bt.successful_run;
  for(int j=0;j<slots;j++){
   double v[9];vec("O ",v,9);int b=integer(v[0],0,3);if(b!=(doubles?j:2*j))bad();pr_pokemon *m=&mons[b];
   if(m->hp!=v[1]||m->status!=v[2]||m->status_toxic_turns!=v[3]||m->status_sleep_turns!=v[4]||m->status_freeze_turns!=v[5]||m->failed_run_away!=v[6]||has(m->ab_applied_wave,m->n_ab_applied_wave,ABILITY_ID_RUN_AWAY)!=v[7]||has(m->ab_applied_summon,m->n_ab_applied_summon,ABILITY_ID_RUN_AWAY)!=v[8])same=false;
   double stats[7];vec("OS ",stats,7);if(stats[0]!=b)bad();for(int t=0;t<6;t++)if(m->stats[t]!=stats[1+t])same=false;
  }
  double r[4],br[4];vec("R ",r,4);vec("B ",br,4);if(bt.rng.c!=r[0]||bt.rng.s0!=r[1]||bt.rng.s1!=r[2]||bt.rng.s2!=r[3]||!bt.battle_rng_live||bt.battle_rng.c!=br[0]||bt.battle_rng.s0!=br[1]||bt.battle_rng.s1!=br[2]||bt.battle_rng.s2!=br[3])same=false;
  nq=0;for(int lv=0;lv<e.n_levels;lv++)for(int j=0;j<e.level_len[lv];j++)emit(lv,j,&e.pool[e.levels[lv][j]]);for(int j=0;j<PH_COUNT_;j++)if(e.n_dyn[j])same=false;
  double q[3];vec("QCOUNT ",q,3);int wantq=integer(q[0],0,128);if(wantq!=nq||q[1]!=e.n_levels||q[2]!=e.deferred_active)same=false;for(int j=0;j<wantq;j++){record("Q ");char scope,name[64],token[24],rest;int group,order,passive;
   if(sscanf(line,"Q %c %d %d %63s %23s %d %c",&scope,&group,&order,name,token,&passive,&rest)!=6||scope!='L'||group<0||group>=PR_MAX_LEVELS||order<0||order>=PR_MAX_PHASES)bad();
   bool known=false;for(int t=1;t<PH_COUNT_;t++)if(pr_phase_name[t]&&!strcmp(name,pr_phase_name[t]))known=true;if(!known)bad();
   if(!strcmp(name,"ShowAbilityPhase")){char *end;long b=strtol(token+1,&end,10);if(token[0]!='I'||end==token+1||*end||b<-1||b>3||passive<0||passive>1)bad();}else if(strcmp(token,"A")||passive!=-1)bad();
   if(j>=nq||strcmp(line,actual[j])){if(fails<8)fprintf(stderr,"case%d queue%d C:%sJS:%s",i,j,j<nq?actual[j]:"missing\n",line);same=false;}}
  char key[128];p=record("T ");if(sscanf(p,"%127s %c",key,&extra)!=1)bad();int messages=0;const pr_msg *got_message=NULL;for(int j=0;j<PR_MAX_PHASES;j++)if(e.used[j]&&e.pool[j].type==PH_MESSAGE){messages++;got_message=&e.pool[j].msg;if(e.pool[j].msg.n_args||!e.pool[j].msg.key||strcmp(e.pool[j].msg.key,key))same=false;}if(messages!=1)same=false;
  char rendered[512],hex[1024];pr_msg missing=pr_msg0("");size_t len=pr_msg_format(got_message?got_message:&missing,&text_source,rendered,sizeof rendered);if(len>=sizeof rendered)bad();for(size_t j=0;j<len;j++)snprintf(hex+2*j,3,"%02x",(unsigned char)rendered[j]);hex[2*len]=0;char *want_text=record("TEXT ");size_t wn=strlen(want_text);if(wn<2||want_text[wn-1]!='\n')bad();want_text[--wn]=0;if(wn%2)bad();for(size_t j=0;j<wn;j++)if(!isxdigit((unsigned char)want_text[j]))bad();if(strcmp(hex,want_text))same=false;
  p=record("CALLS ");char *end;long nc=strtol(p,&end,10);if(end==p||nc<0||nc>4)bad();p=end;int expected_calls[4];for(int j=0;j<nc;j++){long b=strtol(p,&end,10);if(end==p||b<0||b>3)bad();p=end;expected_calls[j]=b;}while(isspace((unsigned char)*p))p++;if(*p)bad();int got_calls=0;for(int j=0;j<PR_MAX_PHASES;j++)if(e.used[j]&&e.pool[j].type==PH_SHOW_ABILITY){if(got_calls>=nc||e.pool[j].battler!=expected_calls[got_calls])same=false;got_calls++;}if(got_calls!=nc)same=false;
  record("E");if(strcmp(line,"E\n"))bad();if(!same){if(fails<20)fprintf(stderr,"flee phase case%d mismatch\n",i);fails++;}
 }
 double footer;vec("FLEE_PHASE_COMPLETE ",&footer,1);if(footer!=n||fgetc(fp)!=EOF)bad();printf("Flee phase cases%d exact%d failures%d doubles%d success%d\n",n,n-fails,fails,doubles_count,success);return fails?1:0;
}
