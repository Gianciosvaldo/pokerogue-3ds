/* Raw pinned TurnStatusEffectModifier/PersistentModifier.add acquisition. */
#include "pr/reward.h"

static bool is_orb(int item)
{
    return item == PR_ITEM_TOXIC_ORB || item == PR_ITEM_FLAME_ORB;
}

static bool supported_orb_stock(const pr_reward_ctx *ctx)
{
    if (!ctx || ctx->n_party < 1 || ctx->n_party > PR_REWARD_MAX_PARTY
        || ctx->n_items < 0 || ctx->n_items > PR_REWARD_MAX_OWNED) return false;
    for (int j = 0; j < ctx->n_party; j++)
        for (int k = 0; k < j; k++) if (ctx->party[j].id == ctx->party[k].id) return false;
    for (int j = 0; j < ctx->n_items; j++) {
        const pr_owned_item *it = &ctx->items[j];
        if (it->item >= PR_ITEM_COUNT_) return false;
        if (is_orb(it->item) && (it->owner < -1 || it->owner >= ctx->n_party
            || it->arg != 0 || it->battle_count != 0 || it->stack != 1)) return false;
    }
    return true;
}

enum pr_reward_held_status pr_reward_add_canonical_orb_item(pr_reward_ctx *ctx, int item, int party_idx)
{
    if (!is_orb(item) || !supported_orb_stock(ctx) || party_idx < 0 || party_idx >= ctx->n_party)
        return PR_REWARD_HELD_UNSUPPORTED;
    /* matchType intentionally ignores status: either Orb matches either Orb. */
    for (int j = 0; j < ctx->n_items; j++) if (is_orb(ctx->items[j].item)
        && ctx->items[j].owner == party_idx) return PR_REWARD_HELD_FULL;
    if (ctx->n_items == PR_REWARD_MAX_OWNED) return PR_REWARD_HELD_UNSUPPORTED;
    ctx->items[ctx->n_items++] = (pr_owned_item){.item=item,.owner=party_idx,.stack=1};
    return PR_REWARD_HELD_ADDED;
}
