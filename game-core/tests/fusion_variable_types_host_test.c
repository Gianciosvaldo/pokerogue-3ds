#include "../src/internal.h"
#include "fixtures/fusion-variable-types.native.inc"
#include <stdio.h>
#include <stdlib.h>
static gc_run run;
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"fusion variable types case%u line%d: %s\n",i,__LINE__,#x);exit(1);}}while(0)
int main(void)
{
 unsigned i=0;
 for(;i<sizeof native_variable_type_cases/sizeof *native_variable_type_cases;i++) {
  const native_variable_type_case*n=&native_variable_type_cases[i];
  gc_mon m={0};pr_pokemon*p=&m.b;
  p->species=n->primary;p->form=n->form;p->fusion_species=n->fusion;p->fusion_form=n->fusionForm;
  p->fusion_gender=n->fusionGender;p->fusion_gender_known=true;p->gender=n->gender;
  p->base_ability=p->ability=n->ability;p->level=50;p->hp=p->stats[STAT_HP]=100;
  run.run.rng=run.bt.rng=run.bt.battle_rng=n->rng;
  gc_mon before=m;pr_rng before_rng=run.run.rng,global_rng=run.bt.rng,battle_rng=run.bt.battle_rng;
  int type=pr_moves[n->move].type;
  pr_apply_variable_move_type(&run.bt,p,n->move,&type);CHECK(type==n->variable);
  CHECK(pr_get_move_type(&run.bt,p,n->move)==n->actual);
  int8_t types[2]={-1,-1};int count=gc_types_for_item_spawn(&run,&m,n->move,types);
  CHECK(count==n->nitems);for(int j=0;j<count;j++)CHECK(types[j]==n->items[j]);
  CHECK(!memcmp(&m,&before,sizeof m));CHECK(!memcmp(&run.run.rng,&before_rng,sizeof before_rng));
  CHECK(!memcmp(&run.bt.rng,&global_rng,sizeof global_rng));CHECK(!memcmp(&run.bt.battle_rng,&battle_rng,sizeof battle_rng));
  CHECK(run.bt.n_unported==0);
 }
 CHECK(i==2976);printf("Native fusion variable types: %u cases, %u checks\n",i,checks);return 0;
}
