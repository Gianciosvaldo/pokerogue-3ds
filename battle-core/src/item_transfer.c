/* HeldItemTransferModifier.apply and the scene's one-stack transfer boundary.
 * Source 8555c08. Unknown item match/cap/name contexts remain explicit refusals. */
#include "pr/engine.h"
#include <string.h>

/* Source getMaxHeldItemCount; these families have class-only matchType. */
static int transfer_cap(const pr_modifier *m)
{
    if (m->unknown_subclass) return -1;
    switch (m->cls) {
    case PR_MOD_TURN_HEAL: case PR_MOD_HIT_HEAL: return 4;
    case PR_MOD_SURVIVE_DAMAGE: return 5;
    case PR_MOD_BYPASS_SPEED_CHANCE: case PR_MOD_FLINCH_CHANCE: return 3;
    case PR_MOD_RESET_NEGATIVE_STAT_STAGE: return 2;
    case PR_MOD_TURN_HELD_TRANSFER: case PR_MOD_INSTANT_REVIVE:
    case PR_MOD_CRIT_BOOSTER: case PR_MOD_TURN_STATUS_EFFECT: return 1;
    case PR_MOD_MOVE_ACCURACY_BOOSTER: return 3;
    case PR_MOD_MULTI_HIT: return 2;
    case PR_MOD_NATURE_WEIGHT: return 10;
    case PR_MOD_DAMAGE_MONEY_REWARD: return 5;
    case PR_MOD_ATTACK_TYPE_BOOSTER: return 99;
    case PR_MOD_BERRY:
        return m->berry_type==BERRY_TYPE_LUM||m->berry_type==BERRY_TYPE_LEPPA
            ||m->berry_type==BERRY_TYPE_SITRUS||m->berry_type==BERRY_TYPE_ENIGMA?2:3;
    default: return -1;
    }
}

static bool same_type(const pr_modifier *m,const pr_modifier *other)
{
    if(m->cls!=other->cls)return false;
    if(m->cls==PR_MOD_BERRY)return m->berry_type==other->berry_type;
    if(m->cls==PR_MOD_ATTACK_TYPE_BOOSTER)return m->move_type==other->move_type&&m->mult==other->mult;
    if(m->cls==PR_MOD_CRIT_BOOSTER||m->cls==PR_MOD_MOVE_ACCURACY_BOOSTER)return m->num==other->num;
    /* Source intentionally matches both Orb effects as one class. */
    return true;
}

static void remove_at(pr_battle *bt,int side,int index)
{
    memmove(&bt->mods[side][index],&bt->mods[side][index+1],
        (size_t)(bt->n_mods[side]-index-1)*sizeof(pr_modifier));
    bt->n_mods[side]--;
}

bool pr_apply_turn_held_item_transfer(pr_battle *bt,pr_engine *e,pr_pokemon *p,const pr_modifier *modifier)
{
    if (!p || !modifier || p->hp <= 0 || (modifier->pokemon_id != -1 && modifier->pokemon_id != (int64_t)p->id)) return false;
    if (modifier->cls != PR_MOD_TURN_HELD_TRANSFER || modifier->unknown_subclass
        || modifier->stack_count != 1 || modifier->virtual_stack_count) {
        pr_note_unported(bt,PR_FEAT_MODIFIERS);return false;
    }
    int side=p->is_player?0:1,other=1-side,n=0;
    pr_pokemon *opponents[2];
    for (int k=other*2;k<other*2+2;k++) {
        pr_pokemon *q=bt->field[k];
        if(q&&q->hp>0&&q->on_field&&!q->switch_out_status)opponents[n++]=q;
    }
    if (!n) return false;
    pr_pokemon *victim=opponents[pr_battle_rand_seed_int(bt,n,0)];
    int indices[PR_MAX_MODIFIERS],count=0;
    for(int k=0;k<bt->n_mods[other];k++){
        const pr_modifier *m=&bt->mods[other][k];
        if(pr_mod_is(m,PR_MOD_HELD_ITEM)&&m->pokemon_id==(int64_t)victim->id&&m->is_transferable)indices[count++]=k;
    }
    if (!count) return false;
    int index=indices[pr_battle_rand_seed_int(bt,count,0)];
    pr_modifier copy=bt->mods[other][index];
    const char *name=pr_held_item_name_key(&copy);
    if (!pr_try_transfer_held_item(bt,e,other,index,p,1,false,true)) return false;
    pr_queue_msg(bt,e,pr_msg4("modifier:turnHeldItemTransferApply",
        pr_arg_pokemon(bt,"pokemonNameWithAffix",victim),pr_arg_key("itemName",name),
        pr_arg_pokemon_name(bt,"pokemonName",p),pr_arg_key("typeName","modifierType:ModifierType.MINI_BLACK_HOLE.name")));
    return true;
}

