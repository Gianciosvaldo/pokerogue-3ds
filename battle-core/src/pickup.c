/* Ordered Battle.postBattleLoot references and the shared Pickup attribute cache.
 * Unlike ModifierType references, these identify the original held-item object.
 * Source: battle.ts addPostBattleLoot and PostBattleLootAbAttr at 8555c08. */
#include "pr/engine.h"
#include <string.h>
#include <limits.h>

bool pr_add_post_battle_loot(pr_battle *bt,pr_modifier *stock)
{
    if(bt->post_battle_loot<0||bt->post_battle_loot>=PR_MAX_MODIFIERS||bt->loot_ref_max==UINT16_MAX){
        pr_note_unported(bt,PR_FEAT_ENGINE_LIMIT);return false;
    }
    stock->loot_ref=++bt->loot_ref_max;
    stock->pokemon_id=0;
    bt->post_battle_loot_refs[bt->post_battle_loot++]=stock->loot_ref;
    return true;
}
static pr_modifier *stock_of(pr_battle *bt,uint16_t ref,int *side,int *index)
{
    if(ref)for(int s=0;s<2;s++)for(int i=0;i<bt->n_mods[s];i++)if(bt->mods[s][i].loot_ref==ref){
        if(side)*side=s;
        if(index)*index=i;
        return &bt->mods[s][i];
    }
    pr_note_unported(bt,PR_FEAT_MODIFIERS);return NULL;
}
static bool select_stock(pr_battle *bt)
{
    int n=bt->post_battle_loot;
    if(n<1||n>PR_MAX_MODIFIERS){pr_note_unported(bt,PR_FEAT_MODIFIERS);return false;}
    if(n>1&&!bt->rng_live){pr_note_unported(bt,PR_FEAT_GLOBAL_RNG);return false;}
    int i=n==1?0:pr_rng_seed_int(&bt->rng,n,0);
    bt->pickup_cached_ref=bt->post_battle_loot_refs[i];
    if(!bt->pickup_cached_ref){pr_note_unported(bt,PR_FEAT_MODIFIERS);return false;}
    return true;
}
bool pr_pickup_can_apply(pr_battle *bt,pr_pokemon *p,bool simulated,bool victory)
{
    if(simulated||!victory||bt->post_battle_loot<=0)return false;
    if(!select_stock(bt))return false;
    pr_modifier *stock=stock_of(bt,bt->pickup_cached_ref,NULL,NULL);
    return stock&&pr_can_transfer_held_item(bt,stock,p,1);
}
bool pr_pickup_apply(pr_battle *bt,pr_engine *e,pr_pokemon *p)
{
    if(bt->post_battle_loot<0||bt->post_battle_loot>PR_MAX_MODIFIERS){
        pr_note_unported(bt,PR_FEAT_ENGINE_LIMIT);return false;
    }
    if(!bt->pickup_cached_ref&&!select_stock(bt))return false;
    int side,index;pr_modifier *stock=stock_of(bt,bt->pickup_cached_ref,&side,&index);
    if(!stock)return false;
    const char *name=pr_held_item_name_key(stock);
    if(!name){pr_note_unported(bt,PR_FEAT_MODIFIERS);return false;}
    bool result=pr_try_transfer_held_item(bt,e,side,index,p,1,false,false);
    if(result){
        int n=bt->post_battle_loot,found=-1;
        for(int i=0;i<n;i++)if(bt->post_battle_loot_refs[i]==bt->pickup_cached_ref){found=i;break;}
        /* Native splice(indexOf(cached),1) removes the last entry for -1. */
        if(n>0){if(found<0)found=n-1;memmove(&bt->post_battle_loot_refs[found],&bt->post_battle_loot_refs[found+1],(size_t)(n-found-1)*sizeof(uint16_t));bt->post_battle_loot--;}
        pr_queue_msg(bt,e,pr_msg2("abilityTriggers:postBattleLoot",pr_arg_pokemon(bt,"pokemonNameWithAffix",p),pr_arg_key("itemName",name)));
    }
    bt->pickup_cached_ref=0;
    return result;
}
