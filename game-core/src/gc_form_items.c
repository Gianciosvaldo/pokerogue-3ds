/* PartyUiHandler.getFormChangeItemsModifiers at upstream 8555c08. */
#include "internal.h"
#include "pr_item_forms.h"

static bool owned_form_item(const gc_item *item, uint32_t holder)
{
    return holder && gc_item_key_form_change(item->key) && item->pokemon_id == holder
        && !item->detached && !item->eviolite_base && item->stack == 1
        && !item->virtual_stack && pr_form_item_known(item->arg);
}

static int form_item_options(const gc_run *run, int party, int *indices, int capacity, int wanted, bool *found)
{
    if (found) *found = false;
    if (!run || party < 0 || party >= run->n_party || run->n_party > 6
        || run->n_items < 0 || run->n_items > PR_MAX_MODIFIERS
        || capacity < 0 || (capacity && !indices)) return 0;
    const pr_pokemon *pokemon = &run->party[party].b;
    bool ultra = false, active = false;
    for (int i = 0; i < run->n_items; i++) {
        const gc_item *item = &run->items[i];
        if (!owned_form_item(item, pokemon->id) || item->form_inactive) continue;
        active = true;
        ultra |= item->arg == FORM_CHANGE_ITEM_ULTRANECROZIUM_Z;
    }
    int count = 0;
    for (int i = 0; i < run->n_items; i++) {
        const gc_item *item = &run->items[i];
        if (!owned_form_item(item, pokemon->id)) continue;
        if (ultra) {
            if (item->form_inactive || item->arg != FORM_CHANGE_ITEM_ULTRANECROZIUM_Z) continue;
        } else if (active) {
            if (item->form_inactive && item->arg != FORM_CHANGE_ITEM_ULTRANECROZIUM_Z) continue;
        } else if (pokemon->species == SPECIES_ID_NECROZMA && item->arg == FORM_CHANGE_ITEM_ULTRANECROZIUM_Z) continue;
        if (found && i == wanted) *found = true;
        if (count < capacity) indices[count] = i;
        count++;
    }
    return count;
}

int gc_form_item_options(const gc_run *run, int party, int *indices, int capacity)
{
    return form_item_options(run, party, indices, capacity, -1, NULL);
}

/* The six caught predicates are the only item-form conditions that need a
 * system profile. Preserve an explicit refusal when those facts are absent. */
bool gc_form_items_context_known(const gc_run *run, const pr_pokemon *pokemon, int candidate)
{
    if (run->sys) return true;
    for (int i = 0; i < PR_ITEM_FORM_RULE_COUNT; i++) {
        const pr_item_form_rule *rule = &pr_item_form_rules[i];
        if (!rule->caught_species || rule->species != pokemon->species || rule->from != pokemon->form) continue;
        if (candidate == rule->item && (rule->flags & 1)) return false;
        for (int k = 0; k < run->n_items; k++) {
            const gc_item *item = &run->items[k];
            if (owned_form_item(item, pokemon->id) && item->arg == rule->item
                && (!item->form_inactive) == !!(rule->flags & 1)) return false;
        }
    }
    return true;
}

bool gc_form_item_toggle(gc_run *run, int party, int item, const char **why)
{
    const char *unused;
    if (!why) why = &unused;
    *why = NULL;
    if (!run || run->screen.kind != GC_SCREEN_REWARD || run->finished
        || gc_int(run)->rq.current.type != RP_SELECT_MODIFIER || !gc_int(run)->rq.waiting
        || run->n_enemy < 0 || run->n_enemy > 6 || run->n_enemy_items < 0 || run->n_enemy_items > PR_MAX_MODIFIERS) {
        *why = "form items can be changed between battles";
        return false;
    }
    bool found = false;
    form_item_options(run, party, NULL, 0, item, &found);
    if (!found) { *why = "unavailable form item"; return false; }
    gc_item *stock = &run->items[item];
    const bool old = stock->form_inactive;
    stock->form_inactive = !old;
    pr_pokemon *pokemon = &run->party[party].b;
    if (!gc_form_items_context_known(run, pokemon, -1)) {
        stock->form_inactive = old;
        pr_note_unported(&run->bt, PR_FEAT_FORM_CHANGE);
        *why = "Pokédex data unavailable";
        return false;
    }
    gc_items_to_battle(run);
    const int rule = pr_item_form_rule_index(&run->bt, pokemon);
    /* Source modal FormChangePhase returns to the same SelectModifierPhase.
     * Its animation completion maps to the existing form-state/event owner;
     * no reward is consumed and the waiting reward queue remains in place. */
    if (rule >= 0) gc_reward_change_form(run, party, pr_item_form_rules[rule].to);
    return true;
}