void pr_apply_turn_held_item_transfers(pr_battle *bt,pr_engine *e,pr_pokemon *p)
{
    /* One canonical Mini Black Hole can be held. Capture its value because
     * successful transfers can remove/append entries in the holder's list. */
    int side=p->is_player?0:1;
    for(int k=0;k<bt->n_mods[side];k++)if(bt->mods[side][k].cls==PR_MOD_TURN_HELD_TRANSFER
        &&(bt->mods[side][k].pokemon_id==-1||bt->mods[side][k].pokemon_id==(int64_t)p->id)){
        pr_modifier copy=bt->mods[side][k];
        pr_apply_turn_held_item_transfer(bt,e,p,&copy);
        return;
    }
}

/* getOrInferTier caches positive tiers. A cached COMMON tier is falsy, so the
 * pool is searched again. These canonical identities have pool-invariant tier0
 * or positive tiers, avoiding shared-type alias ambiguity after cloning. */
int pr_held_item_tier_for_pool(const pr_modifier *m, bool trainer)
{
    int tier = -1;
    if (!m->unknown_subclass) switch (m->cls) {
    case PR_MOD_TURN_HEAL: case PR_MOD_HIT_HEAL: case PR_MOD_FLINCH_CHANCE:
    case PR_MOD_CRIT_BOOSTER: tier = trainer ? 4 : 3; break;
    case PR_MOD_BYPASS_SPEED_CHANCE: case PR_MOD_MOVE_ACCURACY_BOOSTER:
        tier = trainer ? 3 : 2; break;
    case PR_MOD_SURVIVE_DAMAGE: case PR_MOD_NATURE_WEIGHT: tier = 3; break;
    case PR_MOD_RESET_NEGATIVE_STAT_STAGE: case PR_MOD_TURN_STATUS_EFFECT:
    case PR_MOD_DAMAGE_MONEY_REWARD: case PR_MOD_ATTACK_TYPE_BOOSTER:
    case PR_MOD_INSTANT_REVIVE: tier = 2; break;
    case PR_MOD_TURN_HELD_TRANSFER: case PR_MOD_MULTI_HIT: tier = 4; break;
    case PR_MOD_BERRY: tier = 0; break;
    default: break;
    }
    return tier;
}

static int infer_move_item_tier(pr_battle *bt, pr_modifier *m, bool trainer)
{
    if (m->type_tier > 1 && m->type_tier <= 6) return m->type_tier - 1;
    int tier = m->type_tier <= 6 ? pr_held_item_tier_for_pool(m, trainer) : -1;
    if (tier < 0) { pr_note_unported(bt, PR_FEAT_MODIFIERS); return -1; }
    m->type_tier = (uint8_t)(tier + 1);
    return tier;
}

/* Native canTransferHeldItemModifier clones before matching. The currently
 * supported stock families preserve their class through clone(). The broader
 * clone-family cases must be added here before enabling them for Pickup. */
bool pr_can_transfer_held_item(pr_battle *bt, const pr_modifier *stock,
                              pr_pokemon *receiver, int quantity)
{
    if (!stock || !receiver || quantity<0 || stock->stack_count<0
        || transfer_cap(stock)<0) {
        pr_note_unported(bt,PR_FEAT_MODIFIERS); return false;
    }
    pr_modifier copy=*stock;
    copy.virtual_stack_count=0;
    copy.is_transferable=true;
    pr_pokemon *source=copy.pokemon_id ? pr_pokemon_by_id(bt,copy.pokemon_id) : NULL;
    bool cancelled=false;
    if (source && source->is_player!=receiver->is_player) {
        pr_ab_params block={.pokemon=source,.move=-1,.flag=&cancelled};
        pr_apply_ab_attrs(bt,PR_AC_BlockItemTheftAbAttr,&block);
    }
    if (cancelled) return false;
    int side=receiver->is_player?0:1;
    for (int k=0;k<bt->n_mods[side];k++) {
        const pr_modifier *m=&bt->mods[side][k];
        if (!pr_mod_is(m,PR_MOD_HELD_ITEM) || !same_type(m,&copy)
            || m->pokemon_id!=(int64_t)receiver->id) continue;
        int cap=transfer_cap(m);
        if (cap<0) { pr_note_unported(bt,PR_FEAT_MODIFIERS); return false; }
        if (m->stack_count>=cap) return false;
        if (quantity>cap-m->stack_count) quantity=cap-m->stack_count;
        break;
    }
    int taken=copy.stack_count<quantity ? copy.stack_count : quantity;
    if (copy.stack_count!=taken || !source) return true;
    int from_side=source->is_player?0:1;
    for (int k=0;k<bt->n_mods[from_side];k++)
        if (stock==&bt->mods[from_side][k]) return true;
    return false;
}

/* Scene tryTransferHeldItemModifier. The caller has already selected
 * an actual transferable stock. The native helper itself does not test HP or
 * the transferable flag, and Sticky Hold is only queried across sides. */
