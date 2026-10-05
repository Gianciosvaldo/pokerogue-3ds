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

void gc_item_remove_at(gc_run *run, int idx)
{
    if (idx < 0 || idx >= run->n_items) return;
    for (int i = idx; i < run->n_items - 1; i++) {
        run->items[i] = run->items[i + 1];
    }
    run->n_items--;
    memset(&run->items[run->n_items], 0, sizeof(gc_item));
}

int gc_item_remove_stacks(gc_run *run, const char *key, int count)
{
    int removed = 0;
    for (int i = 0; i < run->n_items && count > 0; ) {
        if (!strcmp(run->items[i].key, key) || (strstr(key, "BERRY") && strstr(run->items[i].key, "BERRY"))) {
            if (run->items[i].stack <= count) {
                int take = run->items[i].stack;
                count -= take;
                removed += take;
                gc_item_remove_at(run, i);
                continue;
            } else {
                run->items[i].stack -= count;
                removed += count;
                count = 0;
                break;
            }
        }
        i++;
    }
    return removed;
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
    /* These classes now have turn-path implementations in battle-core.  Parameters and
     * transferability come from upstream modifierTypes, not an inferred item name. */
    static const struct { const char *key; uint8_t cls; } held[] = {
        {"LEFTOVERS", PR_MOD_TURN_HEAL}, {"SHELL_BELL", PR_MOD_HIT_HEAL},
        {"FOCUS_BAND", PR_MOD_SURVIVE_DAMAGE}, {"QUICK_CLAW", PR_MOD_BYPASS_SPEED_CHANCE},
        {"KINGS_ROCK", PR_MOD_FLINCH_CHANCE}, {"WHITE_HERB", PR_MOD_RESET_NEGATIVE_STAT_STAGE},
        {"BERRY", PR_MOD_BERRY}
    };
    for (size_t i = 0; i < sizeof held / sizeof held[0]; i++) {
        if (strcmp(it->key, held[i].key)) continue;
        m->cls = held[i].cls;
        m->unknown_subclass = false;
        if (m->cls == PR_MOD_BERRY) m->berry_type = (int8_t)it->arg;
        return;
    }
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
    gc_int(run)->battle_item_count[0] = bt->n_mods[0];
    gc_int(run)->battle_item_count[1] = bt->n_mods[1];
}

/* Persist the losses of mapped consumables before another encounter rebuilds bt->mods or
 * a save serializes the run inventory. Match owner AND berry kind: two party members' berries
 * are separate entries. Unsupported items and entries beyond battle-core's capacity stay intact.
 * The tracked prefix shrinks with removed entries, so repeated synchronization never mistakes
 * an unrepresented overflow item for a consumed one. Transfers/additions require a later adapter. */
void gc_items_from_battle(gc_run *run)
{
    for (int side = 0; side < 2; side++) {
        gc_item *items = side ? run->enemy_items : run->items;
        int *n = side ? &run->n_enemy_items : &run->n_items;
        const int tracked = gc_int(run)->battle_item_count[side];
        int w = 0, removed = 0;
        for (int i = 0; i < *n; i++) {
            gc_item it = items[i];
            pr_modifier expected;
            to_modifier(&it, &expected);
            if (i < tracked && (expected.cls == PR_MOD_BERRY ||
                                expected.cls == PR_MOD_RESET_NEGATIVE_STAT_STAGE)) {
                int remaining = 0;
                for (int j = 0; j < run->bt.n_mods[side]; j++) {
                    const pr_modifier *m = &run->bt.mods[side][j];
                    if (m->unknown_subclass || m->cls != expected.cls ||
                        m->pokemon_id != expected.pokemon_id) continue;
                    if (m->cls == PR_MOD_BERRY && m->berry_type != expected.berry_type) continue;
                    remaining += m->stack_count;
                }
                if (remaining <= 0) { removed++; continue; }
                it.stack = remaining;
            }
            items[w++] = it;
        }
        memset(&items[w], 0, (size_t)(*n - w) * sizeof *items);
        *n = w;
        gc_int(run)->battle_item_count[side] = tracked - removed;
    }
}

/* ---- catching: the held items change hands (gc_catch.c) ---- */

/* The entries of one side that are held items (they carry a Pokemon's id): all of them, or one Pokemon's */
static void drop_held(gc_run *run, int side, uint32_t id, bool all)
{
    gc_item *items = side ? run->enemy_items : run->items;
    int *n = side ? &run->n_enemy_items : &run->n_items;
    int w = 0;
    for (int i = 0; i < *n; i++) {
        if (items[i].pokemon_id && (all || items[i].pokemon_id == id)) continue;
        items[w++] = items[i];
    }
    memset(&items[w], 0, (size_t)(*n - w) * sizeof *items);
    *n = w;
}

/* addToParty: every enemy held item is added to the player's modifiers (a Pokemon takes its id with it, so they are
 * the new member's), stacking where the same item is already held */
void gc_items_take_enemy_held(gc_run *run)
{
    for (int i = 0; i < run->n_enemy_items; i++) {
        const gc_item *it = &run->enemy_items[i];
        if (it->pokemon_id) gc_item_add(run, it->key, it->arg, it->pokemon_id, it->stack);
    }
}

void gc_items_drop_enemy_held(gc_run *run)
{
    drop_held(run, 1, 0, true);
    gc_items_to_battle(run);
}

void gc_items_drop_held_of(gc_run *run, uint32_t pokemon_id)
{
    drop_held(run, 0, pokemon_id, false);
    gc_items_to_battle(run);
}
