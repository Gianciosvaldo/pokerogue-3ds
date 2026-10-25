#include "pr/engine.h"
#include "nonvirtual-history-fixture.inc"
static pr_battle bt;static nv_case c;static pr_pokemon other;
int main(int argc,char**argv){if(argc!=2)return 2;FILE*f=fopen(argv[1],"r");if(!f)return 2;nv_header(f);pr_closures_init();int failures=0,observations=0;
 for(int i=0;i<972;i++){nv_read(f,i,&c);memset(&other,0,sizeof other);nv_movesets(&c.user,&other);for(int stage=0;stage<6;stage++){
  nv_stage(&c.user,&c,stage);
  for(int q=0;q<4;q++){bool unknown=true;pr_rng global=bt.rng,battle=bt.battle_rng;bt.n_unported=0;
   const pr_turn_move*m=pr_last_non_virtual_move(&bt,&c.user,q/2,q%2,&unknown);
   if(unknown||bt.n_unported||((m!=NULL)!=c.present[stage][q])||(m&&!nv_equal(m,&c.expected[stage][q]))||memcmp(&global,&bt.rng,sizeof global)||memcmp(&battle,&bt.battle_rng,sizeof battle)){
    if(failures<8)fprintf(stderr,"native nonvirtual mismatch case%d stage%d query%d unknown%d\n",i,stage,q,unknown);failures++;
   }observations++;
  }
  for(int k=0;k<5;k++){bt.n_unported=0;int got=pr_move_apply_conditions(&bt,nv_owner_moves[k],&other,&c.user,3);if(got!=c.owners[stage][k]||bt.n_unported){if(failures<8)fprintf(stderr,"native owner mismatch case%d stage%d move%d got%d expected%d unsupported%d\n",i,stage,nv_owner_moves[k],got,c.owners[stage][k],bt.n_unported);failures++;}observations++;}
 }}nv_aliases(f);nv_footer(f);fclose(f);printf("Nonvirtual history: %d native observations + 336 shared-target queries, %d mismatches/uncovered\n",observations,failures);return failures?1:0;}
