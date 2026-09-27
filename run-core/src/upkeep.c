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

/* getPartyLuckValue(party) (src/modifier/modifier-type.ts):
 *   if (isDaily) { executeWithSeedOffset(() => luck = getDailyEventSeedLuck() ?? randSeedInt(15), 0, seed); return luck; }
 *   const luck = Phaser.Math.Clamp(party.map(p => p.isAllowedInBattle()
 *       ? p.getLuck() + (eventLuckBoostedSpecies.includes(p.species.speciesId) ? 1 : 0) : 0).reduce(sum), 0, 14);
 *   return Math.min(timedEventManager.getEventLuckBoost() + luck, 14);
 * getLuck(): luck + (isFusion() ? fusionLuck : 0) */
int pr_run_get_party_luck_value(pr_run *run, const pr_luck_member *party, int n, const int *event_species,
                                int n_event_species, int event_luck_boost)
{
    if (run->mode == GAME_MODES_DAILY) {
        if (run->daily_custom) { run->unported |= PR_RUN_UNPORTED_DAILY; return 0; }   /* the event seed's luck */
        pr_rng r = pr_run_seed_offset_rng(run, 0, NULL);
        return pr_rng_seed_int(&r, 15, 0);
    }
    int luck = 0;
    for (int i = 0; i < n; i++) {
        if (!party[i].allowed) continue;
        luck += party[i].luck + (party[i].fusion ? party[i].fusion_luck : 0);
        for (int k = 0; k < n_event_species; k++)
            if (event_species[k] == party[i].species) { luck++; break; }
    }
    luck = luck < 0 ? 0 : luck > 14 ? 14 : luck;
    return event_luck_boost + luck < 14 ? event_luck_boost + luck : 14;
}
