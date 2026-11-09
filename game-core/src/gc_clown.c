/* Clowning Around (src/data/mystery-encounters/encounters/clowning-around-encounter.ts, upstream 8555c08).
 *
 * onInit (the wave's stream): the ability Blacephalon will have, RANDOM_ABILITY_POOL[randSeedInt(15)], then its two types: firstType = randSeedInt(18),
 * secondType = randSeedInt(17), plus one when it is not below firstType (never the same type twice). The dialogue tokens are the ability's name and
 * Blacephalon's name.
 *
 * Option 2 (Remain Unprovoked), onPreOptionPhase (executeWithSeedOffset(.., getSeedOffset())): the Pokemon holding the most transferable non-berry stacks
 * (the first one of the party on a tie) has its Berries taken and as many made again from the berry pool, then its Ultra and Rogue items (Lucky Egg and Soothe
 * Bell counting as Ultra, Golden Egg as Rogue) taken and as many made from the Ultra and Rogue pools. Every pool entry is drawn at most as often as the item
 * can stack (a pool entry is dropped at its limit). A Pokemon already at an item's maximum stack gets nothing for that draw (applyModifierTypeToPlayerPokemon).
 * The option then leaves with the heal shop.
 *
 * Option 3 (Return the Insults), onPreOptionPhase: every Pokemon of the party gets a new second type (the first slot is kept, null in customPokemonData.types):
 * a type of one of its attacking moves that it does not have yet when it has one (Set, sorted as strings - 0, 1, 10, 11 ... 17, 2, 3 ... 9 - and shuffled), else a random
 * regular type it does not have. A fused Pokemon's fusion half gets the same types. The option then leaves with the heal shop.
 */
#include "internal.h"
#include <string.h>

static const uint16_t ABILITY_POOL[15] = {
    ABILITY_ID_STURDY, ABILITY_ID_PICKUP, ABILITY_ID_INTIMIDATE, ABILITY_ID_GUTS, ABILITY_ID_DROUGHT, ABILITY_ID_DRIZZLE, ABILITY_ID_SNOW_WARNING,
    ABILITY_ID_SAND_STREAM, ABILITY_ID_ELECTRIC_SURGE, ABILITY_ID_PSYCHIC_SURGE, ABILITY_ID_GRASSY_SURGE, ABILITY_ID_MISTY_SURGE, ABILITY_ID_MAGICIAN,
    ABILITY_ID_SHEER_FORCE, ABILITY_ID_PRANKSTER,
};

/* onInit on the wave's stream (a local copy: asking draws nothing from the run) */
void gc_clown_roll(const gc_run *run, int *ability, int *first_type, int *second_type)
{
    pr_rng r = pr_run_seed_offset_rng(&run->run, run->wave, NULL);
    const int a = pr_rng_seed_int(&r, 15, 0);
    const int first = pr_rng_seed_int(&r, 18, 0);
    int second = pr_rng_seed_int(&r, 17, 0);
    if (second >= first) second++;
    if (ability) *ability = ABILITY_POOL[a];
    if (first_type) *first_type = first;
    if (second_type) *second_type = second;
}

enum { TIER_ULTRA = 2, TIER_ROGUE = 3 };
typedef struct { const char *key; int16_t arg; int left; } pool_entry;

/* applyModifierTypeToPlayerPokemon: a Pokemon at the item's maximum stack of it gets nothing */
static bool give(gc_run *run, const gc_mon *to, const char *key, int16_t arg)
{
    int held = 0;
    for (int i = 0; i < run->n_items; i++) {
        const gc_item *it = &run->items[i];
        if (!strcmp(it->key, key) && it->arg == arg && it->pokemon_id == to->b.id) held += it->stack + it->virtual_stack;
    }
    if (held && held >= gc_item_max_held(run, key, arg, to)) return false;
    gc_item_add(run, key, arg, to->b.id, 1);
    return true;
}

