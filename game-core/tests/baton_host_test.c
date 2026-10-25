/* Real Baton Pass/ordinary switch commands, inventory projection and saves. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run run,loaded,after;
static unsigned char saved[GC_SAVE_MAX];
static int checks,bench;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"Baton host line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void drain(gc_run *r){gc_event ev;while(gc_run_next_event(r,&ev)){};}
static void setup(int source,int target,int locked,int seed){
 const char *why=NULL;char text[48];snprintf(text,sizeof text,"baton-host-%d",seed);
 gc_run_new(&run,GAME_MODES_CLASSIC,text,777);
 gc_starter starters[3]={{.species=SPECIES_ID_BULBASAUR,.tera_type=-1,.moves={MOVE_ID_SPLASH}},
  {.species=SPECIES_ID_PIDGEY,.tera_type=-1,.moves={MOVE_ID_SPLASH}},
  {.species=SPECIES_ID_SQUIRTLE,.tera_type=-1,.moves={MOVE_ID_SPLASH}}};
 for(int k=0;k<3;k++)memset(starters[k].ivs,15,sizeof starters[k].ivs);
 CHECK(gc_run_set_starters(&run,starters,3,&why));drain(&run);CHECK(gc_run_can_save(&run));
 bench=run.bt.double_battle?2:1;
 run.n_items=run.n_enemy_items=0;
 for(int k=0;k<run.n_party;k++){
  pr_pokemon *p=&run.party[k].b;p->custom_ability=ABILITY_ID_UNBURDEN;p->ability_override=-1;
  p->passive_flag=false;pr_refresh_ability(p);p->n_moves=1;p->moves[0]=k==0?MOVE_ID_BATON_PASS:MOVE_ID_SPLASH;
  p->pp_used[0]=p->pp_up[0]=0;p->max_pp_override[0]=-1;
 }
 for(int field=2;field<4;field++){pr_pokemon *enemy=run.bt.field[field];if(!enemy)continue;enemy->custom_ability=ABILITY_ID_NONE;
 enemy->ability_override=-1;enemy->passive_flag=false;pr_refresh_ability(enemy);
 enemy->n_moves=1;enemy->moves[0]=MOVE_ID_SPLASH;enemy->pp_used[0]=enemy->pp_up[0]=0;enemy->max_pp_override[0]=-1;}
 pr_pokemon *enemy=run.bt.field[2];CHECK(enemy);
 run.party[0].b.stat_stages[STAT_ATK-1]=2;
 if(source)run.items[run.n_items++]=(gc_item){.key="BATON",.arg=-1,.stack=1,.pokemon_id=run.party[0].b.id,.non_transferable=locked};
 if(target)run.items[run.n_items++]=(gc_item){.key="BATON",.arg=-1,.stack=1,.pokemon_id=run.party[bench].b.id};
 run.enemy_items[0]=(gc_item){.key="BATON",.arg=-1,.stack=1,.pokemon_id=enemy->id};run.n_enemy_items=1;
 gc_items_to_battle(&run);
}
static void complete_choices(gc_run *r){
 const char *why=NULL;
 while(r->screen.kind==GC_SCREEN_TARGET||(r->screen.kind==GC_SCREEN_COMMAND&&r->screen.slot==1)){
  gc_choice c={GC_CH_FIGHT,0,-1,0};
  if(r->screen.kind==GC_SCREEN_TARGET){c.kind=GC_CH_TARGET;c.a=2;while(c.a<4&&!r->screen.target_ok[c.a])c.a++;CHECK(c.a<4);}
  CHECK(gc_run_choose(r,&c,&why));drain(r);
 }
}
static int held(const gc_run *r,uint32_t owner){
 for(int k=0;k<r->n_items;k++)if(r->items[k].pokemon_id==owner&&!strcmp(r->items[k].key,"BATON"))return k;
 return -1;
}
static void finish_to_save(void){
 const char *why=NULL;
 pr_pokemon *p=run.bt.field[0];p->moves[0]=MOVE_ID_WATER_GUN;p->pp_used[0]=0;
 run.bt.field[2]->hp=1;if(run.bt.field[3])run.bt.field[3]->hp=1;
 gc_choice c={GC_CH_FIGHT,0,-1,0};CHECK(gc_run_choose(&run,&c,&why));drain(&run);complete_choices(&run);
 for(int step=0;step<30&&!gc_run_can_save(&run);step++){
  c=(gc_choice){0};
  switch(run.screen.kind){
  case GC_SCREEN_COMMAND:c.kind=GC_CH_FIGHT;c.a=0;c.b=-1;break;
  case GC_SCREEN_REWARD:c.kind=GC_CH_SKIP;break;
  case GC_SCREEN_LEARN_MOVE:c.kind=GC_CH_FORGET;c.a=-1;break;
  case GC_SCREEN_EVOLVE:c.kind=GC_CH_CANCEL;c.a=1;break;
  case GC_SCREEN_BIOME:c.kind=GC_CH_BIOME;break;
  default:CHECK(false);
  }
  CHECK(gc_run_choose(&run,&c,&why));drain(&run);complete_choices(&run);
 }
 CHECK(gc_run_can_save(&run));
}
int main(void){
 const char *why=NULL;
 for(int ordinary=0;ordinary<2;ordinary++)for(int source=0;source<2;source++)
 for(int target=0;target<2;target++)for(int locked=0;locked<2;locked++)for(int seed=0;seed<8;seed++){
  setup(source,target,locked,seed);uint32_t last=run.party[0].b.id,incoming=run.party[bench].b.id;
  size_t n=gc_run_save(&run,saved,sizeof saved);CHECK(n>0&&gc_run_load(&loaded,saved,n,&why)&&!loaded.load_dropped);
  gc_choice c=ordinary?(gc_choice){GC_CH_POKEMON,bench,-1,0}:(gc_choice){GC_CH_FIGHT,0,-1,0};
  CHECK(gc_run_choose(&run,&c,&why));CHECK(gc_run_choose(&loaded,&c,&why));drain(&run);drain(&loaded);complete_choices(&run);complete_choices(&loaded);
  if(!ordinary){
   CHECK(run.screen.kind==GC_SCREEN_SWITCH&&loaded.screen.kind==GC_SCREEN_SWITCH);
   c=(gc_choice){GC_CH_PICK,bench,-1,0};CHECK(gc_run_choose(&run,&c,&why));CHECK(gc_run_choose(&loaded,&c,&why));drain(&run);drain(&loaded);complete_choices(&run);complete_choices(&loaded);
  }
  CHECK(run.screen.kind==GC_SCREEN_COMMAND&&run.bt.field[0]->id==incoming);
  bool sent=!ordinary&&source&&!target;
  CHECK((held(&run,last)>=0)==(source&&!sent));
  CHECK((held(&run,incoming)>=0)==(target||sent));
  CHECK(run.n_items==source+target&&run.n_enemy_items==1);
  if(source&&!sent)CHECK(run.items[held(&run,last)].non_transferable==locked);
  if(sent)CHECK(!run.items[held(&run,incoming)].non_transferable);
  CHECK(run.bt.field[0]->stat_stages[STAT_ATK-1]==(ordinary?0:2));
  for(int k=0;k<run.n_party;k++)CHECK(!pr_get_battler_tag(&run.party[k].b,BATTLER_TAG_TYPE_UNBURDEN));
  CHECK(held(&run,last)==held(&loaded,last)&&held(&run,incoming)==held(&loaded,incoming));
  CHECK(!memcmp(&run.run.rng,&loaded.run.rng,sizeof(pr_rng)));
  CHECK(!memcmp(&run.bt.battle_rng,&loaded.bt.battle_rng,sizeof(pr_rng)));
  int count=run.n_items;gc_items_from_battle(&run);gc_items_from_battle(&run);CHECK(run.n_items==count);
  finish_to_save();n=gc_run_save(&run,saved,sizeof saved);
  CHECK(n>0&&gc_run_load(&after,saved,n,&why)&&!after.load_dropped);
  CHECK(after.n_items==run.n_items&&held(&after,last)==held(&run,last)&&held(&after,incoming)==held(&run,incoming));
 }
 printf("Baton real commands/inventory/save: %d checks passed\n",checks);return 0;
}
