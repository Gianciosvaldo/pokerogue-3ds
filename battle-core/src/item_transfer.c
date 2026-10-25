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
    case PR_MOD_SURVIVE_DAMAGE: case PR_MOD_CONTACT_HELD_TRANSFER: return 5;
    case PR_MOD_BYPASS_SPEED_CHANCE: case PR_MOD_FLINCH_CHANCE: return 3;
    case PR_MOD_RESET_NEGATIVE_STAT_STAGE: return 2;
    case PR_MOD_SWITCH_EFFECT_TRANSFER:
    case PR_MOD_TURN_HELD_TRANSFER: case PR_MOD_INSTANT_REVIVE:
    case PR_MOD_CRIT_BOOSTER: case PR_MOD_TURN_STATUS_EFFECT: return 1;
    case PR_MOD_MOVE_ACCURACY_BOOSTER: return 3;
    case PR_MOD_MULTI_HIT: return 2;
    case PR_MOD_NATURE_WEIGHT: return 10;
    case PR_MOD_DAMAGE_MONEY_REWARD: return 5;
    case PR_MOD_ATTACK_TYPE_BOOSTER: return 99;
    case PR_MOD_FIELD_EFFECT: return 2;
    case PR_MOD_SPECIES_CRIT_BOOSTER: case PR_MOD_STAT_BOOSTER:
    case PR_MOD_EVOLUTION_STAT_BOOSTER: case PR_MOD_SPECIES_STAT_BOOSTER:
        return m->n_species<=4?1:-1;
    case PR_MOD_EXP_BOOSTER: return 99;
    case PR_MOD_FRIENDSHIP_BOOSTER: return 3;
    case PR_MOD_BERRY:
        return m->berry_type==BERRY_TYPE_LUM||m->berry_type==BERRY_TYPE_LEPPA
            ||m->berry_type==BERRY_TYPE_SITRUS||m->berry_type==BERRY_TYPE_ENIGMA?2:3;
    default: return -1;
    }
}

