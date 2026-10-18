/* Augmented CommandedTag state, cached form saves, and actual Order Up commands. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run run,loaded;
static unsigned char saved[GC_SAVE_MAX];
static int checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"Commander host line%d: %s\n",__LINE__,#x); exit(1); } } while (0)
int main(void)
{
    const char *why=NULL,*keys[]={"curly","droopy","stretchy"};
    const int boosted[]={STAT_ATK,STAT_DEF,STAT_SPD};
    for(int mode=0;mode<3;mode++) {
        gc_run_new(&run,GAME_MODES_CLASSIC,"commander-cache-command-save",777);
        gc_starter starters[2]={{.species=SPECIES_ID_BULBASAUR,.tera_type=-1,.moves={MOVE_ID_ORDER_UP}},
                               {.species=SPECIES_ID_TATSUGIRI,.tera_type=-1,.moves={MOVE_ID_SPLASH}}};
        for(int k=0;k<2;k++)memset(starters[k].ivs,15,sizeof starters[k].ivs);
        CHECK(gc_run_set_starters(&run,starters,2,&why));
        gc_event ev;while(gc_run_next_event(&run,&ev)){}
        pr_pokemon *p=&run.party[0].b,*source=&run.party[1].b;
        source->form=mode;pr_recalculate_stats(&run.bt,source);
        p->custom_ability=source->custom_ability=ABILITY_ID_NONE;pr_refresh_ability(p);pr_refresh_ability(source);
        pr_enemy_gen gen={.id=run.enemy[0].b.id,.gender=-1,.fusion_species=-1};
        gc_mon_new_enemy(&run,&run.enemy[0],SPECIES_ID_CHANSEY,20,&gen,TRAINER_SLOT_NONE);
        pr_pokemon *enemy=&run.enemy[0].b;enemy->on_field=true;enemy->custom_ability=ABILITY_ID_NONE;pr_refresh_ability(enemy);
        enemy->n_moves=1;pr_set_original_move(enemy,0,MOVE_ID_SPLASH);
        pr_calculate_stats(&run.bt,enemy);
        run.bt.rng=run.run.rng;
        pr_engine scratch={0};scratch.current=-1;scratch.n_levels=1;pr_engine *old=run.bt.engine;run.bt.engine=&scratch;
        CHECK(pr_add_tag(&run.bt,&scratch,p,BATTLER_TAG_TYPE_COMMANDED,99,MOVE_ID_TACKLE,source->id));
        pr_run_player_cleanup(&run.bt,&scratch);run.bt.engine=old;
        CHECK(!run.bt.n_unported&&!scratch.overflow);
        const pr_battler_tag *tag=pr_get_battler_tag(p,BATTLER_TAG_TYPE_COMMANDED);
        CHECK(tag&&tag->turn_count==0&&tag->source_move==MOVE_ID_NONE&&tag->source_id==source->id);
        CHECK(!strcmp(pr_commanded_form_key(tag),keys[mode]));
        CHECK(p->stat_stages[boosted[mode]-1]==2);
        source->form=(mode+1)%3;pr_recalculate_stats(&run.bt,source);
        run.run.rng=run.bt.rng;
        CHECK(gc_run_can_save(&run));
        uint16_t cache=p->tags[0].commanded_form_key;
        p->tags[0].commanded_form_key=PR_FORM_COUNT+1;
        size_t n=gc_run_save(&run,saved,sizeof saved);CHECK(n>0);
        CHECK(!gc_run_load(&loaded,saved,n,&why)); /* valid checksum, invalid cache index */
        p->tags[0].commanded_form_key=cache;
        n=gc_run_save(&run,saved,sizeof saved);CHECK(n>0);
        CHECK(gc_run_load(&loaded,saved,n,&why));CHECK(!loaded.load_dropped);
        const pr_battler_tag *copy=pr_get_battler_tag(&loaded.party[0].b,BATTLER_TAG_TYPE_COMMANDED);
        CHECK(copy&&copy->commanded_form_key==tag->commanded_form_key&&!strcmp(pr_commanded_form_key(copy),keys[mode]));
        CHECK(loaded.party[1].b.form==(mode+1)%3);
        gc_choice fight={GC_CH_FIGHT,0,-1,0};
        CHECK(gc_run_choose(&run,&fight,&why));CHECK(gc_run_choose(&loaded,&fight,&why));
        CHECK(run.screen.kind==GC_SCREEN_COMMAND&&loaded.screen.kind==GC_SCREEN_COMMAND);
        CHECK(p->stat_stages[boosted[mode]-1]==3&&loaded.party[0].b.stat_stages[boosted[mode]-1]==3);
        CHECK(!memcmp(p->stat_stages,loaded.party[0].b.stat_stages,sizeof p->stat_stages));
        CHECK(p->hp==loaded.party[0].b.hp&&enemy->hp==loaded.enemy[0].b.hp);
        CHECK(!memcmp(&run.run.rng,&loaded.run.rng,sizeof(pr_rng)));
        CHECK(!memcmp(&run.bt.rng,&loaded.bt.rng,sizeof(pr_rng)));
        CHECK(!memcmp(&run.bt.battle_rng,&loaded.bt.battle_rng,sizeof(pr_rng)));
    }
    printf("Commander cached form/Order Up command/save: %d checks passed\n",checks);
    return 0;
}
