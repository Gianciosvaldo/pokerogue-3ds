/* Raw pinned PersistentModifier.add, canonical ordinary held items only. */
#include "pr/reward.h"

static int canonical_cap(int item)
{
    switch (item) {
    case PR_ITEM_WIDE_LENS: return 3; /* canonical accuracyAmount=5 */
    default: return 0;
    }
}

static bool supported(const pr_reward_ctx *ctx)
{
    if (!ctx || ctx->n_party < 1 || ctx->n_party > PR_REWARD_MAX_PARTY
        || ctx->n_items < 0 || ctx->n_items > PR_REWARD_MAX_OWNED) return false;
    for (int j = 0; j < ctx->n_party; j++)
        for (int k = 0; k < j; k++) if (ctx->party[j].id == ctx->party[k].id) return false;
    for (int j = 0; j < ctx->n_items; j++) {
        const pr_owned_item *it = &ctx->items[j];
        if (it->item >= PR_ITEM_COUNT_) return false;
        int cap = canonical_cap(it->item);
        if (cap && (it->owner < -1 || it->owner >= ctx->n_party || it->arg != 0
            || it->battle_count != 0 || it->stack < 1 || it->stack > cap)) return false;
    }
    return true;
}

enum pr_reward_held_status pr_reward_add_canonical_held_item(pr_reward_ctx *ctx, int item, int party_idx)
{
    int cap = canonical_cap(item);
    if (!cap || !supported(ctx) || party_idx < 0 || party_idx >= ctx->n_party)
        return PR_REWARD_HELD_UNSUPPORTED;
    for (int j = 0; j < ctx->n_items; j++) if (ctx->items[j].item == item
        && ctx->items[j].owner == party_idx) {
        if (ctx->items[j].stack == cap) return PR_REWARD_HELD_FULL;
        ctx->items[j].stack++;
        return PR_REWARD_HELD_STACKED;
    }
    if (ctx->n_items == PR_REWARD_MAX_OWNED) return PR_REWARD_HELD_UNSUPPORTED;
    ctx->items[ctx->n_items++] = (pr_owned_item){.item=item,.owner=party_idx,.stack=1};
    return PR_REWARD_HELD_ADDED;
}
