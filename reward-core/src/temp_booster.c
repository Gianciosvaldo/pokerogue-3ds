/* Pinned 8555c08 LapsingPersistentModifier and TempStatStageBooster/TempCritBooster.
 * Modifier boundary only: never calls the broader scene/phase lifecycle. */
#include "pr/reward.h"
#include "pr/battle.h"
#include <string.h>

static int duration(int item)
{
    switch (item) {
    case PR_ITEM_TEMP_STAT_STAGE_BOOSTER: case PR_ITEM_DIRE_HIT: return 5;
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
        /* Non-temporary entries are opaque: no broad updateModifiers call is made here. */
        if (max && (it->owner != -1
            || (it->item == PR_ITEM_DIRE_HIT ? it->arg != 0 : it->arg < STAT_ATK || it->arg > STAT_ACC)
            || it->stack < 1 || it->stack > 2
            || it->battle_count < 1 || it->battle_count > max)) return false;
    }
    return true;
}
enum pr_reward_temp_status pr_reward_add_temp_booster(pr_reward_ctx *ctx, int item, int arg)
{
    int max = duration(item);
    if (!max || (item == PR_ITEM_DIRE_HIT ? arg != 0 : arg < STAT_ATK || arg > STAT_ACC)
        || !supported(ctx)) return PR_REWARD_TEMP_UNSUPPORTED;
    /* Stat class matches stat, Dire Hit matches class; both return on first match. */
    for (int j = 0; j < ctx->n_items; j++) if (ctx->items[j].item == item && ctx->items[j].arg == arg) {
        if (ctx->items[j].battle_count == max) return PR_REWARD_TEMP_FULL;
        ctx->items[j].battle_count = max;
        return PR_REWARD_TEMP_REFRESHED;
    }
    if (ctx->n_items == PR_REWARD_MAX_OWNED) return PR_REWARD_TEMP_UNSUPPORTED;
    ctx->items[ctx->n_items++] = (pr_owned_item){.item = item, .arg = arg, .owner = -1, .stack = 1, .battle_count = max};
    return PR_REWARD_TEMP_ADDED;
}
bool pr_reward_lapse_temp_boosters(pr_reward_ctx *ctx, int *expired)
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
