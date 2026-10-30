#include "../src/internal.h"
#include "constructor-finish.native.inc"
#include <stdio.h>
#include <stdlib.h>
static gc_run run;
static unsigned checks,c;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"constructor finish case%u line%d %s\n",c,__LINE__,#x);exit(1);}}while(0)
/* Observe the party state at the first external threshold RNG call. This
 * passthrough hook never supplies results or changes the real random stream. */
static bool observe_threshold,observed_threshold;
static const native_finish_case *expected_threshold;
int32_t __real_pr_rng_seed_int(pr_rng*,int32_t,int32_t);
int32_t __wrap_pr_rng_seed_int(pr_rng*r,int32_t range,int32_t min){
 if(observe_threshold&&!observed_threshold&&r==&run.run.rng){
  observed_threshold=true;for(int k=0;k<6;k++)CHECK(run.enemy[0].b.stats[k]==expected_threshold->final_stats[k]);CHECK(run.enemy[0].b.hp==expected_threshold->hp);
 }
 return __real_pr_rng_seed_int(r,range,min);
}
static bool same(pr_rng a,pr_rng b){return a.c==b.c&&a.s0==b.s0&&a.s1==b.s1&&a.s2==b.s2;}
static void gen_equal(const pr_enemy_gen*a,const pr_enemy_gen*b){CHECK(a->id==b->id);CHECK(a->form==b->form);CHECK(a->gender==b->gender);CHECK(a->nature==b->nature);CHECK(a->ability_index==b->ability_index);CHECK(a->fusion_species==b->fusion_species);if(a->fusion_species>0){CHECK(a->fusion_form==b->fusion_form);CHECK(a->fusion_gender==b->fusion_gender);CHECK(a->fusion_ability_index==b->fusion_ability_index);}CHECK(a->tera_type==b->tera_type);CHECK(a->boss_segments==b->boss_segments);CHECK(!a->rival_pending);CHECK(!a->shiny&&!a->variant&&!a->luck);for(int k=0;k<6;k++)CHECK(a->ivs[k]==b->ivs[k]);}
int main(void){for(c=0;c<1744;c++){
 const native_finish_case*n=&native_finish_cases[c];gc_run_new(&run,n->mode,"fused-boss-movesets",1);run.wave=1;run.biome=0;run.double_battle=n->dbl;run.battle_type=BATTLE_TYPE_WILD;run.bt.game_mode=run.run.mode;run.bt.wave_index=1;pr_seed_from_ascii(&run.run.wave_seed,"fused-boss-wave");run.run.rng=n->before;
 pr_enemy_gen_ctx ctx={.species=n->species,.level=n->level,.boss=true,.shiny_lock=true,.wave=1,.biome=0,.wave_cycle_offset=5,.battle_type=BATTLE_TYPE_WILD,.trainer_type=-1,.hidden_ability_charm_stacks=n->charms};pr_enemy_gen g;CHECK(!pr_run_enemy_pokemon_base(&run.run,&ctx,&g));CHECK(same(run.run.rng,n->gen_before));gen_equal(&g,&n->pre);
 gc_mon*m=&run.enemy[0];gc_mon_new_enemy(&run,m,n->species,n->level,&g,0);CHECK(m->b.ability==n->ability);pr_battle constructor={0};constructor.game_mode=run.run.mode;constructor.wave_index=1;pr_pokemon view=m->b;pr_update_passive(&constructor,&view);pr_calculate_stats(&constructor,&view);for(int k=0;k<6;k++)CHECK(view.stats[k]==n->stats[k]);CHECK(gc_enemy_moveset(&run,m,&g));CHECK(same(run.run.rng,n->gen_after));CHECK(m->b.n_moves==n->n);for(int j=0;j<n->n;j++){CHECK(m->b.moves[j]==n->moves[j][0]);CHECK(m->b.pp_used[j]==n->moves[j][1]);CHECK(m->b.pp_up[j]==n->moves[j][2]);CHECK(m->b.max_pp_override[j]==n->moves[j][3]);}
 CHECK(!pr_run_enemy_pokemon_finish(&run.run,&ctx,&g));if(!same(run.run.rng,n->after))fprintf(stderr,"finish RNG C%g,%g,%g,%g native%g,%g,%g,%g\n",run.run.rng.c,run.run.rng.s0,run.run.rng.s1,run.run.rng.s2,n->after.c,n->after.s0,n->after.s1,n->after.s2);CHECK(same(run.run.rng,n->after));gen_equal(&g,&n->final);gc_refresh_enemy(&run,m,n->species,n->level,&g,0);CHECK(same(run.run.rng,n->after));run.n_enemy=1;expected_threshold=n;observed_threshold=false;observe_threshold=true;gc_enemy_finish_party_items(&run,NULL,0);observe_threshold=false;CHECK(observed_threshold);CHECK(same(run.run.rng,n->pool_after));CHECK(m->b.hp==n->hp);CHECK(m->exp==n->exp);CHECK(m->b.friendship==n->final.friendship);CHECK(m->b.n_moves==n->n);for(int j=0;j<n->n;j++){CHECK(m->b.moves[j]==n->moves[j][0]);CHECK(m->b.pp_used[j]==n->moves[j][1]);CHECK(m->b.pp_up[j]==n->moves[j][2]);CHECK(m->b.max_pp_override[j]==n->moves[j][3]);}for(int k=0;k<6;k++){CHECK(m->b.stats[k]==n->final_stats[k]);CHECK(m->b.ivs[k]==n->final.ivs[k]);}CHECK(pr_has_passive(&run.bt,&m->b));CHECK(m->b.passive==n->passive);CHECK(!m->b.shiny&&m->variant==0&&m->luck==0);CHECK(m->b.ability==n->ability);CHECK(m->b.boss_segments==n->final.boss_segments);CHECK(run.n_enemy_items==0&&run.n_items==0);int8_t types[PR_MAX_TYPES];int nt=pr_get_types(&m->b,types,PR_TYPES_NO_TERA|PR_TYPES_BYPASS_SUMMON|PR_TYPES_IGNORE_THIRD);CHECK(nt==n->ntypes);for(int k=0;k<nt;k++)CHECK(types[k]==n->types[k]);CHECK(!run.run.unported);CHECK(!run.stubs[GC_STUB_ENEMY_MOVESET]);
 }printf("Constructor finish chain: %u cases %u checks passed\n",c,checks);return 0;}
