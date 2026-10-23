/* Actual command/save continuity and legacy lock/capacity boundaries. */
#define main retained_doubles_main
#include "doubles_host_test.c"
#undef main
static void play_tracker(gc_run *run){int turn=run->bt.turn,n=0;while(run->bt.turn==turn&&n++<8){
 gc_choice c=run->screen.kind==GC_SCREEN_COMMAND?(gc_choice){GC_CH_FIGHT,0,-1,0}:(gc_choice){GC_CH_TARGET,2,0,0};choose(run,c);
 }CHECK(run->bt.turn==turn+1&&!run->bt.n_unported&&!run->eng.overflow);}
int main(void){
 for(int seed=1;seed<=4;seed++){
  setup(seed);r.n_items=r.n_enemy_items=0;
  for(int k=0;k<4;k++){pr_pokemon *p=r.bt.field[k];p->custom_ability=ABILITY_ID_NONE;pr_refresh_ability(p);p->passive_flag=false;p->moves[0]=k?MOVE_ID_SPLASH:MOVE_ID_TACKLE;p->n_moves=1;p->pp_used[0]=0;p->hp=p->stats[STAT_HP]=100000;}
  pr_pokemon *p=r.bt.field[0];p->species=SPECIES_ID_GIMMIGHOUL;p->form=0;
  gc_item_add(&r,"EVOLUTION_TRACKER_GIMMIGHOUL",0,p->id,seed);gc_items_to_battle(&r);
  CHECK(r.bt.n_mods[0]==1&&pr_mod_is_ported_item(&r.bt.mods[0][0])&&!r.bt.mods[0][0].is_transferable);
  const char *why;size_t n=gc_run_save(&r,save,sizeof save);CHECK(n&&gc_run_load(&back,save,n,&why)&&!back.load_dropped);
  play_tracker(&r);play_tracker(&back);
  CHECK(r.bt.turn==back.bt.turn&&!memcmp(&r.bt.rng,&back.bt.rng,sizeof r.bt.rng)&&r.n_items==1&&back.n_items==1&&r.items[0].stack==seed&&back.items[0].stack==seed);
  for(int k=0;k<4;k++)CHECK(r.bt.field[k]->hp==back.bt.field[k]->hp);
  /* Even old saves with the obsolete transfer flag cannot move/discard coins. */
  r.items[0].non_transferable=false;gc_item original=r.items[0];
  CHECK(!gc_item_transfer(&r,0,1,1)&&!gc_item_discard(&r,0,1)&&!memcmp(&original,&r.items[0],sizeof original));
 }
 setup(1);r.n_items=PR_MAX_MODIFIERS;r.money=1000;r.party[0].b.species=SPECIES_ID_GIMMIGHOUL;r.party[0].b.fusion_species=0;
 for(int i=0;i<r.n_items;i++)r.items[i]=(gc_item){.key="POKEBALL",.arg=0,.stack=1};
 const char *why;gc_offer o={.key="NUGGET",.target=GC_TARGET_NONE};
 CHECK(gc_reward_apply(&r,&o,-1,-1,&why)==GC_APPLY_UNHANDLED&&r.money==1000&&r.n_items==PR_MAX_MODIFIERS&&r.bt.n_unported);
 for(int i=0;i<r.n_items;i++)CHECK(!strcmp(r.items[i].key,"POKEBALL")&&r.items[i].stack==1);
 /* A supported full inventory can still increment its existing tracker. */
 r.items[0]=(gc_item){.key="EVOLUTION_TRACKER_GIMMIGHOUL",.pokemon_id=r.party[0].b.id,.arg=0,.stack=1,.non_transferable=true};
 CHECK(gc_reward_apply(&r,&o,-1,-1,&why)==GC_APPLY_DONE&&r.money>1000&&r.n_items==PR_MAX_MODIFIERS&&r.items[0].stack==2);
 printf("EvoTracker command/save/legacy/capacity: %d checks passed\n",checks);return 0;
}
