/* reward-core's own shared helpers (not part of the API) */
#ifndef PR_REWARD_INTERNAL_H
#define PR_REWARD_INTERNAL_H
#include "pr/reward.h"

const pr_reward_species *pr_reward_species_find(int species);
int pr_reward_form_key(int species, int form);
const pr_owned_item *pr_reward_held(const pr_reward_ctx *ctx, int p, int item, int arg);
bool pr_reward_has_item(const pr_reward_ctx *ctx, int item);
bool pr_reward_tera_excluded_species(const pr_reward_mon *m);
/* generateType(party) of a generator item: the arg, -2 for null, -1 for a plain item */
int pr_reward_generate(pr_reward_ctx *ctx, int item);
/* the PLAYER pool's weight function of `item` in `tier` (src/pools.c) */
int pr_reward_weight(pr_reward_ctx *ctx, int item, int tier, int reroll_count);
/* getHpRatio(): Math.round(hp / maxHp * 100) / 100 */
double pr_reward_hp_ratio(const pr_reward_mon *m);
bool pr_reward_is_classic(const pr_run *run);

#endif
