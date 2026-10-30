/* Stand-ins for the workstreams that haven't landed (gc/ports.h). Each one keeps a run playable and
 * counts itself in run->stubs; none of them rolls on Phaser.Math.RND (they use gc_stub_rand), so the
 * run's own rolls stay where upstream has them, minus what the stand-in itself would have rolled. */
#include "internal.h"

/* ---- run-core: the last four level moves the enemy could know at its level ---- */
void gc_port_enemy_moveset(gc_run *run, gc_mon *m)
{
    gc_stub(run, GC_STUB_ENEMY_MOVESET);
    const pr_species *sp = gc_species(m->b.species);
    uint16_t mv[4];
    int n = 0;
    if (sp)
        for (int i = sp->level_moves.n - 1; i >= 0 && n < 4; i--) {
            const pr_level_move *lm = &sp->level_moves.v[i];
            if (lm->level > m->b.level || pr_moves[lm->move].unimplemented) continue;
            bool dup = false;
            for (int k = 0; k < n; k++) dup |= mv[k] == lm->move;
            if (!dup) mv[n++] = lm->move;
        }
    if (!n) mv[n++] = MOVE_ID_TACKLE;
    m->b.n_moves = 0;
    for (int k = 0; k < n; k++) gc_mon_set_move(m, k, mv[n - 1 - k]);
}

/* AddVoucherModifier.apply: vouchers are system data (the gacha), not run items; adds one to the count of the
 * reward's VoucherType (sys NULL = no system updates). false = not a voucher, nothing changed */
static bool give_voucher(gc_run *run, const char *key)
{
    if (!strcmp(key, "VOUCHER")) gc_system_add_voucher_count(run->sys, GC_VOUCHER_REGULAR, 1);
    else if (!strcmp(key, "VOUCHER_PLUS")) gc_system_add_voucher_count(run->sys, GC_VOUCHER_PLUS, 1);
    else if (!strcmp(key, "VOUCHER_PREMIUM")) gc_system_add_voucher_count(run->sys, GC_VOUCHER_PREMIUM, 1);
    else return false;
    return true;
}

/* ---- reward-core: what a reward does when it is taken. The options themselves are reward-core's
 * (gc_reward.c), and so is the application of Poke Balls, Full Heal, healing, revives, PP restores and PP Ups
 * (gc_reward_apply). What is left here is the rest: the items that are added to the player's items as they are,
 * each one that needs a real effect counted ---- */
static bool apply_reward(gc_run *run, const gc_offer *o, int party, int move_slot, const char **why);
static bool offer_stock(const gc_item *item,const gc_offer *o,uint32_t owner)
{
    if(strcmp(item->key,o->key)||item->pokemon_id!=owner)return false;
    return (strcmp(o->key,"BERRY")&&strcmp(o->key,"ATTACK_TYPE_BOOSTER"))||item->arg==o->arg;
}
bool gc_port_apply_reward(gc_run *run,const gc_offer *o,int party,int move_slot,const char **why)
{
    bool held=party>=0&&party<run->n_party&&o->target==GC_TARGET_POKEMON;
    uint32_t owner=held?run->party[party].b.id:0;bool existing=false;
    if(held)for(int i=0;i<run->n_items;i++)existing|=offer_stock(&run->items[i],o,owner);
    bool ok=apply_reward(run,o,party,move_slot,why);
    /* A reward option's type already carries its selected tier. Stacking keeps
     * the existing modifier/type, while a fresh stock keeps the incoming type. */
    if(ok&&held&&!existing&&o->tier>=0&&o->tier<=5){
        bool changed=false;
        for(int i=0;i<run->n_items;i++)if(offer_stock(&run->items[i],o,owner)){
            run->items[i].type_tier=(uint8_t)(o->tier+1);changed=true;
        }
        if(changed)gc_items_to_battle(run);
    }
    return ok;
}

