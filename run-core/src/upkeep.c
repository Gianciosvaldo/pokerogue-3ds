/* Between-wave upkeep: Pokerus spreading through the party and the Coin Case's interest
 * (src/battle-scene.ts trySpreadPokerus and addMoney, src/modifier/modifier.ts MoneyInterestModifier).
 * The JS each function replaces is quoted above it. */
#include "pr/run.h"
#include <math.h>

#define MAX_SAFE_INTEGER 9007199254740991.0

/* trySpreadPokerus():
 *   const spread = (index, spreadTo) => { const partyMember = party[index + spreadTo];
 *     if (!partyMember.pokerus && !randSeedInt(10)) { partyMember.pokerus = true; infectedIndexes.push(index + spreadTo); } };
 *   party.forEach((pokemon, p) => {
 *     if (!pokemon.pokerus || infectedIndexes.indexOf(p) > -1) return;
 *     executeWithSeedOffset(() => { if (p) spread(p, -1); if (p < party.length - 1) spread(p, 1); },
 *                           currentBattle.waveIndex + (p << 8)); });
 * `pokerus` is the party's flags in order, updated in place; returns how many caught it. */
int pr_run_try_spread_pokerus(const pr_run *run, int wave, bool *pokerus, int n_party)
{
    bool infected[PR_MAX_EXP_PARTY] = {false};
    int caught = 0;
    for (int p = 0; p < n_party && p < PR_MAX_EXP_PARTY; p++) {
        if (!pokerus[p] || infected[p]) continue;
        pr_rng r = pr_run_seed_offset_rng(run, wave + (p << 8), NULL);
        for (int d = -1; d <= 1; d += 2) {
            int q = p + d;
            if ((d < 0 && !p) || (d > 0 && p >= n_party - 1)) continue;
            if (!pokerus[q] && !pr_rng_seed_int(&r, 10, 0)) {
                pokerus[q] = true;
                infected[q] = true;
                caught++;
            }
        }
    }
    return caught;
}

/* addMoney(amount): money = Math.min(money + amount, Number.MAX_SAFE_INTEGER) */
double pr_run_add_money(double money, double amount)
{
    double m = money + amount;
    return m < MAX_SAFE_INTEGER ? m : MAX_SAFE_INTEGER;
}

/* applyModifiers(MoneyInterestModifier, true) as the biome change does on X1 waves; each Coin Case:
 *   const interestAmount = Math.floor(globalScene.money * 0.1 * this.getStackCount()); addMoney(interestAmount);
 * stacks[i] is each one's getStackCount(), in modifier-list order. Returns the new money. */
double pr_run_apply_money_interest(double money, const int *stacks, int n)
{
    for (int i = 0; i < n; i++) money = pr_run_add_money(money, floor(money * 0.1 * stacks[i]));
    return money;
}
