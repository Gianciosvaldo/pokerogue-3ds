/* Bounded canonical temporary inventory caller. Unknown keys, virtual records
 * and legacy timerless stock refuse transport rather than losing information. */
#include "internal.h"
static bool temporary(const char *key) {
 return !strcmp(key,"TEMP_STAT_STAGE_BOOSTER")||!strcmp(key,"DIRE_HIT");
}
static bool context(gc_run *run,pr_reward_ctx *c) {
 memset(c,0,sizeof *c);c->run=&run->run;
 if(run->run.has_challenges||run->n_items<0||run->n_items>GC_MAX_ITEMS||run->n_items>PR_REWARD_MAX_OWNED||run->n_items>PR_MAX_MODIFIERS)return false;
 for(int j=0;j<run->n_items;j++) {
  const gc_item *it=&run->items[j];int item=gc_reward_item_index(it->key);
  if(item<0||it->virtual_stack||(temporary(it->key)&&(it->pokemon_id||it->detached)))return false;
  c->items[c->n_items++]=(pr_owned_item){.item=item,.arg=it->arg,.stack=it->stack,.owner=-1,.battle_count=it->battle_count};
 }
 return true;
}
int gc_temp_add(gc_run *run,int item,int arg) {
 static pr_reward_ctx c;if(!context(run,&c))return PR_REWARD_TEMP_UNSUPPORTED;
 int result=pr_reward_add_temp_booster(&c,item,arg);
 if(result!=PR_REWARD_TEMP_ADDED&&result!=PR_REWARD_TEMP_REFRESHED)return result;
 for(int j=0;j<run->n_items;j++)run->items[j].battle_count=c.items[j].battle_count;
 if(c.n_items>run->n_items) {
  if(run->n_items>=PR_MAX_MODIFIERS)return PR_REWARD_TEMP_UNSUPPORTED;
  const pr_owned_item *it=&c.items[c.n_items-1];
  run->items[run->n_items++]=(gc_item){.key=pr_item_keys[it->item],.arg=it->arg,.stack=it->stack,.battle_count=it->battle_count};
 }
 gc_items_to_battle(run);return result;
}
bool gc_temp_lapse(gc_run *run,int *expired) {
 if(run->n_items<0||run->n_items>GC_MAX_ITEMS)return false;
 bool any=false;for(int j=0;j<run->n_items;j++)any|=temporary(run->items[j].key);
 if(!any){if(expired)*expired=0;return true;}
 static pr_reward_ctx c;if(!context(run,&c))return false;
 int removed;if(!pr_reward_lapse_temp_boosters(&c,&removed))return false;
 int out=0;for(int j=0;j<run->n_items;j++) {
  gc_item it=run->items[j];if(temporary(it.key)&&it.battle_count==1)continue;
  it.battle_count=c.items[out].battle_count;run->items[out++]=it;
 }
 memset(&run->items[out],0,(run->n_items-out)*sizeof run->items[0]);run->n_items=out;
 if(expired)*expired=removed;
 gc_items_to_battle(run);return true;
}
int gc_temp_apply(gc_run *run,const gc_offer *o,const char **why) {
 int item=gc_reward_item_index(o->key),result=gc_temp_add(run,item,o->arg);
 if(result==PR_REWARD_TEMP_UNSUPPORTED)return GC_APPLY_UNHANDLED;
 if(result!=PR_REWARD_TEMP_FULL) {
  /* Persistent Scene.addModifier mutates stock but returns false. Paid caller
   * plays an error and retains its wallet; free selection still completes. */
  if(o->is_purchase){if(why)*why="";return GC_APPLY_REFUSED;}
  return GC_APPLY_DONE;
 }
 /* Scene.addModifier replaces a full persistent stack by tier's default ball. */
 if(o->tier>=5)return GC_APPLY_UNHANDLED;
 int fallback=pr_player_pool_entries[pr_player_pool[o->tier].start].item;
 static const char *names[]={"","modifierType:TempStatStageBoosterItem.x_attack","modifierType:TempStatStageBoosterItem.x_defense","modifierType:TempStatStageBoosterItem.x_sp_atk","modifierType:TempStatStageBoosterItem.x_sp_def","modifierType:TempStatStageBoosterItem.x_speed","modifierType:TempStatStageBoosterItem.x_accuracy"};
 gc_ev_msg(run,"battle:itemStackFull",2,pr_arg_key("fullItemName",item==PR_ITEM_DIRE_HIT?"modifierType:ModifierType.DIRE_HIT.name":names[o->arg]),(pr_msg_arg){"itemName",PR_ARG_STR,0,pr_reward_strings[pr_items[fallback].name]});
 gc_offer replacement={.key=pr_item_keys[fallback],.arg=-1,.target=GC_TARGET_NONE};
 return gc_port_apply_reward(run,&replacement,-1,-1,why)?GC_APPLY_DONE:GC_APPLY_REFUSED;
}
