/* Actual owning commands and augmented attention-tag save/expiry continuity. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run run,loaded;
static unsigned char saved[GC_SAVE_MAX];
static int checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"Attention host line%d: %s\n",__LINE__,#x); exit(1); } } while (0)
int main(void)
{
    const char *why=NULL;
    const int moves[]={MOVE_ID_FOLLOW_ME,MOVE_ID_RAGE_POWDER,MOVE_ID_SPOTLIGHT};
    for(int mode=0;mode<3;mode++) for(int seeded=0;seeded<2;seeded++) {
        gc_run_new(&run,GAME_MODES_CLASSIC,"attention-command-save",777);
        gc_starter starters[2]={{.species=SPECIES_ID_BULBASAUR,.tera_type=-1},
                               {.species=SPECIES_ID_CHARMANDER,.tera_type=-1,.moves={MOVE_ID_SPLASH}}};
        starters[0].moves[0]=moves[mode];
        for(int k=0;k<2;k++)memset(starters[k].ivs,15,sizeof starters[k].ivs);
        CHECK(gc_run_set_starters(&run,starters,2,&why));
        gc_event ev;while(gc_run_next_event(&run,&ev)){}
        pr_pokemon *p=&run.party[0].b,*bench=&run.party[1].b,*enemy=&run.enemy[0].b;
        p->custom_ability=enemy->custom_ability=ABILITY_ID_NONE;pr_refresh_ability(p);pr_refresh_ability(enemy);
        enemy->n_moves=1;pr_set_original_move(enemy,0,MOVE_ID_SPLASH);
        if(seeded) {
        pr_engine scratch={0};scratch.current=-1;scratch.n_levels=1;pr_engine *old=run.bt.engine;run.bt.engine=&scratch;
        CHECK(pr_add_tag(&run.bt,&scratch,p,BATTLER_TAG_TYPE_CENTER_OF_ATTENTION,99,moves[mode],bench->id));
        CHECK(p->tags[p->n_tags-1].turn_count==1&&p->tags[p->n_tags-1].source_move==moves[mode]&&p->tags[p->n_tags-1].source_id==-1);
        CHECK(!pr_add_tag(&run.bt,&scratch,bench,BATTLER_TAG_TYPE_CENTER_OF_ATTENTION,99,moves[mode],p->id));
        run.bt.engine=old;
        }
        CHECK(gc_run_can_save(&run));
        size_t n=gc_run_save(&run,saved,sizeof saved);CHECK(n>0);
        CHECK(gc_run_load(&loaded,saved,n,&why));CHECK(!loaded.load_dropped);
        const pr_battler_tag *t=pr_get_battler_tag(&loaded.party[0].b,BATTLER_TAG_TYPE_CENTER_OF_ATTENTION);
        CHECK(seeded ? t&&t->turn_count==1&&t->source_move==moves[mode]&&t->source_id==-1 : !t);
        gc_choice fight={GC_CH_FIGHT,0,-1,0};
        CHECK(gc_run_choose(&run,&fight,&why));CHECK(gc_run_choose(&loaded,&fight,&why));
        CHECK(run.screen.kind==GC_SCREEN_COMMAND&&loaded.screen.kind==GC_SCREEN_COMMAND);
        CHECK(!pr_get_battler_tag(p,BATTLER_TAG_TYPE_CENTER_OF_ATTENTION));
        CHECK(!pr_get_battler_tag(enemy,BATTLER_TAG_TYPE_CENTER_OF_ATTENTION));
        CHECK(p->hp==loaded.party[0].b.hp&&enemy->hp==loaded.enemy[0].b.hp);
        CHECK(!memcmp(&run.run.rng,&loaded.run.rng,sizeof(pr_rng)));
        CHECK(!memcmp(&run.bt.rng,&loaded.bt.rng,sizeof(pr_rng)));
        CHECK(!memcmp(&run.bt.battle_rng,&loaded.bt.battle_rng,sizeof(pr_rng)));
    }
    printf("Attention commands/save/expiry: %d checks passed\n",checks);
    return 0;
}