bool pr_try_transfer_held_item(pr_battle *bt, pr_engine *e, int from_side,
                              int index, pr_pokemon *receiver, int quantity,
                              bool ignore_update, bool item_lost)
{
    if (!receiver || from_side<0 || from_side>1 || index<0
        || index>=bt->n_mods[from_side] || quantity<0) {
        pr_note_unported(bt,PR_FEAT_MODIFIERS); return false;
    }
    pr_modifier copy = bt->mods[from_side][index];
    pr_pokemon *source = copy.pokemon_id ? pr_pokemon_by_id(bt, copy.pokemon_id) : NULL;
    int side = receiver->is_player ? 0 : 1;
    bool cancelled = false;
    if (source && source->is_player != receiver->is_player) {
        pr_ab_params block = {.pokemon=source,.move=-1,.flag=&cancelled};
        pr_apply_ab_attrs(bt, PR_AC_BlockItemTheftAbAttr, &block);
    }
    if (cancelled) return false;
    int matching = -1, cap = transfer_cap(&copy);
    if (cap < 0 || copy.stack_count < 0 || !pr_held_item_name_key(&copy)) {
        pr_note_unported(bt, PR_FEAT_MODIFIERS); return false;
    }
    for (int k=0;k<bt->n_mods[side];k++) {
        pr_modifier *m=&bt->mods[side][k];
        if (pr_mod_is(m, PR_MOD_HELD_ITEM) && same_type(m,&copy)
            && m->pokemon_id==(int64_t)receiver->id) { matching=k; break; }
    }
    /* Self-target attacks are not legal Thief/Covet targets. Preserve an
     * explicit boundary for raw callbacks with that alias rather than corrupt
     * the shared inventory while removing the same stock twice. */
    if (from_side==side && matching==index) {
        pr_note_unported(bt, PR_FEAT_MODIFIERS); return false;
    }
    if (matching>=0 && (transfer_cap(&bt->mods[side][matching])<0)) {
        pr_note_unported(bt, PR_FEAT_MODIFIERS); return false;
    }
    if (matching>=0) cap=transfer_cap(&bt->mods[side][matching]);
    int existing=matching>=0 ? bt->mods[side][matching].stack_count : 0;
    if (matching>=0 && existing>=cap) return false;
    bool removes=copy.stack_count<=quantity && source;
    if (matching<0 && bt->n_mods[side]>=PR_MAX_MODIFIERS && !(from_side==side && removes)) {
        pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); return false;
    }
    int taken=copy.stack_count<quantity ? copy.stack_count : quantity;
    if (matching>=0 && taken>cap-existing) taken=cap-existing;
    bt->mods[from_side][index].stack_count-=taken;
    if (!bt->mods[from_side][index].stack_count && source) {
        remove_at(bt,from_side,index);
        if (from_side==side && matching>index) matching--;
    }
    if (matching>=0) remove_at(bt,side,matching);
    copy.pokemon_id=receiver->id;
    copy.stack_count=existing+taken;
    copy.virtual_stack_count=0;
    copy.is_transferable=true;
    if (!ignore_update && source && source->is_player != receiver->is_player)
        pr_refresh_berry_modifiers(bt,source->is_player?0:1);
    bt->mods[side][bt->n_mods[side]++]=copy;
    if (!ignore_update) pr_refresh_berry_modifiers(bt,side);
    if (source && item_lost) {
        pr_ab_params lost={.pokemon=source,.move=-1};
        pr_apply_ab_attrs(bt,PR_AC_PostItemLostAbAttr,&lost);
    }
    return true;
}

bool pr_steal_move_held_item(pr_battle *bt, pr_engine *e, pr_pokemon *user,
                            pr_pokemon *target, double chance)
{
    if (!bt->rng_live) { pr_note_unported(bt,PR_FEAT_GLOBAL_RNG); return false; }
    if (pr_rng_seed_float(&bt->rng)>chance) return false;
    if (!user || !target) { pr_note_unported(bt,PR_FEAT_PARTY); return false; }
    int side=target->is_player?0:1, indices[PR_MAX_MODIFIERS], count=0;
    for (int k=0;k<bt->n_mods[side];k++) {
        pr_modifier *m=&bt->mods[side][k];
        if (pr_mod_is(m,PR_MOD_HELD_ITEM) && m->pokemon_id==(int64_t)target->id
            && m->is_transferable) indices[count++]=k;
    }
    if (!count) return false;
    int tiers[PR_MAX_MODIFIERS], highest=0, chosen[PR_MAX_MODIFIERS], n=0;
    for (int k=0;k<count;k++) {
        tiers[k]=infer_move_item_tier(bt,&bt->mods[side][indices[k]],!target->is_player&&target->has_trainer);
        if (tiers[k]<0) return false;
        if (tiers[k]>highest) highest=tiers[k];
    }
    for (int k=0;k<count;k++) if (tiers[k]==highest) chosen[n++]=indices[k];
    if (!n) { pr_note_unported(bt,PR_FEAT_MODIFIERS); return false; }
    int selected=chosen[pr_battle_rand_seed_int(bt,n,0)];
    const char *name=pr_held_item_name_key(&bt->mods[side][selected]);
    if (!pr_try_transfer_held_item(bt,e,side,selected,user,1,false,true)) return false;
    pr_queue_msg(bt,e,pr_msg3("moveTriggers:stoleItem",
        pr_arg_pokemon(bt,"pokemonName",user),pr_arg_pokemon(bt,"targetName",target),
        pr_arg_key("itemName",name)));
    return true;
}
