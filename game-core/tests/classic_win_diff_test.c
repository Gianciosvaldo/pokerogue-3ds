/* Independent actual GameOverPhase bookkeeping replay. Whole local profile and
 * all 54 modeled party fields compare; duplicate and terminal reload safety follow. */
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
static void bad(const char *s) { fprintf(stderr,"invalid Classic trace line %d: %s\n",lineno,s); exit(2); }
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
static gc_system expected,loaded_sys;static gc_run loaded;static unsigned char savebuf[GC_SAVE_MAX],sysbuf[524288];
static void profile_read(gc_system *s,char tag){double v[18];vec(tag,v,18);for(int j=0;j<18;j++)if(v[j]<0||v[j]!=floor(v[j])||v[j]>UINT32_MAX)bad("profile integer");s->stats.classic_wins=v[0];s->stats.ribbons_owned=v[1];for(int j=0;j<4;j++)s->voucher_counts[j]=v[2+j];s->classic_voucher_unlocked=integer(v[6],0,1);s->unlocks=(gc_unlocks){integer(v[7],0,1),integer(v[8],0,1),integer(v[9],0,1),integer(v[10],0,1)};for(int j=0;j<7;j++)s->achievement_flags[GC_ACHV_CLASSIC_VICTORY+j]=integer(v[11+j],0,1);}
static void entries(gc_system *s,char tag,char itemtag){double n;vec(tag,&n,1);int count=integer(n,1,GC_MAX_SPECIES_ID);bool used[GC_MAX_SPECIES_ID+1]={0};for(int j=0;j<count;j++){double v[3];vec(itemtag,v,3);int id=integer(v[0],1,GC_MAX_SPECIES_ID);if(used[id])bad("duplicate species");used[id]=true;s->starters[id].classic_win_count=integer(v[1],0,UINT16_MAX);if(v[2]<0||v[2]>9007199254740991.0||v[2]!=floor(v[2]))bad("ribbon bitfield");s->dex[id].ribbon_flags=v[2];}}
int main(int argc,char **argv){
 if(argc!=2||!(fp=fopen(argv[1],"r")))return 2;
 int version,n,used;if(!fgets(line,sizeof line,fp)||sscanf(line,"CLASSIC_WIN %d %d%n",&version,&n,&used)!=2||version!=1||n<120||n>100000||strcmp(line+used,"\n"))return 2;
 int wins=0,defeats=0,fused=0,first=0,plusTotal=0,premiumTotal=0,rootExtra=0,saves=0,repeats=0;
 for(int ci=0;ci<n;ci++){
  double c[4],r[4],mons[6][FIELDS],out[6][FIELDS],a[6],after[4];vec('C',c,4);if(c[0]!=ci)bad("case order");int count=integer(c[1],1,6),victory=integer(c[2],0,1),wave=integer(c[3],1,201);vec('R',r,4);
  gc_system_init_defaults(&sys);profile_read(&sys,'U');entries(&sys,'I','H');before_sys=sys;
  for(int j=0;j<count;j++){vec('P',mons[j],FIELDS);for(int k=0;k<FIELDS;k++)if(mons[j][k]!=floor(mons[j][k])||mons[j][k]<-1||mons[j][k]>UINT32_MAX)bad("party integer");if(mons[j][0]<1)bad("party id");integer(mons[j][25],0,4);integer(mons[j][5],1,10000);integer(mons[j][7],0,65535);const pr_species *species=gc_species(integer(mons[j][1],1,GC_MAX_SPECIES_ID));if(!species)bad("party species");integer(mons[j][2],0,species->nforms-1);if(mons[j][3]>=0){const pr_species *fusion=gc_species(integer(mons[j][3],1,GC_MAX_SPECIES_ID));if(!fusion)bad("fusion species");integer(mons[j][4],0,fusion->nforms-1);}}
  vec('A',a,6);for(int j=0;j<6;j++)integer(a[j],0,20);if(a[0]>1||a[4]!=1||a[5]!=1)bad("source completion");expected=sys;profile_read(&expected,'V');entries(&expected,'J','K');for(int j=0;j<count;j++)vec('T',out[j],FIELDS);vec('S',after,4);double end;vec('E',&end,1);if(end!=ci)bad("case end");
  gc_run_new(&run,GAME_MODES_CLASSIC,"classic-win",777);run.sys=&sys;run.n_party=count;run.wave=wave;run.money=10000.25+ci;run.n_enemy=1;run.enemy[0].b=(pr_pokemon){.id=99,.species=SPECIES_ID_MAGIKARP,.level=50,.ability_override=-1,.custom_passive=-1};run.in_battle=true;run.run.rng=(pr_rng){.c=r[0],.s0=r[1],.s1=r[2],.s2=r[3]};run.bt.rng=run.run.rng;int rotation=ci%count;
  for(int j=0;j<count;j++){int storage=(j+rotation)%count;run.party_order[j]=storage;read_mon(&run.party[storage],mons[j]);run.bt.party[0][j]=&run.party[storage].b;}run.bt.n_party[0]=count;run.bt.party[1][0]=&run.enemy[0].b;run.bt.n_party[1]=1;
  gc_rphase phase=gc_rp(RP_GAME_OVER,victory,0,0);bool wait=gc_phase_start(&run,&phase);
  bool same=!wait&&run.finished&&!run.in_battle&&run.terminal_result==(a[0]?2:1)&&run.screen.kind==(a[0]?GC_SCREEN_CLEAR:GC_SCREEN_GAME_OVER)&&!memcmp(&sys,&expected,sizeof sys)&&run.money==10000.25+ci;
  for(int j=0;j<count;j++){double values[FIELDS];mon_vec(&run.party[run.party_order[j]],values);for(int k=0;k<FIELDS;k++)if(values[k]!=out[j][k])same=false;}
  if(run.run.rng.c!=after[0]||run.run.rng.s0!=after[1]||run.run.rng.s1!=after[2]||run.run.rng.s2!=after[3])same=false;
  int unlockDelta=(sys.unlocks.endless_mode&&!before_sys.unlocks.endless_mode)+(sys.unlocks.mini_black_hole&&!before_sys.unlocks.mini_black_hole)+(sys.unlocks.spliced_endless_mode&&!before_sys.unlocks.spliced_endless_mode)+(sys.unlocks.eviolite&&!before_sys.unlocks.eviolite);
  if(unlockDelta!=a[1]||(double)sys.voucher_counts[1]-before_sys.voucher_counts[1]!=a[2]||(double)sys.voucher_counts[2]-before_sys.voucher_counts[2]!=a[3])same=false;
  if(ci<2&&!same){fprintf(stderr,"pre-save profileeq%d rng%g/%g/%g/%g vs%g/%g/%g/%g\n",!memcmp(&sys,&expected,sizeof sys),run.run.rng.c,run.run.rng.s0,run.run.rng.s1,run.run.rng.s2,after[0],after[1],after[2],after[3]);for(int j=0;j<count;j++){double v[FIELDS];mon_vec(&run.party[run.party_order[j]],v);for(int k=0;k<FIELDS;k++)if(v[k]!=out[j][k])fprintf(stderr,"party%d field%d %g/%g\n",j,k,v[k],out[j][k]);}}
  gc_system awarded=sys;gc_phase_start(&run,&phase);if(memcmp(&sys,&awarded,sizeof sys))same=false;repeats++;
  const char *why=NULL;size_t sn=gc_system_save(&sys,sysbuf,sizeof sysbuf),rn=gc_run_save(&run,savebuf,sizeof savebuf);if(!sn||!rn||!gc_system_load(&loaded_sys,sysbuf,sn,&why)||!gc_run_load(&loaded,savebuf,rn,&why))same=false;
  else{loaded.sys=&loaded_sys;if(!loaded.finished||loaded.in_battle||loaded.screen.kind!=run.screen.kind||loaded.terminal_result!=run.terminal_result||memcmp(&loaded_sys,&sys,sizeof sys))same=false;gc_phase_start(&loaded,&phase);if(memcmp(&loaded_sys,&sys,sizeof sys)){same=false;if(ci<2){for(size_t k=0;k<sizeof sys;k++)if(((unsigned char*)&loaded_sys)[k]!=((unsigned char*)&sys)[k]){fprintf(stderr,"loaded profile firstbyte %zu %u/%u\n",k,((unsigned char*)&loaded_sys)[k],((unsigned char*)&sys)[k]);break;}}}saves++;}
  wins+=a[0];defeats+=!a[0];bool fusion=false,extra=false;for(int j=0;j<count;j++){fusion|=mons[j][3]>=0;int species=mons[j][1];extra|=gc_species_root_id(species)!=gc_species_root_for_starter(species);}fused+=fusion;rootExtra+=extra;first+=sys.voucher_counts[3]>before_sys.voucher_counts[3];plusTotal+=a[2];premiumTotal+=a[3];
  if(!same){if(failures<8)fprintf(stderr,"FAIL Classic case%d clear%g sessions%u/%u ribbons%u/%u vouchers%u,%u,%u,%u / %u,%u,%u,%u marker%d save%zu/%zu (%s)\n",ci,a[0],sys.stats.classic_wins,expected.stats.classic_wins,sys.stats.ribbons_owned,expected.stats.ribbons_owned,sys.voucher_counts[0],sys.voucher_counts[1],sys.voucher_counts[2],sys.voucher_counts[3],expected.voucher_counts[0],expected.voucher_counts[1],expected.voucher_counts[2],expected.voucher_counts[3],run.terminal_result,rn,sn,why?why:"");failures++;}
 }
 double footer[8];vec('Z',footer,8);int counts[]={n,wins,defeats,fused,first,plusTotal,premiumTotal,rootExtra};for(int j=0;j<8;j++)if(footer[j]!=counts[j])bad("footer coverage");if(fgetc(fp)!=EOF)bad("trailing record");fclose(fp);
 printf("Classic actual caller: cases%d exact%d failed%d wins%d defeats%d fused%d golden%d plus%d premium%d distinctroots%d terminalreloads%d duplicatechecks%d\n",n,n-failures,failures,wins,defeats,fused,first,plusTotal,premiumTotal,rootExtra,saves,repeats);
 if(!wins||!defeats||!fused||!first||!plusTotal||!premiumTotal||!rootExtra||saves!=n||repeats!=n)bad("coverage");
 return failures?1:0;
}
