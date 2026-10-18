/* Real cure/Heal Pulse commands and save continuity; augmented starting status/HP. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run run,loaded;
static unsigned char saved[GC_SAVE_MAX];
static int checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"Partner cures host line%d: %s\n",__LINE__,#x); exit(1); } } while (0)
int main(void)
{
    const char *why=NULL;
    int healed[2]={0};
    for (int mode=0;mode<4;mode++) {
        int move=mode==0?MOVE_ID_HEAL_BELL:mode==1?MOVE_ID_AROMATHERAPY:MOVE_ID_HEAL_PULSE;
        gc_run_new(&run,GAME_MODES_CLASSIC,"partner-command-save",777);
        gc_starter starters[2]={{.species=SPECIES_ID_BULBASAUR,.tera_type=-1},
                               {.species=SPECIES_ID_CHARMANDER,.tera_type=-1,.moves={MOVE_ID_SPLASH}}};
        starters[0].moves[0]=move;
        for(int k=0;k<2;k++)memset(starters[k].ivs,15,sizeof starters[k].ivs);
        CHECK(gc_run_set_starters(&run,starters,2,&why));
        gc_event ev;while(gc_run_next_event(&run,&ev)){}
        pr_pokemon *p=&run.party[0].b,*bench=&run.party[1].b,*enemy=&run.enemy[0].b;
        p->custom_ability=mode==3?ABILITY_ID_MEGA_LAUNCHER:ABILITY_ID_NONE;pr_refresh_ability(p);
        enemy->custom_ability=ABILITY_ID_NONE;pr_refresh_ability(enemy);
        enemy->n_moves=1;pr_set_original_move(enemy,0,MOVE_ID_SPLASH);
        if(mode<2){p->status=STATUS_EFFECT_POISON;bench->status=STATUS_EFFECT_BURN;}
        else enemy->hp=1;
        CHECK(gc_run_can_save(&run));
        size_t n=gc_run_save(&run,saved,sizeof saved);CHECK(n>0);
        CHECK(gc_run_load(&loaded,saved,n,&why));CHECK(!loaded.load_dropped);
        gc_choice fight={GC_CH_FIGHT,0,-1,0};
        CHECK(gc_run_choose(&run,&fight,&why));CHECK(gc_run_choose(&loaded,&fight,&why));
        CHECK(run.screen.kind==GC_SCREEN_COMMAND&&loaded.screen.kind==GC_SCREEN_COMMAND);
        if(mode<2){CHECK(!p->status&&!bench->status);CHECK(!loaded.party[0].b.status&&!loaded.party[1].b.status);}
        else {healed[mode-2]=enemy->hp-1;CHECK(healed[mode-2]>0);}
        CHECK(p->hp==loaded.party[0].b.hp&&enemy->hp==loaded.enemy[0].b.hp);
        CHECK(!memcmp(&run.run.rng,&loaded.run.rng,sizeof(pr_rng)));
        CHECK(!memcmp(&run.bt.rng,&loaded.bt.rng,sizeof(pr_rng)));
        CHECK(!memcmp(&run.bt.battle_rng,&loaded.bt.battle_rng,sizeof(pr_rng)));
    }
    CHECK(healed[1]>healed[0]);
    printf("Partner cures and Heal Pulse command/save: %d checks passed\n",checks);
    return 0;
}
