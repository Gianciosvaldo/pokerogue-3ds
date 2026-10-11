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
static void bad(const char *s) { fprintf(stderr,"invalid Lure acquisition trace line %d: %s\n",lineno,s); exit(2); }
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
static const char *locale_get(void *ctx, const char *file, int fallback, size_t *len)
{
    (void)ctx;
    static struct { char name[64]; char *text; size_t len; } cache[8];
    static int n_cache;
    const char *dir = getenv("GC_LOCALES");
    if (fallback) return NULL;
    if (!dir) { fputs("GC_LOCALES must name upstream's locales/en folder (tools/applydiff.sh sets it)\n", stderr); exit(2); }
    for (int i = 0; i < n_cache; i++)
        if (!strcmp(cache[i].name, file)) { *len = cache[i].len; return cache[i].text; }
    if (n_cache >= 8) return NULL;
    char path[512];
    snprintf(path, sizeof path, "%s/%s.json", dir, file);
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", path); exit(2); }
    fseek(f, 0, SEEK_END);
    const long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *text = malloc((size_t)sz + 1);
    const size_t n = fread(text, 1, (size_t)sz, f);
    text[n] = 0;
    fclose(f);
    snprintf(cache[n_cache].name, sizeof cache[n_cache].name, "%s", file);
    cache[n_cache].text = text;
    cache[n_cache].len = n;
    n_cache++;
    *len = n;
    return text;
}
static void text_record(char *out,size_t cap){char *s=record('M');size_t len=strlen(s);if(len<2||s[len-1]!='\n')bad("text newline");s[--len]=0;if(!strcmp(s,"-")){out[0]=0;return;}if(len%2||len/2>=cap)bad("text length");for(size_t j=0;j<len;j+=2){char a=s[j],b=s[j+1];if(!isxdigit((unsigned char)a)||!isxdigit((unsigned char)b))bad("text hex");char part[]={a,b,0};long value=strtol(part,NULL,16);if(!value)bad("text NUL");out[j/2]=(char)value;}out[len/2]=0;}

