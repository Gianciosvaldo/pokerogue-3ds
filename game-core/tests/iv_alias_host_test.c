#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run r,back;static gc_system sys;static unsigned char saved[GC_SAVE_MAX];static unsigned checks;static int case_no;
#define EQ(a,b) do{checks++;if((a)!=(b)){fprintf(stderr,"case%d line%d %s actual%.17g expected%.17g\n",case_no,__LINE__,#a,(double)(a),(double)(b));exit(1);}}while(0)
#include "fixtures/iv-alias.native.inc"
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
static void identity_guards(void){
 install(cases[0].before,GAME_MODES_CLASSIC);
 bool fusion=false;int e=gc_mon_get_evolution_target(&r,&r.party[0],&fusion);
 gc_mon_evolve_target(&r,&r.party[0],e,fusion);
 EQ(r.party[0].iv_alias,r.party[2].iv_alias);
 EQ(r.party[0].iv_alias!=r.party[0].used_tm_alias,true);
 gc_internal*in=gc_int(&r);in->training.mon=r.party[0];in->training.away=true;
 gc_party_remove(&r,0);
 /* The unrelated member's colliding ID must not create an array reference. */
 r.party[0].b.id=r.party[1].b.id;uint8_t unrelated[6];memcpy(unrelated,r.party[0].b.ivs,6);
 int32_t old_stats[6];memcpy(old_stats,r.party[1].b.stats,sizeof old_stats);
 in->training.mon.b.ivs[0]=23;in->training.mon.b.ivs[1]=23;gc_mon_publish_ivs(&r,&in->training.mon);
 EQ(r.party[1].b.ivs[0],23);EQ(memcmp(unrelated,r.party[0].b.ivs,6),0);
 EQ(memcmp(old_stats,r.party[1].b.stats,sizeof old_stats),0);
 /* Independent IV identity can be severed while the TM identity stays shared. */
 r.party[1].iv_alias=0;in->training.mon.b.ivs[0]=24;
 gc_mon_publish_ivs(&r,&in->training.mon);EQ(r.party[1].b.ivs[0],23);
 EQ(r.party[1].used_tm_alias,in->training.mon.used_tm_alias);
}
int main(void){for(case_no=0;case_no<(int)(sizeof cases/sizeof *cases);case_no++){
 install(cases[case_no].before,GAME_MODES_CLASSIC);compare(cases[case_no].before);
 bool fusion=false;int e=gc_mon_get_evolution_target(&r,&r.party[0],&fusion);EQ(e>=0,true);gc_mon_evolve_target(&r,&r.party[0],e,fusion);compare(cases[case_no].evolved);EQ(r.party[0].iv_alias!=0,true);EQ(r.party[0].iv_alias,r.party[2].iv_alias);
 if(cases[case_no].reload){r.in_battle=true;r.screen.kind=GC_SCREEN_COMMAND;memset(&gc_int(&r)->rq,0,sizeof gc_int(&r)->rq);gc_int(&r)->rq.current=gc_rp(RP_COMMAND,0,0,0);gc_int(&r)->rq.waiting=true;gc_int(&r)->at_save_point=true;size_t n=gc_run_save(&r,saved,sizeof saved);const char*why=NULL;EQ(n>0,true);EQ(gc_run_load(&back,saved,n,&why),true);EQ(back.load_dropped,0);r=back;r.sys=&sys;memset(&back,0,sizeof back);}
 compare(cases[case_no].ready);if(cases[case_no].reload){for(int i=0;i<r.n_party;i++)EQ(r.party[i].iv_alias,0);}gc_internal*in=gc_int(&r);in->training.mon=r.party[cases[case_no].index];in->training.kind=0;in->training.away=true;gc_party_remove(&r,cases[case_no].index);gc_me_prune_items(&r);gc_me_update_modifiers(&r);
 /* The battle-installation/option entry is outside this reward-owner proof.
  * Its captured global RNG is the reward callback's actual input. */
 r.run.rng=r.bt.rng=frames[cases[case_no].away].global;r.bt.battle_rng=frames[cases[case_no].away].battle;
 compare(cases[case_no].away);
 gc_training_return(&r);compare(cases[case_no].after);
 }int ncases=case_no;identity_guards();printf("IV owner: %d cases, %u checks\n",ncases,checks);return 0;}