static bool same_type(const pr_modifier *m,const pr_modifier *other)
{
    /* Native matchType is called on the existing recipient object. Eviolite
     * inherits the base clone but overrides matching, making this asymmetric. */
    if(pr_mod_is(m,PR_MOD_STAT_BOOSTER)){
        if(m->cls==PR_MOD_EVOLUTION_STAT_BOOSTER)return other->cls==PR_MOD_EVOLUTION_STAT_BOOSTER;
        if(!pr_mod_is(other,PR_MOD_STAT_BOOSTER)||m->mult!=other->mult||m->stats_mask!=other->stats_mask)return false;
        return m->cls!=PR_MOD_SPECIES_STAT_BOOSTER||(other->cls==m->cls&&m->n_species==other->n_species&&!memcmp(m->species,other->species,m->n_species*sizeof *m->species));
    }
    if(m->cls!=other->cls)return false;
    if(m->cls==PR_MOD_EXP_BOOSTER)return m->mult==other->mult;
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
 * pool is searched again. Clones share their mutable ModifierType reference. */
int pr_held_item_tier_for_pool(const pr_modifier *m, bool trainer)
{
    int tier = -1;
    if (!m->unknown_subclass) switch (m->cls) {
    case PR_MOD_TURN_HEAL: case PR_MOD_HIT_HEAL: case PR_MOD_FLINCH_CHANCE:
    case PR_MOD_CRIT_BOOSTER: tier = trainer ? 4 : 3; break;
    case PR_MOD_BYPASS_SPEED_CHANCE: case PR_MOD_MOVE_ACCURACY_BOOSTER:
        tier = trainer ? 3 : 2; break;
    case PR_MOD_SWITCH_EFFECT_TRANSFER: case PR_MOD_CONTACT_HELD_TRANSFER:
    case PR_MOD_SURVIVE_DAMAGE: case PR_MOD_NATURE_WEIGHT: tier = 3; break;
    case PR_MOD_RESET_NEGATIVE_STAT_STAGE: case PR_MOD_TURN_STATUS_EFFECT:
    case PR_MOD_DAMAGE_MONEY_REWARD: case PR_MOD_ATTACK_TYPE_BOOSTER:
    case PR_MOD_INSTANT_REVIVE: tier = 2; break;
    case PR_MOD_TURN_HELD_TRANSFER: case PR_MOD_MULTI_HIT: tier = 4; break;
    case PR_MOD_FIELD_EFFECT: case PR_MOD_SPECIES_CRIT_BOOSTER:
    case PR_MOD_STAT_BOOSTER: case PR_MOD_EVOLUTION_STAT_BOOSTER: tier = 2; break;
    case PR_MOD_SPECIES_STAT_BOOSTER:
        if(m->n_species&&m->species[0]==SPECIES_ID_CLAMPERL)tier=1;
        else if(m->n_species&&(m->species[0]==SPECIES_ID_PIKACHU||m->species[0]==SPECIES_ID_CUBONE||m->species[0]==SPECIES_ID_DITTO))tier=2;
        break;
    case PR_MOD_EXP_BOOSTER: if(m->mult==0.4)tier=3;else if(m->mult==1.0)tier=4;break;
    case PR_MOD_FRIENDSHIP_BOOSTER: tier = 1; break;
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
    if (m->type_ref) for (int side=0;side<2;side++)
        for (int k=0;k<bt->n_mods[side];k++)
            if (bt->mods[side][k].type_ref==m->type_ref)
                bt->mods[side][k].type_tier=m->type_tier;
    return tier;
}

/* Native canTransferHeldItemModifier clones before matching. The currently
 * supported transferable stock families preserve their class through clone().
 * Eviolite inherits StatBoosterModifier.clone and loses its evolution guard. */
bool pr_can_transfer_held_item(pr_battle *bt, const pr_modifier *stock,
                              pr_pokemon *receiver, int quantity)
{
    if (!stock || !receiver || quantity<0 || stock->stack_count<0
        || transfer_cap(stock)<0) {
        pr_note_unported(bt,PR_FEAT_MODIFIERS); return false;
    }
    pr_modifier copy=*stock;
    if(copy.cls==PR_MOD_EVOLUTION_STAT_BOOSTER)copy.cls=PR_MOD_STAT_BOOSTER;
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
    if(copy.cls==PR_MOD_EVOLUTION_STAT_BOOSTER)copy.cls=PR_MOD_STAT_BOOSTER;
    int matching = -1, cap = transfer_cap(&copy);
    if (cap < 0 || copy.stack_count < 0 || !pr_held_item_name_key(&copy)) {
        pr_note_unported(bt, PR_FEAT_MODIFIERS); return false;
    }
    for (int k=0;k<bt->n_mods[side];k++) {
        pr_modifier *m=&bt->mods[side][k];
        if (pr_mod_is(m, PR_MOD_HELD_ITEM) && same_type(m,&bt->mods[from_side][index])
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
    /* clone() copies the type object reference. Allocate only after preflight;
     * independent factory instances with equal keys must stay independent. */
    if (!copy.type_ref) {
        uint32_t largest=bt->modifier_type_ref_max;
        for (int s=0;s<2;s++) for (int k=0;k<bt->n_mods[s];k++)
            if (bt->mods[s][k].type_ref>largest) largest=bt->mods[s][k].type_ref;
        if (largest==UINT32_MAX) { pr_note_unported(bt,PR_FEAT_ENGINE_LIMIT); return false; }
        copy.type_ref=largest+1;
        bt->modifier_type_ref_max=copy.type_ref;
        bt->mods[from_side][index].type_ref=copy.type_ref;
    }
    int taken=copy.stack_count<quantity ? copy.stack_count : quantity;
    if (matching>=0 && taken>cap-existing) taken=cap-existing;
    bt->mods[from_side][index].stack_count-=taken;
    if (!bt->mods[from_side][index].stack_count && source) {
        remove_at(bt,from_side,index);
        if (from_side==side && matching>index) matching--;
    }
    if (matching>=0) remove_at(bt,side,matching);
    copy.loot_ref=0; /* clone is a distinct object, outside the original loot list */
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

/* HeldItemTransferModifier.apply, specialized to Grip Claw's one possible
 * transfer. Its name does not impose a contact flag: the owning MoveEffectPhase
 * calls it for attacks. The raw override does not invoke shouldApply. */
bool pr_apply_contact_held_item_transfer(pr_battle *bt,pr_engine *e,
    pr_pokemon *user,pr_pokemon *target,const pr_modifier *modifier)
{
    if (!target) return false;
    if (!user || !modifier || modifier->cls!=PR_MOD_CONTACT_HELD_TRANSFER
        || !pr_mod_is_ported_item(modifier)) {
        pr_note_unported(bt,PR_FEAT_MODIFIERS); return false;
    }
    (void)pr_battle_rand_seed_int(bt,1,0); /* choose the singleton target first */
    if (!bt->rng_live) { pr_note_unported(bt,PR_FEAT_GLOBAL_RNG); return false; }
    if (pr_rng_seed_float(&bt->rng)>modifier->mult*pr_mod_get_stack_count(modifier)) return false;
    int side=target->is_player?0:1,indices[PR_MAX_MODIFIERS],count=0;
    for (int k=0;k<bt->n_mods[side];k++) {
        const pr_modifier *m=&bt->mods[side][k];
        if (pr_mod_is(m,PR_MOD_HELD_ITEM) && m->pokemon_id==(int64_t)target->id
            && m->is_transferable) indices[count++]=k;
    }
    if (!count) return false;
    int index=indices[pr_battle_rand_seed_int(bt,count,0)];
    const char *name=pr_held_item_name_key(&bt->mods[side][index]);
    if (!pr_try_transfer_held_item(bt,e,side,index,user,1,false,true)) return false;
    pr_queue_msg(bt,e,pr_msg4("modifier:contactHeldItemTransferApply",
        pr_arg_pokemon(bt,"pokemonNameWithAffix",target),pr_arg_key("itemName",name),
        pr_arg_pokemon(bt,"pokemonName",user),pr_arg_key("typeName","modifierType:ModifierType.GRIP_CLAW.name")));
    return true;
}

void pr_apply_contact_held_item_transfers(pr_battle *bt,pr_engine *e,
                                         pr_pokemon *user,pr_pokemon *target)
{
    if (!user || user->hp<=0) return;
    int side=user->is_player?0:1;
    bool found=false;
    /* Canonical matching inventory holds one Grip Claw stack per owner.
     * Refuse raw duplicate aliases until object identity is represented. */
    pr_modifier selected;
    for (int k=0;k<bt->n_mods[side];k++) {
        const pr_modifier *m=&bt->mods[side][k];
        if (pr_mod_is(m,PR_MOD_CONTACT_HELD_TRANSFER)
            && (m->pokemon_id==-1 || m->pokemon_id==(int64_t)user->id)) {
            if (found) { pr_note_unported(bt,PR_FEAT_MODIFIERS); return; }
            selected=*m;found=true;
        }
    }
    if (found) pr_apply_contact_held_item_transfer(bt,e,user,target,&selected);
}

void pr_transfer_baton_on_switch(pr_battle *bt,pr_engine *e,
                                 pr_pokemon *last,pr_pokemon *incoming)
{
    if (!last || !incoming) { pr_note_unported(bt,PR_FEAT_PARTY); return; }
    /* Both pinned findModifier calls omit player, hence always scan player
     * stock, including when the phase is switching an enemy. */
    bool recipient_has_baton=false;
    int baton=-1;
    for (int k=0;k<bt->n_mods[0];k++) {
        const pr_modifier *m=&bt->mods[0][k];
        if (!pr_mod_is(m,PR_MOD_SWITCH_EFFECT_TRANSFER)) continue;
        if (m->pokemon_id==(int64_t)incoming->id) recipient_has_baton=true;
        if (baton<0 && m->pokemon_id==(int64_t)last->id) baton=k;
    }
    if (!recipient_has_baton && baton>=0)
        pr_try_transfer_held_item(bt,e,0,baton,incoming,1,false,false);
}
