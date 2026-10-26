#include "pr/engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static pr_battle bt;static pr_engine e;static pr_pokemon mons[13];static int checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"turn-order guard line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void query(int move,double want,bool unknown){bt.n_unported=0;pr_rng global=bt.rng,local=bt.battle_rng;double power=1;pr_apply_variable_power(&bt,&mons[0],&mons[6],move,&power);CHECK(power==want);CHECK((bt.n_unported!=0)==unknown);for(int i=0;i<bt.n_unported;i++)CHECK(bt.unported[i]==PR_FEAT_TURN_ORDER);CHECK(!memcmp(&global,&bt.rng,sizeof global)&&!memcmp(&local,&bt.battle_rng,sizeof local));}
static void last(int actor,int move,int result){pr_push_move_history(&mons[actor],(pr_turn_move){.move=move,.result=result,.use_mode=MOVE_USE_MODE_NORMAL});}
int main(void){pr_closures_init();bt.n_party[0]=bt.n_party[1]=6;for(int i=0;i<13;i++){mons[i].id=i==0?0:i==1?UINT32_MAX:(uint32_t)i;mons[i].hp=100;mons[i].species=SPECIES_ID_MEW;mons[i].is_player=i<6;if(i<12)bt.party[i/6][i%6]=&mons[i];last(i,MOVE_ID_TACKLE,MOVE_RESULT_FAIL);}bt.field[0]=&mons[0];bt.field[1]=&mons[1];bt.field[2]=&mons[6];bt.field[3]=&mons[7];
 query(MOVE_ID_FUSION_FLARE,1,false);last(0,MOVE_ID_FUSION_BOLT,MOVE_RESULT_SUCCESS);pr_record_turn_order(&bt,mons[0].id);query(MOVE_ID_FUSION_FLARE,1,false);pr_record_turn_order(&bt,mons[0].id);query(MOVE_ID_FUSION_FLARE,2,false);
 last(0,MOVE_ID_FUSION_BOLT,MOVE_RESULT_PENDING);query(MOVE_ID_FUSION_FLARE,1,false);last(0,MOVE_ID_FUSION_BOLT,MOVE_RESULT_MISS);query(MOVE_ID_FUSION_FLARE,1,false);last(0,MOVE_ID_FUSION_BOLT,MOVE_RESULT_OTHER);query(MOVE_ID_FUSION_FLARE,1,false);last(0,MOVE_ID_FUSION_BOLT,MOVE_RESULT_SUCCESS);query(MOVE_ID_FUSION_FLARE,2,false);
 /* Pending/miss/other stop before an older successful partner move. */
 pr_clear_turn_order(&bt);last(1,MOVE_ID_FUSION_BOLT,MOVE_RESULT_SUCCESS);pr_record_turn_order(&bt,mons[1].id);pr_record_turn_order(&bt,mons[0].id);pr_record_turn_order(&bt,mons[2].id);
 last(0,MOVE_ID_TACKLE,MOVE_RESULT_PENDING);query(MOVE_ID_FUSION_FLARE,1,false);last(0,MOVE_ID_TACKLE,MOVE_RESULT_MISS);query(MOVE_ID_FUSION_FLARE,1,false);last(0,MOVE_ID_TACKLE,MOVE_RESULT_OTHER);query(MOVE_ID_FUSION_FLARE,1,false);
 pr_clear_turn_order(&bt);last(0,MOVE_ID_FUSION_BOLT,MOVE_RESULT_SUCCESS);pr_record_turn_order(&bt,mons[0].id);pr_record_turn_order(&bt,mons[0].id);
 /* A complete raw prefix must override a cache, including equal lengths. */
 last(1,MOVE_ID_FUSION_FLARE,MOVE_RESULT_SUCCESS);bt.last_turn_order[0]=mons[1].id;query(MOVE_ID_FUSION_BOLT,2,false);bt.n_last_turn_order=1;query(MOVE_ID_FUSION_BOLT,1,false);
 /* Nine raw entries omit only the excluded last entry: the first eight still answer. */
 memset(&bt.turn_order_history,0,sizeof bt.turn_order_history);bt.n_last_turn_order=9;for(int i=0;i<8;i++)bt.last_turn_order[i]=mons[0].id;query(MOVE_ID_FUSION_FLARE,2,false);pr_record_turn_order(&bt,mons[2].id);query(MOVE_ID_FUSION_FLARE,1,true);pr_record_turn_order(&bt,mons[3].id);query(MOVE_ID_FUSION_FLARE,1,true);last(2,MOVE_ID_TACKLE,MOVE_RESULT_SUCCESS);query(MOVE_ID_FUSION_FLARE,1,false);last(2,MOVE_ID_FUSION_BOLT,MOVE_RESULT_SUCCESS);query(MOVE_ID_FUSION_FLARE,2,false);
 /* Longer unknown suffixes cannot be reconstructed from their first entries. */
 memset(&bt.turn_order_history,0,sizeof bt.turn_order_history);bt.n_last_turn_order=10;query(MOVE_ID_FUSION_FLARE,1,true);
 /* Distinct bench/fainted party identities still refer to their current histories. */
 pr_clear_turn_order(&bt);last(11,MOVE_ID_FUSION_BOLT,MOVE_RESULT_SUCCESS);mons[11].hp=0;pr_record_turn_order(&bt,mons[11].id);pr_record_turn_order(&bt,mons[0].id);query(MOVE_ID_FUSION_FLARE,2,false);
 /* A discarded thirteenth identity is an explicit gap, not an absent move. */
 pr_clear_turn_order(&bt);for(int i=0;i<13;i++){last(i,MOVE_ID_TACKLE,MOVE_RESULT_FAIL);pr_record_turn_order(&bt,mons[i].id);}pr_record_turn_order(&bt,mons[2].id);query(MOVE_ID_FUSION_FLARE,1,true);last(2,MOVE_ID_TACKLE,MOVE_RESULT_SUCCESS);pr_record_turn_order(&bt,mons[3].id);query(MOVE_ID_FUSION_FLARE,1,false);
 /* Empty histories correspond to the native .result-of-undefined exception. */
 pr_clear_turn_order(&bt);mons[0].n_move_history=0;pr_record_turn_order(&bt,mons[0].id);pr_record_turn_order(&bt,mons[1].id);query(MOVE_ID_FUSION_FLARE,1,true);
 pr_clear_turn_order(&bt);last(1,MOVE_ID_FUSION_BOLT,MOVE_RESULT_SUCCESS);for(int i=0;i<65540;i++)pr_record_turn_order(&bt,mons[1].id);CHECK(bt.n_last_turn_order==UINT8_MAX);query(MOVE_ID_FUSION_FLARE,2,false);pr_clear_turn_order(&bt);query(MOVE_ID_FUSION_FLARE,1,false);
 /* Invalid cached lengths or duplicates fall back to the unknown raw suffix. */
 bt.n_last_turn_order=40;bt.turn_order_history=(pr_turn_order_history){.valid=true,.count=40,.n_recent=255};query(MOVE_ID_FUSION_FLARE,1,true);bt.turn_order_history=(pr_turn_order_history){.valid=true,.count=40,.n_recent=2,.complete=true,.before_complete=true};query(MOVE_ID_FUSION_FLARE,1,true);
 /* Keep all 13 observable shared target references per party actor while
  * recycling old aliases; resetting a summon invalidates only that epoch. */
 memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);memset(mons,0,sizeof mons);int list=pr_targets_new(&e,(int8_t[]){2},1);for(int k=0;k<12;k++){pr_pokemon*p=&mons[k];p->on_field=true;pr_push_history_shared(&bt,&e,p,(pr_turn_move){.move=MOVE_ID_LOCK_ON,.use_mode=MOVE_USE_MODE_INDIRECT},list);pr_push_history_shared(&bt,&e,p,(pr_turn_move){.move=MOVE_ID_TACKLE,.use_mode=MOVE_USE_MODE_NORMAL},list);pr_push_history_shared(&bt,&e,p,(pr_turn_move){.move=MOVE_ID_STRUGGLE,.use_mode=MOVE_USE_MODE_NORMAL},list);pr_push_history_shared(&bt,&e,p,(pr_turn_move){.move=MOVE_ID_TACKLE,.use_mode=MOVE_USE_MODE_FOLLOW_UP},list);pr_push_history_shared(&bt,&e,p,(pr_turn_move){.move=MOVE_ID_STRUGGLE,.use_mode=MOVE_USE_MODE_FOLLOW_UP},list);for(int i=0;i<80;i++)pr_push_history_shared(&bt,&e,p,(pr_turn_move){.move=MOVE_ID_NONE,.use_mode=MOVE_USE_MODE_INDIRECT},list);}CHECK(!e.overflow&&!bt.n_unported&&e.n_hist_alias==PR_MAX_HISTORY_ALIASES);
 for(int k=0;k<12;k++)for(int i=0;i<20;i++)pr_push_history_shared(&bt,&e,&mons[k],(pr_turn_move){.move=MOVE_ID_NONE,.use_mode=MOVE_USE_MODE_INDIRECT},list);CHECK(!e.overflow&&!bt.n_unported&&e.n_hist_alias==PR_MAX_HISTORY_ALIASES);e.target_lists[list].v[0]=3;pr_targets_changed(&bt,&e,list);for(int k=0;k<12;k++){for(int q=0;q<4;q++)CHECK(mons[k].history_last[q].targets[0]==3);CHECK(mons[k].history_aim.targets[0]==3);for(int h=0;h<8;h++)CHECK(mons[k].move_history[h].targets[0]==3);}pr_reset_summon_data(&mons[0]);pr_push_history_shared(&bt,&e,&mons[0],(pr_turn_move){.move=MOVE_ID_TACKLE},list);CHECK(!e.overflow&&!bt.n_unported&&e.n_hist_alias<=PR_MAX_HISTORY_ALIASES);int live=0;for(int i=0;i<e.n_hist_alias;i++)if(e.hist_alias[i].p==&mons[0])live++;CHECK(live==1);
 printf("Turn-order raw/legacy/reset/identity and live-alias guards: %d checks passed\n",checks);return 0;}
