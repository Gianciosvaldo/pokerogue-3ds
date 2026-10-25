#include "pr/engine.h"
#include "protect-history-fixture.inc"
static pr_engine e;static pr_battle bt;static pr_pokemon user,target;
int main(int argc,char**argv){if(argc!=2)return 2;FILE*f=fopen(argv[1],"r");if(!f)return 2;pt_header(f);pr_closures_init();bt.engine=&e;int failures=0,observations=0;
 for(int i=0;i<1224;i++){int history_move,mode;pt_case(f,i,&user,&history_move,&mode);user.hp=target.hp=100;bt.field[0]=&user;bt.field[2]=&target;
  for(int stage=0;stage<5;stage++){pt_word(f,"STAGE");pt_int(f,stage,stage);pt_stage(&user,stage,history_move,mode);
   for(int q=0;q<40;q++){pt_word(f,"OBS");int move=pt_int(f,pt_moves[q/4],pt_moves[q/4]);pt_int(f,q%4,q%4);int before=pt_int(f,0,pt_n_states-1),after=pt_int(f,0,pt_n_states-1),expected=pt_int(f,0,1);pt_apply_state(&bt,before);pr_rng global=bt.rng;bt.n_unported=0;
    int got=pr_move_apply_conditions(&bt,move,&user,&target,4);
    if(got!=expected||bt.n_unported||!pt_after(&bt,after)||!pt_rngsame(&global,&bt.rng)){if(failures<8)fprintf(stderr,"native Protect mismatch case%d stage%d move%d rng%d got%d expected%d unsupported%d\n",i,stage,move,q%4,got,expected,bt.n_unported);failures++;}observations++;
   }
  }pt_word(f,"END");
 }pt_gates(f);pt_footer(f);fclose(f);printf("Protect: %d native streak/RNG observations +24 phase gates, %d mismatches/uncovered\n",observations,failures);return failures?1:0;}
