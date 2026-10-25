/* Doubles command ownership and save continuity for fused held-item actors. */
#define main retained_doubles_main
#include "doubles_host_test.c"
#undef main
static void play_turn(gc_run *run){
    int start=run->bt.turn,inputs=0;
    while(run->bt.turn==start&&inputs++<8){
        gc_choice c={0};
        if(run->screen.kind==GC_SCREEN_COMMAND)c=(gc_choice){GC_CH_FIGHT,0,-1,0};
        else if(run->screen.kind==GC_SCREEN_TARGET)c=(gc_choice){GC_CH_TARGET,2,0,0};
        else CHECK(false);
        choose(run,c);
    }
    CHECK(run->bt.turn==start+1&&!run->bt.n_unported&&!run->eng.overflow);
}
int main(void){
    const int eligible[]={SPECIES_ID_BULBASAUR,SPECIES_ID_PIKACHU,SPECIES_ID_CUBONE,SPECIES_ID_DITTO,SPECIES_ID_DITTO,SPECIES_ID_CLAMPERL,SPECIES_ID_CLAMPERL,SPECIES_ID_FARFETCHD};
    int cases=0;
    for(int family=0;family<8;family++)for(int shape=0;shape<4;shape++)for(int slot=0;slot<2;slot++){
        setup(1);r.n_items=r.n_enemy_items=0;
        for(int k=0;k<4;k++){
            pr_pokemon *p=r.bt.field[k];p->custom_ability=p->fusion_custom_ability=ABILITY_ID_NONE;
            pr_refresh_ability(p);p->passive=p->custom_passive=0;p->passive_flag=false;
            p->n_moves=1;p->moves[0]=MOVE_ID_SPLASH;
        }
        pr_pokemon *holder=r.bt.field[slot];
        holder->species=shape%2?SPECIES_ID_MEW:eligible[family];
        holder->fusion_species=shape<2?eligible[family]:SPECIES_ID_MEW;
        holder->form=holder->fusion_form=0;holder->fusion_ability_index=0;
        holder->moves[0]=MOVE_ID_TACKLE;holder->level=50;pr_calculate_stats(&r.bt,holder);
        for(int k=0;k<4;k++)r.bt.field[k]->hp=r.bt.field[k]->stats[STAT_HP]=100000;
        r.items[r.n_items++]=(gc_item){.key=family==0?"EVIOLITE":family==7?"LEEK":family<5?"RARE_SPECIES_STAT_BOOSTER":"SPECIES_STAT_BOOSTER",.arg=family==0||family==7?-1:family-1,.stack=1,.pokemon_id=holder->id};
        gc_items_to_battle(&r);r.bt.rng=r.run.rng;
        const char *why=NULL;size_t length=gc_run_save(&r,save,sizeof save);
        CHECK(length&&gc_run_load(&back,save,length,&why)&&!back.load_dropped);
        CHECK(back.bt.field[slot]->fusion_species==holder->fusion_species);
        play_turn(&r);play_turn(&back);
        for(int k=0;k<4;k++)CHECK(r.bt.field[k]->hp==back.bt.field[k]->hp);
        CHECK(rngsame(r.bt.rng,back.bt.rng)&&rngsame(r.bt.battle_rng,back.bt.battle_rng));
        CHECK(r.screen.kind==back.screen.kind&&r.n_items==back.n_items);
        cases++;
    }
    printf("Fused held-item doubles commands/save: %d cases, %d checks passed\n",cases,checks);return 0;
}
