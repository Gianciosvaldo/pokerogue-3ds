/* The player's items and the enemy's (upstream's modifier lists), and what game-core itself reads from
 * them. What each item does when taken or used is reward-core's; turning the lists into battle-core's
 * modifier entries (bt->mods) covers the classes battle-core knows, and an item it doesn't know goes in
 * as a plain held item or persistent modifier, which battle-core reports where it would matter. */
#include "internal.h"

int gc_item_stack(const gc_run *run, const char *key)
{
    int n = 0;
    for (int i = 0; i < run->n_items; i++)
        if (!strcmp(run->items[i].key, key)) n += run->items[i].stack;
    return n;
}

/* addModifier: a matching entry (same key, arg and Pokemon) stacks, else a new entry at the end */
void gc_item_add(gc_run *run, const char *key, int16_t arg, uint32_t pokemon_id, int stack)
{
    for (int i = 0; i < run->n_items; i++) {
        gc_item *it = &run->items[i];
        if (!strcmp(it->key, key) && it->arg == arg && it->pokemon_id == pokemon_id) { it->stack += stack; return; }
    }
    if (run->n_items >= GC_MAX_ITEMS) return;
    gc_item *it = &run->items[run->n_items++];
    it->key = key;
    it->arg = arg;
    it->pokemon_id = pokemon_id;
    it->stack = stack;
}

/* ExpBoosterModifier.apply for each Exp. Charm kind, in list order: floor(exp * (1 + stacks * boost)) */
double gc_apply_exp_boosters(const gc_run *run, double exp)
{
    for (int i = 0; i < run->n_items; i++) {
        const gc_item *it = &run->items[i];
        double b = 0;
        if (!strcmp(it->key, "EXP_CHARM")) b = 0.25;
        else if (!strcmp(it->key, "SUPER_EXP_CHARM")) b = 0.60;
        else if (!strcmp(it->key, "GOLDEN_EXP_CHARM")) b = 1.00;
        if (b > 0) exp = floor(exp * (1 + it->stack * b));
    }
    return exp;
}

/* One gc_item as battle-core's modifier. The held items and tokens battle-core reads on the damage path
 * get their class; everything else is a PokemonHeldItemModifier or PersistentModifier with
 * unknown_subclass set, so a query that would call its own apply reports itself unported. */
static void to_modifier(const gc_item *it, pr_modifier *m)
{
    memset(m, 0, sizeof *m);
    m->stack_count = it->stack;
    m->pokemon_id = it->pokemon_id ? (int64_t)it->pokemon_id : -1;
    m->is_transferable = true;
    m->unknown_subclass = true;
    m->cls = it->pokemon_id ? PR_MOD_HELD_ITEM : PR_MOD_PERSISTENT;
    if (!strcmp(it->key, "BASE_STAT_BOOSTER")) {          /* vitamins: BaseStatModifier(stat) */
        m->cls = PR_MOD_BASE_STAT; m->stat = (int8_t)it->arg; m->unknown_subclass = false;
    } else if (!strcmp(it->key, "ATTACK_TYPE_BOOSTER")) { /* AttackTypeBoosterModifier(type, 20) */
        m->cls = PR_MOD_ATTACK_TYPE_BOOSTER; m->move_type = (int8_t)it->arg; m->mult = 0.2; m->unknown_subclass = false;
    } else if (!strcmp(it->key, "SCOPE_LENS")) {          /* CritBoosterModifier(1) */
        m->cls = PR_MOD_CRIT_BOOSTER; m->num = 1; m->unknown_subclass = false;
    } else if (!strcmp(it->key, "ENEMY_DAMAGE_BOOSTER")) {
        m->cls = PR_MOD_ENEMY_DAMAGE_BOOSTER; m->mult = 1.05; m->unknown_subclass = false; m->is_transferable = false;
    } else if (!strcmp(it->key, "ENEMY_DAMAGE_REDUCTION")) {
        m->cls = PR_MOD_ENEMY_DAMAGE_REDUCER; m->mult = 0.975; m->unknown_subclass = false; m->is_transferable = false;
    }
}

void gc_items_to_battle(gc_run *run)
{
    pr_battle *bt = &run->bt;
    bt->n_mods[0] = bt->n_mods[1] = 0;
    for (int i = 0; i < run->n_items && bt->n_mods[0] < PR_MAX_MODIFIERS; i++)
        to_modifier(&run->items[i], &bt->mods[0][bt->n_mods[0]++]);
    for (int i = 0; i < run->n_enemy_items && bt->n_mods[1] < PR_MAX_MODIFIERS; i++)
        to_modifier(&run->enemy_items[i], &bt->mods[1][bt->n_mods[1]++]);
}
