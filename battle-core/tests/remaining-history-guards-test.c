#include "pr/engine.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static int checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"remaining guard line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static pr_pokemon p;static pr_battle bt;static pr_engine e;
int main(void){pr_closures_init();bool unknown;const pr_turn_move*aim;
 for(int length=0;length<=9;length++)for(int defaults=0;defaults<2;defaults++){
  memset(&p,0,sizeof p);p.n_move_history=length;p.history_all_known=defaults?3:0;p.on_field=true;
  bt.n_unported=0;CHECK(!pr_history_any_move(&bt,&p,&unknown)&&unknown==(length>8));CHECK((bt.n_unported!=0)==(length>8));
  bt.n_unported=0;CHECK(!pr_last_aim_move(&bt,&p,&unknown)&&unknown==(length>8));CHECK((bt.n_unported!=0)==(length>8));
  if(length){p.move_history[0]=(pr_turn_move){.move=MOVE_ID_LOCK_ON,.use_mode=6,.result=2,.n_targets=1,.targets={3}};
   bt.n_unported=0;CHECK(pr_history_any_move(&bt,&p,&unknown)&&!unknown&&!bt.n_unported);aim=pr_last_aim_move(&bt,&p,&unknown);CHECK(aim&&aim->targets[0]==3&&!unknown&&!bt.n_unported);}
 }
 // Reconstructing a legacy long all-NONE tail cannot supply the missing prefix.
 memset(&p,0,sizeof p);p.n_move_history=9;pr_rebuild_all_history(&p);CHECK(!p.history_all_known&&!p.history_all_present);
 pr_push_move_history(&p,(pr_turn_move){.move=MOVE_ID_TACKLE,.use_mode=6,.result=3});CHECK(pr_history_any_move(&bt,&p,&unknown)&&!unknown);CHECK(!pr_last_aim_move(&bt,&p,&unknown)&&unknown);
 pr_push_move_history(&p,(pr_turn_move){.move=MOVE_ID_MIND_READER,.use_mode=5,.result=2,.n_targets=1,.targets={1}});aim=pr_last_aim_move(&bt,&p,&unknown);CHECK(aim&&aim->targets[0]==1&&!unknown);
 // A manually supplied long tail can establish a positive before it ages away.
 memset(&p,0,sizeof p);p.n_move_history=p.history_serial=9;
 p.move_history[0]=(pr_turn_move){.move=MOVE_ID_TACKLE,.use_mode=6,.result=3};
 for(int i=0;i<9;i++)pr_push_move_history(&p,(pr_turn_move){.move=MOVE_ID_NONE,.use_mode=6,.result=4});
 CHECK(pr_history_any_move(&bt,&p,&unknown)&&!unknown);CHECK(!pr_last_aim_move(&bt,&p,&unknown)&&unknown);
 // Old summon shared arrays must not rewrite a fresh summon, even with equal serials.
 memset(&p,0,sizeof p);memset(&e,0,sizeof e);p.on_field=true;int8_t a[]={2};int old=pr_targets_new(&e,a,1);
 pr_push_history_shared(&bt,&e,&p,(pr_turn_move){.move=MOVE_ID_LOCK_ON,.use_mode=6,.result=1},old);for(int i=0;i<9;i++)pr_push_move_history(&p,(pr_turn_move){.move=MOVE_ID_NONE,.use_mode=6});
 e.target_lists[old].v[0]=3;pr_targets_changed(&bt,&e,old);aim=pr_last_aim_move(&bt,&p,&unknown);CHECK(aim&&aim->targets[0]==3&&!unknown);
 pr_reset_summon_data(&p);CHECK(!pr_history_any_move(&bt,&p,&unknown)&&!unknown&&!pr_last_aim_move(&bt,&p,&unknown)&&!unknown);
 int fresh=pr_targets_new(&e,a,1);pr_push_history_shared(&bt,&e,&p,(pr_turn_move){.move=MOVE_ID_MIND_READER,.use_mode=5,.result=2},fresh);for(int i=0;i<9;i++)pr_push_move_history(&p,(pr_turn_move){.move=MOVE_ID_NONE,.use_mode=6});
 e.target_lists[old].v[0]=0;pr_targets_changed(&bt,&e,old);aim=pr_last_aim_move(&bt,&p,&unknown);CHECK(aim&&aim->move==MOVE_ID_MIND_READER&&aim->targets[0]==2&&!unknown);
 e.target_lists[fresh].v[0]=1;pr_targets_changed(&bt,&e,fresh);aim=pr_last_aim_move(&bt,&p,&unknown);CHECK(aim&&aim->targets[0]==1&&!unknown);
 printf("Remaining history raw/legacy/alias/reset guards: %d checks passed\n",checks);return 0;}
