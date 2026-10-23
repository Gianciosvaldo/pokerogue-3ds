#include "pr/engine.h"
#include "last-resort-history-fixture.inc"
static pr_battle battle;
static pr_pokemon target;
int main(int argc,char **argv){
 if(argc!=2)return 2;FILE *f=fopen(argv[1],"r");if(!f)return 2;lr_header(f);pr_closures_init();
 int fail=0,shapes[8]={0},modes[6]={0},observations=0;
 for(int i=0;i<432;i++){
  lr_case c;lr_read(f,i,&c);shapes[c.shape]++;modes[c.mode-1]++;
  for(int stage=0;stage<5;stage++){
   if(stage==1)lr_changed(&c.user);
   if(stage==2)c.user.moves[1]=c.future_move;
   if(stage==3){lr_changed(&c.user);pr_reset_summon_data(&c.user);}
   if(stage==4)pr_push_move_history(&c.user,(pr_turn_move){.move=MOVE_ID_SPLASH,.use_mode=MOVE_USE_MODE_NORMAL,.result=PR_MOVE_RESULT_UNDEFINED,.n_targets=1,.targets={2}});
   pr_rng before=battle.rng,seed=battle.battle_rng;
   battle.n_unported=0;int got=pr_move_apply_conditions(&battle,MOVE_ID_LAST_RESORT,&c.user,&target,3);
   if(got!=c.expected[stage]||battle.n_unported||memcmp(&before,&battle.rng,sizeof before)||memcmp(&seed,&battle.battle_rng,sizeof seed)){fprintf(stderr,"Last Resort case%d stage%d expected%d got%d unsupported%d\n",i,stage,c.expected[stage],got,battle.n_unported);fail++;}
   observations++;
  }
 }
 lr_footer(f);fclose(f);for(int i=0;i<8;i++)if(shapes[i]!=54)return 2;for(int i=0;i<6;i++)if(modes[i]!=72)return 2;
 printf("Last Resort history: %d native owning-condition observations, %d mismatches/uncovered\n",observations,fail);return fail?1:0;
}
