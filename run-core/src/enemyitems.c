/* An enemy's held items, as the EncounterPhase rolls them after the enemies are made:
 * regenerateModifierPoolThresholds and getNewModifierTypeOption (src/modifier/modifier-type.ts),
 * generateEnemyModifiers (src/battle-scene.ts) and getEnemyModifierTypesForWave. The JS each step
 * replaces is quoted where it happens. */
#include "pr/run.h"
#include <math.h>
#include <string.h>

static int seed_int(pr_run *run, int range) { return pr_rng_seed_int(&run->rng, range, 0); }

/* A generator's generateType(party): the item's argument, or -2 for null. BERRY:
 *   rand = randSeedInt(12): < 2 SITRUS, < 4 LUM, < 6 LEPPA, else berryTypes[randSeedInt(berryTypes.length - 3) + 2]
 * BASE_STAT_BOOSTER: randSeedInt(Stat.SPD + 1).
 * ATTACK_TYPE_BOOSTER: each attack move's types, at most 3 each, as weights in first-seen order;
 *   none → null; else randSeedInt(totalWeight) over them. */
static int generate(pr_run *run, int kind, const pr_item_enemy *party, int n_party)
{
    switch (kind) {
    case PR_POOL_BERRY: {
        const int rand = seed_int(run, 12);
        if (rand < 2) return BERRY_TYPE_SITRUS;
        if (rand < 4) return BERRY_TYPE_LUM;
        if (rand < 6) return BERRY_TYPE_LEPPA;
        return seed_int(run, (BERRY_TYPE_LEPPA + 1) - 3) + 2;
    }
    case PR_POOL_BASE_STAT:
        return seed_int(run, STAT_SPD + 1);
    case PR_POOL_ATTACK_TYPE: {
        int8_t types[32];
        int weights[32], n = 0, total = 0;
        for (int p = 0; p < n_party; p++)
            for (int i = 0; i < party[p].n_attack_types; i++) {
                const int t = party[p].attack_types[i];
                int k = 0;
                while (k < n && types[k] != t) k++;
                if (k == n) {
                    if (n == 32) continue;
                    types[n] = (int8_t)t;
                    weights[n++] = 0;
                }
                if (weights[k] < 3) { weights[k]++; total++; }
            }
        if (!n) return -2;
        const int r = seed_int(run, total);
        int w = 0;
        for (int k = 0; k < n; k++) {
            if (r < w + weights[k]) return types[k];
            w += weights[k];
        }
        return -2;
    }
    }
    return -1;
}

/* the tier's thresholds: the running totals of its non-zero weights, each with its entry's index */
typedef struct { int n; int total[8], index[8]; } thresholds;

int pr_run_generate_enemy_items(pr_run *run, const pr_enemy_items_ctx *ctx, pr_enemy_items *out)
{
    memset(out, 0, sizeof *out);
    const int pool = ctx->trainer_battle ? 1 : 0;
    const int n_field = ctx->n_party < (ctx->double_battle ? 2 : 1) ? ctx->n_party : (ctx->double_battle ? 2 : 1);

    /* regenerateModifierPoolThresholds(getEnemyField(), poolType): for each tier, each entry asks a
     * generator for its type (generateType(party): it rolls) before its weight is read; with no enemy
     * modifiers yet, every weight counts as it is */
    thresholds th[6];
    for (int t = 0; t < 6; t++) {
        th[t].n = 0;
        const pr_pool_tier *pt = &pr_enemy_pools[pool][t];
        int total = 0;
        for (int i = 0; i < pt->n; i++) {
            const pr_pool_entry *e = &pr_pool_entries[pt->start + i];
            if (e->kind != PR_POOL_PLAIN) generate(run, e->kind, ctx->party, n_field);
            if (!e->weight) continue;
            total += e->weight;
            th[t].total[th[t].n] = total;
            th[t].index[th[t].n++] = i;
        }
    }

    /* generateEnemyModifiers(): nothing at the classic final boss */
    if (pr_run_is_battle_classic_final_boss(run, ctx->wave)) return 0;
    const int difficulty_wave = pr_run_get_wave_for_difficulty(run, ctx->wave, false);
    const bool final_boss = pr_run_is_wave_final(run->mode, ctx->wave);
    int chances = (int)ceil(difficulty_wave / 10.0);
    if (final_boss) chances = (int)ceil(chances * 2.5);
    uint8_t tflags = 0;
    if (ctx->trainer_battle && ctx->trainer_type >= 0 && ctx->trainer_type < pr_trainer_item_flag_count)
        tflags = pr_trainer_item_flags[ctx->trainer_type];
    /* trainer.genModifiers(party): config.genModifiersFunc (a closure) */
    if (tflags & 2) { run->unported |= PR_RUN_UNPORTED_TRAINER; return -1; }

    for (int p = 0; p < ctx->n_party && p < 6; p++) {
        /* isBoss = enemyPokemon.isBoss() || (TRAINER && trainer.config.isBoss) */
        const bool boss = ctx->party[p].boss || (ctx->trainer_battle && (tflags & 1));
        int upgrade_chance = 32;
        if (boss) upgrade_chance /= 2;
        if (final_boss) upgrade_chance /= 8;
        int count = 0;
        for (int c = 0; c < chances; c++)
            if (!seed_int(run, pr_run_get_enemy_modifier_chance(run, boss))) count++;
        if (boss && count < chances / 2) count = chances / 2;

        /* getEnemyModifierTypesForWave(difficultyWave, count, [enemy], poolType, upgradeChance): each one
         * getNewModifierTypeOption(party, poolType, undefined, upgradeChance && !randSeedInt(upgradeChance) ? 1 : 0) */
        for (int k = 0; k < count; k++) {
            int upgrade = upgrade_chance && !seed_int(run, upgrade_chance) ? 1 : 0;
            /* tierValue = randSeedInt(1024): > 255 COMMON, > 60 GREAT, > 12 ULTRA, > 0 ROGUE, else MASTER;
             * + upgradeCount, dropping to the next tier down while the pool has none there */
            const int v = seed_int(run, 1024);
            int tier = v > 255 ? 0 : v > 60 ? 1 : v > 12 ? 2 : v ? 3 : 4;
            tier += upgrade;
            while (tier && (tier > 5 || pr_enemy_pools[pool][tier].n <= 0)) {
                tier--;
                if (upgrade) upgrade--;
            }
            /* the pick, retried from the same tier while a generator gives null; after 100 retries a tier down */
            int item = -1, arg = -1;
            for (int retry = 0;; retry++) {
                if (retry >= 100 && tier) { retry = 0; tier--; }
                if (!th[tier].n) { run->unported |= PR_RUN_UNDEFINED_UPSTREAM; return -1; }   /* no thresholds */
                const int value = seed_int(run, th[tier].total[th[tier].n - 1]);
                int index = -1;
                for (int i = 0; i < th[tier].n; i++)
                    if (value < th[tier].total[i]) { index = th[tier].index[i]; break; }
                if (index < 0) { run->unported |= PR_RUN_UNDEFINED_UPSTREAM; return -1; }
                const pr_pool_entry *e = &pr_pool_entries[pr_enemy_pools[pool][tier].start + index];
                if (e->kind == PR_POOL_PLAIN) { item = e->item; break; }
                arg = generate(run, e->kind, &ctx->party[p], 1);
                if (arg != -2) { item = e->item; break; }
            }
            if (out->n[p] < PR_MAX_ENEMY_ITEMS) out->items[p][out->n[p]++] = (pr_enemy_item){(uint8_t)item, (int8_t)arg};
            else out->overflow = true;
        }
        /* every 1000th wave also gives a Mini Black Hole */
        if (!(difficulty_wave % 1000)) {
            if (out->n[p] < PR_MAX_ENEMY_ITEMS) out->items[p][out->n[p]++] = (pr_enemy_item){(uint8_t)pr_item_mini_black_hole, -1};
            else out->overflow = true;
        }
    }
    return 0;
}

