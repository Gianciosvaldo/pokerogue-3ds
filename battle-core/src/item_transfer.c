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
    bool cancelled=false;
    /* Source treats a zero holder ID as absent at this scene helper boundary. */
    if (copy.pokemon_id) {
        pr_ab_params block={.pokemon=victim,.move=-1,.flag=&cancelled};
        pr_apply_ab_attrs(bt,PR_AC_BlockItemTheftAbAttr,&block);
    }
    if(cancelled)return false;
    int cap=transfer_cap(&copy),matching=-1;
    const char *name=pr_held_item_name_key(&copy);
    if(cap<0||!name||copy.stack_count<0){pr_note_unported(bt,PR_FEAT_MODIFIERS);return false;}
    for(int k=0;k<bt->n_mods[side];k++){
        const pr_modifier *m=&bt->mods[side][k];
        if(same_type(m,&copy)&&m->pokemon_id==(int64_t)p->id){
            if(transfer_cap(m)<0){pr_note_unported(bt,PR_FEAT_MODIFIERS);return false;}
            matching=k;break;
        }
    }
    if(matching>=0&&bt->mods[side][matching].stack_count>=cap)return false;
    if(matching<0&&bt->n_mods[side]>=PR_MAX_MODIFIERS){pr_note_unported(bt,PR_FEAT_ENGINE_LIMIT);return false;}
    int existing=matching>=0?bt->mods[side][matching].stack_count:0;
    int taken=copy.stack_count>0?1:0;
    bt->mods[other][index].stack_count-=taken;
    if(!bt->mods[other][index].stack_count&&victim->id)remove_at(bt,other,index);
    if(matching>=0)remove_at(bt,side,matching);
    copy.pokemon_id=p->id;copy.stack_count=existing+taken;copy.virtual_stack_count=0;
    /* The common native clone constructors restore their transferable default. */
    copy.is_transferable=true;
    if(copy.stack_count>0)bt->mods[side][bt->n_mods[side]++]=copy;
    if(victim->id)pr_refresh_berry_modifiers(bt,other);
    pr_refresh_berry_modifiers(bt,side);
    if(victim->id){pr_ab_params lost={.pokemon=victim,.move=-1};pr_apply_ab_attrs(bt,PR_AC_PostItemLostAbAttr,&lost);}
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
