/* Additive real-upstream money replay through gc_port_apply_reward. All 54
 * modeled party fields, complete fixture inventory and RNG compare; profile stays
 * unchanged. Rotate storage and replay direct plus public free/shop boundaries. */
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
static void bad(const char *s) { fprintf(stderr,"invalid money trace line %d: %s\n",lineno,s); exit(2); }
static char *record(char c) { if(!fgets(line,sizeof line,fp))bad("EOF");lineno++;if(line[0]!=c||(line[1]!=' '&&line[1]!='\n')||!strchr(line,'\n'))bad("record order/length");return line+2; }
static void vec(char c,double *v,int n) {char *s=record(c),*e;for(int i=0;i<n;i++){v[i]=strtod(s,&e);if(e==s||!isfinite(v[i]))bad("number");s=e;}while(isspace((unsigned char)*s))s++;if(*s)bad("extra field");}
static int integer(double v,int lo,int hi) {if(v<lo||v>hi||v!=floor(v))bad("integer range");return (int)v;}
static void read_mon(gc_mon *m,const double *v) {
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
static gc_item item(char c,const double mons[6][FIELDS],int count) {
    char key[40];int n;char *s=record(c);if(sscanf(s,"%39s%n",key,&n)!=1)bad("item name");s+=n;
    double v[5];char *e;for(int j=0;j<5;j++){v[j]=strtod(s,&e);if(e==s||!isfinite(v[j]))bad("item number");s=e;}
    while(isspace((unsigned char)*s))s++;
    if(*s)bad("item fields");
    const char *stable=!strcmp(key,"LEFTOVERS")?"LEFTOVERS":!strcmp(key,"HEALING_CHARM")?"HEALING_CHARM":!strcmp(key,"AMULET_COIN")?"AMULET_COIN":NULL;
    if(!stable)bad("fixture item class");
    int owner=integer(v[1],-1,count-1),stack=integer(v[0],1,31),arg=integer(v[2],0,5);
    if(v[3]!=0||v[4]!=1||(!strcmp(key,"AMULET_COIN")&&(owner!=-1||arg!=0||stack>5)))bad("fixture inventory shape");
    return(gc_item){stable,(int16_t)arg,stack,owner<0?0:(uint32_t)mons[owner][0]};
}
static void reward_screen(gc_offer offer) {
    gc_rqueue *q=&gc_int(&run)->rq;gc_rq_reset(q);q->current=gc_rp(RP_SELECT_MODIFIER,0,0,0);q->waiting=true;
    gc_rq_push(&run,gc_rp(RP_COMMAND,0,0,0));memset(&run.screen,0,sizeof run.screen);
    run.screen.kind=GC_SCREEN_REWARD;run.screen.n_options=run.screen.n_shop=1;
    run.screen.options[0]=run.screen.shop[0]=offer;
}
int main(int argc,char **argv) {
 if(argc!=2)return 2;
 fp=fopen(argv[1],"r");if(!fp)return 2;
 double h[2];vec('V',h,2);if(h[0]!=1)bad("version");int n=integer(h[1],100,1000000),families[3]={0},paid=0,free=0,capped=0;
 const char *names[]={"NUGGET","BIG_NUGGET","RELIC_GOLD"};
 for(int ci=0;ci<n;ci++) {
  double c[6],rng[4],mons[6][FIELDS],out[6][FIELDS],nr;vec('C',c,6);if(c[0]!=ci)bad("case order");
  int count=integer(c[1],1,6),which=integer(c[2],0,2),wave=integer(c[3],1,10000);double cost=c[4],money=c[5];
  if(cost < -1 || money<0 || money>9007199254740991.0)bad("cost/money");
  vec('R',rng,4);for(int j=0;j<count;j++)vec('P',mons[j],FIELDS);
  vec('I',&nr,1);int ni=integer(nr,0,GC_MAX_ITEMS);gc_item input[GC_MAX_ITEMS],output[GC_MAX_ITEMS];
  for(int j=0;j<ni;j++)input[j]=item('H',mons,count);
  double a[6];vec('A',a,6);if(a[0]!=1||a[1]<0||a[2]<0||a[3]!=0||a[4]!=(cost<0))bad("phase result");
  if(a[5] != fmin(money+a[1],9007199254740991.0)-(cost>=0?cost:0)||a[2]!=fmin(money+a[1],9007199254740991.0)-money)bad("nominal/credited money");
  for(int j=0;j<count;j++)vec('T',out[j],FIELDS);
  vec('J',&nr,1);int no=integer(nr,0,GC_MAX_ITEMS);for(int j=0;j<no;j++)output[j]=item('K',mons,count);
  double after_rng[4];vec('S',after_rng,4);char *empty=record('E');while(isspace((unsigned char)*empty))empty++;if(*empty)bad("end fields");
  families[which]++;paid+=cost>=0;free+=cost<0;capped+=money+a[2]==9007199254740991.0;
  for(int path=0;path<2;path++) {
   gc_run_new(&run,GAME_MODES_CLASSIC,"money-diff",777);gc_system_init_defaults(&sys);before_sys=sys;run.sys=&sys;run.n_party=count;run.n_items=ni;run.wave=wave;
   run.run.rng=(pr_rng){.c=rng[0],.s0=rng[1],.s1=rng[2],.s2=rng[3]};int rotation=ci%count;
   for(int j=0;j<count;j++){int storage=(j+rotation)%count;run.party_order[j]=storage;read_mon(&run.party[storage],mons[j]);run.bt.party[0][j]=&run.party[storage].b;
    double initial[FIELDS];mon_vec(&run.party[storage],initial);for(int k=0;k<FIELDS;k++)if(initial[k]!=mons[j][k])bad("input roundtrip");}
   run.bt.n_party[0]=count;run.bt.field[0]=run.bt.party[0][0];memcpy(run.items,input,sizeof(gc_item)*ni);run.money=money;
   gc_offer offer={.key=names[which],.arg=0,.target=GC_TARGET_NONE,.cost=cost,.is_purchase=cost>=0};const char *why=NULL;
   bool got;
   if(path==0)got=gc_port_apply_reward(&run,&offer,-1,-1,&why);
   else {reward_screen(offer);gc_choice pick={cost<0?GC_CH_REWARD:GC_CH_SHOP,0,-1,-1};got=gc_run_choose(&run,&pick,&why);}
   double balance=path==0?money+a[2]:a[5];
   bool same=got&&run.n_items==no&&run.stubs[GC_STUB_REWARD_APPLY]==0&&run.money==balance;
   if(path==1&&run.screen.kind!=(cost<0?GC_SCREEN_COMMAND:GC_SCREEN_REWARD))same=false;
   for(int j=0;j<count;j++){double v[FIELDS];mon_vec(&run.party[run.party_order[j]],v);for(int k=0;k<FIELDS;k++)if(v[k]!=out[j][k])same=false;}
   for(int j=0;j<no&&j<run.n_items;j++)if(strcmp(run.items[j].key,output[j].key)||run.items[j].arg!=output[j].arg||run.items[j].stack!=output[j].stack||run.items[j].pokemon_id!=output[j].pokemon_id)same=false;
   if(memcmp(&sys,&before_sys,sizeof sys)||run.run.rng.c!=after_rng[0]||run.run.rng.s0!=after_rng[1]||run.run.rng.s1!=after_rng[2]||run.run.rng.s2!=after_rng[3])same=false;
   if(!same){if(failures<8)fprintf(stderr,"FAIL money case%d path%d result%d balance%.17g/%.17g\n",ci,path,got,run.money,balance);failures++;}
  }
 }
 double z;vec('Z',&z,1);if(z!=n||fgetc(fp)!=EOF)bad("completion");fclose(fp);
 printf("game money: %d cases, %d direct/public comparisons, %d mismatches; family=%d/%d/%d paid=%d free=%d capped=%d\n",n,2*n,failures,families[0],families[1],families[2],paid,free,capped);
 if(failures)return 1;
 for(int j=0;j<3;j++)if(!families[j])bad("family coverage");
 if(!paid||!free||!capped)bad("boundary coverage");
 return 0;
}
