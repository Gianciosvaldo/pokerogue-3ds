/* New game transport replay; owning reward comparator and producer untouched. */
static gc_run bridge_run;
static bool transport(const pr_reward_ctx *c) {
 memset(&bridge_run,0,sizeof bridge_run);
 if(!c||c->n_items<0||c->n_items>GC_MAX_ITEMS)return false;
 if(c->run)bridge_run.run=*c->run;
 bridge_run.n_items=c->n_items;
 for(int j=0;j<c->n_items;j++) {
  const pr_owned_item *it=&c->items[j];if(it->item>=PR_ITEM_COUNT_||it->owner < -1)return false;
  bridge_run.items[j]=(gc_item){.key=pr_item_keys[it->item],.arg=it->arg,.stack=it->stack,.pokemon_id=it->owner>=0?123:0,.battle_count=it->battle_count};
 }
 return true;
}
static void returned(pr_reward_ctx *c) {
 /* Adapter should alter only represented inventory fields. */
 for(int j=0;j<bridge_run.n_items;j++) {
  gc_item *it=&bridge_run.items[j];pr_owned_item prior={0};
  /* Raw trace gives active=0. No game active flag is invented. */
  prior=(pr_owned_item){.item=gc_reward_item_index(it->key),.arg=it->arg,.stack=it->stack,.owner=it->pokemon_id?0:-1,.battle_count=it->battle_count};c->items[j]=prior;
 }
 memset(&c->items[bridge_run.n_items],0,(c->n_items>bridge_run.n_items?c->n_items-bridge_run.n_items:0)*sizeof c->items[0]);c->n_items=bridge_run.n_items;
}
static int game_add_temp(pr_reward_ctx *c,int item,int arg) {
 if(!transport(c))return PR_REWARD_TEMP_UNSUPPORTED;
 int result=gc_temp_add(&bridge_run,item,arg);
 if(result==PR_REWARD_TEMP_ADDED||result==PR_REWARD_TEMP_REFRESHED)returned(c);
 return result;
}
static bool game_lapse_temp(pr_reward_ctx *c,int *expired) {
 if(!transport(c))return false;
 if(!gc_temp_lapse(&bridge_run,expired))return false;
 returned(c);return true;
}
