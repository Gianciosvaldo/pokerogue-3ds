/* Strict replay for actual source copying, shared moves, PP and summon cleanup. */
#include "pr/engine.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

enum { WIDTH = 40 };
static FILE *input;
static int lineno;
static char line[8192];
static void bad(void) { fprintf(stderr, "invalid copying fixture at line %d\n", lineno); exit(2); }
static void read_vector(const char *key, double *v, int n)
{
    if (!fgets(line, sizeof line, input)) bad();
    lineno++;
    if (strncmp(line, key, strlen(key))) bad();
    char *p = line + strlen(key), *end;
    for (int i = 0; i < n; i++) {
        v[i] = strtod(p, &end);
        if (end == p || !isfinite(v[i])) bad();
        p = end;
    }
    while (isspace((unsigned char)*p)) p++;
    if (*p) bad();
}
static int integer(double v, double low, double high)
{
    if (v < low || v > high || v != floor(v)) bad();
    return (int)v;
}
static const char *locale_get(void *ctx,const char *file,int fallback,size_t *len){
 (void)ctx;if(fallback)return NULL;static struct{char name[64];char *text;size_t len;}cache[16];static int count;
 for(int i=0;i<count;i++)if(!strcmp(file,cache[i].name)){*len=cache[i].len;return cache[i].text;}
 if(count==16){bad();}const char *dir=getenv("TURNTEST_LOCALES");if(!dir)bad();char path[4096];snprintf(path,sizeof path,"%s/%s.json",dir,file);FILE *f=fopen(path,"rb");if(!f)return NULL;if(fseek(f,0,SEEK_END))bad();long n=ftell(f);if(n<1||n>1048576)bad();rewind(f);char *s=malloc(n+1);if(!s||fread(s,1,n,f)!=(size_t)n)bad();s[n]=0;fclose(f);snprintf(cache[count].name,64,"%s",file);cache[count].text=s;cache[count++].len=(size_t)n;*len=(size_t)n;return s;
}
static void load(pr_pokemon *p,const double *v){
 memset(p,0,sizeof *p);p->species=integer(v[0],1,65535);p->form=integer(v[1],0,255);if(!pr_pokemon_species(p)||p->form>=pr_pokemon_species(p)->nforms)bad();
 p->transformed=integer(v[2],0,1);p->summon_moves_active=integer(v[3],0,1);p->summon_moves_alias_mask=integer(v[4],0,15);p->n_moves=integer(v[5],0,4);
 p->transform_species=p->transformed?SPECIES_ID_MAGIKARP:0;p->hp=integer(v[39],0,INT_MAX);p->is_player=true;p->id=1;p->on_field=true;
 for(int i=0;i<4;i++){p->moves[i]=integer(v[6+i],0,PR_MOVE_COUNT-1);p->pp_used[i]=integer(v[10+i],0,255);p->pp_up[i]=integer(v[14+i],0,3);p->max_pp_override[i]=integer(v[18+i],-1,32767);}
 p->transform_n_moves=integer(v[22],0,4);
 for(int i=0;i<4;i++){p->transform_moves[i]=integer(v[23+i],0,PR_MOVE_COUNT-1);p->transform_pp_used[i]=integer(v[27+i],0,255);p->transform_pp_up[i]=integer(v[31+i],0,3);p->transform_max_pp_override[i]=integer(v[35+i],-1,32767);}
}
static void snapshot(const pr_pokemon *p,double *v){
 v[0]=p->species;v[1]=p->form;v[2]=p->transformed;v[3]=PR_HAS_SUMMON_MOVES(p);v[4]=p->summon_moves_alias_mask;v[5]=p->n_moves;v[22]=PR_N_MOVES(p);v[39]=p->hp;
 for(int i=0;i<4;i++){
  v[6+i]=i<p->n_moves?p->moves[i]:0;v[10+i]=i<p->n_moves?p->pp_used[i]:0;v[14+i]=i<p->n_moves?p->pp_up[i]:0;v[18+i]=i<p->n_moves?p->max_pp_override[i]:-1;
  v[23+i]=i<PR_N_MOVES(p)?PR_MOVES(p)[i]:0;v[27+i]=i<PR_N_MOVES(p)?PR_PP_USED(p)[i]:0;v[31+i]=i<PR_N_MOVES(p)?PR_PP_UP(p)[i]:0;v[35+i]=i<PR_N_MOVES(p)?PR_MAX_PP(p)[i]:-1;
 }
}
static bool check(const pr_pokemon *p,const char *key,int c){double want[WIDTH],got[WIDTH];read_vector(key,want,WIDTH);snapshot(p,got);bool ok=true;for(int i=0;i<WIDTH;i++)if(want[i]!=got[i]){if(c<100)fprintf(stderr,"case%d %sfield%d C:%g JS:%g\n",c,key,i,got[i],want[i]);ok=false;}return ok;}
static int ppmax(int move,int up,int override){int step=pr_moves[move].pp/5;if(step<1)step=1;return override>0?override:pr_moves[move].pp+up*step;}
static void rng_read(const char *key,pr_rng *r){double v[4];read_vector(key,v,4);r->c=integer(v[0],0,INT_MAX);for(int i=1;i<4;i++)if(v[i]<0||v[i]>=1)bad();r->s0=v[1];r->s1=v[2];r->s2=v[3];}
int main(int argc,char **argv){
 if(argc!=2)return 2;input=fopen(argv[1],"r");if(!input)return 2;pr_closures_init();double h[3];read_vector("COPYING ",h,3);if(h[0]!=1)bad();int n=integer(h[1],192,100000),seed=integer(h[2],0,INT_MAX),fails=0;(void)seed;
 for(int c=0;c<n;c++){
  static pr_battle bt;static pr_engine e;static pr_pokemon p,t;memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);memset(&t,0,sizeof t);bt.engine=&e;e.n_levels=1;e.current=-1;bt.ignoring_effect_source=-1;
  double row[5];read_vector("C ",row,5);if(row[0]!=c)bad();int kind=integer(row[1],0,5),slot=integer(row[2],0,3),amount=integer(row[3],1,8),nh=integer(row[4],1,2);if(kind!=(c/32)%6)bad();
  double in[WIDTH];read_vector("I ",in,WIDTH);load(&p,in);bt.field[0]=bt.party[0][0]=&p;bt.field[2]=bt.party[1][0]=&t;bt.n_party[0]=bt.n_party[1]=1;t.id=2;t.hp=1;
  for(int j=0;j<nh;j++){double v[3];read_vector("H ",v,3);pr_turn_move tm={0};tm.move=integer(v[0],0,PR_MOVE_COUNT-1);tm.use_mode=integer(v[1],MOVE_USE_MODE_NORMAL,MOVE_USE_MODE_DELAYED_ATTACK);tm.result=integer(v[2],MOVE_RESULT_PENDING,MOVE_RESULT_OTHER);pr_push_move_history(&t,tm);}
  rng_read("R ",&bt.rng);bt.rng_live=true;int ph=pr_phase_new(&e,PH_MOVE,0);if(ph<0)bad();pr_phase *held=&e.pool[ph];held->pokemon=&p;held->move.slot=(int8_t)slot;held->move.from_transform=PR_HAS_SUMMON_MOVES(&p);held->move.id=PR_MOVES(&p)[slot];
  int move=kind==1||kind==4?MOVE_ID_MIMIC:MOVE_ID_SKETCH;bool condition=true,applied=true,ok=true;
  if(kind==2)pr_set_move(&bt,&e,&p,slot,MOVE_ID_GROWL);
  else if(kind==3){
   /* Learning runs between phases. Keep a value snapshot of its old object. */
   if(!PR_HAS_SUMMON_MOVES(&p)){held->move.pp_used=p.pp_used[slot];held->move.pp_up=p.pp_up[slot];held->move.max_pp_override=p.max_pp_override[slot];held->move.slot=-1;}
   pr_set_original_move(&p,slot,MOVE_ID_LEER);
  }else{
   condition=pr_move_apply_conditions(&bt,move,&p,&t,4);applied=false;
   if(condition)applied=pr_copy_last_move(&bt,&e,&p,&t,move,kind!=1&&kind!=4);
   if(kind==4){pr_turn_move tm={.move=MOVE_ID_LEER,.use_mode=MOVE_USE_MODE_NORMAL,.result=MOVE_RESULT_SUCCESS};pr_push_move_history(&t,tm);if(pr_move_apply_conditions(&bt,MOVE_ID_SKETCH,&p,&t,4))pr_copy_last_move(&bt,&e,&p,&t,MOVE_ID_SKETCH,true);}
  }
  double a[2];read_vector("A ",a,2);integer(a[0],0,1);integer(a[1],0,1);if(a[0]!=condition||a[1]!=applied){if(c<100)fprintf(stderr,"case%d condition/apply C:%d/%d JS:%g/%g\n",c,condition,applied,a[0],a[1]);ok=false;}
  ok&=check(&p,"O ",c);int max=ppmax(PR_MOVES(&p)[slot],PR_PP_UP(&p)[slot],PR_MAX_PP(&p)[slot]);if(max!=-1)pr_use_moveset_pp(&p,slot,PR_HAS_SUMMON_MOVES(&p),amount);
  ok&=check(&p,"P ",c);double old[4];read_vector("OLD ",old,4);double actual[4]={held->move.id,held->move.pp_used,held->move.pp_up,held->move.max_pp_override};if(held->move.slot>=0){int s=held->move.slot;bool temp=held->move.from_transform;actual[1]=(temp?p.transform_pp_used:p.pp_used)[s];actual[2]=(temp?p.transform_pp_up:p.pp_up)[s];actual[3]=(temp?p.transform_max_pp_override:p.max_pp_override)[s];}
  for(int i=0;i<4;i++)if(old[i]!=actual[i]){if(c<100)fprintf(stderr,"case%d oldref%d C:%g JS:%g\n",c,i,actual[i],old[i]);ok=false;}
  double tc[1];read_vector("TEXT ",tc,1);int count=integer(tc[0],0,4),got=0;
  for(int l=0;l<e.n_levels;l++)for(int j=0;j<e.level_len[l];j++){pr_phase *msg=&e.pool[e.levels[l][j]];if(msg->type!=PH_MESSAGE)bad();if(got++>=count){ok=false;continue;}char rendered[1024],hex[2048];pr_text_source src={locale_get,NULL,NULL};size_t len=pr_msg_format(&msg->msg,&src,rendered,sizeof rendered);if(len>=sizeof rendered)bad();for(size_t k=0;k<len;k++)snprintf(hex+2*k,3,"%02x",(unsigned char)rendered[k]);hex[2*len]=0;if(!fgets(line,sizeof line,input))bad();lineno++;if(strncmp(line,"T ",2))bad();line[strcspn(line,"\n")]=0;size_t hs=strlen(line+2);if(!hs||hs%2)bad();for(size_t k=2;line[k];k++)if(!isxdigit((unsigned char)line[k]))bad();if(strcmp(line+2,hex)){if(c<100)fprintf(stderr,"case%d text C:%s JS:%s\n",c,hex,line+2);ok=false;}}
  while(got<count){if(!fgets(line,sizeof line,input))bad();lineno++;if(strncmp(line,"T ",2))bad();got++;ok=false;}
  pr_rng want;rng_read("RO ",&want);if(want.c!=bt.rng.c||want.s0!=bt.rng.s0||want.s1!=bt.rng.s1||want.s2!=bt.rng.s2)ok=false;
  pr_reset_summon_data(&p);ok&=check(&p,"Z ",c);if(!fgets(line,sizeof line,input)||strcmp(line,"E\n"))bad();lineno++;if(bt.n_unported||bt.n_deferred||e.overflow)ok=false;if(!ok)fails++;
 }
 double end[1];read_vector("COPYING_COMPLETE ",end,1);if(end[0]!=n||fgetc(input)!=EOF)bad();printf("copying APIs: %d complete, %d exact, %d failures\n",n,n-fails,fails);return fails?1:0;
}
