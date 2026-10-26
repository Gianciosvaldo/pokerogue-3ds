/* Legacy IDs cannot resolve an ambiguous object; native actor references can. */
#include "pr/engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static pr_battle bt;static pr_pokemon p[3];static int checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"actor guard line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void query(double expected,bool unknown){double power=1;bt.n_unported=0;pr_apply_variable_power(&bt,&p[0],&p[2],MOVE_ID_FUSION_FLARE,&power);CHECK(power==expected);CHECK((bt.n_unported!=0)==unknown);for(int i=0;i<bt.n_unported;i++)CHECK(bt.unported[i]==PR_FEAT_TURN_ORDER);}
int main(void){pr_closures_init();bt.n_party[0]=2;bt.n_party[1]=1;for(int i=0;i<3;i++){p[i].id=i<2?957185811:1234;p[i].species=SPECIES_ID_MEW;p[i].hp=100;bt.party[i/2][i%2]=&p[i];bt.field[i]=&p[i];pr_push_move_history(&p[i],(pr_turn_move){.move=i?MOVE_ID_TACKLE:MOVE_ID_FUSION_BOLT,.result=MOVE_RESULT_SUCCESS,.use_mode=MOVE_USE_MODE_NORMAL});}
 pr_record_turn_actor(&bt,&p[0]);pr_record_turn_actor(&bt,&p[1]);query(2,false);
 p[0].id=55;query(2,false);p[0].id=957185811;
 bt.n_party[0]=1;bt.party[0][0]=&p[1];query(2,false);bt.n_party[0]=2;bt.party[0][0]=&p[0];
 /* Equal raw count with a changed prefix must override an existing reference cache. */
 bt.last_turn_order[0]=p[2].id;query(1,false);
 pr_clear_turn_order(&bt);pr_record_turn_order(&bt,p[0].id);pr_record_turn_order(&bt,p[1].id);query(1,true);
 memset(&bt.turn_order_history,0,sizeof bt.turn_order_history);bt.n_last_turn_order=2;bt.last_turn_order[0]=bt.last_turn_order[1]=p[0].id;query(1,true);
 memset(&bt.turn_order_history,0,sizeof bt.turn_order_history);bt.n_last_turn_order=9;for(int i=0;i<8;i++)bt.last_turn_order[i]=p[0].id;query(1,true);
 /* A known newer terminal actor can answer before an unknown legacy identity. */
 pr_record_turn_actor(&bt,&p[2]);pr_record_turn_actor(&bt,&p[1]);query(1,false);
 pr_clear_turn_order(&bt);query(1,false);bool complete=false;const pr_pokemon*actors[12];CHECK(pr_prior_turn_actors(&bt,actors,&complete)==0&&complete);
 printf("Turn actor raw/cache/ambiguity guards: %d checks passed\n",checks);return 0;}
