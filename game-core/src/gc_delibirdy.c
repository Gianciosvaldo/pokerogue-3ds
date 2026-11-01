/* Delibird-y (src/data/mystery-encounters/encounters/delibirdy-encounter.ts at pinned upstream 8555c08).
 *
 * Three options, nothing drawn from any RNG (the timed event's extra gift, getDelibirdyBuff, is out with the other timed events):
 *
 * 1 Give Money (MoneyRequirement(0, 2)): onPreOptionPhase takes the wave's money times 2 (requiredMoney as the requirement last worked it out,
 * so getWaveMoneyAmount(2); updatePlayerMoney(.., false showMessage): no text). The option phase gives an Amulet Coin through a ModifierRewardPhase
 * unshifted ahead of the rewards phase, or, when the Amulet Coin is at its maximum stack, a Shell Bell for the first Pokemon of the field
 * (applyModifierTypeToPlayerPokemon(getPlayerPokemon()): the first Pokemon allowed in battle among the field of a mystery encounter, which is
 * never a double battle, so party slot 0 or none), with the text "battle:rewardGain" said at once. Then leaveEncounterWithoutBattle(true).
 * 2 Give Food (a Pokemon holding a Berry or a Reviver Seed that can be transferred): the party screen filters on the Pokemon, a second list
 * names its held Berries and Reviver Seeds (getHeldItems() order; the modifier's name). A Berry is paid with a Candy Jar, a Reviver Seed with a Berry
 * Pouch; at the maximum stack of that one, a Shell Bell as above. The Pokemon loses one stack of the item (loseHeldItem(.., false): no updateModifiers;
 * the item goes with its last stack).
 * 3 Give an Item (a Pokemon holding any other transferable item): the same, paid with a Healing Charm, or a Shell Bell for the party's first
 * Pokemon (getPlayerParty()[0], fainted or not) at the maximum stack.
 *
 * The encounter spawns when a Pokemon of the whole party (fainted ones too) holds an item option 2 or option 3 takes (the Some(..) of two
 * HeldItemRequirements); an option is on when the same holds for it (the party screen's filter is the same query per Pokemon).
 * OPTION_3_DISALLOWED_MODIFIERS also names PokemonBaseStatModifier (no such class: vitamins are BaseStatModifier), PokemonBaseStatTotalModifier,
 * and TerastallizeModifier; those items are not transferable (or, the Tera Shard, never held), so option 3 is every transferable held item
 * except a Berry or a Reviver Seed.
 *
 * Not ported: a field without an active Pokemon on the Shell Bell branches (upstream's getPlayerPokemon() answers undefined and
 * `.newModifier` throws; here nothing is given and the stand-in is counted), the intro sprites, the outro text. */
#include "internal.h"
#include <math.h>
#include <string.h>

#define DELIBIRDY_MONEY_PRICE_MULTIPLIER 2.0

/* BerryModifier, PokemonInstantReviveModifier */
static bool food_item(const gc_item *it)
{
    return !strcmp(it->key, "BERRY") || !strcmp(it->key, "REVIVER_SEED");
}

/* HeldItemRequirement(OPTION_2_ALLOWED_MODIFIERS) for one held item: a transferable Berry or Reviver Seed */
static bool option2_item(const gc_item *it)
{
    return gc_gts_item_transferable(it) && food_item(it);
}

/* HeldItemRequirement(OPTION_3_DISALLOWED_MODIFIERS, 1, true) for one held item: transferable and not on the list */
static bool option3_item(const gc_item *it)
{
    return gc_gts_item_transferable(it) && !food_item(it) && strcmp(it->key, "TERA_SHARD") && strcmp(it->key, "BASE_STAT_BOOSTER")
        && strcmp(it->key, "MYSTERY_ENCOUNTER_SHUCKLE_JUICE");
}

/* the held items of party member `idx` the option takes (option 1 = Give Food, 2 = Give an Item), as indexes into run->items in getHeldItems()
 * order. These are the second list the player picks from */
int gc_delibirdy_held_items(const gc_run *run, int option, int idx, int out[GC_MAX_ITEMS])
{
    if (idx < 0 || idx >= run->n_party || (option != 1 && option != 2)) return 0;
    const uint32_t id = run->party[idx].b.id;
    int n = 0;
    for (int i = 0; i < run->n_items; i++) {
        const gc_item *it = &run->items[i];
        if (it->pokemon_id != id) continue;
        if (option == 1 ? option2_item(it) : option3_item(it)) out[n++] = i;
    }
    return n;
}

/* the party screen's filter (selectableFilter: pokemonMeetsPrimaryRequirements) of the option for one Pokemon */
bool gc_delibirdy_pick_ok(const gc_run *run, int option, int idx)
{
    int held[GC_MAX_ITEMS];
    return gc_delibirdy_held_items(run, option, idx, held) > 0;
}

static bool any_pick_ok(const gc_run *run, int option)
{
    for (int i = 0; i < run->n_party; i++) if (gc_delibirdy_pick_ok(run, option, i)) return true;
    return false;
}

