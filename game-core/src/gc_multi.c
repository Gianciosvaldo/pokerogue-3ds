/* Canonical Multi Lens acquisition only. Preserve source inventory identity/order
 * in game storage; normalize constructor-only args in the shadow reward context. */
#include "internal.h"
static bool context(gc_run *run,pr_reward_ctx *c,int storage,int *target) {
 memset(c,0,sizeof *c);c->run=&run->run;
 if(run->run.has_challenges||run->n_party<1||run->n_party>PR_REWARD_MAX_PARTY
    ||storage<0||storage>=run->n_party||run->n_items<0||run->n_items>GC_MAX_ITEMS
    ||run->n_items>PR_REWARD_MAX_OWNED||run->n_items>PR_MAX_MODIFIERS)return false;
 bool used[PR_REWARD_MAX_PARTY]={0};*target=-1;
 for(int k=0;k<run->n_party;k++) {
  int j=run->party_order[k];if(j<0||j>=run->n_party||used[j]||!run->party[j].b.id)return false;
  used[j]=true;c->party[k].id=run->party[j].b.id;c->party[k].hp=run->party[j].b.hp;
  for(int prior=0;prior<k;prior++)if(c->party[prior].id==c->party[k].id)return false;
  if(j==storage)*target=k;
 }
 c->n_party=run->n_party;
 for(int j=0;j<run->n_items;j++) {
  const gc_item *it=&run->items[j];if(!it->key)return false;int item=gc_reward_item_index(it->key);
  if(item<0||it->virtual_stack||it->detached)return false;
  int owner=-1;if(it->pokemon_id){for(int k=0;k<c->n_party;k++)if(c->party[k].id==it->pokemon_id)owner=k;if(owner<0)return false;}
  int arg=it->arg;
  if(item==PR_ITEM_WIDE_LENS||item==PR_ITEM_REVIVER_SEED||item==PR_ITEM_GOLDEN_PUNCH||item==PR_ITEM_MULTI_LENS){if(arg!=-1&&arg!=0)return false;arg=0;}
  c->items[c->n_items++]=(pr_owned_item){.item=item,.arg=arg,.stack=it->stack,.owner=owner,.battle_count=it->battle_count};
 }
 return *target>=0;
}
int gc_multi_add(gc_run *run,int storage) {
 static pr_reward_ctx c;int target;
 if(!context(run,&c,storage,&target))return PR_REWARD_HELD_UNSUPPORTED;
 int status=pr_reward_add_canonical_held_item(&c,PR_ITEM_MULTI_LENS,target);
 if(status!=PR_REWARD_HELD_ADDED&&status!=PR_REWARD_HELD_STACKED)return status;
 if(c.n_items>PR_MAX_MODIFIERS)return PR_REWARD_HELD_UNSUPPORTED;
 for(int j=0;j<run->n_items;j++)run->items[j].stack=c.items[j].stack;
 if(c.n_items>run->n_items)run->items[run->n_items++]=(gc_item){.key="MULTI_LENS",.arg=0,.stack=1,.pokemon_id=run->party[storage].b.id};
 gc_items_to_battle(run);
 /* Scene.updateModifiers recalculates the party. Multi Lens changes no base
  * stat; callers with stat-changing inventories are guarded below. */
 return status;
}
static bool recalc_supported(const gc_run *run) {
 /* Avoid claiming broad source stat/updateModifiers parity through opaque stock. */
 static const char *keys[]={"EVOLUTION_TRACKER_GIMMIGHOUL","FORM_CHANGE_ITEM","RARE_FORM_CHANGE_ITEM","BASE_STAT_BOOSTER","SOUL_DEW","SPECIES_STAT_BOOSTER","RARE_SPECIES_STAT_BOOSTER","EVIOLITE","MYSTERY_ENCOUNTER_SHUCKLE_JUICE","MYSTERY_ENCOUNTER_OLD_GATEAU","MYSTERY_ENCOUNTER_MACHO_BRACE"};
 if(run->n_party<1||run->n_party>PR_REWARD_MAX_PARTY||run->n_items<0||run->n_items>GC_MAX_ITEMS)return false;
 for(int j=0;j<run->n_items;j++)if(!run->items[j].key||run->items[j].stack<=0)return false;
 for(int j=0;j<run->n_items;j++)for(unsigned k=0;k<sizeof keys/sizeof *keys;k++)if(!strcmp(run->items[j].key,keys[k]))return false;
 for(int j=0;j<run->n_items;j++){int item=gc_reward_item_index(run->items[j].key);if(item>=0&&!run->items[j].pokemon_id&&(pr_items[item].flags&PR_ITEMF_HELD)&&item!=PR_ITEM_MULTI_LENS)return false;}
 for(int j=0;j<run->n_party;j++)if(run->party[j].b.fusion_species)return false;
 return true;
}
int gc_multi_scene_apply(gc_run *run,const gc_offer *o,int storage,const char **why,bool selection) {
 if(storage<0||storage>=run->n_party){if(why)*why="pick a Pokemon";return GC_APPLY_REFUSED;}
 if((o->arg!=-1&&o->arg!=0)||!recalc_supported(run))return GC_APPLY_UNHANDLED;
 int status=gc_multi_add(run,storage);
 if(status==PR_REWARD_HELD_UNSUPPORTED)return GC_APPLY_UNHANDLED;
 if(status==PR_REWARD_HELD_FULL) {
  if(selection){if(why)*why="modifierType:ModifierType.PokemonHeldItemModifierType.extra.tooMany";return GC_APPLY_REFUSED;}
  if(o->tier>=5)return GC_APPLY_UNHANDLED;
  int fallback=pr_player_pool_entries[pr_player_pool[o->tier].start].item;
  gc_ev_msg(run,"battle:itemStackFull",2,pr_arg_key("fullItemName","modifierType:ModifierType.MULTI_LENS.name"),(pr_msg_arg){"itemName",PR_ARG_STR,0,pr_reward_strings[pr_items[fallback].name]});
  gc_offer replacement={.key=pr_item_keys[fallback],.arg=-1,.target=GC_TARGET_NONE};
  return gc_port_apply_reward(run,&replacement,-1,-1,why)?GC_APPLY_DONE:GC_APPLY_REFUSED;
 }
 /* Scene.updateModifiers removes held records with no actual holder. Raw
  * .add above preserves wildcard stock; only successful Scene adds clean it. */
 for(int j=0;j<run->n_items;){int item=gc_reward_item_index(run->items[j].key);if(!run->items[j].pokemon_id&&item==PR_ITEM_MULTI_LENS)gc_item_remove_at(run,j);else j++;}
 gc_items_to_battle(run);
 for(int j=0;j<run->n_party;j++)gc_mon_recalc(run,&run->party[j]);
 if(o->is_purchase){if(why)*why="";return GC_APPLY_REFUSED;}
 return GC_APPLY_DONE;
}
