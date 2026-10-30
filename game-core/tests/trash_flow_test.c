#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run run;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"Trash flow line%d %s screen%d\n",__LINE__,#x,run.screen.kind);exit(1);}}while(0)
static void start(int option){
 gc_run_new(&run,GAME_MODES_CLASSIC,"trash-played-flow",1);run.wave=123;run.biome=BIOME_ID_PLAINS;run.n_party=3;run.battle_type=BATTLE_TYPE_WILD;
 for(int k=0;k<3;k++){gc_starter s={.species=SPECIES_ID_MEW,.tera_type=-1,.moves={MOVE_ID_PSYCHIC}};memset(s.ivs,31,6);gc_mon_new_player(&run,&run.party[k],&s,150);run.party[k].pause_evolutions=true;run.party_order[k]=k;}
 gc_items_to_battle(&run);run.mystery_type=GC_ME_TRASH_TO_TREASURE;gc_rq_reset(&gc_int(&run)->rq);gc_rq_push(&run,gc_rp(RP_MYSTERY,GC_ME_TRASH_TO_TREASURE,GC_ME_ULTRA,0));gc_run_until_screen(&run);CHECK(run.screen.kind==GC_SCREEN_MYSTERY);const char*why=NULL;CHECK(gc_run_choose(&run,&(gc_choice){.kind=GC_CH_MYSTERY,.a=option},&why));
}
int main(void){start(0);CHECK(run.screen.kind==GC_SCREEN_COMMAND);CHECK(run.bt.field[0]->status==STATUS_EFFECT_TOXIC);CHECK(run.enemy[0].b.species==SPECIES_ID_GARBODOR&&run.enemy[0].b.form==1);CHECK(run.enemy[0].b.stat_stages[STAT_DEF-1]==1&&run.enemy[0].b.stat_stages[STAT_SPDEF-1]==1);const char*why=NULL;CHECK(!gc_run_choose(&run,&(gc_choice){.kind=GC_CH_RUN},&why));
 int turns=0,steps=0;while(run.screen.kind!=GC_SCREEN_REWARD&&steps++<30){
  if(run.screen.kind==GC_SCREEN_COMMAND){turns++;CHECK(gc_run_choose(&run,&(gc_choice){.kind=GC_CH_FIGHT,.a=0,.b=-1},&why));}
  else if(run.screen.kind==GC_SCREEN_SWITCH){int pick=-1;for(int k=0;k<run.n_party;k++)if(run.screen.pick_ok[k]){pick=k;break;}CHECK(pick>=0);CHECK(gc_run_choose(&run,&(gc_choice){.kind=GC_CH_PICK,.a=pick},&why));}
  else CHECK(false);
 }
 CHECK(run.screen.kind==GC_SCREEN_REWARD);CHECK(turns<=10);int leftovers=-1;for(int i=0;i<run.screen.n_options;i++)if(!strcmp(run.screen.options[i].key,"LEFTOVERS"))leftovers=i;CHECK(leftovers>=0);CHECK(run.screen.n_options>=4);
 CHECK(gc_run_choose(&run,&(gc_choice){.kind=GC_CH_REWARD,.a=leftovers,.b=0,.c=-1},&why));
 if(run.screen.kind==GC_SCREEN_BIOME)CHECK(gc_run_choose(&run,&(gc_choice){.kind=GC_CH_BIOME,.a=0},&why));CHECK(run.wave==124);CHECK(gc_int(&run)->me_reward.n_tiers==0);
 start(1);CHECK(run.screen.kind==GC_SCREEN_REWARD&&run.screen.n_options==0&&run.screen.n_shop>0);CHECK(gc_item_stack(&run,"MYSTERY_ENCOUNTER_BLACK_SLUDGE")==1);CHECK(run.party[0].b.status==STATUS_EFFECT_NONE);CHECK(gc_run_choose(&run,&(gc_choice){.kind=GC_CH_SKIP},&why));CHECK(run.wave==124);
 puts("Trash flow: executed Toxic/Stockpile, actual battle victory and guaranteed rewards, next wave and digging heal shop passed");return 0;}
