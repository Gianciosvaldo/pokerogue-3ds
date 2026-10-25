/* Actual attack commands, inventory synchronization and native tier-reset saves.
 * The independent upstream capture proves selection, callbacks and exact text. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run run,loaded;
static unsigned char saved[GC_SAVE_MAX];
static int checks;
#define CHECK(x) do {checks++;if(!(x)){fprintf(stderr,"move theft host line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void drain(gc_run *r){gc_event ev;while(gc_run_next_event(r,&ev)){};}
static void setup(int actor,int guard,int seed)
{
    char text[48];snprintf(text,sizeof text,"move-theft-%d",seed);
    gc_run_new(&run,GAME_MODES_CLASSIC,text,777);
    gc_starter starter={.species=SPECIES_ID_BULBASAUR,.tera_type=-1,.moves={MOVE_ID_SPLASH}};
    memset(starter.ivs,15,sizeof starter.ivs);const char *why;
    CHECK(gc_run_set_starters(&run,&starter,1,&why));drain(&run);
    CHECK(gc_run_can_save(&run));run.n_items=run.n_enemy_items=0;
    for(int side=0;side<2;side++){
        pr_pokemon *p=run.bt.field[2*side];CHECK(p);
        p->custom_ability=side!=actor&&guard==1?ABILITY_ID_STICKY_HOLD:ABILITY_ID_NONE;
        p->ability_override=-1;p->passive_flag=false;pr_refresh_ability(p);
        p->n_moves=1;p->moves[0]=side==actor?(seed%2?MOVE_ID_THIEF:MOVE_ID_COVET):MOVE_ID_SPLASH;
        p->pp_used[0]=p->pp_up[0]=0;p->max_pp_override[0]=-1;
        p->stats[STAT_HP]=p->hp=500;
    }
    if(run.bt.field[3]){pr_pokemon *ally=run.bt.field[3];ally->custom_ability=ABILITY_ID_NONE;ally->ability_override=-1;pr_refresh_ability(ally);ally->n_moves=1;ally->moves[0]=MOVE_ID_SPLASH;ally->pp_used[0]=0;}
    pr_pokemon *donor=run.bt.field[2*(1-actor)];
    gc_item stock={.key="WHITE_HERB",.arg=-1,.stack=2,.pokemon_id=donor->id,.non_transferable=guard==2};
    if(actor){run.items[0]=stock;run.n_items=1;}else{run.enemy_items[0]=stock;run.n_enemy_items=1;}
    if(guard==3){stock.pokemon_id=run.bt.field[2*actor]->id;stock.non_transferable=false;if(actor){run.enemy_items[0]=stock;run.n_enemy_items=1;}else{run.items[0]=stock;run.n_items=1;}}
    gc_items_to_battle(&run);
}
static int held(const gc_run *r,int side,uint32_t owner)
{
    const gc_item *items=side?r->enemy_items:r->items;int n=side?r->n_enemy_items:r->n_items;
    for(int i=0;i<n;i++)if(items[i].pokemon_id==owner&&!strcmp(items[i].key,"WHITE_HERB"))return items[i].stack;
    return 0;
}
int main(void)
{
    int successes[2]={0};const char *why;
    for(int side=0;side<2;side++)for(int guard=0;guard<4;guard++)for(int seed=0;seed<32;seed++){
        setup(side,guard,seed);uint32_t owner=run.bt.field[2*side]->id,donor=run.bt.field[2*(1-side)]->id;
        size_t size=gc_run_save(&run,saved,sizeof saved);CHECK(size>0);
        CHECK(gc_run_load(&loaded,saved,size,&why));CHECK(!loaded.load_dropped);
        gc_choice fight={GC_CH_FIGHT,0,side?-1:2,0};CHECK(gc_run_choose(&run,&fight,&why));CHECK(gc_run_choose(&loaded,&fight,&why));drain(&run);drain(&loaded);
        int got=held(&run,side,owner),left=held(&run,1-side,donor);
        if(guard)CHECK(got==(guard==3?2:0)&&left==2);
        else{CHECK((got==0&&left==2)||(got==1&&left==1));successes[side]+=got==1;}
        CHECK(got==held(&loaded,side,owner)&&left==held(&loaded,1-side,donor));
        CHECK(!memcmp(&run.run.rng,&loaded.run.rng,sizeof(pr_rng)));
        CHECK(!memcmp(&run.bt.battle_rng,&loaded.bt.battle_rng,sizeof(pr_rng)));
        CHECK(run.party[0].b.hp==loaded.party[0].b.hp&&run.enemy[0].b.hp==loaded.enemy[0].b.hp);
        /* Repeated projection retains caches on stocks even when not selected. */
        gc_item *items=side?run.enemy_items:run.items;int n=side?run.n_enemy_items:run.n_items;
        int cache=n?items[0].type_tier:0;gc_items_from_battle(&run);CHECK(!n||items[0].type_tier==cache);
    }
    CHECK(successes[0]>0&&successes[1]>0);
    /* Native ModifierData deliberately omits type.tier. Saving must not mutate
     * the live cache; loading reconstructs a fresh type with an absent tier. */
    for(int tier=0;tier<6;tier++){
        setup(0,0,tier);run.enemy_items[0].type_tier=tier+1;gc_items_to_battle(&run);
        size_t size=gc_run_save(&run,saved,sizeof saved);CHECK(size>0);
        CHECK(run.enemy_items[0].type_tier==tier+1);
        CHECK(gc_run_load(&loaded,saved,size,&why));CHECK(!loaded.load_dropped);
        CHECK(loaded.enemy_items[0].type_tier==0);
        CHECK(loaded.bt.mods[1][0].type_tier==0);
    }
    /* Newly taken options keep their actual tier; stacking retains the old
     * type, and generated trainer stock starts at its pool tier. */
    setup(0,0,0);run.n_items=0;
    gc_offer option={.key="FOCUS_BAND",.arg=-1,.target=GC_TARGET_POKEMON,.tier=4};
    CHECK(gc_port_apply_reward(&run,&option,0,-1,&why));
    CHECK(run.n_items==1&&run.items[0].type_tier==5);
    option.tier=2;CHECK(gc_port_apply_reward(&run,&option,0,-1,&why));
    CHECK(run.n_items==1&&run.items[0].stack==2&&run.items[0].type_tier==5);
    run.battle_type=BATTLE_TYPE_TRAINER;run.enemy_items[0].key="LEFTOVERS";
    run.enemy_items[0].type_tier=0;gc_cache_generated_item_tiers(&run);
    CHECK(run.enemy_items[0].type_tier==5);
    run.enemy_items[0].key="MINI_BLACK_HOLE";gc_cache_generated_item_tiers(&run);
    CHECK(run.enemy_items[0].type_tier==0);
    printf("Move theft command/inventory/tier-reset save: %d checks passed, stolen %d/%d\n",checks,successes[0],successes[1]);
    return 0;
}
