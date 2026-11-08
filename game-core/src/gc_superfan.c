/* The Bug-Type Superfan (src/data/mystery-encounters/encounters/bug-type-superfan-encounter.ts at pinned upstream 8555c08): the two options that do not fight.
 *
 * The encounter spawns when a Pokemon of the whole party (fainted ones too) is a Bug type (TypeRequirement(BUG, false), the Tera type left out), or holds a transferable
 * Quick Claw or Grip Claw (HeldItemRequirement of BypassSpeedChanceModifier / ContactHeldItemTransferChanceModifier) or a transferable Bug attack type booster
 * (AttackTypeBoosterHeldItemTypeRequirement(BUG)): a Some(..) of the three. Option 1 (the fight, not ported yet: still the stand-in in gc_mystery.c) needs nothing.
 *
 * 2 Show Your Bug-types (TypeRequirement(BUG, false, 1) on the option): onPreOptionPhase, on the stream wave * 1000 + 512 of the option select, counts the party's Bug types
 * (isOfType(BUG): the Tera type counts, but nothing out of battle is Terastallized) and sets the rewards: 0 or 1 a Super Lure and a Great Ball, 2 or 3 a Quick Claw, a
 * Max Lure and an Ultra Ball, 4 or 5 a Grip Claw, a Max Lure and a Rogue Ball, 6 a Master Ball, a Mega Bracelet and a Dynamax Band the player does not have yet, and one
 * of the evolution / rare evolution / form change / rare form change items that can be made (each generator rolls in that order; then randSeedInt picks one of them).
 * The shop has exactly those (fillRemaining false); the text is option.2.selected0To1 / 2To3 / 4To5 / 6 (the app chooses by the count).
 * 3 Gift a Bug Item (a Pokemon holding a Quick Claw, a Grip Claw or a Bug attack type booster): the party screen's filter passes a Pokemon holding one (instanceof only, the
 * transfer flag is not asked), the second list names its transferable ones in getHeldItems() order. onOptionPhase: loseHeldItem(.., false) (one stack, the item with its last),
 * updateModifiers(true, true), and the shop has the Golden Bug Net (as a Rogue tier option) and a Reviver Seed (fillRemaining false). leaveEncounterWithoutBattle(true), but
 * a mystery encounter's own rewards replace the heal-only shop, so it is the same shop either way.
 *
 * Not ported here: option 1 (the trainer's fight and the Bug-type move tutor), the intro sprites. */
#include "internal.h"
#include <math.h>
#include <string.h>

int pr_reward_generate(pr_reward_ctx *ctx, int item);   /* reward-core/src/reward.c: generateType(party); -2 = the generator makes nothing */

/* AttackTypeBoosterModifier of the Bug type, BypassSpeedChanceModifier (Quick Claw), ContactHeldItemTransferChanceModifier (Grip Claw) */
static bool bug_item(const gc_item *it)
{
    return !strcmp(it->key, "QUICK_CLAW") || !strcmp(it->key, "GRIP_CLAW") || (!strcmp(it->key, "ATTACK_TYPE_BOOSTER") && it->arg == POKEMON_TYPE_BUG);
}

static bool is_bug(const gc_mon *m)
{
    int8_t ty[PR_MAX_TYPES];
    const int nt = pr_get_types(&m->b, ty, PR_TYPES_NO_TERA);
    for (int k = 0; k < nt; k++) if (ty[k] == POKEMON_TYPE_BUG) return true;
    return false;
}

int gc_superfan_num_bug_types(const gc_run *run)
{
    int n = 0;
    for (int i = 0; i < run->n_party; i++) n += is_bug(&run->party[i]);
    return n;
}

/* the party screen's filter (selectableFilter): the Pokemon holds a Quick Claw, a Grip Claw or a Bug attack type booster */
bool gc_superfan_pick_ok(const gc_run *run, int idx)
{
    if (idx < 0 || idx >= run->n_party) return false;
    const uint32_t id = run->party[idx].b.id;
    for (int i = 0; i < run->n_items; i++) if (run->items[i].pokemon_id == id && bug_item(&run->items[i])) return true;
    return false;
}

/* the second list (onPokemonSelected): the transferable ones, as run->items indexes in getHeldItems() order */
int gc_superfan_held_items(const gc_run *run, int idx, int out[GC_MAX_ITEMS])
{
    if (idx < 0 || idx >= run->n_party) return 0;
    const uint32_t id = run->party[idx].b.id;
    int n = 0;
    for (int i = 0; i < run->n_items; i++) {
        const gc_item *it = &run->items[i];
        if (it->pokemon_id == id && bug_item(it) && gc_gts_item_transferable(it)) out[n++] = i;
    }
    return n;
}

/* HeldItemRequirement / AttackTypeBoosterHeldItemTypeRequirement over the whole party: a Pokemon holds a transferable one */
static bool any_transferable_bug_item(const gc_run *run)
{
    for (int i = 0; i < run->n_items; i++) {
        const gc_item *it = &run->items[i];
        if (!it->pokemon_id || !bug_item(it) || !gc_gts_item_transferable(it)) continue;
        for (int k = 0; k < run->n_party; k++) if (run->party[k].b.id == it->pokemon_id) return true;
    }
    return false;
}

bool gc_superfan_spawnable(const gc_run *run)
{
    return gc_superfan_num_bug_types(run) > 0 || any_transferable_bug_item(run);
}

