#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run r,back;static gc_system sys;static unsigned char saved[GC_SAVE_MAX];static unsigned checks;static int case_no;
#define EQ(a,b) do{checks++;if((a)!=(b)){fprintf(stderr,"case%d line%d %s actual%.17g expected%.17g\n",case_no,__LINE__,#a,(double)(a),(double)(b));exit(1);}}while(0)
#include "fixtures/shedinja.native.inc"
static void rng_eq(pr_rng a,pr_rng b){EQ(a.c,b.c);EQ(a.s0,b.s0);EQ(a.s1,b.s1);EQ(a.s2,b.s2);}
static void compare(unsigned n){const frame*f=&frames[n];EQ(r.n_party,f->n);EQ(r.bt.n_unported,0);EQ(r.run.unported,0);for(int i=0;i<GC_STUB_COUNT_;i++)EQ(r.stubs[i],0);
 for(int i=0;i<f->n;i++)mon_io(&r.party[i],f->mons[i],false);
 profile_io(&sys,f->profile,false);
 EQ(r.n_items,f->nstock);for(int i=0;i<f->nstock;i++){EQ(strcmp(r.items[i].key,f->stock[i].key),0);EQ(r.items[i].pokemon_id,f->stock[i].owner);EQ(r.items[i].stack,f->stock[i].stack);EQ(r.items[i].virtual_stack,f->stock[i].virtual_stack);EQ(r.items[i].non_transferable,f->stock[i].locked);EQ(r.items[i].eviolite_base,f->stock[i].eviolite_base);}
 for(int i=0;i<5;i++){EQ(r.pokeballs[i],f->balls[i]);}rng_eq(r.run.rng,f->global);rng_eq(r.bt.rng,f->global);rng_eq(r.bt.battle_rng,f->battle);
}
static void install(unsigned n,int mode){const frame*f=&frames[n];gc_run_new(&r,GAME_MODES_CLASSIC,"shedinja-proof",777);gc_starter starter={.species=SPECIES_ID_MEW,.tera_type=-1,.moves={MOVE_ID_SPLASH}};const char*why=NULL;EQ(gc_run_set_starters(&r,&starter,1,&why),true);gc_event event;while(gc_run_next_event(&r,&event)){}
 memset(&sys,0,sizeof sys);r.sys=&sys;profile_io(&sys,f->profile,true);
 r.wave=env[0];r.biome=env[1];r.wave_cycle_offset=env[2];r.bt.weather=env[3];r.bt.terrain=env[4];r.n_party=f->n;r.bt.challenge_flags_known=true;r.run.mode=r.bt.game_mode=mode;pr_seed_from_ascii(&r.run.seed,seed);pr_seed_from_ascii(&r.run.wave_seed,wave_seed);
 for(int i=0;i<f->n;i++){gc_mon*m=&r.party[i];memset(m,0,sizeof *m);pr_reset_summon_data(&m->b);pr_reset_turn_data(&m->b);mon_io(m,f->mons[i],true);pr_update_passive(&r.bt,&m->b);r.party_order[i]=i;r.bt.party[0][i]=&m->b;}
 r.bt.n_party[0]=r.n_party;r.n_items=f->nstock;for(int i=0;i<f->nstock;i++)r.items[i]=(gc_item){.key=f->stock[i].key,.arg=-1,.pokemon_id=f->stock[i].owner,.stack=f->stock[i].stack,.virtual_stack=f->stock[i].virtual_stack,.non_transferable=f->stock[i].locked,.eviolite_base=f->stock[i].eviolite_base};for(int i=0;i<5;i++)r.pokeballs[i]=f->balls[i];
 gc_items_to_battle(&r);r.run.rng=r.bt.rng=f->global;r.bt.battle_rng=f->battle;r.bt.rng_live=r.bt.battle_rng_live=true;
}
static void teach(int index,int move){gc_rphase phase=gc_rp(RP_LEARN_MOVE,index,move,1);bool ok=false;const char*why=NULL;EQ(gc_phase_choose(&r,&phase,&(gc_choice){GC_CH_FORGET,0,0,0},&why,&ok),true);EQ(ok,true);}
static void alias_guards(void){
 int c=0;while(!cases[c].shared)c++;
 install(cases[c].before,cases[c].mode);
 bool fusion=false;int e=gc_mon_get_evolution_target(&r,&r.party[0],&fusion);
 gc_mon_evolve_target(&r,&r.party[0],e,fusion);
 EQ(r.n_party,2);gc_internal*in=gc_int(&r);
 /* Training Session retains the removed object's source array. */
 in->training.mon=r.party[0];in->training.away=true;gc_party_remove(&r,0);
 unsigned previous=in->training.mon.n_used_tms;
 EQ(gc_mon_remember_tm(&r,&r.party[0],MOVE_ID_SWORDS_DANCE),true);
 EQ(in->training.mon.n_used_tms,previous+1);
 EQ(memcmp(in->training.mon.used_tms,r.party[0].used_tms,sizeof r.party[0].used_tms),0);
 /* A replacement in the old index with the same Pokemon ID is independent. */
 gc_mon replacement=r.party[0];replacement.used_tm_alias=0;replacement.n_used_tms=0;
 r.party[1]=replacement;r.n_party=2;
 EQ(gc_mon_remember_tm(&r,&r.party[1],MOVE_ID_AGILITY),true);
 EQ(r.party[1].n_used_tms,1);EQ(r.party[0].n_used_tms,previous+1);
 /* A later sibling reuses the live array identity, without allocating again. */
 uint32_t counter=in->next_tm_alias;
 EQ(gc_mon_share_tms(&r,&r.party[0],&r.party[2]),true);r.n_party=3;
 EQ(in->next_tm_alias,counter);
 EQ(gc_mon_remember_tm(&r,&r.party[2],MOVE_ID_DOUBLE_TEAM),true);
 EQ(r.party[0].n_used_tms,previous+2);EQ(in->training.mon.n_used_tms,previous+2);
 EQ(r.party[1].n_used_tms,1);
 /* Runtime identity exhaustion must remain a visible engine boundary. */
 in->next_tm_alias=UINT32_MAX;r.party[1].used_tm_alias=0;
 EQ(gc_mon_share_tms(&r,&r.party[1],&replacement),false);
 EQ(r.party[1].used_tm_alias,0);EQ(r.bt.n_unported,1);EQ(r.bt.unported[0],PR_FEAT_ENGINE_LIMIT);
}
int main(void){for(case_no=0;case_no<(int)(sizeof cases/sizeof *cases);case_no++){
 install(cases[case_no].before,cases[case_no].mode);r.wave_cycle_offset=cases[case_no].offset;compare(cases[case_no].before);
 bool fusion=false;int evo=gc_mon_get_evolution_target(&r,&r.party[0],&fusion);EQ(evo>=0,true);EQ(fusion,cases[case_no].fusion);EQ(gc_evo_data[evo].species,cases[case_no].species);
 gc_rphase phase=gc_rp(RP_EVOLUTION,0,evo,0);phase.d=fusion;bool ok=false;const char*why=NULL;gc_phase_start(&r,&phase);EQ(gc_phase_choose(&r,&phase,&(gc_choice){GC_CH_EVOLVE,0,0,0},&why,&ok),true);EQ(ok,true);compare(cases[case_no].after);
 if(cases[case_no].shared){EQ(r.party[0].used_tm_alias!=0,true);EQ(r.party[0].used_tm_alias,r.party[r.n_party-1].used_tm_alias);}
 for(int i=0;i<cases[case_no].nsteps;i++){teach(cases[case_no].steps[i].index,cases[case_no].steps[i].move);compare(cases[case_no].steps[i].after);}
 for(int k=0;k<cases[case_no].nextra;k++){
  int a=cases[case_no].extra_offset+k,index=extras[a].index;
  switch(extras[a].kind){
   case 1:{bool f=false;int e=gc_mon_get_evolution_target(&r,&r.party[index],&f);EQ(e>=0,true);EQ(f,true);EQ(gc_evo_data[e].species,extras[a].arg);gc_mon_evolve_target(&r,&r.party[index],e,f);break;}
   case 2:teach(index,extras[a].arg);break;
   case 3:gc_party_remove(&r,index);gc_me_prune_items(&r);gc_me_update_modifiers(&r);break;
   case 4:{const frame*f=&frames[extras[a].after];gc_mon*m=&r.party[r.n_party];memset(m,0,sizeof *m);pr_reset_summon_data(&m->b);pr_reset_turn_data(&m->b);mon_io(m,f->mons[f->n-1],true);pr_update_passive(&r.bt,&m->b);r.party_order[r.n_party]=r.n_party;r.n_party++;r.bt.n_party[0]=r.n_party;for(int j=0;j<r.n_party;j++)r.bt.party[0][j]=&r.party[j].b;gc_me_update_modifiers(&r);break;}
   default:EQ(0,1);
  }
  compare(extras[a].after);
 }
 r.in_battle=true;r.screen.kind=GC_SCREEN_COMMAND;memset(&gc_int(&r)->rq,0,sizeof gc_int(&r)->rq);gc_int(&r)->rq.current=gc_rp(RP_COMMAND,0,0,0);gc_int(&r)->rq.waiting=true;gc_int(&r)->at_save_point=true;
 size_t n=gc_run_save(&r,saved,sizeof saved);EQ(n>0,true);if(!gc_run_load(&back,saved,n,&why)){fprintf(stderr,"load:%s\n",why);return 1;}EQ(back.load_dropped,0);r=back;r.sys=&sys;memset(&back,0,sizeof back);compare(cases[case_no].restored);EQ(r.party[0].used_tm_alias,0);
 int i=cases[case_no].nsteps;teach(cases[case_no].steps[i].index,cases[case_no].steps[i].move);compare(cases[case_no].steps[i].after);
 }int ncases=case_no;alias_guards();printf("Shedinja native owner: %d cases, %u checks\n",ncases,checks);return 0;}
