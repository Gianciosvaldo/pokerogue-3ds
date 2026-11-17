/* Timed events in a run: the app's source of the effects open now (gc/events.h). */
#include "internal.h"

void gc_run_set_events(gc_run *run, const gc_event_source *src)
{
    gc_int(run)->events = src;
}

/* Asked at the moment of each roll an effect changes, never cached. A field outside what upstream's own events can carry is clamped, so a
 * wrong source can only be generous within the game's own limits: luck stays under the cap of 14 there, a multiplier is a whole number. */
gc_event_effects gc_event_effects_now(const gc_run *run)
{
    gc_event_effects fx;
    memset(&fx, 0, sizeof fx);
    const gc_event_source *src = gc_int((gc_run *)run)->events;
    if (src && src->effects) src->effects(src->user, &fx);
    if (fx.luck_boost > 14) fx.luck_boost = 14;
    return fx;
}

/* ModifierType.getOrInferTier() of a reward item: the first ModifierTier of the PLAYER pool that holds it (upstream looks in the trainer and
 * wild pools after that; the items an event gives are all in the player pool). -1 = not in it. */
static int player_pool_tier(int item)
{
    for (int tier = 0; tier < 6; tier++) {
        for (int k = 0; k < pr_player_pool[tier].n; k++)
            if (pr_player_pool_entries[pr_player_pool[tier].start + k].item == item) return tier;
    }
    return -1;
}

/* VictoryPhase, in a Classic run, after the wave's own rewards and before the shop:
 *   for (const fixedReward of timedEventManager.getFixedBattleEventRewards(currentWaveIndex)) {
 *     let reward = fixedReward;
 *     const existingItem = globalScene.modifiers.find(m => m.type.id === reward);
 *     if (existingItem && existingItem.getStackCount() + 1 > existingItem.getMaxStackCount()) {
 *       const tier = existingItem.type.getOrInferTier();
 *       if (!tier) { console.warn(...); break; }
 *       reward = `${ModifierTier[tier]}_BALL`;
 *     }
 *     globalScene.phaseManager.pushNew("ModifierRewardPhase", modifierTypes[reward]);
 *   }
 * The stack is looked at HERE, when the phases are queued, not when they run, so two rewards of one wave both see the stack as it is now.
 * A tier with no ball of that name (COMMON, LUCK) would be `modifierTypes[undefined]` upstream, a crash; here the loop stops as it does for
 * an item with no tier. An item the game does not know (a wrong source) is skipped. */
void gc_event_queue_wave_rewards(gc_run *run, int wave)
{
    static const char *const BALL[6] = {NULL, "GREAT_BALL", "ULTRA_BALL", "ROGUE_BALL", "MASTER_BALL", NULL};
    const gc_event_effects fx = gc_event_effects_now(run);
    for (int i = 0; i < GC_EVENT_REWARDS_MAX && fx.rewards[i].wave; i++) {
        if (fx.rewards[i].wave != wave || !fx.rewards[i].key) continue;
        const int item = gc_reward_item_index(fx.rewards[i].key);
        if (item < 0) continue;
        const char *key = pr_item_keys[item];
        const int have = gc_item_stack(run, key);
        if (pr_items[item].max_stack > 0 && have + 1 > pr_items[item].max_stack) {   /* max_stack is -1 for an item that is no modifier */
            /* existingItem.type.getOrInferTier(): the tier its type already carries (a reward taken from a shop has one), else the pool's */
            int tier = -1;
            for (int j = 0; j < run->n_items && tier < 0; j++)
                if (!strcmp(run->items[j].key, key) && run->items[j].type_tier) tier = run->items[j].type_tier - 1;
            if (tier < 0) tier = player_pool_tier(item);
            if (tier < 0 || !BALL[tier]) break;
            key = BALL[tier];
        }
        gc_rphase p = gc_rp(RP_MODIFIER_REWARD, 0, 0, 0);
        p.key = key;
        gc_rq_push(run, p);
    }
}
