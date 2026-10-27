/* Source-complete selected moves at the public fused-boss constructor owner. */
#include "../src/internal.h"
#include "fixtures/fusion-power.native.inc"
#include "fixtures/fused-boss-owner.native.inc"
#include <stdio.h>
#include <stdlib.h>
static gc_run run;
static unsigned cases,checks;
static bool inject_unported;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"fused boss owner case %u line %d: %s\n",cases,__LINE__,#x);exit(1);}}while(0)
void __real_pr_calculate_stats(pr_battle*,pr_pokemon*);
void __wrap_pr_calculate_stats(pr_battle*bt,pr_pokemon*p){__real_pr_calculate_stats(bt,p);if(inject_unported)bt->n_unported++;}
static bool same_rng(const pr_rng*a,const pr_rng*b){return a->c==b->c&&a->s0==b->s0&&a->s1==b->s1&&a->s2==b->s2;}
static void moves(const gc_mon*m,const native_fused_boss_result*n)
{
 CHECK(m->b.n_moves==n->n);
 for(int i=0;i<n->n;i++){CHECK(m->b.moves[i]==n->moves[i][0]);CHECK(m->b.pp_used[i]==n->moves[i][1]);CHECK(m->b.pp_up[i]==n->moves[i][2]);CHECK(m->b.max_pp_override[i]==n->moves[i][3]);}
}
static void one(const native_fusion_power_case*n,const native_fused_boss_result*out)
{
 gc_run_new(&run,n->mode,"native-fused-boss",1);run.wave=1;run.battle_type=BATTLE_TYPE_WILD;run.double_battle=n->dbl;run.bt.weather=n->weather;run.bt.terrain=n->terrain;
 gc_mon*m=&run.enemy[0];gc_mon_new_enemy(&run,m,n->species,n->level,&n->g,0);run.run.rng=n->rng;
 CHECK(m->b.ability==n->ability);CHECK(m->b.is_boss);CHECK(m->b.fusion_species==n->g.fusion_species);
 run.bt.n_mods[1]=1;run.bt.mods[1][0]=(pr_modifier){.cls=PR_MOD_BASE_STAT,.pokemon_id=m->b.id,.stat=STAT_ATK,.stack_count=10};
 const gc_mon before=*m;
 CHECK(gc_enemy_moveset(&run,m,&n->g));moves(m,out);CHECK(same_rng(&run.run.rng,&out->after));CHECK(run.stubs[GC_STUB_ENEMY_MOVESET]==0);
 gc_refresh_enemy(&run,m,n->species,n->level,&n->g,0);moves(m,out);CHECK(same_rng(&run.run.rng,&out->after));
 *m=before;run.run.rng=n->rng;CHECK(gc_generate_moveset(&run,&run.run.rng,m));moves(m,out);CHECK(same_rng(&run.run.rng,&out->after));cases++;
}
static void guards(void)
{
 const native_fusion_power_case*n=&native_fusion_power_cases[0];
 for(int k=0;k<14;k++){
  gc_run_new(&run,n->mode,"unsupported-fused-boss",1);run.wave=1;run.battle_type=BATTLE_TYPE_WILD;
  gc_mon m;pr_enemy_gen g=n->g;gc_mon_new_enemy(&run,&m,n->species,n->level,&g,0);run.run.rng=n->rng;
  switch(k){
  case 0:m.b.has_trainer=true;break;case 1:m.trainer_slot=TRAINER_SLOT_TRAINER;break;case 2:run.me_battle=true;break;case 3:run.battle_type=BATTLE_TYPE_TRAINER;break;case 4:run.run.has_challenges=true;break;case 5:run.run.mode=GAME_MODES_DAILY;break;
  case 6:m.b.is_boss=false;break;case 7:m.b.passive_flag=true;break;case 8:g.fusion_species=0;break;case 9:g.fusion_species=SPECIES_ID_DITTO;break;case 10:g.fusion_form++;break;case 11:g.fusion_ability_index=(g.fusion_ability_index+1)%3;break;case 12:g.fusion_gender=g.fusion_gender==0?1:0;break;case 13:m.b.species=SPECIES_ID_KELDEO;break;
  }
  gc_mon before=m;pr_rng rng=run.run.rng;CHECK(!gc_enemy_moveset(&run,&m,&g));CHECK(!memcmp(&m,&before,sizeof m));CHECK(same_rng(&run.run.rng,&rng));
  if(k==6||k==7){CHECK(!gc_generate_moveset(&run,&run.run.rng,&m));CHECK(!memcmp(&m,&before,sizeof m));CHECK(same_rng(&run.run.rng,&rng));}
 }
 gc_run_new(&run,n->mode,"atomic-fused-boss",1);run.wave=1;run.battle_type=BATTLE_TYPE_WILD;gc_mon m;gc_mon_new_enemy(&run,&m,n->species,n->level,&n->g,0);run.run.rng=n->rng;
 gc_mon before=m;inject_unported=true;CHECK(!gc_enemy_moveset(&run,&m,&n->g));CHECK(!memcmp(&m,&before,sizeof m));CHECK(same_rng(&run.run.rng,&n->rng));inject_unported=false;
 CHECK(gc_enemy_moveset(&run,&m,&n->g));moves(&m,&native_fused_boss_results[0]);CHECK(same_rng(&run.run.rng,&native_fused_boss_results[0].after));
}
int main(void){CHECK(sizeof native_fused_boss_results/sizeof *native_fused_boss_results==812);for(unsigned i=0;i<812;i++)one(&native_fusion_power_cases[i],&native_fused_boss_results[i]);guards();printf("native fused boss owner: %u cases, %u checks\n",cases,checks);return 0;}
