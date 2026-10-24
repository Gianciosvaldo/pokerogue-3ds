/* PokemonFormChangeItemModifier.apply and item-trigger form registrations.
 * Native source 8555c08. The scene query intentionally reads player modifiers,
 * even when its Pokemon argument belongs to the opposing side. */
#include "pr/engine.h"
#include "pr_item_forms.h"
#include <stddef.h>

bool pr_form_item_known(int item)
{
    return item >= 0 && item < PR_ITEM_FORM_ENUM_BOUND && pr_form_item_name_keys[item] != NULL;
}

static int default_form(const pr_pokemon *p)
{
    if (p->species == SPECIES_ID_MEOWSTIC) return p->gender == 1 /* Gender.FEMALE */ ? 1 : 0;
    if (p->species == SPECIES_ID_TOXTRICITY) {
        switch (p->nature) {
        case NATURE_LONELY: case NATURE_BOLD: case NATURE_RELAXED: case NATURE_TIMID:
        case NATURE_SERIOUS: case NATURE_MODEST: case NATURE_MILD: case NATURE_QUIET:
        case NATURE_BASHFUL: case NATURE_CALM: case NATURE_GENTLE: case NATURE_CAREFUL:
            return 1;
        default: return 0;
        }
    }
    return -1;
}

static bool item_trigger(const pr_battle *bt, const pr_pokemon *p, const pr_item_form_rule *r)
{
    for (int i = 0; i < bt->n_mods[0]; i++) {
        const pr_modifier *m = &bt->mods[0][i];
        if (m->cls == PR_MOD_FORM_CHANGE_ITEM && !m->unknown_subclass && m->pokemon_id == p->id
            && m->num == r->item && m->form_item_active == !!(r->flags & 1)) return true;
    }
    return false;
}

bool pr_item_form_rule_eligible(pr_battle *bt, const pr_pokemon *p, int rule)
{
    if (!bt || !p || rule < 0 || rule >= PR_ITEM_FORM_RULE_COUNT) return false;
    if (bt->n_mods[0] > PR_MAX_MODIFIERS) { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); return false; }
    const pr_item_form_rule *r = &pr_item_form_rules[rule];
    /* Source uses the permanent primary species/form keys here, independently
     * of fusion species and the visual species/form supplied by Transform. */
    if (p->species != r->species || p->form != r->from || p->form == r->to) return false;
    const pr_species *species = pr_pokemon_species(p);
    if (!species || species->nforms < 2 || p->form >= species->nforms) return false;
    if (!item_trigger(bt, p, r)) return false;
    if (r->ability && !pr_has_ability_active(bt, p, r->ability)) return false;
    if (r->caught_species) {
        if (!bt->form_item_dex_known) { pr_note_unported(bt, PR_FEAT_FORM_CHANGE); return false; }
        int bit = -1;
        for (int i = 0; i < PR_ITEM_FORM_CAUGHT_COUNT; i++)
            if (pr_form_item_caught_species[i] == r->caught_species) bit = i;
        if (bit < 0) { pr_note_unported(bt, PR_FEAT_FORM_CHANGE); return false; }
        if (!(bt->form_item_dex_mask & (1u << bit))) return false;
    }
    if (r->default_form != UINT8_MAX) {
        int form = default_form(p);
        if (form < 0) { pr_note_unported(bt, PR_FEAT_FORM_CHANGE); return false; }
        if (form != r->default_form) return false;
    }
    return true;
}

int pr_item_form_rule_index(pr_battle *bt, const pr_pokemon *p)
{
    int first = -1, second = -1, count = 0;
    for (int i = 0; i < PR_ITEM_FORM_RULE_COUNT; i++) {
        if (!pr_item_form_rule_eligible(bt, p, i)) continue;
        if (!count) first = i;
        else if (count == 1) second = i;
        count++;
    }
    if (!p || p->species != SPECIES_ID_NECROZMA || count <= 1) return first;
    bool lunar = false, solar = false;
    for (int i = 0; i < bt->n_mods[0]; i++) {
        const pr_modifier *m = &bt->mods[0][i];
        if (m->cls != PR_MOD_FORM_CHANGE_ITEM || m->unknown_subclass
            || m->pokemon_id != p->id || !m->form_item_active) continue;
        lunar |= m->num == FORM_CHANGE_ITEM_N_LUNARIZER;
        solar |= m->num == FORM_CHANGE_ITEM_N_SOLARIZER;
    }
    return lunar ? first : solar ? second : -1;
}

bool pr_trigger_item_form_change(pr_battle *bt, pr_pokemon *p, bool delayed, int *selected_rule)
{
    int rule = pr_item_form_rule_index(bt, p);
    if (selected_rule) *selected_rule = rule;
    if (rule < 0) return false;
    const pr_item_form_rule *r = &pr_item_form_rules[rule];
    pr_engine *e = bt->engine;
    if (!e) { pr_note_unported(bt, PR_FEAT_FORM_CHANGE); return false; }
    int phase = pr_phase_new(e, p->is_player && !(r->flags & 2) ? PH_FORM_CHANGE : PH_QUIET_FORM_CHANGE,
                             (int8_t)pr_battler_index(bt, p));
    if (phase < 0) { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); return false; }
    e->pool[phase].pokemon = p;
    e->pool[phase].move_id = r->to;
    if (delayed) pr_phase_push(bt, e, phase);
    else pr_phase_unshift(bt, e, phase);
    return true;
}

bool pr_apply_form_change_item(pr_battle *bt, pr_pokemon *p, pr_modifier *m,
                               bool active, int *selected_rule)
{
    if (selected_rule) *selected_rule = -1;
    if (!bt || !p || !m || m->cls != PR_MOD_FORM_CHANGE_ITEM || m->unknown_subclass) return false;
    /* This override does not call its parent's shouldApply/condition gates.
     * In particular a direct call on a foreign holder still queries the scene. */
    bool switch_active = m->form_item_active && !active;
    if (switch_active) m->form_item_active = false;
    bool result = pr_trigger_item_form_change(bt, p, false, selected_rule);
    if (switch_active) m->form_item_active = true;
    return result;
}
