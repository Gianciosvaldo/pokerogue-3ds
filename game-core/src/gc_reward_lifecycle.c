/* Caller lifecycle at upstream 8555c08: evolution items (EvolutionItemModifier.apply, every species) and Giratina's
 * Griseous Core. A fusion keeps its counted stand-in. */
#include "internal.h"

int gc_reward_lifecycle_apply(gc_run *run, const gc_offer *o, int party, const char **why)
{
    const int item = gc_reward_item_index(o->key);
    const bool evolution = (item == PR_ITEM_EVOLUTION_ITEM || item == PR_ITEM_RARE_EVOLUTION_ITEM);
    const bool form = (item == PR_ITEM_FORM_CHANGE_ITEM || item == PR_ITEM_RARE_FORM_CHANGE_ITEM)
                      && o->arg == FORM_CHANGE_ITEM_GRISEOUS_CORE;
    if (!evolution && !form) return GC_APPLY_UNHANDLED;
    int k;
    if (party < 0 || party >= run->n_party || (k = gc_battle_slot_of(run, party)) < 0) {
        *why = "pick a Pokemon";
        return GC_APPLY_REFUSED;
    }
    gc_mon *m = &run->party[party];
    if (m->b.fusion_species) return GC_APPLY_UNHANDLED;
    int target_form = -1;
    if (!evolution) {
        const pr_species *s = gc_species(SPECIES_ID_GIRATINA);
        if (s) for (int i = 0; i < s->nforms; i++)
            if (!strcmp(gc_form(s->id, i)->form_key, "origin")) target_form = i;
        if (target_form < 0) {
            *why = "unavailable form or item storage";
            return GC_APPLY_REFUSED;
        }
        /* PersistentModifier.add cannot stack a second Core. addModifier replaces
         * the full stack with the PLAYER pool's default item for the offer tier.
         * The five pinned pool defaults are the five ball rewards, already handled
         * by the caller. This is distinct from reward-core's held-item filter. */
        if (m->b.species == SPECIES_ID_GIRATINA) {
            for (int i = 0; i < run->n_items; i++) {
                const gc_item *it = &run->items[i];
                if (it->pokemon_id != m->b.id || it->arg != o->arg
                    || (strcmp(it->key,"FORM_CHANGE_ITEM") && strcmp(it->key,"RARE_FORM_CHANGE_ITEM"))) continue;
                const int tier = o->tier < 5 ? o->tier : 0;
                const int fallback = pr_player_pool_entries[pr_player_pool[tier].start].item;
                gc_ev_msg(run,"battle:itemStackFull",2,
                    pr_arg_key("fullItemName","modifierType:FormChangeItem.GRISEOUS_CORE"),
                    (pr_msg_arg){"itemName",PR_ARG_STR,0,pr_reward_strings[pr_items[fallback].name]});
                gc_offer replacement = {.key=pr_item_keys[fallback], .arg=-1, .target=GC_TARGET_NONE};
                return gc_port_apply_reward(run,&replacement,-1,-1,why) ? GC_APPLY_DONE : GC_APPLY_REFUSED;
            }
        }
        if (run->n_items >= GC_MAX_ITEMS) {
            *why = "unavailable item storage";
            return GC_APPLY_REFUSED;
        }
    }
    pr_reward_ctx *ctx = gc_reward_ctx(run);
    const char *msg = NULL;
    if (!pr_reward_apply(ctx, (uint16_t)item, o->arg, k, -1, &msg)) {
        *why = "partyUiHandler:anyEffect";
        return GC_APPLY_REFUSED;
    }
    if (evolution) {
        const pr_reward_evolution_event *ev = &ctx->evolution_events[0];
        /* reward-core decided that the item works; game-core's own lookup must reach the same evolution */
        int evo;
        if (ctx->n_evolution_events != 1 || ev->fusion || ev->pokemon_id != m->b.id
            || (evo = gc_mon_item_evolution(run, m, ev->item)) < 0
            || gc_evo_data[evo].species != ev->target_species) {
            gc_stub(run, GC_STUB_EVOLUTION);
            *why = "unexpected evolution request";
            return GC_APPLY_REFUSED;
        }
        /* The stone is consumed now; the cancellable phase changes the mon later. */
        gc_rq_unshift(run, gc_rp(RP_EVOLUTION, party, evo, m->b.level - 1));
    } else {
        /* addModifier installs the held item before triggering FormChangePhase.
         * Ownership follows the stable Pokemon ID, not the party position. */
        gc_item_add(run, pr_item_keys[item], o->arg, m->b.id, 1);
        /* Registry also has the automatic origin->altered reverse rule. A new
         * active Core cannot fire that inactive-item trigger. addModifier still
         * retains the item, and a free SelectModifierPhase still completes. */
        if (!strcmp(gc_form(m->b.species, m->b.form)->form_key, "altered"))
            gc_rq_unshift(run, gc_rp(RP_REWARD_FORM, party, target_form, 0));
    }
    return GC_APPLY_DONE;
}

void gc_reward_change_form(gc_run *run, int party, int form)
{
    gc_mon *m = &run->party[party];
    const pr_msg_arg pre_name = gc_m_mon(run,"preName",0,party);
    m->b.form = (uint8_t)form;
    const pr_form *f = gc_form(m->b.species, form);
    const int count = f->ability_hidden ? 3 : 2;
    if (m->ability_index >= count) m->ability_index = count - 1;
    m->b.ability_index = (int8_t)m->ability_index;
    m->b.base_ability = (uint16_t)gc_form_ability(m->b.species, f, m->ability_index);
    if (m->b.ability_override <= 0) m->b.ability = m->b.base_ability;
    pr_refresh_ability(&m->b);
    /* PlayerPokemon.changeForm marks seen/caught before stats. Unlike evolution
     * it neither records IVs nor learns moves. Origin preserves Terastallization. */
    if (run->sys) {
        gc_system_set_pokemon_seen(run->sys, m, false, false);
        gc_system_set_pokemon_caught(run->sys, m, false, false);
    }
    pr_change_form_state(&run->bt, &m->b, form);
    if (m->b.ability_lookup_known) m->ability_index = (uint8_t)m->b.ability_index;
    gc_ev_msg(run,"battlePokemonForm:formChange",1,pre_name);
    gc_event *ev = gc_ev_new(run, GC_EV_EVOLVED);
    if (ev) { ev->a = party; ev->b = m->b.species; ev->c = m->b.species; }
}
