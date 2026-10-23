/* Inventory mapping, non-consumption and real command/save/load boundaries.
 * Full upstream turn/state/message answers are in seed_diff_test. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>

static gc_run run, loaded;
static unsigned char saved[GC_SAVE_MAX];
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"seed host line%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void setup(void)
{
    gc_run_new(&run, GAME_MODES_CLASSIC, "seed-host", 777);
    gc_starter starter = {.species=SPECIES_ID_BULBASAUR,.tera_type=-1};
    memset(starter.ivs,15,6);
    const char *why;
    CHECK(gc_run_set_starters(&run,&starter,1,&why));
    CHECK(run.screen.kind == GC_SCREEN_COMMAND);
    for (int side=0; side<2; side++) {
        pr_pokemon *p=run.bt.field[2*side];
        CHECK(p != NULL);
        p->species=SPECIES_ID_RATTATA; p->form=0;
        p->ability=p->base_ability=ABILITY_ID_NONE; p->ability_override=-1;
        p->custom_passive=-1; p->passive_flag=false; p->passive=ABILITY_ID_NONE;
        p->status=STATUS_EFFECT_NONE; p->status_toxic_turns=0;
        p->n_moves=1; p->moves[0]=MOVE_ID_SPLASH;
        memset(p->pp_used,0,sizeof p->pp_used); memset(p->pp_up,0,sizeof p->pp_up);
        pr_calculate_stats(&run.bt,p); p->hp=p->stats[STAT_HP];
    }
    gc_event ev; while(gc_run_next_event(&run,&ev)) {}
}

static void boundary(void) {
 const char *why;for(int side=0;side<2;side++) for(int wildcard=0;wildcard<2;wildcard++) {
  setup();gc_item *items=side?run.enemy_items:run.items;int *n=side?&run.n_enemy_items:&run.n_items;
  items[0]=(gc_item){"REVIVER_SEED",-1,1,wildcard?0:run.bt.field[2*side]->id};*n=1;
  gc_items_to_battle(&run);CHECK(run.bt.mods[side][0].cls==PR_MOD_INSTANT_REVIVE&&!run.bt.mods[side][0].unknown_subclass);
  CHECK(gc_run_can_save(&run));size_t length=gc_run_save(&run,saved,sizeof saved);CHECK(length&&gc_run_load(&loaded,saved,length,&why));
  CHECK(loaded.bt.mods[side][0].cls==PR_MOD_INSTANT_REVIVE&&loaded.bt.mods[side][0].pokemon_id==(wildcard?-1:(int64_t)run.bt.field[2*side]->id));
  for(int copy=0;copy<2;copy++) {
   gc_run *r=copy?&loaded:&run;pr_pokemon *victim=r->bt.field[2*side],*attacker=r->bt.field[2*(1-side)];
   victim->hp=1;victim->status=STATUS_EFFECT_BURN;victim->status_toxic_turns=7;
   attacker->moves[0]=MOVE_ID_DRAGON_RAGE;
   gc_choice fight={GC_CH_FIGHT,0,-1,0};CHECK(gc_run_choose(r,&fight,&why));
   CHECK(victim->hp>0&&victim->status==STATUS_EFFECT_NONE);CHECK((side?r->n_enemy_items:r->n_items)==0);
   CHECK(r->bt.n_unported==0);gc_items_from_battle(r);CHECK((side?r->n_enemy_items:r->n_items)==0);
   gc_items_to_battle(r);CHECK(r->bt.n_mods[side]==0);
  }
  CHECK(run.bt.field[2*side]->hp==loaded.bt.field[2*side]->hp);CHECK(!memcmp(&run.run.rng,&loaded.run.rng,sizeof run.run.rng));
 }
}
static void inventory(void) {
 for(int side=0;side<2;side++) {
  memset(&run,0,sizeof run);gc_item *items=side?run.enemy_items:run.items;int *n=side?&run.n_enemy_items:&run.n_items;
  *n=GC_MAX_ITEMS;for(int i=0;i<*n;i++)items[i]=(gc_item){"REVIVER_SEED",-1,1,100+i};
  gc_items_to_battle(&run);CHECK(run.bt.n_mods[side]==0&&run.bt.n_unported);gc_items_from_battle(&run);
  CHECK(*n==GC_MAX_ITEMS&&items[0].pokemon_id==100&&items[GC_MAX_ITEMS-1].pokemon_id==100+GC_MAX_ITEMS-1);
  gc_items_from_battle(&run);CHECK(*n==GC_MAX_ITEMS);
  /* All supported entries can be consumed, with no unrepresented tail. */
  memset(&run,0,sizeof run);*n=PR_MAX_MODIFIERS;
  for(int i=0;i<*n;i++)items[i]=(gc_item){"REVIVER_SEED",-1,1,100+i};
  gc_items_to_battle(&run);CHECK(run.bt.n_mods[side]==PR_MAX_MODIFIERS&&!run.bt.n_unported);
  run.bt.n_mods[side]=0;gc_items_from_battle(&run);CHECK(*n==0);
  gc_items_from_battle(&run);CHECK(*n==0);gc_items_to_battle(&run);CHECK(run.bt.n_mods[side]==0);
  memset(&run,0,sizeof run);*n=3;items[0]=(gc_item){"REVIVER_SEED",-1,1,10};items[1]=(gc_item){"REVIVER_SEED",-1,1,20};items[2]=(gc_item){"REVIVER_SEED",0,1,20};
  gc_items_to_battle(&run);CHECK(!run.bt.mods[side][0].unknown_subclass);CHECK(run.bt.mods[side][1].unknown_subclass&&run.bt.mods[side][2].unknown_subclass);
  run.bt.n_mods[side]=0;gc_items_from_battle(&run);CHECK(*n==2&&items[0].pokemon_id==20&&items[1].pokemon_id==20);gc_items_from_battle(&run);CHECK(*n==2);
  memset(&run,0,sizeof run);*n=1;items[0]=(gc_item){"REVIVER_SEED",-1,2,10};gc_items_to_battle(&run);CHECK(run.bt.mods[side][0].unknown_subclass);run.bt.n_mods[side]=0;gc_items_from_battle(&run);CHECK(*n==1&&items[0].stack==2);
  memset(&run,0,sizeof run);*n=1;items[0]=(gc_item){"REVIVER_SEED",1,1,10};gc_items_to_battle(&run);CHECK(run.bt.mods[side][0].unknown_subclass);run.bt.n_mods[side]=0;gc_items_from_battle(&run);CHECK(*n==1&&items[0].arg==1);
 }
}
static void detached_save(void) {
 setup();uint32_t owner=run.bt.field[2]->id;
 run.enemy_items[0]=(gc_item){"REVIVER_SEED",-1,1,owner};run.n_enemy_items=1;gc_items_to_battle(&run);
 run.bt.field[2]->hp=0;run.bt.mods[1][0].pokemon_id=0;gc_items_from_battle(&run);
 CHECK(run.n_enemy_items==1&&run.enemy_items[0].detached&&run.enemy_items[0].pokemon_id==owner);
 gc_items_from_battle(&run);CHECK(run.n_enemy_items==1&&run.enemy_items[0].detached);
 gc_items_to_battle(&run);CHECK(run.bt.mods[1][0].pokemon_id==0);
 /* Named-field serialization probe at an otherwise legal command boundary.
  * Full source turn traces cover how this detached loot state is reached. */
 run.bt.field[2]->hp=run.bt.field[2]->stats[STAT_HP];
 const char *why;size_t n=gc_run_save(&run,saved,sizeof saved);CHECK(n&&gc_run_load(&loaded,saved,n,&why));
 CHECK(loaded.enemy_items[0].detached&&loaded.enemy_items[0].pokemon_id==owner&&loaded.bt.mods[1][0].pokemon_id==0);
 gc_items_to_battle(&loaded);CHECK(loaded.bt.mods[1][0].pokemon_id==0);
 /* Generic restacking has no validated replacement policy: it becomes opaque. */
 run.items[0]=run.enemy_items[0];run.n_items=1;gc_item_add(&run,"REVIVER_SEED",-1,owner,1);gc_items_to_battle(&run);
 CHECK(run.items[0].stack==2&&run.items[0].detached&&run.bt.mods[0][0].unknown_subclass);
 gc_items_drop_held_of(&run,owner);CHECK(run.n_items==0);

}
int main(int argc,char **argv){inventory();boundary();detached_save();
 if(argc==2){FILE *fp=fopen(argv[1],"rb");CHECK(fp!=NULL);size_t n=fread(saved,1,sizeof saved,fp);fclose(fp);const char *why;
  CHECK(gc_run_load(&loaded,saved,n,&why));CHECK(loaded.n_items==1&&!loaded.items[0].detached&&!strcmp(loaded.items[0].key,"REVIVER_SEED"));
  CHECK(loaded.bt.mods[0][0].pokemon_id==(int64_t)loaded.party[loaded.party_order[0]].b.id);
 }
 printf("Seed host: %u checks passed\n",checks);return 0;}
