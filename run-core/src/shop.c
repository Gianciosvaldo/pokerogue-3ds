/* The shop's economy: wave money, the always-available shop items and their prices, the
 * reroll cost, and the modifiers that change them (src/battle-scene.ts,
 * src/modifier/modifier-type.ts, src/phases/select-modifier-phase.ts,
 * src/phases/money-reward-phase.ts). The random reward options (the modifier pools and their
 * weights) aren't ported yet. The JS each function replaces is quoted above it. */
#include "pr/run.h"
#include <math.h>

#define MAX_SAFE_INTEGER 9007199254740991.0

const char *const pr_shop_item_name[PR_SHOP_ITEM_COUNT_] = {
    "POTION", "ETHER", "REVIVE", "SUPER_POTION", "FULL_HEAL", "ELIXIR", "MAX_ETHER", "HYPER_POTION",
    "MAX_REVIVE", "MEMORY_MUSHROOM", "MAX_POTION", "MAX_ELIXIR", "FULL_RESTORE", "SACRED_ASH",
};

/* getWaveMoneyAmount(moneyMultiplier), with currentBattle.waveIndex = wave:
 *   const waveSetIndex = Math.ceil(waveIndex / 10) - 1;
 *   const moneyValue = Math.pow((waveSetIndex + 1 + (0.75 + (((waveIndex - 1) % 10) + 1) / 10)) * 100,
 *                               1 + 0.005 * waveSetIndex) * moneyMultiplier;
 *   return Math.floor(moneyValue / 10) * 10; */
double pr_run_get_wave_money_amount(int wave, double money_multiplier)
{
    double set = ceil(wave / 10.0) - 1;
    double value = js_pow((set + 1 + (0.75 + (fmod(wave - 1, 10) + 1) / 10)) * 100, 1 + 0.005 * set) * money_multiplier;
    return floor(value / 10) * 10;
}

/* new ModifierTypeOption(type, upgradeCount, cost): this.cost = Math.min(Math.round(cost), Number.MAX_SAFE_INTEGER) */
static double option_cost(double cost)
{
    double r = js_round(cost);
    return r < MAX_SAFE_INTEGER ? r : MAX_SAFE_INTEGER;
}

/* getPlayerShopModifierTypeOptionsForWave(waveIndex, baseCost):
 *   if (!(waveIndex % 10)) return [];
 *   const options = [[POTION 0.2, ETHER 0.4, REVIVE 2], [SUPER_POTION 0.45, FULL_HEAL 1], [ELIXIR 1, MAX_ETHER 1],
 *     [HYPER_POTION 0.8, MAX_REVIVE 2.75, MEMORY_MUSHROOM 4], [MAX_POTION 1.5, MAX_ELIXIR 2.5],
 *     [FULL_RESTORE 2.25], [SACRED_ASH 10]];  (each cost baseCost * factor)
 *   return options.slice(0, Math.ceil(Math.max(waveIndex + 10, 0) / 30)).flat()
 *     .filter(shopItem => applyChallenges(SHOP_ITEM, shopItem, status) ... status.value); */
int pr_run_get_player_shop_options(pr_run *run, int wave, double base_cost, pr_shop_option out[PR_SHOP_ITEM_COUNT_])
{
    static const struct { int item; double factor; uint8_t row; } table[PR_SHOP_ITEM_COUNT_] = {
        {PR_SHOP_POTION, 0.2, 0}, {PR_SHOP_ETHER, 0.4, 0}, {PR_SHOP_REVIVE, 2, 0},
        {PR_SHOP_SUPER_POTION, 0.45, 1}, {PR_SHOP_FULL_HEAL, 1, 1},
        {PR_SHOP_ELIXIR, 1, 2}, {PR_SHOP_MAX_ETHER, 1, 2},
        {PR_SHOP_HYPER_POTION, 0.8, 3}, {PR_SHOP_MAX_REVIVE, 2.75, 3}, {PR_SHOP_MEMORY_MUSHROOM, 4, 3},
        {PR_SHOP_MAX_POTION, 1.5, 4}, {PR_SHOP_MAX_ELIXIR, 2.5, 4},
        {PR_SHOP_FULL_RESTORE, 2.25, 5},
        {PR_SHOP_SACRED_ASH, 10, 6},
    };
    if (wave % 10 == 0) return 0;
    if (run->has_challenges) run->unported |= PR_RUN_UNPORTED_CHALLENGES;
    double rows = ceil((wave + 10 > 0 ? wave + 10 : 0) / 30.0);
    int n = 0;
    for (int i = 0; i < PR_SHOP_ITEM_COUNT_; i++) {
        if (table[i].row >= rows) break;
        out[n].item = table[i].item;
        /* the table's factor times baseCost, as the JS writes it (baseCost alone for factor 1) */
        out[n].cost = option_cost(table[i].factor == 1 ? base_cost : base_cost * table[i].factor);
        n++;
    }
    return n;
}