/* which options are on: the fight always, the Bug types for the second, an item for the third (the Some(..) of the two item requirements) */
bool gc_superfan_option_ok(const gc_run *run, int option)
{
    if (option == 0) return true;
    if (option == 1) return gc_superfan_num_bug_types(run) > 0;
    return any_transferable_bug_item(run);
}

/* setEncounterRewards({guaranteedModifierTypeOptions, guaranteedModifierTypeFuncs, fillRemaining: false}), then leaving without a battle */
static void shop(gc_run *run, const pr_reward_option *opts, int n_opts, const char *const *funcs, int n_funcs)
{
    pr_reward_custom *c = &gc_int(run)->reward.me_custom;
    memset(c, 0, sizeof *c);
    for (int i = 0; i < n_opts && c->n_options < PR_REWARD_MAX_OPTIONS; i++) c->options[c->n_options++] = opts[i];
    for (int i = 0; i < n_funcs && c->n_funcs < PR_REWARD_MAX_OPTIONS; i++) c->funcs[c->n_funcs++] = (uint16_t)gc_reward_item_index(funcs[i]);
    c->fill_remaining = false;
    c->reroll_multiplier = NAN;
    c->allow_luck_upgrades = true;
    gc_me_end_shop(run);
}

/* generateModifierTypeOption(func): the generator rolls on the current stream (a null result is no option), the tier is the item's from the player pool */
static bool generated_option(pr_reward_ctx *ctx, const char *key, pr_reward_option *out)
{
    const int item = gc_reward_item_index(key);
    if (item < 0) return false;
    const int arg = pr_reward_generate(ctx, item);
    if (arg == -2) return false;
    *out = pr_reward_pregen_option(ctx, item, arg);
    return true;
}

pr_rng gc_superfan_pre_rng;

/* option 2's onPreOptionPhase and the shop it leaves with */
static void show_bug_types(gc_run *run)
{
    const int n = gc_superfan_num_bug_types(run);
    gc_superfan_pre_rng = pr_run_seed_offset_rng(&run->run, run->wave * 1000 + 512, NULL);
    pr_reward_option opts[8];
    int n_opts = 0;
    if (n < 2) {
        static const char *const f[2] = {"SUPER_LURE", "GREAT_BALL"};
        shop(run, NULL, 0, f, 2);
    } else if (n < 4) {
        static const char *const f[3] = {"QUICK_CLAW", "MAX_LURE", "ULTRA_BALL"};
        shop(run, NULL, 0, f, 3);
    } else if (n < 6) {
        static const char *const f[3] = {"GRIP_CLAW", "MAX_LURE", "ROGUE_BALL"};
        shop(run, NULL, 0, f, 3);
    } else {
        pr_reward_ctx *ctx = gc_reward_ctx(run);
        const pr_rng saved = run->run.rng;
        run->run.rng = pr_run_seed_offset_rng(&run->run, run->wave * 1000 + 512, NULL);
        generated_option(ctx, "MASTER_BALL", &opts[n_opts++]);
        if (!gc_item_stack(run, "MEGA_BRACELET")) generated_option(ctx, "MEGA_BRACELET", &opts[n_opts++]);
        if (!gc_item_stack(run, "DYNAMAX_BAND")) generated_option(ctx, "DYNAMAX_BAND", &opts[n_opts++]);
        pr_reward_option special[4];
        int n_special = 0;
        static const char *const keys[4] = {"EVOLUTION_ITEM", "RARE_EVOLUTION_ITEM", "FORM_CHANGE_ITEM", "RARE_FORM_CHANGE_ITEM"};
        for (int k = 0; k < 4; k++) if (generated_option(ctx, keys[k], &special[n_special])) n_special++;
        if (n_special > 0) opts[n_opts++] = special[pr_rng_seed_int(&run->run.rng, n_special, 0)];
        gc_superfan_pre_rng = run->run.rng;
        run->run.rng = saved;
        shop(run, opts, n_opts, NULL, 0);
    }
}

bool gc_superfan_execute(gc_run *run, int option, int b, int c, const char **why)
{
    if (option == 1) {
        if (gc_superfan_num_bug_types(run) <= 0) { *why = "mysteryEncounters/bugTypeSuperfan:option.2.disabledTooltip"; return false; }
        show_bug_types(run);
        return true;
    }
    if (option != 2) { *why = "no such option"; return false; }
    int held[GC_MAX_ITEMS];
    const int n = gc_superfan_held_items(run, b, held);
    if (!gc_superfan_pick_ok(run, b) || c < 0 || c >= n) { *why = "mysteryEncounters/bugTypeSuperfan:option.3.invalidSelection"; return false; }
    /* chosenPokemon.loseHeldItem(modifier, false): one stack, the item with its last; then updateModifiers(true, true) */
    const gc_item chosen = run->items[held[c]];
    for (int i = 0; i < run->n_items; i++) {
        gc_item *it = &run->items[i];
        if (strcmp(it->key, chosen.key) || it->arg != chosen.arg || it->pokemon_id != chosen.pokemon_id) continue;
        if (--it->stack <= 0) gc_item_remove_at(run, i);
        break;
    }
    gc_items_to_battle(run);
    gc_me_prune_items(run);
    gc_me_update_modifiers(run);
    pr_reward_option net = pr_reward_pregen_option(gc_reward_ctx(run), gc_reward_item_index("MYSTERY_ENCOUNTER_GOLDEN_BUG_NET"), -1);
    net.tier = 3;   /* bugNet.type.tier = ModifierTier.ROGUE */
    static const char *const f[1] = {"REVIVER_SEED"};
    shop(run, &net, 1, f, 1);
    return true;
}