static bool apply_reward(gc_run *run, const gc_offer *o, int party, int move_slot, const char **why)
{
    const char *k = o->key;
    switch (gc_reward_apply(run, o, party, move_slot, why)) {
    case GC_APPLY_DONE: return true;
    case GC_APPLY_REFUSED: return false;
    default: break;
    }
    /* an egg voucher taken from the reward screen: addModifier -> AddVoucherModifier.apply adds one to its count (no
     * text on the screen, no modifier in the player's list) */
    if (give_voucher(run, k)) return true;
    const int idx = gc_reward_item_index(k);
    if (o->target == GC_TARGET_NONE) {
        if (idx < 0 || !(pr_items[idx].flags & PR_ITEMF_PERSISTENT)) gc_stub(run, GC_STUB_REWARD_APPLY);
        gc_item_add(run, k, o->arg, 0, 1);
        return true;
    }
    if (party < 0 || party >= run->n_party) { *why = "pick a Pokemon"; return false; }
    const pr_pokemon *b = &run->party[party].b;
    /* The whole-Scene/stat guard may keep Multi Lens in this fallback (a
     * vitamin, Soul Dew or a fusion). Its inventory owner is still canonical:
     * constructor-only arg -1/0 must not create two independent lenses. Keep
     * the unsupported Scene diagnostic; only the proven .add owner is used. */
    bool multi_added = false;
    if (idx == PR_ITEM_MULTI_LENS && (o->arg == -1 || o->arg == 0)) {
        const int status = gc_multi_add(run, party);
        if (status == PR_REWARD_HELD_FULL) {
            gc_stub(run, GC_STUB_REWARD_APPLY);
            if (why) *why = "modifierType:ModifierType.PokemonHeldItemModifierType.extra.tooMany";
            return false;
        }
        multi_added = status == PR_REWARD_HELD_ADDED || status == PR_REWARD_HELD_STACKED;
    }
    /* a held item is added as it is; anything else would act on the Pokemon, and doesn't yet */
    if (idx < 0 || !(pr_items[idx].flags & PR_ITEMF_HELD) || idx == PR_ITEM_BASE_STAT_BOOSTER || idx == PR_ITEM_WIDE_LENS || idx == PR_ITEM_MULTI_LENS)
        gc_stub(run, GC_STUB_REWARD_APPLY); /* unsupported bounded persistent adapter path */
    if (!multi_added) gc_item_add(run, k, o->arg, b->id, 1);
    gc_event *ev = gc_ev_new(run, GC_EV_HP);
    if (ev) { ev->a = 0; ev->b = party; ev->c = b->hp; }
    return true;
}

int gc_port_give_reward(gc_run *run, const char *key)
{
    if (gc_reward_add_pokeballs(run, key)) return -1;
    if (give_voucher(run, key)) return -1;
    /* addModifier(newModifier): PersistentModifier.add stacks it onto the one the player has (match()), and
     * incrementStack refuses to go past getMaxStackCount(). The four access items (MapModifier,
     * MegaEvolutionAccessModifier, GigantamaxAccessModifier, TerastallizeAccessModifier) don't override match(), which
     * is false by default: each one is added as a modifier of its own, with no stacking and no cap */
    const int idx = gc_reward_item_index(key);
    const bool stacks = idx != PR_ITEM_MAP && idx != PR_ITEM_MEGA_BRACELET && idx != PR_ITEM_DYNAMAX_BAND &&
                        idx != PR_ITEM_TERA_ORB;
    if (stacks && idx >= 0 && (pr_items[idx].flags & PR_ITEMF_PERSISTENT) && pr_items[idx].max_stack > 0 &&
        gc_item_stack(run, key) + 1 > pr_items[idx].max_stack) {
        /* add() failed and the modifier isn't virtual: itemStackFull is queued and addModifier runs again with the
         * default modifier type of the type's tier. A ModifierRewardPhase's type has no tier, so the COMMON pool's
         * first entry (a Poke Ball); the caller shows the message after the reward's own text, as the queued
         * MessagePhase comes after the phase */
        const int fallback = pr_player_pool_entries[pr_player_pool[0].start].item;
        gc_port_give_reward(run, pr_item_keys[fallback]);
        return fallback;
    }
    gc_item_add(run, key, -1, 0, 1);
    return -1;
}