/* the encounter's primary requirement: Some(option 2's, option 3's) over the whole party */
bool gc_delibirdy_spawnable(const gc_run *run)
{
    return any_pick_ok(run, 1) || any_pick_ok(run, 2);
}

/* which options are on: the money for the first, a Pokemon with a fitting item for the others */
bool gc_delibirdy_option_ok(const gc_run *run, int option)
{
    if (option == 0) return run->money >= pr_run_get_wave_money_amount(run->wave, DELIBIRDY_MONEY_PRICE_MULTIPLIER);
    return any_pick_ok(run, option);
}

double gc_delibirdy_price(const gc_run *run)
{
    return pr_run_get_wave_money_amount(run->wave, DELIBIRDY_MONEY_PRICE_MULTIPLIER);
}

/* globalScene.getPlayerPokemon(): the first Pokemon of the field (party slot 0 in a mystery encounter) that is allowed in battle */
static const gc_mon *field_lead(const gc_run *run)
{
    if (run->n_party <= 0) return NULL;
    const gc_mon *m = &run->party[run->party_order[0]];
    return gc_mon_allowed(m) ? m : NULL;
}

/* the player's modifier of a persistent kind (findModifier(m instanceof ...)) is at its maximum stack: existing.getStackCount() >= getMaxStackCount() */
static bool stack_full(const gc_run *run, const char *key)
{
    const int idx = gc_reward_item_index(key);
    return idx >= 0 && gc_item_stack(run, key) > 0 && gc_item_stack(run, key) >= pr_items[idx].max_stack;
}

/* generateModifierType(SHELL_BELL), applyModifierTypeToPlayerPokemon(pokemon, shellBell) (nothing at the maximum stack; else addModifier ends in
 * updateModifiers) and the text "battle:rewardGain" (said whether or not the Shell Bell could be added) */
static void give_shell_bell(gc_run *run, const gc_mon *to)
{
    if (!to) { gc_stub(run, GC_STUB_REWARD_APPLY); return; }
    const char *key = "SHELL_BELL";
    int held = 0;
    for (int i = 0; i < run->n_items; i++) {
        const gc_item *it = &run->items[i];
        if (!strcmp(it->key, key) && it->arg == -1 && it->pokemon_id == to->b.id) held = it->stack + it->virtual_stack;
    }
    if (!held || held < gc_item_max_held(run, key, -1, to)) {
        gc_item_add(run, key, -1, to->b.id, 1);
        gc_me_prune_items(run);
        gc_me_update_modifiers(run);
    }
    gc_ev_msg(run, "battle:rewardGain", 1, gc_m_item("modifierName", key));
}

/* globalScene.phaseManager.unshiftNew("ModifierRewardPhase", modifierTypes[key]) */
static void queue_reward(gc_run *run, const char *key)
{
    gc_rphase reward = gc_rp(RP_MODIFIER_REWARD, 0, 0, 0);
    reward.key = key;
    gc_rq_unshift(run, reward);
}

/* the gift: the persistent item through a ModifierRewardPhase, or the Shell Bell when the player's one is at its maximum stack */
static void gift(gc_run *run, const char *key, const gc_mon *shell_bell_to)
{
    if (stack_full(run, key)) give_shell_bell(run, shell_bell_to);
    else queue_reward(run, key);
}

bool gc_delibirdy_execute(gc_run *run, int option, int b, int c, const char **why)
{
    if (option == 0) {
        /* onPreOptionPhase: updatePlayerMoney(-requiredMoney, true, false) */
        run->money = fmin(fmax(run->money - gc_delibirdy_price(run), 0.0), 9007199254740991.0);
        gift(run, "AMULET_COIN", field_lead(run));
        gc_me_leave_with_heal_shop(run);
        return true;
    }
    if (option != 1 && option != 2) { *why = "no such option"; return false; }
    int held[GC_MAX_ITEMS];
    const int n = gc_delibirdy_held_items(run, option, b, held);
    if (c < 0 || c >= n) { *why = "mysteryEncounters/delibirdy:invalidSelection"; return false; }
    /* the chosen modifier is the same object after the gift (updateModifiers may drop other items): found again by what identifies it */
    const gc_item chosen = run->items[held[c]];
    if (option == 1) {
        /* a Berry (BerryModifier) is paid with a Candy Jar, the Reviver Seed with a Berry Pouch */
        gift(run, !strcmp(chosen.key, "BERRY") ? "CANDY_JAR" : "BERRY_POUCH", field_lead(run));
    } else {
        gift(run, "HEALING_CHARM", run->n_party > 0 ? &run->party[run->party_order[0]] : NULL);
    }
    /* chosenPokemon.loseHeldItem(modifier, false): one stack, the item with its last (no updateModifiers) */
    for (int i = 0; i < run->n_items; i++) {
        gc_item *it = &run->items[i];
        if (strcmp(it->key, chosen.key) || it->arg != chosen.arg || it->pokemon_id != chosen.pokemon_id) continue;
        if (--it->stack <= 0) gc_item_remove_at(run, i);
        break;
    }
    gc_items_to_battle(run);
    gc_me_leave_with_heal_shop(run);
    return true;
}