static bool same_item(gc_item a,gc_item b){return !strcmp(a.key,b.key)&&a.stack==b.stack&&a.pokemon_id==b.pokemon_id&&(a.arg==b.arg||((!strcmp(a.key,"LURE")||!strcmp(a.key,"SUPER_LURE")||!strcmp(a.key,"MAX_LURE"))&&(a.arg==-1||a.arg==0)))&&a.battle_count==b.battle_count&&!a.virtual_stack&&!a.detached;}
static void screen(gc_offer o,bool eligible){gc_rqueue *q=&gc_int(&run)->rq;gc_rq_reset(q);q->current=gc_rp(RP_SELECT_MODIFIER,0,0,0);q->waiting=true;gc_rq_push(&run,gc_rp(RP_COMMAND,0,0,0));memset(&run.screen,0,sizeof run.screen);run.screen.kind=GC_SCREEN_REWARD;run.screen.n_options=run.screen.n_shop=eligible?1:0;run.screen.options[0]=run.screen.shop[0]=o;}
int main(int argc,char **argv){
 if(argc!=2||!getenv("GC_LOCALES")||!(fp=fopen(argv[1],"r")))return 2;
 int version,n,used;if(!fgets(line,sizeof line,fp)||sscanf(line,"LURE_ACQUISITION %d %d%n",&version,&n,&used)!=2||version!=1||n<120||n>100000||strcmp(line+used,"\n"))return 2;
 int kinds[3]={0},paths[2]={0},full=0,filtered=0,refresh=0;
 for(int ci=0;ci<n;ci++){
  double c[8],r[4],f[3],mons[6][FIELDS],out[6][FIELDS],a[11],after[4],nr;
  vec('C',c,8);if(c[0]!=ci)bad("case order");int count=integer(c[1],1,6),kind=integer(c[2],0,2),path=integer(c[3],0,1),cost=integer(c[4],-1,10000),tier=integer(c[5],0,4),wave=integer(c[7],1,199);if(c[6]<0)bad("wallet");vec('R',r,4);vec('F',f,3);for(int j=0;j<3;j++)integer(f[j],0,1);
  for(int j=0;j<count;j++){vec('P',mons[j],FIELDS);validate_mon(mons[j]);for(int k=0;k<j;k++)if(mons[j][0]==mons[k][0])bad("duplicate ID");}
  vec('I',&nr,1);int ni=integer(nr,0,PR_MAX_MODIFIERS);gc_item in[GC_MAX_ITEMS]={0},expected[GC_MAX_ITEMS]={0};for(int j=0;j<ni;j++)in[j]=item('H',mons,count);
  vec('A',a,11);for(int j=0;j<5;j++)integer(a[j],0,1);char sourceText[1024];text_record(sourceText,sizeof sourceText);for(int j=0;j<count;j++){vec('T',out[j],FIELDS);validate_mon(out[j]);}
  vec('J',&nr,1);int no=integer(nr,0,PR_MAX_MODIFIERS);for(int j=0;j<no;j++)expected[j]=item('K',mons,count);vec('S',after,4);double end;vec('E',&end,1);if(end!=ci)bad("end order");
  gc_run_new(&run,GAME_MODES_CLASSIC,"lure-acquisition",777);gc_system_init_defaults(&sys);run.sys=&sys;before_sys=sys;run.wave=wave;run.n_party=count;run.n_items=ni;run.money=c[6];run.run.rng=(pr_rng){.c=r[0],.s0=r[1],.s1=r[2],.s2=r[3]};run.bt.n_party[0]=count;
  for(int j=0;j<count;j++){int storage=(j+ci%count)%count;run.party_order[j]=storage;read_mon(&run.party[storage],mons[j]);run.bt.party[0][j]=&run.party[storage].b;}
  memcpy(run.items,in,sizeof in);for(int j=0;j<ni;j++)if(!strcmp(in[j].key,"LURE")||!strcmp(in[j].key,"SUPER_LURE")||!strcmp(in[j].key,"MAX_LURE"))run.items[j].arg=j%2?-1:0;gc_items_to_battle(&run);
  pr_run shadow=run.run;pr_reward_ctx ctx=*gc_reward_ctx(&run);ctx.run=&shadow;pr_reward_thresholds th;pr_reward_regenerate_thresholds(&ctx,0,&th);
  int ids[]={PR_ITEM_LURE,PR_ITEM_SUPER_LURE,PR_ITEM_MAX_LURE};bool same=true;for(int k=0;k<3;k++){bool eligible=false;for(int t=0;t<5;t++)for(int j=0;j<th.n[t];j++)eligible|=pr_player_pool_entries[pr_player_pool[t].start+th.index[t][j]].item==ids[k];if(eligible!=(bool)f[k])same=false;}
  pr_rng gb=run.bt.rng,bb=run.bt.battle_rng;uint32_t balls[5];for(int j=0;j<5;j++)balls[j]=run.pokeballs[j];gc_rq_reset(&gc_int(&run)->rq);
  gc_offer offer={.key=pr_item_keys[ids[kind]],.arg=ci%2?-1:0,.target=GC_TARGET_NONE,.tier=tier,.cost=cost,.is_purchase=cost>=0};const char *why=NULL;bool got;
  if(path==0){int status=gc_lure_apply(&run,&offer,&why);got=status==GC_APPLY_DONE;if(got&&cost>=0)run.money-=cost;}
  else{screen(offer,f[kind]);gc_choice pick={cost<0?GC_CH_REWARD:GC_CH_SHOP,0,-1,-1};got=gc_run_choose(&run,&pick,&why);}
  bool blocked=path&&f[kind]==0;bool sourceGot=!blocked&&(cost<0||a[0]);if(got!=sourceGot||run.money!=a[5]||run.n_items!=no||run.stubs[GC_STUB_REWARD_APPLY])same=false;
  if(a[1]!=(!blocked&&cost>=0&&!a[0])||a[2]!=(!blocked&&cost<0))bad("source selection transport");
  for(int j=0;j<5;j++)if((double)run.pokeballs[j]-balls[j]!=a[6+j])same=false;
  for(int j=0;j<no&&j<run.n_items;j++)if(!same_item(run.items[j],expected[j]))same=false;
  for(int j=0;j<count;j++){double values[FIELDS];mon_vec(&run.party[run.party_order[j]],values);for(int k=0;k<FIELDS;k++)if(values[k]!=out[j][k])same=false;}
  int messages=0;gc_event ev;while(gc_run_next_event(&run,&ev)){if(ev.kind==GC_EV_MESSAGE&&!strcmp(ev.msg.key,"battle:itemStackFull")){messages++;char text[1024];pr_text_source textSource={locale_get,NULL,"en"};if(!pr_msg_format(&ev.msg,&textSource,text,sizeof text)||strcmp(text,sourceText))same=false;}else same=false;}if(messages!=a[3]||(!messages&&sourceText[0]))same=false;
  if(memcmp(&sys,&before_sys,sizeof sys)||memcmp(&gb,&run.bt.rng,sizeof gb)||memcmp(&bb,&run.bt.battle_rng,sizeof bb)||run.run.rng.c!=after[0]||run.run.rng.s0!=after[1]||run.run.rng.s1!=after[2]||run.run.rng.s2!=after[3])same=false;
  if(!same){if(failures<8)fprintf(stderr,"FAIL Lure acquisition case%d kind%d path%d filtered%d inventory%d/%d wallet%g/%g\n",ci,kind,path,blocked,run.n_items,no,run.money,a[5]);failures++;}
  kinds[kind]++;paths[path]++;full+=messages;filtered+=blocked;refresh+=a[4];
 }
 double footer;vec('Z',&footer,1);if(footer!=n||fgetc(fp)!=EOF)bad("footer/trailing");fclose(fp);
 printf("Lure acquisition cases%d exact%d failures%d kinds%d/%d/%d paths%d/%d full%d filtered%d refresh%d\n",n,n-failures,failures,kinds[0],kinds[1],kinds[2],paths[0],paths[1],full,filtered,refresh);return failures?1:0;
}
