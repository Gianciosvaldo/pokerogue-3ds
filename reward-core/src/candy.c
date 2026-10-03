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

static double canonical_exp(int level, int growth_rate)
{
    const double n = level;
    switch (growth_rate) {
    case 0: /* ERRATIC */
        if (level < 50) return floor(n*n*n * (100.0 - n) / 50.0);
        if (level < 68) return floor(n*n*n * (150.0 - n) / 100.0);
        if (level < 98) return floor(n*n*n * floor((1911.0 - 10.0*n) / 3.0) / 500.0);
        return floor(n*n*n * (160.0 - n) / 100.0);
    case 1: return floor(n*n*n * 4.0 / 5.0); /* FAST */
    case 2: return floor(n*n*n);             /* MEDIUM_FAST */
    case 3: return floor(n*n*n * 6.0 / 5.0 - 15.0*n*n + 100.0*n - 140.0); /* MEDIUM_SLOW */
    case 4: return floor(n*n*n * 5.0 / 4.0); /* SLOW */
    case 5: /* FLUCTUATING */
        if (level < 15) return floor(n*n*n * (floor((n + 1.0) / 3.0) + 24.0) / 50.0);
        if (level < 36) return floor(n*n*n * (n + 14.0) / 50.0);
        return floor(n*n*n * (floor(n / 2.0) + 32.0) / 50.0);
    default:
        return floor(n*n*n);
    }
}

double pr_reward_level_total_exp(int level, int growth_rate)
{
    if (level < 1) return 0.0;
    double raw;
    if (level < 100) {
        raw = canonical_exp(level, growth_rate);
    } else {
        const double n = level;
        switch (growth_rate) {
        case 0: raw = (pow(n, 4.0) + pow(n, 3.0) * 2000.0) / 3500.0; break;
        case 1: raw = pow(n, 3.0) * 4.0 / 5.0; break;
        case 2: raw = pow(n, 3.0); break;
        case 3: raw = pow(n, 3.0) * 6.0 / 5.0 - 15.0 * pow(n, 2.0) + 100.0 * n - 140.0; break;
        case 4: raw = pow(n, 3.0) * 5.0 / 4.0; break;
        case 5: raw = pow(n, 3.0) * (n / 2.0 + 8.0) * 4.0 / (100.0 + n); break;
        default: raw = pow(n, 3.0); break;
        }
    }

    if (growth_rate != 2) {
        const double medium_fast = floor(pow((double)level, 3.0));
        return floor(raw * 0.325 + medium_fast * 0.675);
    }
    return floor(raw);
}

int pr_reward_candy_level_count(const pr_reward_ctx *ctx)
{
    int levels = 1;
    if (!ctx) return levels;
    for (int i = 0; i < ctx->n_items; i++)
        if (ctx->items[i].item == PR_ITEM_CANDY_JAR && ctx->items[i].stack > 0)
            levels += ctx->items[i].stack;
    return levels;
}

static void apply_one(pr_reward_ctx *ctx, int party_idx, int level_count)
{
    pr_reward_mon *p = &ctx->party[party_idx];
    const uint16_t old_level = p->level;
    unsigned next = (unsigned)p->level + (unsigned)level_count;
    if (next > 65535u) next = 65535u;
    p->level = (uint16_t)next;

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

    if (item == PR_ITEM_RARE_CANDY) {
        if (party_idx < 0 || party_idx >= ctx->n_party) return false;
        apply_one(ctx, party_idx, count);
        return true;
    }
    if (item == PR_ITEM_RARER_CANDY) {
        if (ctx->n_party <= 0) return false;
        for (int i = 0; i < ctx->n_party; i++) apply_one(ctx, i, count);
        return true;
    }
    return false;
}
