/* Native configuration/constructor owners plus played C trainer entry/rewards.
 * Generated species/movesets/stat/finish parity is deliberately not claimed. */
#include "../src/internal.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "challengers.native.inc"
static gc_run run;
static unsigned checks,index;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"Challengers case%u line%d: %s\n",index,__LINE__,#x);exit(1);}}while(0)
bool __real_gc_gen_trainer_party(gc_run*,const pr_party_template*,int,int,const int*,int);
bool __wrap_gc_gen_trainer_party(gc_run*r,const pr_party_template*t,int type,int variant,const int*levels,int n){
 const challenger_case*c=&challenger_native[index];CHECK(t->size==c->tpl_size&&t->strength==c->tpl_strength&&n==c->n_levels);
 for(int i=0;i<n;i++){CHECK(levels[i]==c->levels[i]);CHECK(t->strengths[i]==c->members[i][0]);CHECK(((t->member[i]>>2)&3)==c->members[i][1]);CHECK((t->member[i]&1)==c->members[i][2]);CHECK(!!(t->member[i]&2)==c->members[i][3]);}
 return __real_gc_gen_trainer_party(r,t,type,variant,levels,n);
}
int main(void){
 for(index=0;index<sizeof challenger_native/sizeof *challenger_native;index++){
  const challenger_case*c=&challenger_native[index];char seed[48],wave[48];snprintf(seed,sizeof seed,"challengers-run-%u",index);snprintf(wave,sizeof wave,"challengers-wave-%u",index);
  gc_run_new(&run,GAME_MODES_CLASSIC,seed,1);
  gc_starter starters[2]={{.species=SPECIES_ID_BULBASAUR,.tera_type=-1},{.species=SPECIES_ID_RATTATA,.tera_type=-1}};const char*why=NULL;CHECK(gc_run_set_starters(&run,starters,2,&why));
  run.wave=c->wave;run.biome=c->biome;run.wave_cycle_offset=5;pr_seed_from_ascii(&run.run.wave_seed,wave);
  int types[3],variants[3];const pr_rng before=run.run.rng;CHECK(gc_mc_trainers(&run,types,variants));CHECK(!memcmp(&before,&run.run.rng,sizeof before));
  for(int k=0;k<3;k++){if(types[k]!=c->types[k]||variants[k]!=c->variants[k])fprintf(stderr,"got trainer%d variant%d want%d/%d option%d\n",types[k],variants[k],c->types[k],c->variants[k],k);CHECK(types[k]==c->types[k]&&variants[k]==c->variants[k]);}
  /* Enter this mystery on an already initialized party, without another wave. */
  gc_rq_reset(&gc_int(&run)->rq);run.ev_head=run.ev_count=0;run.battle_type=BATTLE_TYPE_MYSTERY_ENCOUNTER;
  run.mystery_type=run.screen.mystery_type=GC_ME_MYSTERIOUS_CHALLENGERS;run.screen.kind=GC_SCREEN_MYSTERY;gc_mystery_setup_screen(&run);
  bool applied=false;CHECK(gc_mystery_execute(&run,c->option,0,0,&why,&applied)&&applied);CHECK(run.me_battle&&run.battle_type==BATTLE_TYPE_TRAINER&&run.n_enemy>0);
  CHECK(run.trainer_type==c->types[c->option]&&run.trainer_variant==c->variants[c->option]);CHECK(run.double_battle==(run.trainer_variant==TRAINER_VARIANT_DOUBLE));
  CHECK(run.n_enemy==c->n_levels);if(run.trainer_name!=c->name||run.trainer_partner_name!=c->partner)fprintf(stderr,"names got%d/%d expected%d/%d\n",run.trainer_name,run.trainer_partner_name,c->name,c->partner);CHECK(run.trainer_name==c->name&&run.trainer_partner_name==c->partner);
  if(c->name_key)CHECK(strstr(run.trainer_names[GC_TNAME_TRAINER],c->name_key));
  if(c->partner_key)CHECK(strstr(run.trainer_names[GC_TNAME_TRAINER],c->partner_key));
  for(int j=0;j<run.n_enemy;j++){if(run.enemy[j].b.level!=c->levels[j])fprintf(stderr,"level%d got%u expected%d\n",j,run.enemy[j].b.level,c->levels[j]);CHECK(run.enemy[j].b.level==c->levels[j]);}
  CHECK(gc_int(&run)->pillar.active&&gc_int(&run)->pillar.option==c->option);CHECK(!gc_run_can_save(&run));
  gc_event e;int trainer_events=0;while(gc_run_next_event(&run,&e))if(e.kind==GC_EV_TRAINER){trainer_events++;CHECK(e.a==run.trainer_type&&e.b==run.trainer_variant);}CHECK(trainer_events==1);
  gc_pillar_victory(&run);const pr_reward_custom*r=&gc_int(&run)->reward.me_custom;
  CHECK(r->n_funcs==c->n_funcs&&r->n_tiers==c->n_tiers&&r->fill_remaining==c->fill);
  for(int j=0;j<r->n_funcs;j++)CHECK(r->funcs[j]==gc_reward_item_index(c->funcs[j]));
  for(int j=0;j<r->n_tiers;j++)CHECK(r->tiers[j]==c->tiers[j]);
 }
 printf("Mysterious Challengers: 320 native configs/constructor levels and C battle-entry/reward owners, %u checks passed\n",checks);return 0;
}
