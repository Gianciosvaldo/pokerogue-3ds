#include "pr/engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include "optional-phase-map.inc"
static pr_battle bt;static pr_engine engine;static pr_pokemon lead,enemy,bench;static FILE *fp;static char line[4096],actual[128][256];static int na,failures;
static void bad(void){fputs("invalid optional fixture\n",stderr);exit(2);}
static char *record(const char *tag){if(!fgets(line,sizeof line,fp)||strncmp(line,tag,strlen(tag))||!strchr(line,'\n'))bad();return line+strlen(tag);}
static void vec(const char *tag,double *v,int n){char *s=record(tag),*e;for(int j=0;j<n;j++){v[j]=strtod(s,&e);if(e==s||!isfinite(v[j]))bad();s=e;}while(isspace((unsigned char)*s))s++;if(*s)bad();}
static int integer(double v,int lo,int hi){if(v<lo||v>hi||v!=floor(v))bad();return (int)v;}
static void emit(char scope,int group,int order,const pr_phase *p){char name[96],token[24];if(p->type==PH_MARKER)snprintf(name,sizeof name,"Marker:%s",pr_phase_name[p->marker_of]);else snprintf(name,sizeof name,"%s",pr_phase_name[p->type]);if(p->type==PH_MARKER||optional_phase_absent(p))strcpy(token,"A");else snprintf(token,sizeof token,"I%d",p->battler);if(na>=128)bad();snprintf(actual[na++],256,"Q %c %d %d %s %s\n",scope,group,order,name,token);}
static void dump(void){na=0;for(int lv=0;lv<engine.n_levels;lv++)for(int j=0;j<engine.level_len[lv];j++)emit('L',lv,j,&engine.pool[engine.levels[lv][j]]);int types[PH_COUNT_],n=0;for(int t=0;t<PH_COUNT_;t++)if(engine.n_dyn[t])types[n++]=t;for(int j=1;j<n;j++)for(int k=j;k>0&&strcmp(pr_phase_name[types[k]],pr_phase_name[types[k-1]])<0;k--){int tmp=types[k];types[k]=types[k-1];types[k-1]=tmp;}for(int j=0;j<n;j++)for(int k=0;k<engine.n_dyn[types[j]];k++)emit('D',j,k,&engine.pool[engine.dyn[types[j]][k]]);}
static void push(pr_phase_type type,int b){int ph=pr_phase_new(&engine,type,b);if(ph<0)bad();if(type==PH_SWITCH_SUMMON){engine.pool[ph].sw_player=false;engine.pool[ph].sw_field_index=0;engine.pool[ph].sw_slot=-1;engine.pool[ph].sw_type=SWITCH_TYPE_SWITCH;}pr_phase_push(&bt,&engine,ph);}
int main(int argc,char **argv){if(argc!=2||!(fp=fopen(argv[1],"r")))return 2;int version,n,used;if(!fgets(line,sizeof line,fp)||sscanf(line,"OPTIONAL_FIXTURES %d %d%n",&version,&n,&used)!=2||version!=1||n!=6||strcmp(line+used,"\n"))return 2;
 for(int ci=0;ci<n;ci++){
  double id,r[4],p[9],a[4],s[4],nr;vec("C ",&id,1);if(id!=ci)bad();vec("R ",r,4);integer(r[0],1,2091639);for(int j=1;j<4;j++)if(r[j]<0||r[j]>=1)bad();vec("P ",p,9);if(p[0]<1||p[0]>4294967295.0||floor(p[0])!=p[0])bad();integer(p[1],1,2147483647);for(int j=2;j<8;j++)integer(p[j],1,2147483647);if(p[8]<0)bad();
  memset(&bt,0,sizeof bt);memset(&engine,0,sizeof engine);engine.n_levels=1;engine.current=-1;bt.engine=&engine;for(int j=0;j<4;j++)engine.command_targets[j]=-1;memset(&lead,0,sizeof lead);memset(&enemy,0,sizeof enemy);memset(&bench,0,sizeof bench);lead=(pr_pokemon){.id=1,.is_player=true,.on_field=true,.hp=20,.ability=ABILITY_ID_NONE,.base_ability=ABILITY_ID_NONE};enemy=lead;enemy.id=2;enemy.is_player=false;bench=lead;bench.id=p[0];bench.on_field=false;bench.hp=p[1];for(int j=0;j<6;j++)bench.stats[j]=p[2+j];lead.stats[5]=enemy.stats[5]=bench.stats[5];bt.party[0][0]=&lead;bt.party[0][1]=&bench;bt.n_party[0]=2;bt.party[1][0]=&enemy;bt.n_party[1]=1;bt.field[0]=&lead;bt.field[2]=&enemy;bt.money=p[8];bt.rng=(pr_rng){.c=r[0],.s0=r[1],.s1=r[2],.s2=r[3]};pr_rng br=bt.battle_rng;
  if(ci==0||ci==4||ci==5)push(PH_SWITCH_SUMMON,-1);
  if(ci==1||ci==4)push(PH_POST_SUMMON,0);
  if(ci==2||ci==4)push(PH_POST_SUMMON,2);
  if(ci==3||ci==5){bt.n_mods[0]=1;bt.mods[0][0]=(pr_modifier){.cls=PR_MOD_BERRY,.pokemon_id=bench.id,.stack_count=1,.berry_type=BERRY_TYPE_SITRUS};if(pr_battler_index(&bt,&bench)!=-1||!pr_berry_should_apply(&bt,&bench,&bt.mods[0][0]))return 1;pr_unshift_common_anim(&bt,&engine,pr_battler_index(&bt,&bench),pr_battler_index(&bt,&bench),COMMON_ANIM_USE_ITEM);pr_apply_berry(&bt,&engine,&bench,&bt.mods[0][0]);}
  vec("A ",a,4);integer(a[0],1,2147483647);integer(a[1],0,1);integer(a[2],0,1);if(a[3]<0)bad();bool same=!bt.n_unported&&!bt.n_deferred&&!engine.overflow&&bench.hp==a[0]&&bench.has_eaten_berry==a[1]&&bt.n_mods[0]==a[2]&&a[3]==bt.money&&lead.hp==20&&enemy.hp==20&&memcmp(&br,&bt.battle_rng,sizeof br)==0;for(int j=0;j<6;j++)if(bench.stats[j]!=p[2+j])same=false;
  dump();double shape[3];vec("QCOUNT ",shape,3);int nq=integer(shape[0],0,128),levels=integer(shape[1],1,PR_MAX_LEVELS),deferred=integer(shape[2],0,1);if(nq!=na||levels!=engine.n_levels||deferred!=engine.deferred_active)same=false;for(int j=0;j<nq;j++){record("Q ");char scope,name[96],token[24],rest;int group,order;if(sscanf(line,"Q %c %d %d %95s %23s %c",&scope,&group,&order,name,token,&rest)!=5||(scope!='L'&&scope!='D')||group<0||group>=PH_COUNT_||order<0||order>=PR_MAX_PHASES)bad();bool known=false;const char *kind=!strncmp(name,"Marker:",7)?name+7:name;for(int t=0;t<PH_COUNT_;t++)if(pr_phase_name[t]&&!strcmp(kind,pr_phase_name[t]))known=true;if(!known)bad();if(strcmp(token,"A")&&strcmp(token,"U")&&strcmp(token,"N")){char *end;long index=strtol(token+1,&end,10);if(token[0]!='I'||end==token+1||*end||index<-1||index>3)bad();}if(j>=na||strcmp(line,actual[j])){same=false;fprintf(stderr,"fixture%d queue%d C:%sJS:%s",ci,j,j<na?actual[j]:"missing\n",line);}}
  vec("S ",s,4);if(bt.rng.c!=s[0]||bt.rng.s0!=s[1]||bt.rng.s1!=s[2]||bt.rng.s2!=s[3])same=false;vec("E ",&id,1);if(id!=ci)bad();if(!same)failures++;
 }
 double footer;vec("Z ",&footer,1);if(footer!=n||fgetc(fp)!=EOF)bad();printf("Optional deterministic fixtures%d exact%d failures%d full queue kind/order/presence and state/RNG controls\n",n,n-failures,failures);return failures?1:0;
}