static pr_enemy_buffs *find_buff(pr_enemy_buffs *b, int item, int *index)
{
    for (int i = 0; i < b->n; i++)
        if (b->buff[i].item == item) { *index = i; return b; }
    *index = -1;
    return b;
}

/* AddEnemyBuffModifierPhase:
 *   tier = waveIndex % 1000 ? (waveIndex % 250 ? COMMON : GREAT) : ULTRA;
 *   regenerateModifierPoolThresholds(enemyParty, ENEMY_BUFF);   // a buff whose held stacks are all at
 *                                                              // getMaxStackCount(true) weighs 0
 *   for (count = ceil(waveIndex / 250)) addEnemyModifier(getEnemyBuffModifierForWave(tier, enemy buffs))
 * getEnemyBuffModifierForWave: tierStackCount ULTRA 5, GREAT 3, else 1;
 *   candidate = getNewModifierTypeOption([], ENEMY_BUFF, tier)   // the weighted pick, no tier roll
 *   while (++r < 50 && (held = the held buff of candidate's type)
 *          && held.getMaxStackCount() < held.stackCount + (r < 10 ? tierStackCount : 1)) candidate = the pick again;
 *   the buff, with stackCount = tierStackCount: add() raises a held one's stack when it fits, else nothing */
int pr_run_add_enemy_buffs(pr_run *run, int wave, pr_enemy_buffs *buffs)
{
    const int tier = wave % 1000 ? (wave % 250 ? 0 : 1) : 2;
    const int hi = wave < 2000 ? 0 : 2;   /* the limits read currentBattle.waveIndex (the damage reducer's rises) */
    const pr_pool_tier *pt = &pr_enemy_pools[2][tier];
    /* the thresholds, built once before the buffs are added */
    int total[16], index[16], n = 0, sum = 0;
    for (int i = 0; i < pt->n && n < 16; i++) {
        const pr_pool_entry *e = &pr_pool_entries[pt->start + i];
        int h;
        find_buff(buffs, e->item, &h);
        const bool maxed = h >= 0 && buffs->buff[h].stack >= pr_item_max_stacks[e->item][hi + 1];
        if (!e->weight || maxed) continue;
        sum += e->weight;
        total[n] = sum;
        index[n++] = i;
    }
    const int tier_stack = tier == 2 ? 5 : tier == 1 ? 3 : 1;
    const int count = (wave + 249) / 250;
    for (int k = 0; k < count; k++) {
        int item = -1;
        for (int r = 0;;) {
            if (!n) { run->unported |= PR_RUN_UNDEFINED_UPSTREAM; return -1; }
            const int value = pr_rng_seed_int(&run->rng, total[n - 1], 0);
            for (int i = 0; i < n; i++)
                if (value < total[i]) { item = pr_pool_entries[pt->start + index[i]].item; break; }
            int h;
            find_buff(buffs, item, &h);
            if (!(++r < 50 && h >= 0 && pr_item_max_stacks[item][hi] < buffs->buff[h].stack + (r < 10 ? tier_stack : 1)))
                break;
        }
        int h;
        find_buff(buffs, item, &h);
        if (h < 0) {
            if (buffs->n < 32) {
                buffs->buff[buffs->n].item = (uint8_t)item;
                buffs->buff[buffs->n++].stack = tier_stack;
            }
        } else if (buffs->buff[h].stack + tier_stack <= pr_item_max_stacks[item][hi]) {
            buffs->buff[h].stack += tier_stack;
        }
    }
    return 0;
}
