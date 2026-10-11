/* Actual BattleEnd expiry with an untouched legal save boundary and supplied trainer-member controls. */
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
static void bad(const char *s) { fprintf(stderr,"invalid Lure lapse trace line %d: %s\n",lineno,s); exit(2); }
static char *record(char c) { if(!fgets(line,sizeof line,fp))bad("EOF");lineno++;if(line[0]!=c||(line[1]!=' '&&line[1]!='\n')||!strchr(line,'\n'))bad("record order/length");return line+2; }
static void vec(char c,double *v,int n) {char *s=record(c),*e;for(int i=0;i<n;i++){v[i]=strtod(s,&e);if(e==s||!isfinite(v[i]))bad("number");s=e;}while(isspace((unsigned char)*s))s++;if(*s)bad("extra field");}
static int integer(double v,int lo,int hi) {if(v<lo||v>hi||v!=floor(v))bad("integer range");return (int)v;}
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


static gc_item item(char tag,const double mons[6][FIELDS],int count){
 char key[40];int used;char *s=record(tag);if(sscanf(s,"%39s%n",key,&used)!=1)bad("item key");s+=used;double v[4];char *end;for(int j=0;j<4;j++){v[j]=strtod(s,&end);if(end==s||!isfinite(v[j]))bad("item number");s=end;}while(isspace((unsigned char)*s))s++;if(*s)bad("item shape");
 int idx=gc_reward_item_index(key);if(idx<0)bad("unknown item");const char *stable=pr_item_keys[idx];int stack=integer(v[0],1,31),owner=integer(v[1],-1,count-1),arg=integer(v[2],0,6),timer=integer(v[3],0,30);
 int max=idx==PR_ITEM_LURE?10:idx==PR_ITEM_SUPER_LURE?15:idx==PR_ITEM_MAX_LURE?30:0;
 if(max&&(owner!=-1||arg||stack>2||timer<1||timer>max))bad("Lure shape");
 if(!max&&strcmp(key,"HEALING_CHARM")&&strcmp(key,"LEFTOVERS")&&strcmp(key,"DIRE_HIT"))bad("fixture class");
 if((!strcmp(key,"LEFTOVERS")&&(owner<0||arg||timer))||(!strcmp(key,"DIRE_HIT")&&(owner!=-1||arg||timer<1||timer>5||stack>2))||(!strcmp(key,"HEALING_CHARM")&&(owner!=-1||arg||timer)))bad("other shape");
 return(gc_item){.key=stable,.stack=stack,.pokemon_id=owner<0?0:(uint32_t)mons[owner][0],.arg=arg,.battle_count=timer};
}

