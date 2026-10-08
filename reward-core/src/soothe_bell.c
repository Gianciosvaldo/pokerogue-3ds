/* Pinned 8555c08 PersistentModifier.add and singular Scene.applyModifier.
 * Raw held acquisition/boost boundary only, not Pokemon.addFriendship. */
#include "pr/reward.h"
#include <math.h>

static bool supported(const pr_reward_ctx *ctx)
{
    if (!ctx || ctx->n_party < 1 || ctx->n_party > PR_REWARD_MAX_PARTY
        || ctx->n_items < 0 || ctx->n_items > PR_REWARD_MAX_OWNED) return false;
    for (int j = 0; j < ctx->n_party; j++)
        for (int k = 0; k < j; k++) if (ctx->party[j].id == ctx->party[k].id) return false;
    for (int j = 0; j < ctx->n_items; j++) {
        const pr_owned_item *it = &ctx->items[j];
        if (it->item >= PR_ITEM_COUNT_) return false;
        if (it->item == PR_ITEM_SOOTHE_BELL && (it->owner < -1 || it->owner >= ctx->n_party
            || it->arg != 0 || it->battle_count != 0 || it->stack < 1 || it->stack > 3)) return false;
    }
    return true;
}
enum pr_reward_soothe_status pr_reward_add_soothe_bell(pr_reward_ctx *ctx, int party_idx)
{
    if (!supported(ctx) || party_idx < 0 || party_idx >= ctx->n_party) return PR_REWARD_SOOTHE_UNSUPPORTED;
    for (int j = 0; j < ctx->n_items; j++) if (ctx->items[j].item == PR_ITEM_SOOTHE_BELL
        && ctx->items[j].owner == party_idx) {
        if (ctx->items[j].stack == 3) return PR_REWARD_SOOTHE_FULL;
        ctx->items[j].stack++;
        return PR_REWARD_SOOTHE_STACKED;
    }
    if (ctx->n_items == PR_REWARD_MAX_OWNED) return PR_REWARD_SOOTHE_UNSUPPORTED;
    ctx->items[ctx->n_items++] = (pr_owned_item){.item=PR_ITEM_SOOTHE_BELL,.owner=party_idx,.stack=1};
    return PR_REWARD_SOOTHE_ADDED;
}
bool pr_reward_soothe_bell_gain(const pr_reward_ctx *ctx, int party_idx, double base,
                               double *amount, int *matched_index)
{
    if (!supported(ctx) || party_idx < 0 || party_idx >= ctx->n_party || !amount
        || !isfinite(base) || base < 0 || base > 1000000) return false;
    double value = base; int matched = -1;
    for (int j = 0; j < ctx->n_items; j++) if (ctx->items[j].item == PR_ITEM_SOOTHE_BELL
        && (ctx->items[j].owner == -1 || ctx->items[j].owner == party_idx)) {
        value = floor(value * (1 + 0.5 * ctx->items[j].stack)); matched = j;
        break; /* singular applyModifier uses only the first eligible object */
    }
    *amount = value;
    if (matched_index) *matched_index = matched;
    return true;
}
