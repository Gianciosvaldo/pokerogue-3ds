/* Augmented command/save fixture; not a natural doubles run or legal-clear proof. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run run,loaded;static unsigned char saved[GC_SAVE_MAX];static int checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"Helping Hand host line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
int main(void){int damage[2];const char *why=NULL;
 for(int helping=0;helping<2;helping++){
  gc_run_new(&run,GAME_MODES_CLASSIC,"helping-command-save",777);gc_starter starters[2]={{.species=SPECIES_ID_BULBASAUR,.tera_type=-1,.moves={MOVE_ID_TACKLE}},{.species=SPECIES_ID_CHARMANDER,.tera_type=-1,.moves={MOVE_ID_SCRATCH}}};for(int k=0;k<2;k++)memset(starters[k].ivs,15,sizeof starters[k].ivs);CHECK(gc_run_set_starters(&run,starters,2,&why));gc_event ev;while(gc_run_next_event(&run,&ev)){}CHECK(gc_run_can_save(&run));
  pr_enemy_gen gen={.id=run.enemy[0].b.id,.gender=-1,.fusion_species=-1};gc_mon_new_enemy(&run,&run.enemy[0],SPECIES_ID_CHANSEY,20,&gen,TRAINER_SLOT_NONE);pr_pokemon *enemy=&run.enemy[0].b,*p=&run.party[0].b;run.bt.field[2]=run.bt.party[1][0]=enemy;enemy->on_field=true;enemy->n_moves=1;pr_set_original_move(enemy,0,MOVE_ID_SPLASH);enemy->custom_ability=ABILITY_ID_NONE;pr_refresh_ability(enemy);pr_calculate_stats(&run.bt,enemy);enemy->hp=enemy->stats[STAT_HP];
  if(helping){pr_engine scratch={0};scratch.current=-1;scratch.n_levels=1;pr_engine *old=run.bt.engine;run.bt.engine=&scratch;CHECK(pr_add_tag(&run.bt,&scratch,p,BATTLER_TAG_TYPE_HELPING_HAND,99,MOVE_ID_TACKLE,run.party[1].b.id));run.bt.engine=old;CHECK(p->n_tags==1&&p->tags[0].turn_count==1&&p->tags[0].source_move==MOVE_ID_HELPING_HAND);}
  int before=enemy->hp;size_t n=gc_run_save(&run,saved,sizeof saved);CHECK(n>0);CHECK(gc_run_load(&loaded,saved,n,&why));CHECK(!loaded.load_dropped);CHECK(loaded.party[0].b.n_tags==helping);if(helping)CHECK(loaded.party[0].b.tags[0].source_id==run.party[1].b.id&&loaded.party[0].b.tags[0].source_move==MOVE_ID_HELPING_HAND&&loaded.party[0].b.tags[0].turn_count==1);
  gc_choice fight={GC_CH_FIGHT,0,-1,0};CHECK(gc_run_choose(&run,&fight,&why));CHECK(gc_run_choose(&loaded,&fight,&why));CHECK(enemy->hp>0&&enemy->hp<before);damage[helping]=before-enemy->hp;CHECK(loaded.enemy[0].b.hp==enemy->hp&&loaded.party[0].b.hp==p->hp);CHECK(!p->n_tags&&!loaded.party[0].b.n_tags);CHECK(!memcmp(&run.run.rng,&loaded.run.rng,sizeof run.run.rng));
 }
 CHECK(damage[1]>damage[0]);printf("Helping Hand command/save/expiry: %d checks passed\n",checks);return 0;
}
