/* Task 44: Candy Jar & level increment consumables.
 * Pinned upstream: LevelIncrementBoosterModifier + PokemonLevelIncrementModifier at 8555c08. */
#include "pr/reward.h"
#include "pr_data.h"
#include <math.h>
#include <string.h>

static const pr_species *species_row(int id)
{
    int lo = 0, hi = PR_SPECIES_COUNT - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (pr_species_table[mid].id == id) return &pr_species_table[mid];
        if (pr_species_table[mid].id < id) lo = mid + 1; else hi = mid - 1;
    }
    return NULL;
}

double pr_reward_level_total_exp(int level, int growth_rate)
{
    if (level < 1) return 0.0;
    /* Consume the pinned table/formula implementation, including JS pow rounding. */
    return pr_run_get_level_total_exp(level, growth_rate);
}

int pr_reward_candy_level_count(const pr_reward_ctx *ctx)
{
    int64_t levels = 1;
    if (!ctx) return levels;
    for (int i = 0; i < ctx->n_items; i++)
        if (ctx->items[i].item == PR_ITEM_CANDY_JAR && ctx->items[i].stack > 0)
            levels += ctx->items[i].stack;
    if (levels > INT32_MAX) {
        if (ctx->run) ctx->run->unported |= PR_REWARD_UNPORTED_LEVEL;
        return -1;
    }
    return (int)levels;
}

static bool can_increment(pr_reward_ctx *ctx, int party_idx, int count)
{
    int level = ctx->party[party_idx].level;
    if (level < 1 || count < 1 || (int64_t)level + count > INT32_MAX) {
        if (ctx->run) ctx->run->unported |= PR_REWARD_UNPORTED_LEVEL;
        return false;
    }
    return true;
}

static void apply_one(pr_reward_ctx *ctx, int party_idx, int level_count)
{
    pr_reward_mon *p = &ctx->party[party_idx];
    const int32_t old_level = p->level;
    p->level += level_count;

    /* PokemonLevelIncrementModifier: only synchronize exp while at/below getMaxExpLevel(true).
     * Once a candy pushes past that absolute cap, upstream leaves exp untouched. */
    if (ctx->run) {
        const double absolute_cap = pr_run_get_max_exp_level(ctx->run, ctx->wave, true);
        const pr_species *sp = species_row(p->species);
        if (sp && p->level <= absolute_cap)
            p->exp = pr_reward_level_total_exp(p->level, sp->growth_rate);
    }

    /* addFriendship(FRIENDSHIP_GAIN_FROM_RARE_CANDY=6, true). Candy Jar changes levels,
     * not the friendship amount; an existing friendship above the 200 candy cap is preserved. */
    int friendship = p->friendship + 6;
    if (friendship > 200) friendship = p->friendship > 200 ? p->friendship : 200;
    if (friendship > 255) friendship = 255;
    p->friendship = (uint8_t)friendship;

    if (ctx->n_level_events < PR_REWARD_MAX_PARTY) {
        int e = ctx->n_level_events++;
        ctx->level_events[e].party_idx = (int8_t)party_idx;
        ctx->level_events[e].old_level = old_level;
        ctx->level_events[e].new_level = p->level;
    }
}

bool pr_reward_apply_candy(pr_reward_ctx *ctx, uint16_t item, int party_idx)
{
    if (!ctx) return false;
    ctx->n_level_events = 0;
    const int count = pr_reward_candy_level_count(ctx);
    if (count < 1) return false;

    if (item == PR_ITEM_RARE_CANDY) {
        if (party_idx < 0 || party_idx >= ctx->n_party || !can_increment(ctx, party_idx, count)) return false;
        apply_one(ctx, party_idx, count);
        return true;
    }
    if (item == PR_ITEM_RARER_CANDY) {
        if (ctx->n_party <= 0) return false;
        for (int i = 0; i < ctx->n_party; i++) if (!can_increment(ctx, i, count)) return false;
        for (int i = 0; i < ctx->n_party; i++) apply_one(ctx, i, count);
        return true;
    }
    return false;
}
