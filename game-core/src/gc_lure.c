/* Canonical Lure acquisition. Legacy timerless records remain opaque; the
 * shadow projection never changes unrelated identities, args or timers. */
#include "internal.h"
static int lure_duration(const char *key){return !strcmp(key,"LURE")?10:!strcmp(key,"SUPER_LURE")?15:!strcmp(key,"MAX_LURE")?30:0;}
static bool context(gc_run *run,pr_reward_ctx *c){
 memset(c,0,sizeof *c);c->run=&run->run;
 if(run->run.has_challenges||run->n_items<0||run->n_items>GC_MAX_ITEMS||run->n_items>PR_REWARD_MAX_OWNED||run->n_items>PR_MAX_MODIFIERS)return false;
 for(int j=0;j<run->n_items;j++){
  const gc_item *it=&run->items[j];int item=gc_reward_item_index(it->key);
  if(item<0||it->virtual_stack||(lure_duration(it->key)&&(it->pokemon_id||it->detached)))return false;
  int arg=it->arg;
  if(lure_duration(it->key)){if(arg!=-1&&arg!=0)return false;arg=0;}
  c->items[c->n_items++]=(pr_owned_item){.item=item,.arg=arg,.stack=it->stack,.owner=-1,.battle_count=it->battle_count};
 }
 return true;
}
int gc_lure_add(gc_run *run,int item){
 static pr_reward_ctx c;if(!context(run,&c))return PR_REWARD_LURE_UNSUPPORTED;
 int result=pr_reward_add_lure(&c,item);
 if(result!=PR_REWARD_LURE_ADDED&&result!=PR_REWARD_LURE_REFRESHED)return result;
 if(c.n_items>GC_MAX_ITEMS||c.n_items>PR_MAX_MODIFIERS)return PR_REWARD_LURE_UNSUPPORTED;
 for(int j=0;j<run->n_items;j++)run->items[j].battle_count=c.items[j].battle_count;
 if(c.n_items>run->n_items){const pr_owned_item *it=&c.items[c.n_items-1];run->items[run->n_items++]=(gc_item){.key=pr_item_keys[it->item],.arg=0,.stack=it->stack,.battle_count=it->battle_count};}
 gc_items_to_battle(run);return result;
}
int gc_lure_apply(gc_run *run,const gc_offer *o,const char **why){
 if((o->arg!=-1&&o->arg!=0)||o->tier>=5||run->n_party<1||run->n_party>PR_REWARD_MAX_PARTY||run->n_items<0||run->n_items>GC_MAX_ITEMS)return GC_APPLY_UNHANDLED;
 /* Scene.updateModifiers recalculates stats. The vitamins (BASE_STAT_BOOSTER) are in battle-core's modifier list, so the
  * recalculation sees them; the other stat-changing stock and fused Pokemon are not, so for those the Lure is still added
  * with its timer (the inventory does not depend on the stats) but the party's stats are left as they were. Refusing the
  * add instead left an untimed, opaque Lure that never lapsed. */
 static const char *stats[]={"SOUL_DEW","SPECIES_STAT_BOOSTER","RARE_SPECIES_STAT_BOOSTER","EVIOLITE","MYSTERY_ENCOUNTER_SHUCKLE_JUICE","MYSTERY_ENCOUNTER_OLD_GATEAU","MYSTERY_ENCOUNTER_MACHO_BRACE"};
 bool recalc=true;
 for(int j=0;j<run->n_party;j++)if(run->party[j].b.fusion_species)recalc=false;
 for(int j=0;j<run->n_items;j++){
  if(run->items[j].stack<1)return GC_APPLY_UNHANDLED;
  for(unsigned k=0;k<sizeof stats/sizeof *stats;k++)if(!strcmp(run->items[j].key,stats[k]))recalc=false;
  int item=gc_reward_item_index(run->items[j].key);
  if(item>=0&&(pr_items[item].flags&PR_ITEMF_HELD)){
   bool found=false;for(int k=0;k<run->n_party;k++)found|=run->items[j].pokemon_id==run->party[k].b.id;
   if(!found)recalc=false;
  }
 }
 int item=gc_reward_item_index(o->key),result=gc_lure_add(run,item);
 if(result==PR_REWARD_LURE_UNSUPPORTED)return GC_APPLY_UNHANDLED;
 if(result!=PR_REWARD_LURE_FULL){
  if(recalc)for(int j=0;j<run->n_party;j++)gc_mon_recalc(run,&run->party[j]);
  if(o->is_purchase){if(why)*why="";return GC_APPLY_REFUSED;}
  return GC_APPLY_DONE;
 }
 int fallback=pr_player_pool_entries[pr_player_pool[o->tier].start].item;
 const char *name=item==PR_ITEM_LURE?"modifierType:ModifierType.LURE.name":item==PR_ITEM_SUPER_LURE?"modifierType:ModifierType.SUPER_LURE.name":"modifierType:ModifierType.MAX_LURE.name";
 gc_ev_msg(run,"battle:itemStackFull",2,pr_arg_key("fullItemName",name),(pr_msg_arg){"itemName",PR_ARG_STR,0,pr_reward_strings[pr_items[fallback].name]});
 gc_offer replacement={.key=pr_item_keys[fallback],.arg=-1,.target=GC_TARGET_NONE};
 return gc_port_apply_reward(run,&replacement,-1,-1,why)?GC_APPLY_DONE:GC_APPLY_REFUSED;
}

/* BattleEnd only, one tick per canonical record. Unrelated fields and timers
 * retain their original representation through compaction. */
bool gc_lure_lapse(gc_run *run,int *expired){
 if(!run||run->n_items<0||run->n_items>GC_MAX_ITEMS)return false;
 bool any=false;for(int j=0;j<run->n_items;j++)any|=lure_duration(run->items[j].key)!=0;
 if(!any){if(expired)*expired=0;return true;}
 static pr_reward_ctx c;if(!context(run,&c))return false;
 int removed;if(!pr_reward_lapse_lures(&c,&removed))return false;
 int out=0;for(int j=0;j<run->n_items;j++){
  gc_item it=run->items[j];if(lure_duration(it.key)&&it.battle_count==1)continue;
  it.battle_count=c.items[out].battle_count;run->items[out++]=it;
 }
 memset(&run->items[out],0,(run->n_items-out)*sizeof run->items[0]);run->n_items=out;
 if(expired)*expired=removed;
 gc_items_to_battle(run);return true;
}

/* getDoubleBattleChance applies each modifier record once, regardless of stack.
 * Validate through the same canonical projection as acquisition/lapse. The
 * numerical chance remains run-core's responsibility. No-Lure stock is opaque. */
bool gc_lure_count(gc_run *run,int *count){
 if(!run||!count||run->n_items<0||run->n_items>GC_MAX_ITEMS)return false;
 bool any=false;for(int j=0;j<run->n_items;j++)any|=lure_duration(run->items[j].key)!=0;
 if(!any){*count=0;return true;}
 static pr_reward_ctx c;double unused;
 if(!context(run,&c)||!pr_reward_lure_chance(&c,1,&unused))return false;
 int n=0;for(int j=0;j<run->n_items;j++)n+=lure_duration(run->items[j].key)!=0;
 *count=n;return true;
}
