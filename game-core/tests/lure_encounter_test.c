/* Delegating observer of actual gc_new_battle -> pr_run_new_battle. */
#include "../src/internal.h"
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
static FILE *fp;static char line[4096];static int lineno,failures;static jmp_buf stop;
static gc_run run,loaded;static unsigned char saved[GC_SAVE_MAX];static bool watching;static gc_run *observed;
static pr_new_battle_ctx seen;static pr_new_battle built;static pr_rng initial;static double draws[2];static int ndraws,callcount;
static void bad(const char *s){fprintf(stderr,"invalid encounter line%d: %s\n",lineno,s);exit(2);}
static char *record(char tag){if(!fgets(line,sizeof line,fp))bad("EOF");lineno++;if(line[0]!=tag||line[1]!=' '||!strchr(line,'\n'))bad("record/order");return line+2;}
static void numbers(char *s,double *v,int n){char *e;for(int j=0;j<n;j++){v[j]=strtod(s,&e);if(e==s||!isfinite(v[j]))bad("number");s=e;}while(isspace((unsigned char)*s))s++;if(*s)bad("shape");}
static void vec(char t,double *v,int n){numbers(record(t),v,n);}
static int integer(double v,int lo,int hi){if(v!=floor(v)||v<lo||v>hi)bad("integer");return(int)v;}
static bool rng_eq(pr_rng r,const double *v){return r.c==v[0]&&r.s0==v[1]&&r.s1==v[2]&&r.s2==v[3];}
static bool rng_same(pr_rng a,pr_rng b){return a.c==b.c&&a.s0==b.s0&&a.s1==b.s1&&a.s2==b.s2;}
static void validate_rng(const double *r){integer(r[0],1,2091639);for(int j=1;j<4;j++)if(r[j]<0||r[j]>=1)bad("RNG fraction");}
int32_t __real_pr_rng_seed_int(pr_rng *,int32_t,int32_t);
int32_t __wrap_pr_rng_seed_int(pr_rng *r,int32_t range,int32_t min){
 if(!watching||r!=&observed->run.rng)return __real_pr_rng_seed_int(r,range,min);
 if(min||callcount++)bad("unexpected global draw boundary");
 pr_rng audit=*r;
 if(range>1){draws[0]=pr_rng_rnd(&audit);draws[1]=pr_rng_rnd(&audit);ndraws=2;}
 int32_t result=__real_pr_rng_seed_int(r,range,min);if(!rng_same(*r,audit))bad("raw draw audit");return result;
}
int __real_pr_run_new_battle(pr_run *,int,const pr_new_battle_ctx *,pr_new_battle *);
int __wrap_pr_run_new_battle(pr_run *r,int wave,const pr_new_battle_ctx *ctx,pr_new_battle *out){
 if(!watching)return __real_pr_run_new_battle(r,wave,ctx,out);
 seen=*ctx;pr_run audit=*r;pr_run_reset_seed(&audit,wave);initial=audit.rng;
 int result=__real_pr_run_new_battle(r,wave,ctx,out);if(result)bad("unexpected unsupported caller");built=*out;longjmp(stop,1);
}
#define FIELDS 54
static void validate_mon(const double *v){
    for(int j=0;j<FIELDS;j++)if(v[j]!=floor(v[j])||v[j]<-1||v[j]>4294967295.0)bad("party scalar");
    if(v[0]<1||v[0]>4294967295.0)bad("Pokemon ID");
    integer(v[15],0,255);integer(v[9],0,24);integer(v[10],0,24);integer(v[11],-1,24);integer(v[1],1,PR_SPECIES_COUNT-1);integer(v[2],0,255);integer(v[5],1,100);integer(v[8],0,STATUS_EFFECT_FAINT);integer(v[25],0,4);
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


static void observe_caller(gc_run *x){ndraws=callcount=0;observed=x;watching=true;if(!setjmp(stop))gc_new_battle(x);watching=false;}
static gc_item item(char tag){char key[40];int used;char *s=record(tag);if(sscanf(s,"%39s%n",key,&used)!=1)bad("key");double v[2];numbers(s+used,v,2);int idx=gc_reward_item_index(key),max=idx==PR_ITEM_LURE?10:idx==PR_ITEM_SUPER_LURE?15:idx==PR_ITEM_MAX_LURE?30:0;
 if(!max&&idx!=PR_ITEM_HEALING_CHARM)bad("class");
 int stack=integer(v[0],1,max?2:1),timer=integer(v[1],max?1:0,max);
 return(gc_item){.key=pr_item_keys[idx],.stack=stack,.arg=0,.battle_count=timer};}
static bool same_item(gc_item a,gc_item b){return !strcmp(a.key,b.key)&&a.stack==b.stack&&a.battle_count==b.battle_count&&!a.pokemon_id&&!a.virtual_stack&&!a.detached&&(a.arg==0||a.arg==-1);}
int main(int argc,char **argv){
 if(argc!=2||!(fp=fopen(argv[1],"r")))return 2;
 int version,n,used;if(!fgets(line,sizeof line,fp)||sscanf(line,"LURE_ENCOUNTER %d %d%n",&version,&n,&used)!=2||version!=1||n<120||n>100000||strcmp(line+used,"\n"))return 2;
 int roundtrips=0,doubles=0,gates=0,counts[4]={0},totaldraws=0;
 for(int ci=0;ci<n;ci++){
  char seed[25];double c[4],r[4],a[4],b[4],after[4],nr,mon[FIELDS],monAfter[FIELDS];char *s=record('C');int id;if(sscanf(s,"%d %24s%n",&id,seed,&used)!=2||id!=ci||strlen(seed)!=24)bad("case/seed");for(int j=0;j<24;j++)if(!isalnum((unsigned char)seed[j]))bad("seed");numbers(s+used,c,3);int wave=integer(c[0],2,1000),prior=integer(c[1],0,1);if(c[2]<0)bad("wallet");vec('R',r,4);validate_rng(r);
  vec('P',mon,FIELDS);validate_mon(mon);gc_item in[GC_MAX_ITEMS]={0},out[GC_MAX_ITEMS]={0};vec('I',&nr,1);int ni=integer(nr,1,4);for(int j=0;j<ni;j++)in[j]=item('H');vec('J',&nr,1);int no=integer(nr,1,4);for(int j=0;j<no;j++)out[j]=item('K');vec('A',a,4);integer(a[0],0,3);if(a[1]!=1&&a[1]!=2&&a[1]!=8&&a[1]!=32)bad("denominator");integer(a[2],0,1);integer(a[3],0,2);vec('B',b,4);validate_rng(b);
  double draw[2]={0};s=record('D');double nd;char *end;nd=strtod(s,&end);if(end==s)bad("draw count");int dn=integer(nd,0,2);if(dn!=0&&dn!=2)bad("draw shape");numbers(end,draw,dn);for(int j=0;j<dn;j++)if(draw[j]<0||draw[j]>=1)bad("draw value");vec('S',after,4);validate_rng(after);vec('T',monAfter,FIELDS);validate_mon(monAfter);vec('E',&nr,1);if(nr!=ci)bad("end");
  const char *why;gc_run_new(&run,GAME_MODES_ENDLESS,seed,777);gc_starter starter={.species=SPECIES_ID_BULBASAUR,.tera_type=-1};memset(starter.ivs,15,6);if(!gc_run_set_starters(&run,&starter,1,&why))return 1;gc_event ev;while(gc_run_next_event(&run,&ev)){};
  read_mon(&run.party[0],mon);run.party[0].b.on_field=true;run.wave=wave-1;run.n_items=ni;memcpy(run.items,in,sizeof in);for(int j=0;j<ni;j++)run.items[j].arg=j%2?-1:0;gc_items_to_battle(&run);run.money=c[2];run.run.rng=run.bt.rng=(pr_rng){.c=r[0],.s0=r[1],.s1=r[2],.s2=r[3]};run.bt.n_participants=0;
  if(!gc_run_can_save(&run))return 1;
  size_t size=gc_run_save(&run,saved,sizeof saved);if(!size||!gc_run_load(&loaded,saved,size,&why))return 1;roundtrips++;while(gc_run_next_event(&loaded,&ev))if(ev.kind!=GC_EV_WAVE&&ev.kind!=GC_EV_SUMMON&&ev.kind!=GC_EV_SAVE_POINT)return 1;
  bool same=true;gc_run *copies[]={&run,&loaded};for(int copy=0;copy<2;copy++){
   gc_run *x=copies[copy];x->bt.n_participants=0;if(prior){gc_rq_clear(x);x->bt.successful_run=false;x->bt.money_scattered=0;x->bt.enemy_faints=0;gc_rphase phase=gc_rp(RP_BATTLE_END,0,0,0);gc_phase_start(x,&phase);}
   if(x->n_items!=no||!rng_eq(x->run.rng,r)){same=false;if(ci<8)fprintf(stderr,"pre copy%d case%d stock%d/%d rng%d\n",copy,ci,x->n_items,no,rng_eq(x->run.rng,r));}for(int j=0;j<no&&j<x->n_items;j++)if(!same_item(x->items[j],out[j])){same=false;if(ci<8)fprintf(stderr,"stock copy%d case%d item%d %s timer%d expected%d arg%d\n",copy,ci,j,x->items[j].key,x->items[j].battle_count,out[j].battle_count,x->items[j].arg);}
   gc_mon party[6];memcpy(party,x->party,sizeof party);pr_rng br=x->bt.battle_rng;gc_item stock[GC_MAX_ITEMS];memcpy(stock,x->items,sizeof stock);gc_rq_clear(x);while(gc_run_next_event(x,&ev)){};
   observe_caller(x);
   if(seen.n_lures!=a[0]||seen.n_double_chance_abilities||seen.prev_wave!=wave-1||built.battle_type!=BATTLE_TYPE_WILD||built.double_battle!=a[2]||pr_run_get_double_battle_chance(wave,seen.n_lures,seen.n_double_chance_abilities)!=a[1]||!rng_eq(initial,b)||!rng_eq(x->run.rng,after)||ndraws!=a[3]||ndraws!=dn||x->money!=c[2]||memcmp(party,x->party,sizeof party)||memcmp(stock,x->items,sizeof stock)||!rng_same(br,x->bt.battle_rng))same=false;
   double actualMon[FIELDS];mon_vec(&x->party[0],actualMon);for(int j=0;j<FIELDS;j++)if(actualMon[j]!=monAfter[j])same=false;for(int j=0;j<dn;j++)if(draws[j]!=draw[j])same=false;if(gc_run_next_event(x,&ev))same=false;
  }
  if(!same){if(failures<8)fprintf(stderr,"FAIL encounter case%d records%d/%g denominator%g/%g dbl%d/%g draws%d/%g before%d after%d\n",ci,seen.n_lures,a[0],pr_run_get_double_battle_chance(wave,seen.n_lures,0),a[1],built.double_battle,a[2],ndraws,a[3],rng_eq(initial,b),rng_eq(loaded.run.rng,after));failures++;}
  counts[(int)a[0]]++;doubles+=a[2];gates+=wave%50==0;totaldraws+=dn;
 }
 double footer;vec('Z',&footer,1);if(footer!=n||fgetc(fp)!=EOF)bad("footer");printf("Lure encounter cases%d exact%d failures%d legalResumes%d counts%d/%d/%d/%d doubles%d gates%d rawDraws%d\n",n,n-failures,failures,roundtrips,counts[0],counts[1],counts[2],counts[3],doubles,gates,totaldraws);return failures?1:0;
}
