/* Additive real-upstream Mint replay through gc_port_apply_reward. All 54
 * modeled party fields, complete fixture inventory, dex entries and RNG compare.
 * Rotate storage order, replay both free and bought application boundaries. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#define FIELDS 54
static FILE *fp;
static int lineno, failures;
static gc_run run;
static gc_system sys, before_sys, expected_sys;
static char line[4096];
static void bad(const char *s) { fprintf(stderr,"invalid mint trace line %d: %s\n",lineno,s); exit(2); }
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
static gc_item item(char c,const double mons[6][FIELDS]) {
    char key[40];int n;char *s=record(c);if(sscanf(s,"%39s%n",key,&n)!=1)bad("item name");s+=n;
    double v[5];char *e;for(int j=0;j<5;j++){v[j]=strtod(s,&e);if(e==s||!isfinite(v[j]))bad("item number");s=e;}
    while(isspace((unsigned char)*s))s++;
    if(*s)bad("item fields");
    const char *stable=!strcmp(key,"LEFTOVERS")?"LEFTOVERS":!strcmp(key,"HEALING_CHARM")?"HEALING_CHARM":NULL;
    if(!stable)bad("fixture item class");
    int owner=integer(v[1],-1,5);
    if(v[3]!=0||v[4]!=1)bad("fixture lifecycle input");
    return (gc_item){stable,(int16_t)v[2],(int32_t)v[0],owner<0?0:(uint32_t)mons[owner][0]};
}
static void dex(char c,gc_system *s) {
    double v[13];vec(c,v,13);int id=integer(v[0],1,GC_MAX_SPECIES_ID);gc_dex_entry *d=&s->dex[id];
    d->seen_attr=v[1];d->caught_attr=v[2];d->nature_attr=v[3];d->seen_count=v[4];d->caught_count=v[5];d->hatched_count=v[6];
    for(int j=0;j<6;j++)d->ivs[j]=v[7+j];
}
int main(int argc,char **argv) {
    int mutate=argc==3?atoi(argv[2]):0;if(argc<2||argc>3){fprintf(stderr,"usage: mint_diff_test trace [mutation1..3]\n");return 2;}
    fp=fopen(argv[1],"r");if(!fp){perror(argv[1]);return 2;}double h[2];vec('V',h,2);if(h[0]!=2)bad("version");int n=integer(h[1],100,1000000);
    int accepted[25]={0},refused[25]={0},locked=0,unlocked=0,high=0;
    for(int ci=0;ci<n;ci++) {
        double c[4],rng[4],mons[6][FIELDS],out[6][FIELDS],nr;vec('C',c,4);
        if(c[0]!=ci)bad("case order");
        int count=integer(c[1],1,6),target=integer(c[2],0,count-1),nature=integer(c[3],0,24);
        vec('R',rng,4);for(int j=0;j<count;j++)vec('P',mons[j],FIELDS);
        vec('I',&nr,1);int ni=integer(nr,0,GC_MAX_ITEMS);gc_item input[GC_MAX_ITEMS],output[GC_MAX_ITEMS];
        for(int j=0;j<ni;j++)input[j]=item('H',mons);
        vec('D',&nr,1);int nd=integer(nr,1,30);memset(&before_sys,0,sizeof before_sys);for(int j=0;j<nd;j++)dex('B',&before_sys);
        double a[2];vec('A',a,2);int ok=integer(a[0],0,1),nq=integer(a[1],0,1);if(ok!=nq)bad("request count");
        for(int j=0;j<nq;j++){double q[2];vec('Q',q,2);if(q[0]!=mons[target][1]||q[1]!=nature)bad("unlock request");}
        for(int j=0;j<count;j++)vec('T',out[j],FIELDS);
        vec('J',&nr,1);int no=integer(nr,0,GC_MAX_ITEMS);for(int j=0;j<no;j++)output[j]=item('K',mons);
        double after_rng[4];vec('S',after_rng,4);char *empty=record('E');while(isspace((unsigned char)*empty))empty++;if(*empty)bad("end fields");
        vec('L',&nr,1);if(nr!=nd)bad("dex shape");memset(&expected_sys,0,sizeof expected_sys);for(int j=0;j<nd;j++)dex('U',&expected_sys);
        (ok?accepted:refused)[nature]++;high+=mons[target][5]==255;
        uint16_t root=gc_species_root_id(mons[target][1]);if(before_sys.dex[root].caught_attr)unlocked++;else locked++;
        for(int purchase=0;purchase<2;purchase++) {
            gc_run_new(&run,GAME_MODES_CLASSIC,"mint-diff",777);sys=before_sys;run.sys=&sys;run.n_party=count;run.n_items=ni;
            run.run.rng=(pr_rng){.c=rng[0],.s0=rng[1],.s1=rng[2],.s2=rng[3]};int rotation=ci%count;
            for(int j=0;j<count;j++){int storage=(j+rotation)%count;run.party_order[j]=storage;read_mon(&run.party[storage],mons[j]);run.bt.party[0][j]=&run.party[storage].b;}
            run.bt.n_party[0]=count;run.bt.field[0]=run.bt.party[0][0];memcpy(run.items,input,sizeof(gc_item)*ni);
            gc_offer offer={.key="MINT",.arg=nature,.target=GC_TARGET_POKEMON,.cost=purchase?300:-1};const char *why=NULL;
            bool got=gc_port_apply_reward(&run,&offer,run.party_order[target],-1,&why);bool same=got==ok&&run.n_items==no&&run.stubs[GC_STUB_REWARD_APPLY]==0;
            if(!got&&(!why||strcmp(why,"partyUiHandler:anyEffect")))same=false;
            if(mutate==1)run.party[run.party_order[target]].base_nature=(run.party[run.party_order[target]].base_nature+1)%25;
            if(mutate==2)run.party[run.party_order[target]].b.stats[STAT_ATK]++;
            if(mutate==3)sys.dex[(int)mons[target][1]].nature_attr^=1U<<(nature+1);
            for(int j=0;j<count;j++){double v[FIELDS];mon_vec(&run.party[run.party_order[j]],v);for(int k=0;k<FIELDS;k++)if(v[k]!=out[j][k]){if(failures<4)fprintf(stderr,"mint field case%d mon%d field%d got%.17g wanted%.17g\n",ci,j,k,v[k],out[j][k]);same=false;}}
            for(int j=0;j<no&&j<run.n_items;j++)if(strcmp(run.items[j].key,output[j].key)||run.items[j].arg!=output[j].arg||run.items[j].stack!=output[j].stack||run.items[j].pokemon_id!=output[j].pokemon_id)same=false;
            if(memcmp(&sys,&expected_sys,sizeof sys)||run.run.rng.c!=after_rng[0]||run.run.rng.s0!=after_rng[1]||run.run.rng.s1!=after_rng[2]||run.run.rng.s2!=after_rng[3])same=false;
            if(!same){if(failures<8)fprintf(stderr,"FAIL mint case%d purchase%d result%d/%d\n",ci,purchase,got,ok);failures++;}
        }
    }
    double z;vec('Z',&z,1);if(z!=n||fgetc(fp)!=EOF)bad("completion");fclose(fp);
    printf("game mint: %d cases, %d free/bought comparisons, %d mismatches; root locked%d unlocked%d level255=%d\n",n,2*n,failures,locked,unlocked,high);
    if(failures)return 1;
    for(int j=0;j<25;j++)if(!accepted[j]||!refused[j])bad("nature coverage");
    if(!locked||!unlocked||!high)bad("boundary coverage");
    return 0;
}
