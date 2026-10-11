/* Actual VictoryPhase battle friendship caller and separate unchanged API controls. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#define FIELDS 54
static FILE *fp;
static int lineno, failures;
static gc_run run;
static gc_system sys, before_sys;
static char line[4096];
static void bad(const char *s) { fprintf(stderr,"invalid Soothe battle trace line %d: %s\n",lineno,s); exit(2); }
static char *record(char c) { if(!fgets(line,sizeof line,fp))bad("EOF");lineno++;if(line[0]!=c||(line[1]!=' '&&line[1]!='\n')||!strchr(line,'\n'))bad("record order/length");return line+2; }
static void vec(char c,double *v,int n) {char *s=record(c),*e;for(int i=0;i<n;i++){v[i]=strtod(s,&e);if(e==s||!isfinite(v[i]))bad("number");s=e;}while(isspace((unsigned char)*s))s++;if(*s)bad("extra field");}
static int integer(double v,int lo,int hi) {if(v<lo||v>hi||v!=floor(v))bad("integer range");return (int)v;}
static void validate_mon(const double *v){
    for(int j=0;j<FIELDS;j++)if(v[j]!=floor(v[j])||v[j]<-1||v[j]>4294967295.0)bad("party scalar");
    if(v[0]<1||v[0]>4294967295.0)bad("Pokemon ID");integer(v[15],0,255);integer(v[9],0,24);integer(v[10],0,24);integer(v[11],-1,24);integer(v[1],1,PR_SPECIES_COUNT-1);integer(v[2],0,255);integer(v[5],1,100);integer(v[8],0,STATUS_EFFECT_FAINT);integer(v[25],0,4);
    for(int j=42;j<48;j++)integer(v[j],0,31);
    for(int j=48;j<54;j++)integer(v[j],1,2147483647);
}
static void read_mon(gc_mon *m,const double *v) {
    validate_mon(v);
    memset(m,0,sizeof *m);pr_pokemon *p=&m->b;int k=0;
#define R(f) f=v[k++]
    R(p->id);R(p->species);R(p->form);int fusion=v[k++];p->fusion_species=fusion<0?0:fusion;
    R(p->fusion_form);R(p->level);R(m->exp);R(p->hp);R(p->status);R(p->nature);R(m->base_nature);
    m->has_custom_nature=v[k++]>=0;R(p->base_ability);R(p->ability);R(p->passive);R(p->friendship);
    R(p->gender);R(m->ability_index);R(p->fusion_ability_index);R(p->shiny);R(p->passive_flag);
    R(m->variant);R(m->luck);R(m->pause_evolutions);R(p->tera_type);R(p->n_moves);
    for(int j=0;j<4;j++){R(p->moves[j]);R(p->pp_used[j]);R(p->pp_up[j]);k++;}
    for(int j=0;j<6;j++){R(p->ivs[j]);}for(int j=0;j<6;j++){R(p->stats[j]);}
#undef R
    p->is_player=true;p->ability_override=p->custom_passive=-1;p->status_sleep_turns=p->status_freeze_turns=-1;
    if(k!=FIELDS||p->n_moves>4)bad("mon shape");
}
static void mon_vec(const gc_mon *m,double *v) {
    const pr_pokemon *p=&m->b;int k=0;
#define W(f) v[k++]=f
    W(p->id);W(p->species);W(p->form);W(p->fusion_species?p->fusion_species:-1);W(p->fusion_form);
    W(p->level);W(m->exp);W(p->hp);W(p->status);W(p->nature);W(m->has_custom_nature?m->base_nature:p->nature);
    W(m->has_custom_nature?p->nature:-1);W(p->base_ability);W(p->ability);W(p->passive);W(p->friendship);
    W(p->gender);W(m->ability_index);W(p->fusion_ability_index);W(p->shiny);W(p->passive_flag);
    W(m->variant);W(m->luck);W(m->pause_evolutions);W(p->tera_type);W(p->n_moves);
    for(int j=0;j<4;j++){W(p->moves[j]);W(p->pp_used[j]);W(p->pp_up[j]);W(j<p->n_moves?gc_move_max_pp(p,j):0);}
    for(int j=0;j<6;j++){W(p->ivs[j]);}for(int j=0;j<6;j++){W(p->stats[j]);}
#undef W
    if(k!=FIELDS)bad("mon output");
}

int main(int argc,char **argv){
 if(argc!=2||!(fp=fopen(argv[1],"r")))return 2;
 int version,n,used;if(!fgets(line,sizeof line,fp)||sscanf(line,"SOOTHE_BATTLE %d %d%n",&version,&n,&used)!=2||version!=1||n<120||n>100000||strcmp(line+used,"\n"))return 2;
 int changes=0,classic=0,endless=0;
 for(int ci=0;ci<n;ci++){
  double c[6],r[4],mons[6][FIELDS],out[6][FIELDS],participant[6],a[2],after[4],nr;
  vec('C',c,6);if(c[0]!=ci)bad("case order");int count=integer(c[1],1,6),mode=integer(c[2],0,1);integer(c[3],0,100000);integer(c[4],0,9999);if(c[5]<0)bad("wallet");vec('R',r,4);integer(r[0],0,2147483647);for(int j=1;j<4;j++)if(r[j]<0||r[j]>=1)bad("RNG range");
  for(int j=0;j<count;j++){double full[FIELDS+1];vec('P',full,FIELDS+1);memcpy(mons[j],full,sizeof mons[j]);participant[j]=integer(full[FIELDS],0,1);validate_mon(mons[j]);integer(mons[j][15],0,255);}
  for(int j=0;j<count;j++)for(int k=0;k<j;k++)if(mons[j][0]==mons[k][0])bad("duplicate Pokemon ID");
  vec('I',&nr,1);int ni=integer(nr,0,PR_MAX_MODIFIERS);gc_item input[PR_MAX_MODIFIERS]={0};
  for(int j=0;j<ni;j++){double h[2];vec('H',h,2);int owner=integer(h[0],-1,count-1),stack=integer(h[1],1,3);input[j]=(gc_item){.key="SOOTHE_BELL",.arg=j%2?-1:0,.stack=stack,.pokemon_id=owner<0?0:(uint32_t)mons[owner][0]};}
  for(int j=0;j<count;j++){vec('T',out[j],FIELDS);validate_mon(out[j]);integer(out[j][15],0,255);}
  vec('A',a,2);integer(a[0],0,100000);integer(a[1],0,9999);vec('S',after,4);integer(after[0],0,2147483647);for(int j=1;j<4;j++)if(after[j]<0||after[j]>=1)bad("RNG range");double end;vec('E',&end,1);if(end!=ci)bad("end order");
  gc_run_new(&run,mode?GAME_MODES_ENDLESS:GAME_MODES_CLASSIC,"soothe-battle",777);gc_system_init_defaults(&sys);run.sys=&sys;
  run.n_party=count;run.n_items=ni;run.money=c[5];run.run.rng=(pr_rng){.c=r[0],.s0=r[1],.s1=r[2],.s2=r[3]};run.bt.n_participants=0;run.bt.max_exp_level=100;
  for(int j=0;j<count;j++){int storage=(j+ci%count)%count;run.party_order[j]=storage;read_mon(&run.party[storage],mons[j]);run.bt.party[0][j]=&run.party[storage].b;if(participant[j])run.bt.participants[run.bt.n_participants++]=mons[j][0];}
  uint16_t root=gc_species_root_id(run.party[0].b.species);sys.starters[root].friendship=c[3];sys.starters[root].candy_count=c[4];before_sys=sys;
  memcpy(run.items,input,sizeof input);gc_items_to_battle(&run);pr_rng gb=run.bt.rng,bb=run.bt.battle_rng;gc_rq_reset(&gc_int(&run)->rq);
  run.n_enemy=1;run.enemy[0].b.species=129;run.enemy[0].b.level=5;gc_rphase phase=gc_rp(RP_VICTORY,0,0,0);gc_phase_start(&run,&phase);
  bool same=run.money==c[5]&&!memcmp(run.items,input,sizeof input)&&run.n_items==ni&&sys.starters[root].friendship==a[0]&&sys.starters[root].candy_count==a[1];
  before_sys.starters[root].friendship=sys.starters[root].friendship;before_sys.starters[root].candy_count=sys.starters[root].candy_count;if(memcmp(&before_sys,&sys,sizeof sys)||memcmp(&gb,&run.bt.rng,sizeof gb)||memcmp(&bb,&run.bt.battle_rng,sizeof bb)||run.run.rng.c!=after[0]||run.run.rng.s0!=after[1]||run.run.rng.s1!=after[2]||run.run.rng.s2!=after[3])same=false;
  for(int j=0;j<count;j++){double values[FIELDS];mon_vec(&run.party[run.party_order[j]],values);for(int k=0;k<FIELDS;k++)if(values[k]!=out[j][k]){same=false;if(ci<2)fprintf(stderr,"MON%d field%d actual%g expected%g\n",j,k,values[k],out[j][k]);}changes+=values[15]!=mons[j][15];}
  gc_rqueue *q=&gc_int(&run)->rq;int continuations=0;for(int j=0;j<q->n_levels;j++)for(int k=0;k<q->n[j];k++){if(q->q[j][k].type!=RP_CONTINUE)same=false;else continuations++;}if(continuations!=1)same=false;
  gc_event ev;while(gc_run_next_event(&run,&ev)){same=false;if(ci<2)fprintf(stderr,"unexpected event%d\n",ev.kind);}
  if(run.stubs[GC_STUB_REWARD_APPLY])same=false;
  if(!same){if(failures<8)fprintf(stderr,"FAIL soothe battle case%d profile%u/%g candy%u/%g\n",ci,sys.starters[root].friendship,a[0],sys.starters[root].candy_count,a[1]);failures++;}
  double f[8];vec('F',f,8);integer(f[0],0,255);integer(f[1],0,100000);integer(f[2],0,9999);integer(f[3],-1,6);integer(f[4],0,1);integer(f[5],0,255);integer(f[6],0,100000);integer(f[7],0,9999);
  gc_mon *control=&run.party[run.party_order[0]];control->b.friendship=f[0];sys.starters[root].friendship=f[1];sys.starters[root].candy_count=f[2];gc_system_add_friendship(&sys,&run,control,f[3],f[4]);
  if(control->b.friendship!=f[5]||sys.starters[root].friendship!=f[6]||sys.starters[root].candy_count!=f[7]){fprintf(stderr,"FAIL unchanged shared control case%d\n",ci);failures++;}
  mode?endless++:classic++;
 }
 double footer;vec('Z',&footer,1);if(footer!=n||fgetc(fp)!=EOF)bad("footer/trailing");fclose(fp);
 printf("Soothe battle cases%d exact%d failures%d changedPokemon%d Classic%d Endless%d\n",n,n-failures,failures,changes,classic,endless);return failures?1:0;
}
