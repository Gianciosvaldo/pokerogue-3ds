#include <stdio.h>
#include <stdlib.h>
/* Additive game raw acquisition fixture; owning reward harness remains untouched. */
static gc_run bridge_run;static int calls;
static int game_wide_add(pr_reward_ctx *c,int item,int target) {
 if(!c||item!=PR_ITEM_WIDE_LENS||c->n_party<1||c->n_party>6||c->n_items<0||c->n_items>GC_MAX_ITEMS||c->n_items>PR_REWARD_MAX_OWNED||target<0||target>=c->n_party)return PR_REWARD_HELD_UNSUPPORTED;
 memset(&bridge_run,0,sizeof bridge_run);if(c->run)bridge_run.run=*c->run;
 bridge_run.n_party=c->n_party;bridge_run.n_items=c->n_items;int rotate=calls++%c->n_party;
 for(int j=0;j<c->n_party;j++){int storage=(j+rotate)%c->n_party;bridge_run.party_order[j]=storage;bridge_run.party[storage].b.id=c->party[j].id;}
 for(int j=0;j<c->n_items;j++){
  const pr_owned_item *it=&c->items[j];if(it->item>=PR_ITEM_COUNT_||it->owner< -1||it->owner>=c->n_party)return PR_REWARD_HELD_UNSUPPORTED;
  bridge_run.items[j]=(gc_item){.key=pr_item_keys[it->item],.arg=it->arg,.stack=it->stack,.pokemon_id=it->owner<0?0:c->party[it->owner].id,.battle_count=it->battle_count};
  if(it->item==PR_ITEM_WIDE_LENS&&it->arg==0&&(calls+j)%2)bridge_run.items[j].arg=-1;
 }
 gc_item original[GC_MAX_ITEMS];memcpy(original,bridge_run.items,sizeof original);
 int status=gc_wide_add(&bridge_run,bridge_run.party_order[target]);
 if(status==PR_REWARD_HELD_ADDED||status==PR_REWARD_HELD_STACKED){
  for(int j=0;j<c->n_items;j++){if(bridge_run.items[j].arg!=original[j].arg||bridge_run.items[j].pokemon_id!=original[j].pokemon_id||strcmp(bridge_run.items[j].key,original[j].key)){fprintf(stderr,"raw identity changed\n");exit(1);}c->items[j].stack=bridge_run.items[j].stack;}
  if(bridge_run.n_items>c->n_items)c->items[c->n_items++]=(pr_owned_item){.item=PR_ITEM_WIDE_LENS,.owner=target,.stack=1};
 }
 return status;
}
