/* Owning commands and the game inventory/save adapter. Native leaves, phase
 * execution and complete doubles turns have separate differential captures. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run run,loaded;
static unsigned char saved[GC_SAVE_MAX];
static int checks;
#define CHECK(x) do {checks++;if(!(x)){fprintf(stderr,"berry moves host line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void drain(gc_run *r){gc_event ev;while(gc_run_next_event(r,&ev)){};}
static int berries(const gc_run *r,int side,uint32_t owner)
{
    const gc_item *a=side?r->enemy_items:r->items;int n=side?r->n_enemy_items:r->n_items,result=0;
    for(int k=0;k<n;k++)if(!strcmp(a[k].key,"BERRY")&&a[k].pokemon_id==owner)result+=a[k].stack;
    return result;
}
static void add(int side,int berry,uint32_t owner)
{
    gc_item item={.key="BERRY",.arg=berry,.stack=1,.pokemon_id=owner};
    if(side)run.enemy_items[run.n_enemy_items++]=item;else run.items[run.n_items++]=item;
}
int main(void)
{
    const int moves[]={MOVE_ID_STUFF_CHEEKS,MOVE_ID_TEATIME,MOVE_ID_BUG_BITE,MOVE_ID_PLUCK};const char *why;
    for(int mode=0;mode<4;mode++)for(int type=0;type<=BERRY_TYPE_LEPPA;type++)for(int side=0;side<2;side++)for(int pouch=0;pouch<2;pouch++){
        gc_run_new(&run,GAME_MODES_CLASSIC,"berry-move-command-save",777);
        gc_starter starter={.species=SPECIES_ID_BULBASAUR,.tera_type=-1,.moves={MOVE_ID_SPLASH}};memset(starter.ivs,15,sizeof starter.ivs);
        CHECK(gc_run_set_starters(&run,&starter,1,&why));drain(&run);run.n_items=run.n_enemy_items=0;
        for(int k=0;k<2;k++){
            gc_mon *m=k?&run.enemy[0]:&run.party[0];m->b.custom_ability=k==side?(type%2?ABILITY_ID_RIPEN:ABILITY_ID_CHEEK_POUCH):ABILITY_ID_NONE;m->b.passive_flag=false;pr_refresh_ability(&m->b);
            m->b.n_moves=1;gc_mon_set_move(m,0,k==side?moves[mode]:MOVE_ID_SPLASH);
            m->b.stats[STAT_HP]=m->b.hp=500;
        }
        pr_pokemon *actor=run.bt.field[side*2],*other=run.bt.field[(1-side)*2];CHECK(actor&&other);
        int donor=mode>=2?1-side:side;
        add(donor,type,run.bt.field[donor*2]->id);if(mode==1)add(1-donor,type,run.bt.field[(1-donor)*2]->id);
        if(pouch)for(int k=0;k<2;k++){gc_item item={.key="BERRY_POUCH",.arg=-1,.stack=3};if(k)run.enemy_items[run.n_enemy_items++]=item;else run.items[run.n_items++]=item;}
        gc_items_to_battle(&run);CHECK(gc_run_can_save(&run));size_t n=gc_run_save(&run,saved,sizeof saved);CHECK(n>0);CHECK(gc_run_load(&loaded,saved,n,&why));CHECK(!loaded.load_dropped);
        gc_choice fight={GC_CH_FIGHT,0,mode>=2&&!side?2:-1,0};CHECK(gc_run_choose(&run,&fight,&why));CHECK(gc_run_choose(&loaded,&fight,&why));drain(&run);drain(&loaded);
        CHECK(actor->has_eaten_berry);if(mode==1)CHECK(other->has_eaten_berry);
        if(mode==0)CHECK(actor->stat_stages[STAT_DEF-1]>=2);
        if(mode>=2){CHECK(berries(&run,1-side,other->id)==0);CHECK(actor->n_battle_berries==0);}
        else if(!pouch){CHECK(berries(&run,side,actor->id)==0);CHECK(actor->n_battle_berries>=1);}
        for(int k=0;k<2;k++){
            const pr_pokemon *a=k?&run.enemy[0].b:&run.party[0].b,*b=k?&loaded.enemy[0].b:&loaded.party[0].b;
            CHECK(a->hp==b->hp&&a->status==b->status&&a->has_eaten_berry==b->has_eaten_berry);
            CHECK(a->n_battle_berries==b->n_battle_berries&&a->n_turn_berries==b->n_turn_berries&&a->n_last_berries==b->n_last_berries);
            CHECK(!memcmp(a->battle_berries,b->battle_berries,a->n_battle_berries));CHECK(!memcmp(a->last_berries,b->last_berries,a->n_last_berries));
            CHECK(!memcmp(a->stat_stages,b->stat_stages,sizeof a->stat_stages));CHECK(berries(&run,k,a->id)==berries(&loaded,k,b->id));
        }
        CHECK(!memcmp(&run.run.rng,&loaded.run.rng,sizeof(pr_rng)));CHECK(!memcmp(&run.bt.battle_rng,&loaded.bt.battle_rng,sizeof(pr_rng)));
    }
    printf("Berry move commands/inventory/save: %d checks passed\n",checks);return 0;
}
