/* Independent actual Scene/selection acquisition replay. Compare all 54 modeled
 * party fields, inventory, wallet, RNG, profile and observed count/damage helpers. */
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
static void bad(const char *s) { fprintf(stderr,"invalid Multi caller trace line %d: %s\n",lineno,s); exit(2); }
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
    const char *stable=!strcmp(key,"LEFTOVERS")?"LEFTOVERS":!strcmp(key,"HEALING_CHARM")?"HEALING_CHARM":!strcmp(key,"MULTI_LENS")?"MULTI_LENS":!strcmp(key,"WIDE_LENS")?"WIDE_LENS":NULL;
    if(!stable)bad("fixture item class");
    int owner=integer(v[1],-1,count-1),stack=integer(v[0],1,31),arg=integer(v[2],0,5);
    if(v[3]!=0||v[4]!=0||(!strcmp(key,"MULTI_LENS")&&(arg!=0||stack>2)))bad("fixture inventory shape");
    return(gc_item){.key=stable,.arg=(int16_t)arg,.stack=stack,.pokemon_id=owner<0?0:(uint32_t)mons[owner][0]};
}
static void reward_screen(gc_offer offer) {
    gc_rqueue *q=&gc_int(&run)->rq;gc_rq_reset(q);q->current=gc_rp(RP_SELECT_MODIFIER,0,0,0);q->waiting=true;
    gc_rq_push(&run,gc_rp(RP_COMMAND,0,0,0));memset(&run.screen,0,sizeof run.screen);
    run.screen.kind=GC_SCREEN_REWARD;run.screen.n_options=run.screen.n_shop=1;
    run.screen.options[0]=run.screen.shop[0]=offer;
}
int main(int argc,char **argv){
 if(argc!=2||!(fp=fopen(argv[1],"r")))return 2;
 int version,n,used;if(!fgets(line,sizeof line,fp)||sscanf(line,"MULTI_CALLER %d %d%n",&version,&n,&used)!=2||version!=1||n<120||n>100000||strcmp(line+used,"\n"))return 2;
 int changes=0,full=0,blocked=0,wild=0,duplicate=0,fainted=0,paid=0,count_changes=0,paths[2]={0},damage_changes=0;
 for(int ci=0;ci<n;ci++){
  double c[7],r[4],mons[6][FIELDS],out[6][FIELDS],nr,a[12],damage[7],after[4];vec('C',c,7);if(c[0]!=ci)bad("case order");int count=integer(c[1],2,6),target=integer(c[2],0,count-1),path=integer(c[3],0,1),tier=integer(c[5],0,4);double cost=c[4],money=c[6];if(cost< -1||cost!=floor(cost)||cost>1000000||money<0)bad("money");vec('R',r,4);
  for(int j=0;j<count;j++){vec('P',mons[j],FIELDS);for(int k=0;k<FIELDS;k++)if(mons[j][k]!=floor(mons[j][k])||mons[j][k]<-1||mons[j][k]>UINT32_MAX)bad("party integer");if(mons[j][0]<1)bad("party id");}
  vec('I',&nr,1);int ni=integer(nr,0,PR_MAX_MODIFIERS);gc_item input[GC_MAX_ITEMS]={0},output[GC_MAX_ITEMS]={0};for(int j=0;j<ni;j++)input[j]=item('H',mons,count);
  vec('A',a,12);vec('D',damage,7);for(int j=0;j<5;j++)integer(a[j],0,1);for(int j=0;j<count;j++)vec('T',out[j],FIELDS);
  vec('J',&nr,1);int no=integer(nr,0,PR_MAX_MODIFIERS);for(int j=0;j<no;j++)output[j]=item('K',mons,count);vec('S',after,4);double endid;vec('E',&endid,1);if(endid!=ci)bad("end order");
  gc_run_new(&run,GAME_MODES_CLASSIC,"multi-caller",777);gc_system_init_defaults(&sys);before_sys=sys;run.sys=&sys;run.n_party=count;run.n_items=ni;run.run.rng=(pr_rng){.c=r[0],.s0=r[1],.s1=r[2],.s2=r[3]};int rotation=ci%count;
  for(int j=0;j<count;j++){int storage=(j+rotation)%count;run.party_order[j]=storage;read_mon(&run.party[storage],mons[j]);run.bt.party[0][j]=&run.party[storage].b;}
  run.bt.n_party[0]=count;run.bt.field[0]=run.bt.party[0][0];memcpy(run.items,input,ni*sizeof input[0]);for(int j=0;j<ni;j++)if((!strcmp(run.items[j].key,"MULTI_LENS")||!strcmp(run.items[j].key,"WIDE_LENS"))&&(ci+j)%2)run.items[j].arg=-1;
  run.money=money;gc_items_to_battle(&run);int beforeWild=0,hasDuplicate=0;
  for(int j=0;j<ni;j++)if(!strcmp(input[j].key,"MULTI_LENS")){beforeWild|=input[j].pokemon_id==0;for(int k=0;k<j;k++)if(!strcmp(input[k].key,"MULTI_LENS")&&input[k].pokemon_id==input[j].pokemon_id)hasDuplicate=1;}
  gc_item original[GC_MAX_ITEMS];memcpy(original,run.items,sizeof original);uint32_t balls[5];for(int j=0;j<5;j++)balls[j]=run.pokeballs[j];const char *why=NULL;
  int storage=run.party_order[target];gc_offer offer={.key="MULTI_LENS",.arg=ci%2?-1:0,.target=GC_TARGET_POKEMON,.tier=tier,.cost=cost,.is_purchase=cost>=0};bool got;
  if(path==0){int status=gc_multi_scene_apply(&run,&offer,storage,&why,false);got=status==GC_APPLY_DONE;if(got&&cost>=0)run.money-=cost;}
  else{reward_screen(offer);gc_choice choice={cost<0?GC_CH_REWARD:GC_CH_SHOP,0,storage,-1};got=gc_run_choose(&run,&choice,&why);}
  bool same=got==(!a[4]&&(cost<0||a[0]))&&run.money==a[5]&&run.n_items==no&&run.stubs[GC_STUB_REWARD_APPLY]==0;
  if(a[4]&&(!why||strcmp(why,"modifierType:ModifierType.PokemonHeldItemModifierType.extra.tooMany")))same=false;
  for(int j=0;j<5;j++)if((double)run.pokeballs[j]-balls[j]!=a[6+j])same=false;
  int messages=0;gc_event ev;while(gc_run_next_event(&run,&ev))if(ev.kind==GC_EV_MESSAGE&&!strcmp(ev.msg.key,"battle:itemStackFull"))messages++;if(messages!=a[3])same=false;
  for(int j=0;j<count;j++){double values[FIELDS];mon_vec(&run.party[run.party_order[j]],values);for(int k=0;k<FIELDS;k++)if(values[k]!=out[j][k])same=false;}
  bool usedOriginal[PR_MAX_MODIFIERS]={0};
  for(int j=0;j<no&&j<run.n_items;j++){const gc_item *actual=&run.items[j],*answer=&output[j];if(strcmp(actual->key,answer->key)||actual->stack!=answer->stack||actual->pokemon_id!=answer->pokemon_id||actual->virtual_stack||actual->detached||actual->battle_count)same=false;int expectedArg=0;bool found=false;for(int k=0;k<ni;k++)if(!usedOriginal[k]&&!strcmp(original[k].key,answer->key)&&original[k].pokemon_id==answer->pokemon_id){usedOriginal[k]=true;expectedArg=original[k].arg;found=true;break;}if(!found&&strcmp(actual->key,"MULTI_LENS"))same=false;if(actual->arg!=expectedArg)same=false;}
  if(a[4]&&(run.n_items!=ni||memcmp(run.items,original,ni*sizeof original[0])))same=false;
  int countHits=1;pr_apply_multi_hit_count_modifiers(&run.bt,&run.party[storage].b,MOVE_ID_TACKLE,&countHits);if(countHits!=a[11])same=false;count_changes+=countHits!=1;for(int ordinal=0;ordinal<7;ordinal++){run.party[storage].b.hit_count=countHits;run.party[storage].b.hits_left=countHits-ordinal;double mult=1;pr_apply_multi_hit_damage_modifiers(&run.bt,&run.party[storage].b,MOVE_ID_TACKLE,&mult);if(mult!=damage[ordinal])same=false;damage_changes+=mult!=1;}
  if(memcmp(&sys,&before_sys,sizeof sys)||run.run.rng.c!=after[0]||run.run.rng.s0!=after[1]||run.run.rng.s1!=after[2]||run.run.rng.s2!=after[3])same=false;
  bool changed=ni!=no;for(int j=0;j<ni&&j<no;j++)if(strcmp(input[j].key,output[j].key)||input[j].stack!=output[j].stack||input[j].pokemon_id!=output[j].pokemon_id)changed=true;
  changes+=changed;full+=a[3];blocked+=a[4];wild+=beforeWild;duplicate+=hasDuplicate;fainted+=mons[target][7]==0;paid+=cost>=0;paths[path]++;
  if(!same){if(failures<8)fprintf(stderr,"FAIL Multi case%d path%d got%d source result%g/filter%g wallet%g/%g inventory%d/%d hits%d/%g\n",ci,path,got,a[0],a[4],run.money,a[5],run.n_items,no,countHits,a[11]);failures++;}
 }
 double footer[7];vec('Z',footer,7);int stats[]={n,changes,full,blocked,wild,duplicate,fainted};for(int j=0;j<7;j++)if(footer[j]!=stats[j])bad("footer counts");if(fgetc(fp)!=EOF)bad("trailing record");fclose(fp);
 printf("Game Multi caller: cases%d exact%d failures%d, changes%d fullfallback%d blocked%d wild%d duplicate%d fainted%d paid%d paths%d/%d actualcountchanges%d damagechanges%d\n",n,n-failures,failures,changes,full,blocked,wild,duplicate,fainted,paid,paths[0],paths[1],count_changes,damage_changes);
 if(!paid||!paths[0]||!paths[1]||!count_changes||!damage_changes)bad("coverage");
 return failures?1:0;
}
