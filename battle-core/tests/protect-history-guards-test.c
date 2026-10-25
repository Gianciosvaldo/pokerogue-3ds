#include "pr/engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static pr_battle bt;static pr_pokemon p;static int checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"Protect guard line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static uint32_t query(bool wanted_unknown){bool unknown=false;bt.n_unported=0;uint32_t count=pr_protect_streak(&bt,&p,&unknown);CHECK(unknown==wanted_unknown&&((bt.n_unported!=0)==wanted_unknown));return count;}
int main(void){pr_closures_init();pr_reset_summon_data(&p);p.n_move_history=9;
 for(int k=0;k<8;k++)p.move_history[k]=(pr_turn_move){.move=MOVE_ID_PROTECT,.use_mode=MOVE_USE_MODE_NORMAL,.result=MOVE_RESULT_SUCCESS};
 CHECK(query(true)==8);pr_push_move_history(&p,(pr_turn_move){.move=MOVE_ID_QUICK_GUARD,.use_mode=MOVE_USE_MODE_FOLLOW_UP,.result=MOVE_RESULT_SUCCESS});CHECK(!p.history_protect_complete&&query(true)==8);
 pr_push_move_history(&p,(pr_turn_move){.move=MOVE_ID_SPLASH,.use_mode=MOVE_USE_MODE_INDIRECT,.result=MOVE_RESULT_SUCCESS});CHECK(p.history_protect_complete&&query(false)==0);
 for(int mode=1;mode<=6;mode++){pr_push_move_history(&p,(pr_turn_move){.move=MOVE_ID_WIDE_GUARD,.use_mode=mode,.result=MOVE_RESULT_SUCCESS});CHECK(query(false)==(uint32_t)mode);}
 const int stopped[]={MOVE_RESULT_PENDING,MOVE_RESULT_FAIL,MOVE_RESULT_MISS,MOVE_RESULT_OTHER,PR_MOVE_RESULT_UNDEFINED};
 for(int i=0;i<5;i++){pr_push_move_history(&p,(pr_turn_move){.move=MOVE_ID_PROTECT,.use_mode=MOVE_USE_MODE_NORMAL,.result=stopped[i]});CHECK(query(false)==0);pr_push_move_history(&p,(pr_turn_move){.move=MOVE_ID_QUICK_GUARD,.use_mode=MOVE_USE_MODE_DELAYED_ATTACK,.result=MOVE_RESULT_SUCCESS});CHECK(query(false)==1);}
 pr_reset_summon_data(&p);CHECK(p.history_protect_complete&&!p.history_protect_count&&query(false)==0);
 for(int k=0;k<65540;k++)pr_push_move_history(&p,(pr_turn_move){.move=MOVE_ID_PROTECT,.use_mode=MOVE_USE_MODE_NORMAL,.result=MOVE_RESULT_SUCCESS});CHECK(p.n_move_history==UINT16_MAX&&p.history_protect_count==65540&&query(false)==65540);
 memset(&p,0,sizeof p);p.n_move_history=20;for(int k=0;k<8;k++)p.move_history[k]=(pr_turn_move){.move=k<5?MOVE_ID_SPLASH:MOVE_ID_PROTECT,.use_mode=MOVE_USE_MODE_NORMAL,.result=MOVE_RESULT_SUCCESS};CHECK(query(false)==3);pr_push_move_history(&p,(pr_turn_move){.move=MOVE_ID_WIDE_GUARD,.use_mode=MOVE_USE_MODE_REFLECTED,.result=MOVE_RESULT_SUCCESS});CHECK(p.history_protect_complete&&query(false)==4);
 printf("Protect raw/legacy/reset/result/saturation guards: %d checks passed\n",checks);return 0;}
