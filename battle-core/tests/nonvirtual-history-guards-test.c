#include "pr/engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static pr_battle bt;static pr_pokemon p;static int checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"nonvirtual guard line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static const pr_turn_move*query(int q,bool want_unknown){bool unknown=false;bt.n_unported=0;const pr_turn_move*m=pr_last_non_virtual_move(&bt,&p,q/2,q%2,&unknown);CHECK(unknown==want_unknown);CHECK((bt.n_unported!=0)==want_unknown);return m;}
int main(void){pr_closures_init();p.n_move_history=9;p.history_last_known=15;
 for(int i=0;i<8;i++)p.move_history[i]=(pr_turn_move){.move=MOVE_ID_SPLASH,.use_mode=MOVE_USE_MODE_INDIRECT};
 for(int q=0;q<4;q++)CHECK(!query(q,true));
 pr_rebuild_nonvirtual_history(&p);CHECK(!p.history_last_known&&!p.history_last_present);
 pr_push_move_history(&p,(pr_turn_move){.move=MOVE_ID_STRUGGLE,.use_mode=MOVE_USE_MODE_FOLLOW_UP});
 CHECK(p.history_last_known==1&&p.history_last_present==1);CHECK(query(0,false)->move==MOVE_ID_STRUGGLE);
 for(int q=1;q<4;q++)CHECK(!query(q,true));
 for(int i=0;i<9;i++)pr_push_move_history(&p,(pr_turn_move){.move=MOVE_ID_NONE,.use_mode=MOVE_USE_MODE_NORMAL});
 CHECK(query(0,false)->move==MOVE_ID_STRUGGLE);for(int q=1;q<4;q++)CHECK(!query(q,true));
 pr_push_move_history(&p,(pr_turn_move){.move=MOVE_ID_TACKLE,.use_mode=MOVE_USE_MODE_NORMAL,.result=MOVE_RESULT_MISS,.n_targets=2,.targets={3,1}});
 for(int q=0;q<4;q++){const pr_turn_move*m=query(q,false);CHECK(m&&m->move==MOVE_ID_TACKLE&&m->result==MOVE_RESULT_MISS&&m->n_targets==2&&m->targets[0]==3&&m->targets[1]==1);}
 CHECK(p.history_last_known==15&&p.history_last_present==15);
 /* No cached prefix may override a fully supplied short raw history. */
 p.n_move_history=1;p.move_history[0]=(pr_turn_move){.move=MOVE_ID_NONE,.use_mode=MOVE_USE_MODE_NORMAL};for(int q=0;q<4;q++)CHECK(!query(q,false));
 pr_reset_summon_data(&p);CHECK(!p.n_move_history&&p.history_last_known==15&&!p.history_last_present);
 for(int i=0;i<65540;i++)pr_push_move_history(&p,(pr_turn_move){.move=i?MOVE_ID_SPLASH:MOVE_ID_GROWL,.use_mode=i?MOVE_USE_MODE_DELAYED_ATTACK:MOVE_USE_MODE_IGNORE_PP});
 CHECK(p.n_move_history==UINT16_MAX);for(int q=0;q<4;q++)CHECK(query(q,false)->move==MOVE_ID_GROWL);
 pr_push_move_history(&p,(pr_turn_move){.move=MOVE_ID_TACKLE,.use_mode=MOVE_USE_MODE_NORMAL});
 CHECK(p.n_move_history==UINT16_MAX);for(int q=0;q<4;q++)CHECK(query(q,false)->move==MOVE_ID_TACKLE);
 pr_reset_summon_data(&p);for(int i=0;i<32;i++)pr_push_move_history(&p,(pr_turn_move){.move=MOVE_ID_SPLASH,.use_mode=MOVE_USE_MODE_REFLECTED});
 CHECK(p.history_last_known==15&&!p.history_last_present);for(int q=0;q<4;q++)CHECK(!query(q,false));
 printf("Nonvirtual raw/legacy/reset/saturation guards: %d checks passed\n",checks);return 0;}