static gc_run loaded;static unsigned char saved[GC_SAVE_MAX];
static bool same_item(gc_item a,gc_item b){return !strcmp(a.key,b.key)&&a.stack==b.stack&&a.pokemon_id==b.pokemon_id&&(a.arg==b.arg||((!strcmp(a.key,"LURE")||!strcmp(a.key,"SUPER_LURE")||!strcmp(a.key,"MAX_LURE"))&&(a.arg==-1||a.arg==0)))&&a.battle_count==b.battle_count&&!a.virtual_stack&&!a.detached;}
static void boundary(gc_run *r,int initial,int dup,int dv,int success){gc_rq_clear(r);for(int j=0;j<dup;j++)gc_rq_push(r,gc_rp(RP_BATTLE_END,dv,0,0));r->bt.successful_run=success;r->bt.money_scattered=0;r->bt.enemy_faints=0;gc_rphase phase=gc_rp(RP_BATTLE_END,initial,0,0);gc_phase_start(r,&phase);}
int main(int argc,char **argv){
 if(argc!=2||!(fp=fopen(argv[1],"r")))return 2;int version,n,used;if(!fgets(line,sizeof line,fp)||sscanf(line,"LURE_LAPSE %d %d%n",&version,&n,&used)!=2||version!=1||n<120||n>100000||strcmp(line+used,"\n"))return 2;
 int expired=0,roundtrips=0,controls=0,kinds[3]={0};
 for(int ci=0;ci<n;ci++){
  double c[7],r[4],m[6][FIELDS]={0},out[FIELDS],a[4],after[4],nr;vec('C',c,7);if(c[0]!=ci)bad("case order");int kind=integer(c[1],0,2),initial=integer(c[2],0,1),dup=integer(c[3],0,2),dv=integer(c[4],0,1),success=integer(c[5],0,1);if(c[6]<0)bad("wallet");vec('R',r,4);vec('P',m[0],FIELDS);validate_mon(m[0]);
  vec('I',&nr,1);int ni=integer(nr,0,PR_MAX_MODIFIERS);gc_item in[GC_MAX_ITEMS]={0},expected[GC_MAX_ITEMS]={0};for(int j=0;j<ni;j++)in[j]=item('H',m,1);
  vec('A',a,4);integer(a[0],0,PR_MAX_MODIFIERS);integer(a[1],0,PR_MAX_MODIFIERS);integer(a[2],0,1);integer(a[3],0,1);vec('T',out,FIELDS);validate_mon(out);vec('J',&nr,1);int no=integer(nr,0,PR_MAX_MODIFIERS);for(int j=0;j<no;j++)expected[j]=item('K',m,1);vec('S',after,4);double end;vec('E',&end,1);if(end!=ci)bad("end order");
  const char *why;gc_run_new(&run,GAME_MODES_CLASSIC,"lure-lapse",777);gc_starter starter={.species=SPECIES_ID_BULBASAUR,.tera_type=-1};memset(starter.ivs,15,6);if(!gc_run_set_starters(&run,&starter,1,&why))return 1;gc_event ev;while(gc_run_next_event(&run,&ev)){};
  gc_system_init_defaults(&sys);before_sys=sys;run.sys=&sys;run.n_party=1;run.party_order[0]=0;read_mon(&run.party[0],m[0]);run.party[0].b.on_field=true;run.bt.party[0][0]=run.bt.field[0]=&run.party[0].b;run.bt.n_party[0]=1;run.bt.n_participants=0;run.n_items=ni;memcpy(run.items,in,sizeof in);for(int j=0;j<ni;j++)if(!strcmp(in[j].key,"LURE")||!strcmp(in[j].key,"SUPER_LURE")||!strcmp(in[j].key,"MAX_LURE"))run.items[j].arg=j%2?-1:0;gc_items_to_battle(&run);run.money=c[6];run.run.rng=run.bt.rng=(pr_rng){.c=r[0],.s0=r[1],.s1=r[2],.s2=r[3]};run.bt.turn=2;run.bt.battle_score=0;
  /* Save at the untouched actual encounter COMMAND boundary, before any
   * supplied phase control. No availability flags or queues are restored. */
  run.n_enemy_items=1;run.enemy_items[0]=(gc_item){.key="LEFTOVERS",.stack=1,.pokemon_id=run.enemy[0].b.id};
  if(!gc_run_can_save(&run)){fprintf(stderr,"cannot save case%d flag%d current%d screen%d\n",ci,gc_int(&run)->at_save_point,gc_int(&run)->rq.current.type,run.screen.kind);return 1;}
  size_t size=gc_run_save(&run,saved,sizeof saved);if(!size||!gc_run_load(&loaded,saved,size,&why)){fprintf(stderr,"save/load failure case%d size%zu why%s\n",ci,size,why?why:"none");return 1;}roundtrips++;
  while(gc_run_next_event(&loaded,&ev))if(ev.kind!=GC_EV_WAVE&&ev.kind!=GC_EV_SUMMON&&ev.kind!=GC_EV_SAVE_POINT)return 1;
  bool same=true;gc_run *memberCopies[]={&run,&loaded};
  for(int copy=0;copy<2;copy++){
   gc_run *x=memberCopies[copy];x->bt.n_participants=0; /* same nonparticipating control input as source */ x->battle_type=BATTLE_TYPE_TRAINER;x->n_enemy=2;x->enemy[0].b.species=129;x->enemy[0].b.level=5;x->enemy[0].b.hp=0;x->enemy[1].b=x->enemy[0].b;x->enemy[1].b.id++;x->enemy[1].b.hp=x->enemy[1].b.stats[0];
   gc_item original[GC_MAX_ITEMS];memcpy(original,x->items,sizeof original);gc_rphase member=gc_rp(RP_VICTORY,0,0,0);gc_phase_start(x,&member);
   if(x->n_items!=ni||memcmp(x->items,original,sizeof original)||gc_rq_has(x,RP_BATTLE_END))same=false;
   x->battle_type=BATTLE_TYPE_WILD;x->n_enemy=1;x->enemy[0].b.hp=x->enemy[0].b.stats[0];
  }
  controls++;
  pr_rng bb=run.bt.battle_rng,lb=loaded.bt.battle_rng;boundary(&run,initial,dup,dv,success);boundary(&loaded,initial,dup,dv,success);
  gc_run *copies[]={&run,&loaded};for(int copy=0;copy<2;copy++){
   gc_run *x=copies[copy];if(x->n_items!=no||x->n_enemy_items||x->money!=c[6]||x->in_battle||gc_run_can_save(x)||gc_rq_has(x,RP_BATTLE_END)||x->stubs[GC_STUB_POST_BATTLE])same=false;
   for(int j=0;j<no&&j<x->n_items;j++)if(!same_item(x->items[j],expected[j]))same=false;
   double actual[FIELDS];mon_vec(&x->party[0],actual);for(int j=0;j<FIELDS;j++)if(actual[j]!=out[j]){same=false;if(ci<2)fprintf(stderr,"copy%d MON field%d actual%g source%g\n",copy,j,actual[j],out[j]);}
   gc_rqueue *q=&gc_int(x)->rq;int selects=0;for(int j=0;j<q->n_levels;j++)for(int k=0;k<q->n[j];k++){if(q->q[j][k].type==RP_SELECT_MODIFIER)selects++;else same=false;}if(selects!=a[3]||a[2]!=(initial||(dup&&dv)))same=false;
   while(gc_run_next_event(x,&ev)){same=false;if(ci<2)fprintf(stderr,"copy%d event%d\n",copy,ev.kind);}
   if(x->run.rng.c!=after[0]||x->run.rng.s0!=after[1]||x->run.rng.s1!=after[2]||x->run.rng.s2!=after[3])same=false;
  }
  if(memcmp(&sys,&before_sys,sizeof sys)||memcmp(&bb,&run.bt.battle_rng,sizeof bb)||memcmp(&lb,&loaded.bt.battle_rng,sizeof lb))same=false;
  int lapse=0,removed=0;for(int j=0;j<ni;j++){lapse+=in[j].battle_count>0;removed+=in[j].battle_count==1;}if(a[0]!=lapse||a[1]!=removed)bad("source callback accounting");expired+=removed;kinds[kind]++;
  if(!same){if(failures<8)fprintf(stderr,"FAIL Lure lapse case%d inventory%d/%d resumed%d queueExpected%g\n",ci,run.n_items,no,loaded.n_items,a[3]);failures++;}
 }
 double footer;vec('Z',&footer,1);if(footer!=n||fgetc(fp)!=EOF)bad("footer/trailing");fclose(fp);printf("Lure lapse cases%d exact%d failures%d removed%d legalResumes%d trainerNoTick%d kinds%d/%d/%d\n",n,n-failures,failures,expired,roundtrips,controls,kinds[0],kinds[1],kinds[2]);return failures?1:0;
}