/* generateItemsOfTier: `n` draws from a pool whose entries are removed at their stack limit, on the stream `r` (run->run.rng while a generator runs) */
static int generate_tier(gc_run *run, pr_rng *r, const gc_mon *to, pool_entry *pool, int len, int n)
{
    int added = 0;
    for (int i = 0; i < n; i++) {
        if (len == 0) return added;   /* out of items to spawn */
        const int idx = pr_rng_seed_int(r, len, 0);
        int16_t arg = pool[idx].arg;
        bool made = true;
        if (!strcmp(pool[idx].key, "ATTACK_TYPE_BOOSTER")) {
            /* generateModifierType(ATTACK_TYPE_BOOSTER): the generator picks the type, on the same stream */
            const pr_rng saved = run->run.rng;
            run->run.rng = *r;
            const int type = pr_reward_generate(gc_reward_ctx(run), PR_ITEM_ATTACK_TYPE_BOOSTER);
            *r = run->run.rng;
            run->run.rng = saved;
            made = type >= 0;   /* null (no attacking move in the party): upstream's apply throws, the draw still counts */
            arg = (int16_t)type;
        }
        if (made && give(run, to, pool[idx].key, arg)) added++;
        if (--pool[idx].left <= 0) {
            memmove(&pool[idx], &pool[idx + 1], (size_t)(len - idx - 1) * sizeof pool[0]);
            len--;
        }
    }
    return added;
}

static int transferable_stacks(const gc_run *run, const gc_mon *m)
{
    int n = 0;
    for (int i = 0; i < run->n_items; i++) {
        const gc_item *it = &run->items[i];
        if (it->pokemon_id == m->b.id && gc_gts_item_transferable(it) && strcmp(it->key, "BERRY")) n += it->stack;
    }
    return n;
}

/* option 2 */
static void shuffle_items(gc_run *run)
{
    pr_rng r = pr_run_seed_offset_rng(&run->run, run->wave * 1000 + 512, NULL);
    const gc_mon *best = &run->party[run->party_order[0]];
    int count = transferable_stacks(run, best);
    for (int k = 0; k < run->n_party; k++) {
        const gc_mon *m = &run->party[run->party_order[k]];
        const int next = transferable_stacks(run, m);
        if (next > count) { best = m; count = next; }
    }
    const pr_msg_arg holder = pr_arg_pokemon_name(&run->bt, "switchPokemon", &best->b);
    const uint32_t id = best->b.id;

    /* the Berries: all of them taken, as many made again */
    int n_berries = 0;
    for (int i = 0; i < run->n_items;) {
        if (run->items[i].pokemon_id == id && !strcmp(run->items[i].key, "BERRY")) { n_berries += run->items[i].stack; gc_item_remove_at(run, i); }
        else i++;
    }
    pool_entry berries[11] = {
        {"BERRY", BERRY_TYPE_APICOT, 3}, {"BERRY", BERRY_TYPE_ENIGMA, 2}, {"BERRY", BERRY_TYPE_GANLON, 3}, {"BERRY", BERRY_TYPE_LANSAT, 3},
        {"BERRY", BERRY_TYPE_LEPPA, 2}, {"BERRY", BERRY_TYPE_LIECHI, 3}, {"BERRY", BERRY_TYPE_LUM, 2}, {"BERRY", BERRY_TYPE_PETAYA, 3},
        {"BERRY", BERRY_TYPE_SALAC, 2}, {"BERRY", BERRY_TYPE_SITRUS, 2}, {"BERRY", BERRY_TYPE_STARF, 3},
    };
    int added = generate_tier(run, &r, best, berries, 11, n_berries);

    /* the Ultra and Rogue items, taken one entry at a time (the tier of an entry is read with the earlier ones already gone) */
    int n_ultra = 0, n_rogue = 0;
    for (int i = 0; i < run->n_items;) {
        const gc_item *it = &run->items[i];
        if (it->pokemon_id != id || !gc_gts_item_transferable(it) || !strcmp(it->key, "BERRY")) { i++; continue; }
        int tier = gc_item_pool_tier(run, it);
        if (tier < 0) tier = TIER_ULTRA;   /* type.tier ?? ModifierTier.ULTRA */
        const int stack = it->stack;
        if (!strcmp(it->key, "GOLDEN_EGG") || tier == TIER_ROGUE) { n_rogue += stack; gc_item_remove_at(run, i); }
        else if (!strcmp(it->key, "LUCKY_EGG") || !strcmp(it->key, "SOOTHE_BELL") || tier == TIER_ULTRA) { n_ultra += stack; gc_item_remove_at(run, i); }
        else i++;
    }
    pool_entry ultra[5] = {
        {"REVIVER_SEED", -1, 1}, {"GOLDEN_PUNCH", -1, 5}, {"ATTACK_TYPE_BOOSTER", -1, 99}, {"QUICK_CLAW", -1, 3}, {"WIDE_LENS", -1, 3},
    };
    pool_entry rogue[8] = {
        {"LEFTOVERS", -1, 4}, {"SHELL_BELL", -1, 4}, {"SOUL_DEW", -1, 10}, {"SCOPE_LENS", -1, 1}, {"BATON", -1, 1}, {"FOCUS_BAND", -1, 5},
        {"KINGS_ROCK", -1, 3}, {"GRIP_CLAW", -1, 5},
    };
    added += generate_tier(run, &r, best, ultra, 5, n_ultra);
    added += generate_tier(run, &r, best, rogue, 8, n_rogue);
    /* removeModifier never runs updateModifiers; every addModifier(.., false, false, false, true) that adds something does (it prunes, e.g., an evolution
     * tracker whose holder cannot use it), so a shuffle that adds nothing leaves the list as the removals left it */
    if (added) {
        gc_me_prune_items(run);
        gc_me_update_modifiers(run);
    } else {
        gc_items_to_battle(run);
    }

    const pr_msg_arg blacephalon = pr_arg_key("blacephalonName", "pokemon:blacephalon");
    gc_ev_msg(run, "mysteryEncounters/clowningAround:option.2.selected2", 2, blacephalon, holder);
    gc_ev_msg(run, "mysteryEncounters/clowningAround:option.2.selected3", 0);
}

