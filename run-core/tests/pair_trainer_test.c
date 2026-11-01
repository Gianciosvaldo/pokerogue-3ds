/* The real pool/evolution owner -> real base constructor boundary. */
#include "pr/run.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pair-trainers.native.inc"
static unsigned current,checks,calls;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"Pair factory case%u line%d: %s\n",current,__LINE__,#x);exit(1);}}while(0)
int __real_pr_run_enemy_pokemon_base(pr_run*,const pr_enemy_gen_ctx*,pr_enemy_gen*);
int __wrap_pr_run_enemy_pokemon_base(pr_run*r,const pr_enemy_gen_ctx*c,pr_enemy_gen*out){
 const pair_case*n=&pair_native[current];calls++;
 if(c->species!=n->species)fprintf(stderr,"got species%d expected%d\n",c->species,n->species);
 CHECK(c->species==n->species&&c->level==n->level&&c->trainer_slot==n->slot&&!c->boss&&!c->shiny_lock);
 CHECK(r->rng.c==n->entry.c&&r->rng.s0==n->entry.s0&&r->rng.s1==n->entry.s1&&r->rng.s2==n->entry.s2);
 return __real_pr_run_enemy_pokemon_base(r,c,out);
}
int main(void){
 CHECK(sizeof pair_native/sizeof *pair_native==640);
 for(current=0;current<sizeof pair_native/sizeof *pair_native;current++){
  const pair_case*n=&pair_native[current];pr_run run={.mode=GAME_MODES_CLASSIC,.rng=n->before};
  pr_party_template tpl={.size=2};tpl.member[n->key]=(int8_t)(n->kind<<2);
  pr_trainer_party_ctx ctx={.enemy={.trainer_type=n->type,.wave=n->wave,.biome=BIOME_ID_PLAINS,.battle_type=BATTLE_TYPE_TRAINER},.template_=&tpl,.n_levels=2,.classic_wave20_trainer=n->wave==20};ctx.levels[n->key]=n->level;
  pr_enemy_gen out={0};int species=-1;unsigned before=calls;
  CHECK(pr_run_gen_pair_closure_member(&run,&ctx,n->key,n->key,&out,&species)==1);CHECK(calls==before+1&&species==n->species);
 }
 current=0;pr_run run={0};pr_trainer_party_ctx ctx={0};pr_enemy_gen out={0};int species=-1;unsigned before=calls;
 CHECK(pr_run_gen_pair_closure_member(&run,&ctx,0,0,&out,&species)==0&&calls==before);
 ctx.enemy.trainer_type=TRAINER_TYPE_TWINS;CHECK(pr_run_gen_pair_closure_member(&run,&ctx,0,2,&out,&species)==0&&calls==before);
 CHECK(pr_run_gen_pair_closure_member(&run,&ctx,0,0,&out,&species)==-1&&calls==before);
 pr_party_template tpl={0};ctx.template_=&tpl;ctx.n_levels=2;tpl.member[0]=PR_EVO_KIND_WILD<<2;
 CHECK(pr_run_gen_pair_closure_member(&run,&ctx,0,0,&out,&species)==-1&&calls==before);
 tpl.member[0]=PR_EVO_KIND_NORMAL<<2;CHECK(pr_run_gen_pair_closure_member(&run,&ctx,8,0,&out,&species)==-1&&calls==before);
 CHECK(pr_run_gen_pair_closure_member(NULL,&ctx,0,0,&out,&species)==-1&&calls==before);
 printf("Pair factories: 640 actual native species/slot/preconstructor-RNG observations and six guards, %u checks passed\n",checks);return 0;
}
