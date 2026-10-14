/* Explicit native item arguments through the bridge, form changes and legal saves. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run run,loaded;static unsigned char saved[GC_SAVE_MAX];static int checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"stat order host line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void drain(void){gc_event e;while(gc_run_next_event(&run,&e)){} }
int main(void){
 const char *why=NULL;
 for(int fused=0;fused<2;fused++)for(int negative=0;negative<2;negative++){
  gc_run_new(&run,GAME_MODES_CLASSIC,"stat-order-save",777);gc_starter s={.species=SPECIES_ID_BULBASAUR,.tera_type=-1,.moves={MOVE_ID_TACKLE,MOVE_ID_SPLASH}};memset(s.ivs,15,sizeof s.ivs);CHECK(gc_run_set_starters(&run,&s,1,&why));drain();pr_pokemon *p=run.bt.field[0];CHECK(p);
  p->species=SPECIES_ID_MORPEKO;p->form=0;p->level=22;p->fusion_species=fused?SPECIES_ID_MAGIKARP:0;p->fusion_form=0;p->fusion_ability_index=0;p->custom_ability=ABILITY_ID_HUNGER_SWITCH;p->ability_override=-1;pr_refresh_ability(p);run.party[run.party_order[0]].exp=gc_level_total_exp(22,gc_species(p->species)->growth_rate);
  gc_item_add(&run,"MYSTERY_ENCOUNTER_SHUCKLE_JUICE",negative?-15:10,p->id,1);gc_item_add(&run,"MYSTERY_ENCOUNTER_OLD_GATEAU",0,p->id,1);gc_item_add(&run,"BASE_STAT_BOOSTER",STAT_ATK,p->id,1);gc_items_to_battle(&run);
  CHECK(run.bt.n_mods[0]==3);for(int k=0;k<3;k++){CHECK(!run.bt.mods[0][k].unknown_subclass&&!run.bt.mods[0][k].is_transferable);CHECK(pr_mod_is_ported_item(&run.bt.mods[0][k]));}
  pr_recalculate_stats(&run.bt,p);p->hp=p->stats[STAT_HP];pr_change_form_state(&run.bt,p,1);CHECK(p->form==1&&!run.bt.n_unported);CHECK(gc_run_can_save(&run));size_t n=gc_run_save(&run,saved,sizeof saved);CHECK(n>0);CHECK(gc_run_load(&loaded,saved,n,&why));pr_pokemon *q=loaded.bt.field[0];CHECK(q&&q->fusion_species==p->fusion_species&&q->form==1&&q->hp==p->hp&&!memcmp(q->stats,p->stats,sizeof p->stats));CHECK(loaded.n_items==3);for(int k=0;k<3;k++)CHECK(!loaded.bt.mods[0][k].unknown_subclass&&!loaded.bt.mods[0][k].is_transferable);
  pr_change_form_state(&run.bt,p,0);pr_change_form_state(&loaded.bt,q,0);CHECK(p->form==0&&q->form==0&&p->hp==q->hp&&!memcmp(p->stats,q->stats,sizeof p->stats));
  /* Missing/invalid constructor arguments remain opaque; no mystery choice or
   * generated vitamin stat is inferred from a generic item key. */
  run.items[0].arg=0;gc_items_to_battle(&run);CHECK(run.bt.mods[0][0].unknown_subclass);run.items[0].arg=negative?-15:10;run.items[1].arg=1;gc_items_to_battle(&run);CHECK(run.bt.mods[0][1].unknown_subclass);run.items[1].arg=0;run.items[2].arg=256;gc_items_to_battle(&run);CHECK(run.bt.mods[0][2].unknown_subclass);
  run.items[2].arg=STAT_ATK;run.items[2].stack=16;gc_items_to_battle(&run);
  pr_pokemon *foe=run.bt.field[2];foe->custom_ability=ABILITY_ID_BALL_FETCH;foe->ability_override=-1;pr_refresh_ability(foe);foe->n_moves=1;foe->moves[0]=MOVE_ID_SPLASH;foe->pp_used[0]=0;
  uint32_t before=run.unported[PR_FEAT_TURN_MODIFIERS];gc_choice c={GC_CH_FIGHT,1,-1,0};CHECK(gc_run_choose(&run,&c,&why));drain();CHECK(run.unported[PR_FEAT_TURN_MODIFIERS]>before);
 }
 printf("Stat ordering item bridge/form/save: %d checks passed\n",checks);return 0;
}