static bool has_type(const int8_t *types, int n, int t)
{
    for (int i = 0; i < n; i++) if (types[i] == t) return true;
    return false;
}

/* [...new Set(types)].sort(): Array.prototype.sort compares numbers as strings */
static bool string_before(int a, int b)
{
    char x[4], y[4];
    snprintf(x, sizeof x, "%d", a);
    snprintf(y, sizeof y, "%d", b);
    return strcmp(x, y) < 0;
}

/* option 3 */
static void shuffle_types(gc_run *run)
{
    pr_rng r = pr_run_seed_offset_rng(&run->run, run->wave * 1000 + 512, NULL);
    for (int k = 0; k < run->n_party; k++) {
        gc_mon *m = &run->party[run->party_order[k]];
        int8_t original[PR_MAX_TYPES];
        const int n_original = pr_get_types(&m->b, original, PR_TYPES_NO_TERA | PR_TYPES_BYPASS_SUMMON | PR_TYPES_IGNORE_THIRD);

        /* the types of its attacks that are not its own (a Set, in order of first appearance) */
        int priority[4], n_priority = 0;
        for (int j = 0; j < m->b.n_moves && j < 4; j++) {
            const int mv = m->b.moves[j];
            if (mv <= 0 || mv >= PR_MOVE_COUNT) continue;
            const int t = pr_moves[mv].type;
            if (pr_moves[mv].category == MOVE_CATEGORY_STATUS || has_type(original, n_original, t)) continue;
            bool seen = false;
            for (int p = 0; p < n_priority; p++) seen = seen || priority[p] == t;
            if (!seen) priority[n_priority++] = t;
        }
        if (n_priority > 0) {
            for (int a = 1; a < n_priority; a++)
                for (int b = a; b > 0 && string_before(priority[b], priority[b - 1]); b--) {
                    const int t = priority[b]; priority[b] = priority[b - 1]; priority[b - 1] = t;
                }
            for (int i = n_priority - 1; i > 0; i--) {   /* randSeedShuffle: integerInRange(0, i) */
                const int j = pr_rng_seed_int(&r, i + 1, 0);
                const int t = priority[i]; priority[i] = priority[j]; priority[j] = t;
            }
        }
        int second = -1;
        while (second < 0 || has_type(original, n_original, second)) {
            if (n_priority > 0) second = priority[--n_priority];
            else second = pr_rng_seed_int(&r, 18, 0);   /* getRandomRegularPokemonType: randSeedIntRange(NORMAL, FAIRY) */
        }
        m->b.custom_types[0] = 0;
        m->b.custom_types[1] = (uint8_t)(second + 1);
        if (m->b.fusion_species) {
            m->b.fusion_custom_types[0] = 0;
            m->b.fusion_custom_types[1] = (uint8_t)(second + 1);
        }
    }
    gc_items_to_battle(run);
    const pr_msg_arg blacephalon = pr_arg_key("blacephalonName", "pokemon:blacephalon");
    gc_ev_msg(run, "mysteryEncounters/clowningAround:option.3.selected2", 1, blacephalon);
    gc_ev_msg(run, "mysteryEncounters/clowningAround:option.3.selected3", 0);
}

/* options 2 and 3 (the fight, option 1, is gc_mystery.c's stand-in until it is ported) */
void gc_clown_execute(gc_run *run, int option)
{
    if (option == 1) shuffle_items(run);
    else shuffle_types(run);
    gc_me_leave_with_heal_shop(run);   /* leaveEncounterWithoutBattle(true) */
}
