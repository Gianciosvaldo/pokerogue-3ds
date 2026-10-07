/* Inventory mapping, non-consumption and real command/save/load boundaries.
 * Full upstream turn/state/message answers are in orb_diff_test. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>

static gc_run run, loaded;
static unsigned char saved[GC_SAVE_MAX];
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"orb host line%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static const char *keys[] = {"TOXIC_ORB", "FLAME_ORB"};
static const int effects[] = {STATUS_EFFECT_TOXIC, STATUS_EFFECT_BURN};

static void setup(void)
{
    gc_run_new(&run, GAME_MODES_CLASSIC, "orb-host", 777);
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

static void same_item(const gc_item *a,const gc_item *b)
{
    CHECK(!strcmp(a->key,b->key)); CHECK(a->arg==b->arg);
    CHECK(a->stack==b->stack); CHECK(a->pokemon_id==b->pokemon_id);
}

static void mapping(void)
{
    for(int side=0;side<2;side++) for(int orb=0;orb<2;orb++) for(int owner=0;owner<3;owner++) {
        memset(&run,0,sizeof run);
        gc_item *it=side?run.enemy_items:run.items;
        *it=(gc_item){keys[orb],-1,1,owner==0?123:owner==1?0:456};
        if(side)run.n_enemy_items=1;else run.n_items=1;
        pr_rng before=run.bt.rng;
        gc_items_to_battle(&run);
        const pr_modifier *m=&run.bt.mods[side][0];
        CHECK(run.bt.n_mods[side]==1 && run.bt.n_mods[1-side]==0);
        CHECK(m->cls==PR_MOD_TURN_STATUS_EFFECT && !m->unknown_subclass);
        CHECK(m->status_effect==effects[orb] && m->is_transferable);
        CHECK(m->pokemon_id==(owner==0?123:owner==1?-1:456) && m->stack_count==1);
        CHECK(!memcmp(&before,&run.bt.rng,sizeof before));
        gc_item old=*it; gc_items_from_battle(&run); same_item(&old,it);
    }
    memset(&run,0,sizeof run);
    gc_item_add(&run,"SCOPE_LENS",-1,123,2);
    gc_item_add(&run,"TOXIC_ORB",-1,123,1);
    gc_item_add(&run,"FLAME_ORB",-1,456,1);
    gc_item_add(&run,"UNSUPPORTED_TEST_ITEM",-1,123,1);
    gc_items_to_battle(&run);
    CHECK(run.bt.n_mods[0]==4);
    CHECK(run.bt.mods[0][0].cls==PR_MOD_CRIT_BOOSTER && !run.bt.mods[0][0].unknown_subclass);
    CHECK(run.bt.mods[0][1].status_effect==STATUS_EFFECT_TOXIC);
    CHECK(run.bt.mods[0][2].status_effect==STATUS_EFFECT_BURN);
    CHECK(run.bt.mods[0][3].cls==PR_MOD_HELD_ITEM && run.bt.mods[0][3].unknown_subclass);
}

static void command_resume(void)
{
    for(int orb=0;orb<2;orb++) {
        setup();
        const char *why;
        gc_offer offer={.key=keys[orb],.arg=-1,.target=GC_TARGET_POKEMON,.cost=-1};
        CHECK(gc_port_apply_reward(&run,&offer,run.party_order[0],-1,&why));
        CHECK(run.stubs[GC_STUB_REWARD_APPLY]==0);
        CHECK(run.n_items==1 && run.items[0].pokemon_id==run.bt.field[0]->id);
        run.enemy_items[0]=(gc_item){keys[1-orb],-1,1,run.bt.field[2]->id};run.n_enemy_items=1;
        gc_items_to_battle(&run);
        const gc_item old[]={run.items[0],run.enemy_items[0]};
        CHECK(gc_run_can_save(&run));size_t n=gc_run_save(&run,saved,sizeof saved);
        CHECK(n>0 && gc_run_load(&loaded,saved,n,&why));
        CHECK(loaded.bt.mods[0][0].cls==PR_MOD_TURN_STATUS_EFFECT);
        CHECK(loaded.bt.mods[1][0].cls==PR_MOD_TURN_STATUS_EFFECT);
        CHECK(loaded.bt.mods[0][0].status_effect==effects[orb]);
        CHECK(loaded.bt.mods[1][0].status_effect==effects[1-orb]);
        same_item(&old[0],&loaded.items[0]);same_item(&old[1],&loaded.enemy_items[0]);
        gc_choice fight={GC_CH_FIGHT,0,-1,0};
        CHECK(gc_run_choose(&run,&fight,&why));CHECK(gc_run_choose(&loaded,&fight,&why));
        CHECK(run.screen.kind==GC_SCREEN_COMMAND && loaded.screen.kind==GC_SCREEN_COMMAND);
        for(int side=0;side<2;side++) {
            pr_pokemon *p=run.bt.field[2*side],*q=loaded.bt.field[2*side];
            CHECK(p->status==effects[side?1-orb:orb] && q->status==p->status);
            CHECK(p->hp==q->hp && !memcmp(p->stats,q->stats,sizeof p->stats));
            CHECK(p->status_toxic_turns==q->status_toxic_turns);
            CHECK(p->pp_used[0]==q->pp_used[0] && p->pp_used[0]==1);
        }
        CHECK(!memcmp(&run.run.rng,&loaded.run.rng,sizeof run.run.rng));
        CHECK(run.bt.n_unported==0 && loaded.bt.n_unported==0);
        CHECK(run.n_items==1 && run.n_enemy_items==1 && loaded.n_items==1 && loaded.n_enemy_items==1);
        same_item(&old[0],&run.items[0]);same_item(&old[1],&run.enemy_items[0]);
        same_item(&old[0],&loaded.items[0]);same_item(&old[1],&loaded.enemy_items[0]);
        CHECK(!gc_run_can_save(&run)); CHECK(gc_run_save(&run,saved,sizeof saved)==0);
        const int wave=run.wave;
        for(int guard=0;run.wave==wave || run.screen.kind!=GC_SCREEN_COMMAND;guard++) {
            CHECK(guard<16);
            gc_choice next;
            if(run.screen.kind==GC_SCREEN_COMMAND) {
                run.bt.field[2]->hp=1; next=fight;
            } else if(run.screen.kind==GC_SCREEN_REWARD) {
                next=(gc_choice){GC_CH_SKIP,0,0,0};
            } else {
                fprintf(stderr,"unexpected Orb boundary screen %d\n",run.screen.kind); exit(1);
            }
            CHECK(gc_run_choose(&run,&next,&why));
            gc_event ev;while(gc_run_next_event(&run,&ev)) {}
        }
        CHECK(run.wave==wave+1 && gc_run_can_save(&run));n=gc_run_save(&run,saved,sizeof saved);
        CHECK(n>0 && gc_run_load(&loaded,saved,n,&why));
        CHECK(loaded.bt.field[0]->status==effects[orb]);
        CHECK(loaded.bt.mods[0][0].status_effect==effects[orb]);
    }
}

int main(void)
{
    mapping();command_resume();
    printf("Orb host: %u checks passed\n",checks);
    return 0;
}
