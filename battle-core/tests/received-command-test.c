#include "pr/engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "received-command.native.inc"
static pr_battle bt;static pr_engine engine;static pr_pokemon mons[4];static int checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"received command line%d: %s\n",__LINE__,#x);for(int i=0;i<bt.n_unported;i++)fprintf(stderr,"  %s\n",pr_unported_name(bt.unported[i]));exit(1);}}while(0)
int main(void){pr_closures_init();bt.engine=&engine;bt.game_mode=GAME_MODES_CLASSIC;bt.wave_index=bt.turn=1;bt.double_battle=true;bt.ignoring_effect_source=-1;pr_seed_from_ascii(&bt.wave_seed,"received-command-wave");bt.rng=received_command_before[0];bt.battle_rng=received_command_before[1];bt.rng_live=bt.battle_rng_live=true;bt.n_party[0]=bt.n_party[1]=2;
 const int stages[]={6,6,-6,-5};for(int k=0;k<4;k++){pr_pokemon*p=&mons[k];pr_reset_turn_data(p);p->id=received_command_actors[k].id;p->species=received_command_actors[k].species;p->level=received_command_actors[k].level;memcpy(p->stats,received_command_actors[k].stats,sizeof p->stats);p->hp=p->stats[STAT_HP];p->nature=0;p->friendship=70;memset(p->ivs,15,6);p->ability_override=ABILITY_ID_NONE;p->added_type=-1;p->tera_type=-1;p->is_player=k<2;p->on_field=true;p->n_moves=1;p->moves[0]=k?MOVE_ID_INSTRUCT:MOVE_ID_POPULATION_BOMB;for(int m=0;m<4;m++)p->max_pp_override[m]=-1;p->stat_stages[STAT_SPD-1]=stages[k];if(!k)p->stat_stages[STAT_ACC-1]=6;bt.field[k]=bt.party[k/2][k%2]=p;bt.commands[k]=(struct pr_turn_command){.command=COMMAND_FIGHT,.move=p->moves[0],.n_targets=1,.targets={k?0:2},.cursor=0,.use_mode=MOVE_USE_MODE_NORMAL};bt.pre_commands[k].command=-1;}
 pr_run_turn(&bt,&engine);
 CHECK(!bt.n_unported&&!engine.overflow&&!engine.events_overflow);CHECK(engine.stop==PH_TURN_INIT);CHECK(mons[0].pp_used[0]==4);CHECK(mons[2].hp==received_command_hp);CHECK(mons[2].n_attacks_received==40);
 for(int i=0;i<32;i++){pr_attack_record*x=&mons[2].attacks_received[i];const pr_attack_record*y=&received_command_records[i];CHECK(x->move==y->move&&x->result==y->result&&x->critical==y->critical&&x->source_battler_index==y->source_battler_index&&x->source_id==y->source_id&&x->damage==y->damage);}
 CHECK(!memcmp(&bt.rng,&received_command_after[0],sizeof(pr_rng)));CHECK(!memcmp(&bt.battle_rng,&received_command_after[1],sizeof(pr_rng)));CHECK(pr_received_oldest_positive(&bt,&mons[2]));CHECK(pr_received_damage_from(&bt,&mons[2],mons[0].id));CHECK(!bt.n_unported);
 printf("Received history: real four-command forty-hit turn, HP/PP/retained records/RNG; %d checks passed\n",checks);return 0;}
