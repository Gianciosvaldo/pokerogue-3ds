/* Pinned 8555c08 LapsingPersistentModifier and DoubleBattleChanceBooster.
 * Modifier boundary only: never calls the broader scene/phase lifecycle. */
#include "pr/reward.h"
#include <math.h>
#include <string.h>

static int duration(int item)
{
    switch (item) {
    case PR_ITEM_LURE: return 10;
    case PR_ITEM_SUPER_LURE: return 15;
    case PR_ITEM_MAX_LURE: return 30;
    default: return 0;
    }
}
static bool supported(const pr_reward_ctx *ctx)
{
    if (!ctx || ctx->n_items < 0 || ctx->n_items > PR_REWARD_MAX_OWNED) return false;
    for (int j = 0; j < ctx->n_items; j++) {
        const pr_owned_item *it = &ctx->items[j];
        if (it->item >= PR_ITEM_COUNT_) return false;
        int max = duration(it->item);
        /* Non-Lures are opaque: no broad updateModifiers call is made here. */
        if (max && (it->owner != -1 || it->arg != 0 || it->stack < 1 || it->stack > 2
            || it->battle_count < 1 || it->battle_count > max)) return false;
    }
    return true;
}
enum pr_reward_lure_status pr_reward_add_lure(pr_reward_ctx *ctx, int item)
{
    int max = duration(item);
    if (!max || !supported(ctx)) return PR_REWARD_LURE_UNSUPPORTED;
    /* Upstream matches class + maxBattles, and returns on the first match.
     * In the canonical subset the three item IDs have distinct durations. */
    for (int j = 0; j < ctx->n_items; j++) if (ctx->items[j].item == item) {
        if (ctx->items[j].battle_count == max) return PR_REWARD_LURE_FULL;
        ctx->items[j].battle_count = max;
        return PR_REWARD_LURE_REFRESHED;
    }
    if (ctx->n_items == PR_REWARD_MAX_OWNED) return PR_REWARD_LURE_UNSUPPORTED;
    ctx->items[ctx->n_items++] = (pr_owned_item){.item = item, .owner = -1, .stack = 1, .battle_count = max};
    return PR_REWARD_LURE_ADDED;
}
bool pr_reward_lapse_lures(pr_reward_ctx *ctx, int *expired)
{
    if (!supported(ctx)) return false;
    int removed = 0, out = 0;
    for (int j = 0; j < ctx->n_items; j++) {
        pr_owned_item it = ctx->items[j];
        if (duration(it.item) && --it.battle_count == 0) { removed++; continue; }
        ctx->items[out++] = it;
    }
    /* Tail is outside the live list. Clearing it keeps deterministic transport. */
    if (removed) memset(&ctx->items[out], 0, (ctx->n_items - out) * sizeof ctx->items[0]);
    ctx->n_items = out;
    if (expired) *expired = removed;
    return true;
}
bool pr_reward_lure_chance(const pr_reward_ctx *ctx, double base, double *result)
{
    if (!result || !isfinite(base) || base < 0 || !supported(ctx)) return false;
    double value = base;
    for (int j = 0; j < ctx->n_items; j++) if (duration(ctx->items[j].item)) value /= 4;
    *result = value;
    return true;
}
