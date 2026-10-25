/* Pinned 8555c08 MoneyRewardModifier.apply + Scene.addMoney monetary state.
 * Gimmighoul tracker additions and presentation/achievement lifecycle are caller-owned. */
#include "pr/reward.h"
#include "pr_data.h"
#include <math.h>

#define MAX_SAFE_INTEGER 9007199254740991.0
bool pr_reward_apply_money_item(const pr_reward_ctx *ctx, int item, double *money,
                               pr_reward_money_item_result *result)
{
    double multiplier;
    switch (item) {
    case PR_ITEM_NUGGET: multiplier = 1; break;
    case PR_ITEM_BIG_NUGGET: multiplier = 2.5; break;
    case PR_ITEM_RELIC_GOLD: multiplier = 10; break;
    default: return false;
    }
    if (!ctx || !money || !isfinite(*money) || *money < 0 || *money > MAX_SAFE_INTEGER
        || ctx->wave < 1 || ctx->wave > 10000 || ctx->n_party < 1 || ctx->n_party > PR_REWARD_MAX_PARTY
        || ctx->n_items < 0 || ctx->n_items > PR_REWARD_MAX_OWNED) return false;
    for (int j = 0; j < ctx->n_party; j++) {
        const pr_reward_mon *p = &ctx->party[j];
        if (!p->species || p->species >= PR_SPECIES_COUNT || !pr_species_table[p->species].nforms
            || p->fusion_species < -1
            || p->fusion_species == 0 || p->fusion_species >= PR_SPECIES_COUNT
            || (p->fusion_species > 0 && !pr_species_table[p->fusion_species].nforms))
            return false;
    }
    int stacks[PR_REWARD_MAX_OWNED], count = 0;
    for (int j = 0; j < ctx->n_items; j++) {
        const pr_owned_item *it = &ctx->items[j];
        if (it->item >= PR_ITEM_COUNT_) return false;
        if (it->item != PR_ITEM_AMULET_COIN) continue; /* other known entries are opaque */
        if (it->owner != -1 || it->arg != 0 || it->battle_count != 0 || it->stack < 1 || it->stack > 5)
            return false;
        stacks[count++] = it->stack;
    }
    /* Ordered object-by-object floor bonuses, with no Happy Hour multiplier. */
    double amount = pr_run_get_money_reward(ctx->wave, multiplier, stacks, count, false);
    if (!isfinite(amount)) return false;
    double before = *money, balance = fmin(before + amount, MAX_SAFE_INTEGER);
    *money = balance;
    if (result) {
        *result = (pr_reward_money_item_result){.amount = amount, .credited = balance - before};
        for (int j = 0; j < ctx->n_party; j++)
            if (ctx->party[j].species == SPECIES_ID_GIMMIGHOUL || ctx->party[j].fusion_species == SPECIES_ID_GIMMIGHOUL)
                result->tracker_stacks[j] = (uint8_t)fmin(floor(multiplier), 3);
    }
    return true; /* upstream applies successfully even if the capped balance cannot rise */
}