/* gameMode.getShopStatus(): !hasNoShop (the daily run has no shop), then the challenges' SHOP hook */
bool pr_run_get_shop_status(pr_run *run)
{
    if (run->has_challenges) run->unported |= PR_RUN_UNPORTED_CHALLENGES;
    return run->mode != GAME_MODES_DAILY;
}

/* HealShopCostModifier (Black Sludge).apply(moneyCost): moneyCost.value = Math.floor(moneyCost.value * this.shopMultiplier).
 * globalScene.applyModifier applies only the first one; black_sludge_multiplier 0 = none held. */
double pr_run_apply_heal_shop_cost(double cost, double black_sludge_multiplier)
{
    return black_sludge_multiplier ? floor(cost * black_sludge_multiplier) : cost;
}

/* MoneyRewardPhase.start():
 *   const moneyAmount = new NumberHolder(getWaveMoneyAmount(this.moneyMultiplier));
 *   applyModifiers(MoneyMultiplierModifier, true, moneyAmount);  // each: value += Math.floor(value * 0.2 * getStackCount())
 *   if (arena.getTag(HAPPY_HOUR)) moneyAmount.value *= 2; */
double pr_run_get_money_reward(int wave, double money_multiplier, const int *amulet_coin_stacks, int n_amulet_coins,
                               bool happy_hour)
{
    double value = pr_run_get_wave_money_amount(wave, money_multiplier);
    for (int i = 0; i < n_amulet_coins; i++) value += floor(value * 0.2 * amulet_coin_stacks[i]);
    if (happy_hour) value *= 2;
    return value;
}

/* SelectModifierPhase.getRerollCost(lockRarities) (WAIVE_ROLL_FEE_OVERRIDE, a dev override, isn't modelled):
 *   if (lockRarities) { const tierValues = [50, 125, 300, 750, 2000];
 *     for (const opt of this.typeOptions) baseValue += tierValues[opt.type.tier ?? 0]; } else baseValue = 250;
 *   let multiplier = 1;
 *   if (customModifierSettings?.rerollMultiplier != null) { if (< 0) return -1; multiplier = rerollMultiplier; }
 *   const baseMultiplier = Math.min(Math.ceil(waveIndex / 10) * baseValue * 2 ** this.rerollCount * multiplier,
 *                                   Number.MAX_SAFE_INTEGER);
 *   applyModifier(HealShopCostModifier, true, modifiedRerollCost);
 * option_tiers[i] < 0 stands for an undefined tier (?? 0); a tier past MASTER reads undefined
 * from tierValues, so the cost is NaN, as upstream's. custom_multiplier NaN = not set. */
double pr_run_get_reroll_cost(int wave, bool lock_rarities, const int *option_tiers, int n_options, int reroll_count,
                              double custom_multiplier, double black_sludge_multiplier)
{
    static const double tier_values[5] = {50, 125, 300, 750, 2000};
    double base = 0;
    if (lock_rarities) {
        for (int i = 0; i < n_options; i++) {
            int t = option_tiers[i] < 0 ? 0 : option_tiers[i];
            base += t < 5 ? tier_values[t] : NAN;
        }
    } else {
        base = 250;
    }
    double multiplier = 1;
    if (custom_multiplier == custom_multiplier) {
        if (custom_multiplier < 0) return -1;
        multiplier = custom_multiplier;
    }
    double cost = ceil(wave / 10.0) * base * js_pow(2, reroll_count) * multiplier;
    cost = cost < MAX_SAFE_INTEGER || cost != cost ? cost : MAX_SAFE_INTEGER;
    return pr_run_apply_heal_shop_cost(cost, black_sludge_multiplier);
}
