/* Actual attack commands, inventory synchronization and native tier-reset saves.
 * The independent upstream capture proves selection, callbacks and exact text. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run run,loaded;
static unsigned char saved[GC_SAVE_MAX];
static int checks;
#define CHECK(x) do {checks++;if(!(x)){fprintf(stderr,"Grip Claw host line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void drain(gc_run *r){gc_event ev;while(gc_run_next_event(r,&ev)){};}
static void setup(int actor,int guard,int seed,int attack)
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
        p->n_moves=1;p->moves[0]=side==actor?(attack==0?MOVE_ID_TACKLE:attack==1?MOVE_ID_WATER_GUN:MOVE_ID_SPLASH):MOVE_ID_SPLASH;
        p->pp_used[0]=p->pp_up[0]=0;p->max_pp_override[0]=-1;
        p->stats[STAT_HP]=p->hp=500;
    }
    pr_pokemon *donor=run.bt.field[2*(1-actor)];
    gc_item stock={.key="WHITE_HERB",.arg=-1,.stack=2,.pokemon_id=donor->id,.non_transferable=guard==2};
    if(actor){run.items[0]=stock;run.n_items=1;}else{run.enemy_items[0]=stock;run.n_enemy_items=1;}
    if(guard==3){stock.pokemon_id=run.bt.field[2*actor]->id;stock.non_transferable=false;if(actor){run.enemy_items[0]=stock;run.n_enemy_items=1;}else{run.items[0]=stock;run.n_items=1;}}
    gc_item *holder_items=actor?run.enemy_items:run.items;
    int *holder_count=actor?&run.n_enemy_items:&run.n_items;
    holder_items[(*holder_count)++]=(gc_item){.key="GRIP_CLAW",.arg=-1,.stack=5,.pokemon_id=run.bt.field[2*actor]->id};
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
    int successes[2][2]={{0}};const char *why;
    for(int side=0;side<2;side++)for(int attack=0;attack<3;attack++)for(int guard=0;guard<4;guard++)for(int seed=0;seed<32;seed++){
        setup(side,guard,seed,attack);uint32_t owner=run.bt.field[2*side]->id,donor=run.bt.field[2*(1-side)]->id;
        size_t size=gc_run_save(&run,saved,sizeof saved);CHECK(size>0);
        CHECK(gc_run_load(&loaded,saved,size,&why));CHECK(!loaded.load_dropped);
        gc_choice fight={GC_CH_FIGHT,0,-1,0};CHECK(gc_run_choose(&run,&fight,&why));CHECK(gc_run_choose(&loaded,&fight,&why));drain(&run);drain(&loaded);
        int got=held(&run,side,owner),left=held(&run,1-side,donor);
        if(guard||attack==2)CHECK(got==(guard==3?2:0)&&left==2);
        else{CHECK((got==0&&left==2)||(got==1&&left==1));successes[side][attack]+=got==1;}
        CHECK(got==held(&loaded,side,owner)&&left==held(&loaded,1-side,donor));
        CHECK(!memcmp(&run.run.rng,&loaded.run.rng,sizeof(pr_rng)));
        CHECK(!memcmp(&run.bt.battle_rng,&loaded.bt.battle_rng,sizeof(pr_rng)));
        CHECK(run.party[0].b.hp==loaded.party[0].b.hp&&run.enemy[0].b.hp==loaded.enemy[0].b.hp);
        /* Repeated projection retains caches on stocks even when not selected. */
        gc_item *items=side?run.enemy_items:run.items;int n=side?run.n_enemy_items:run.n_items;
        int cache=n?items[0].type_tier:0;gc_items_from_battle(&run);CHECK(!n||items[0].type_tier==cache);
    }
    for(int side=0;side<2;side++)for(int attack=0;attack<2;attack++)CHECK(successes[side][attack]>0);
    /* Projection and named-field save/load retain Grip Claw's canonical stock
     * and flag. Duplicate synchronization must not create another copy. */
    setup(0,0,0,0);run.items[0].non_transferable=true;
    gc_items_to_battle(&run);CHECK(run.bt.mods[0][0].cls==PR_MOD_CONTACT_HELD_TRANSFER);
    CHECK(!run.bt.mods[0][0].is_transferable&&run.bt.mods[0][0].mult==.1);
    gc_items_from_battle(&run);gc_items_from_battle(&run);
    CHECK(run.n_items==1&&!strcmp(run.items[0].key,"GRIP_CLAW")&&run.items[0].stack==5&&run.items[0].non_transferable);
    size_t size=gc_run_save(&run,saved,sizeof saved);CHECK(size>0);
    CHECK(gc_run_load(&loaded,saved,size,&why)&&!loaded.load_dropped);
    CHECK(loaded.n_items==1&&loaded.items[0].stack==5&&loaded.items[0].non_transferable);
    CHECK(loaded.bt.mods[0][0].cls==PR_MOD_CONTACT_HELD_TRANSFER&&loaded.bt.mods[0][0].mult==.1);
    printf("Grip Claw real attack/status commands and inventory/save: %d checks; stolen contact/noncontact %d/%d, %d/%d\n",checks,successes[0][0],successes[0][1],successes[1][0],successes[1][1]);
    return 0;
}
