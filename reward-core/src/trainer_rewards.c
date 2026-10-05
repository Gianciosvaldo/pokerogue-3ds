/* The trainers' own rewards (card 97): the fixed battles' reward screen settings (FixedBattleConfig
 * .customModifierRewardSettings, src/data/trainers/fixed-battle-configs.ts, and SingleGenerationChallenge
 * .applyFixedBattle in src/data/challenge.ts), what TrainerVictoryPhase queues (src/phases/trainer-victory-phase.ts:
 * the money, modifierRewardFuncs and the boss voucher) and the trainers' genModifiersFunc closures
 * (src/data/trainers/trainer-config.ts). The data comes from gen/out/reward_data.c (dumped); the closures and the
 * hook's draws are ported here. */
#include "pr/reward.h"
#include "pr_enums.h"
#include "reward_internal.h"
#include <math.h>
#include <string.h>

/* ---- fixed battles ---- */

static const pr_fixed_reward *battle_config_row(int mode, int wave)
{
    for (int i = 0; i < pr_fixed_reward_count; i++) {
        const pr_fixed_reward *r = &pr_fixed_rewards[i];
        if (r->mode == mode && r->single_gen == 0 && r->wave == wave) return r;
    }
    return NULL;
}

/* SingleGenerationChallenge.applyFixedBattle(wave, config): the randSeedItem calls in the trainer type arrays its
 * Elite Four and champion cases build (every one of them picks from two), then whether it takes the wave over */
static const pr_fixed_reward *single_gen_hook(pr_run *run, int wave)
{
    int picks = 0;
    switch (wave) {
    case CLASSIC_FIXED_BOSS_WAVES_ELITE_FOUR_1: picks = 2; break;   /* [HALA, MOLAYNE], [MARNIE_ELITE, BEDE_ELITE] */
    case CLASSIC_FIXED_BOSS_WAVES_ELITE_FOUR_3: picks = 1; break;   /* [BEA_ELITE, ALLISTER_ELITE] */
    case CLASSIC_FIXED_BOSS_WAVES_CHAMPION: picks = 6; break;       /* [RED, LANCE_CHAMPION], [STEVEN, WALLACE],
                                                                     * [ALDER, IRIS], [KUKUI, HAU], [LEON, MUSTARD],
                                                                     * [GEETA, NEMONA] */
    }
    for (int i = 0; i < picks; i++) pr_rng_integer_in_range(&run->rng, 0, 1);   /* Phaser RND.pick of two */
    for (int i = 0; i < pr_fixed_reward_count; i++) {
        const pr_fixed_reward *r = &pr_fixed_rewards[i];
        if (r->mode == -1 && r->single_gen == run->challenge_generation && r->wave == wave) return r;
    }
    return NULL;
}

/* applyChallenges(FIXED_BATTLES, wave, config): Single Generation is the only challenge with the hook, and
 * applyChallenges skips a challenge whose value is 0 */
static const pr_fixed_reward *apply_fixed_battle_challenges(pr_run *run, int wave, bool *applied)
{
    *applied = false;
    if (!run->challenge_generation) return NULL;
    const pr_fixed_reward *r = single_gen_hook(run, wave);
    *applied = r != NULL;
    return r;
}

/* gameMode.getFixedBattle(wave): the challenge's config when the hook applies, else battleConfig[wave] */
static const pr_fixed_reward *get_fixed_battle(pr_run *run, int wave)
{
    bool applied;
    const pr_fixed_reward *r = apply_fixed_battle_challenges(run, wave, &applied);
    return applied ? r : battle_config_row(run->mode, wave);
}

bool pr_reward_fixed_battle_settings(pr_run *run, int wave, bool biome_path, pr_reward_custom *out)
{
    const pr_fixed_reward *r;
    if (biome_path) {
        /* isFixedBattle(wave): Object.hasOwn(battleConfig, wave) || applyChallenges(FIXED_BATTLES, wave, dummy) */
        bool fixed = battle_config_row(run->mode, wave) != NULL;
        if (!fixed) apply_fixed_battle_challenges(run, wave, &fixed);
        if (!fixed) return false;
    }
    r = get_fixed_battle(run, wave);
    if (!r || !r->has_custom) return false;
    memset(out, 0, sizeof *out);
    out->n_tiers = r->n_tiers;
    for (int i = 0; i < r->n_tiers; i++) out->tiers[i] = r->tiers[i];
    out->fill_remaining = r->fill_remaining;
    out->reroll_multiplier = NAN;
    out->allow_luck_upgrades = r->allow_luck_upgrades;
    return true;
}

/* ---- TrainerVictoryPhase ---- */

static const pr_reward_trainer *trainer_row(int trainer_type)
{
    if (trainer_type < 0 || trainer_type >= pr_reward_trainer_count) return NULL;
    const pr_reward_trainer *t = &pr_reward_trainers[trainer_type];
    return t->has_config ? t : NULL;
}

int pr_reward_trainer_victory_rewards(int trainer_type, bool voucher_unlocked, bool upgrade_unlocked_vouchers,
                                      pr_reward_trainer_victory *out)
{
    const pr_reward_trainer *t = trainer_row(trainer_type);
    if (!t) return -1;
    memset(out, 0, sizeof *out);
    out->money_multiplier = t->money_multiplier;
    for (int i = 0; i < t->n_reward_items; i++) out->items[out->n_items++] = (uint16_t)t->reward_items[i];
    out->voucher_type = t->voucher_type;
    /* Object.hasOwn(vouchers, TrainerType[trainerType]) && !validateVoucher(voucher) && config.isBoss.
     * validateVoucher: not unlocked yet and voucher.validate() (a trainer voucher has no condition) unlocks it,
     * counts it and returns true, so the reward phase is only queued once it was unlocked before. */
    if (t->voucher_type >= 0) {
        const bool validated = !voucher_unlocked;
        out->unlock_voucher = validated;
        if (!validated && t->is_boss) {
            static const uint16_t upgraded[4] = {PR_ITEM_VOUCHER_PLUS, PR_ITEM_VOUCHER_PLUS, PR_ITEM_VOUCHER_PLUS,
                                                 PR_ITEM_VOUCHER_PREMIUM};
            static const uint16_t plain[4] = {PR_ITEM_VOUCHER, PR_ITEM_VOUCHER, PR_ITEM_VOUCHER_PLUS,
                                              PR_ITEM_VOUCHER_PREMIUM};
            out->items[out->n_items++] = (upgrade_unlocked_vouchers ? upgraded : plain)[t->voucher_type];
        }
    }
    return 0;
}

/* ---- Trainer.genModifiers(party) ---- */

/* hasAbility(ability, false, true): getAbility(true), or the passive when it has one */
static bool has_ability(const pr_reward_enemy_mon *m, int ability)
{
    return m->ability == ability || (m->passive && m->passive == ability);
}

static void give(pr_owned_item *out, int *n, int item, int arg, int owner)
{
    out[*n] = (pr_owned_item){(uint16_t)item, (int16_t)arg, 1, (int8_t)owner, 0, false};
    (*n)++;
}

int pr_reward_trainer_items(int trainer_type, const pr_reward_enemy_mon *party, int n_party, pr_owned_item out[2])
{
    const pr_reward_trainer *t = trainer_row(trainer_type);
    int n = 0;
    if (!t || !t->gen_modifiers) return 0;
    switch (trainer_type) {
    case TRAINER_TYPE_RED:   /* Light Ball on party[0] (Pikachu), whatever it is */
        if (n_party < 1) return -1;
        give(out, &n, PR_ITEM_RARE_SPECIES_STAT_BOOSTER, PR_SSB_LIGHT_BALL, 0);
        break;
    case TRAINER_TYPE_STEVEN:   /* Mystical Rock on party[0] (Gigalith) */
        if (n_party < 1) return -1;
        give(out, &n, PR_ITEM_MYSTICAL_ROCK, -1, 0);
        break;
    case TRAINER_TYPE_WALLACE:   /* Mystical Rock on party[0] (Pelipper); Toxic Orb on party[5] with Marvel Scale */
        if (n_party < 1) return -1;
        give(out, &n, PR_ITEM_MYSTICAL_ROCK, -1, 0);
        if (n_party > 5 && has_ability(&party[5], ABILITY_ID_MARVEL_SCALE)) give(out, &n, PR_ITEM_TOXIC_ORB, -1, 5);
        break;
    case TRAINER_TYPE_KIERAN:   /* Toxic Orb on party[2] (Gliscor) with Poison Heal */
        if (n_party > 2 && has_ability(&party[2], ABILITY_ID_POISON_HEAL)) give(out, &n, PR_ITEM_TOXIC_ORB, -1, 2);
        break;
    case TRAINER_TYPE_GUZMA_2:   /* Toxic Orb on party[4] (Gliscor) with Poison Heal */
        if (n_party > 4 && has_ability(&party[4], ABILITY_ID_POISON_HEAL)) give(out, &n, PR_ITEM_TOXIC_ORB, -1, 4);
        break;
    case TRAINER_TYPE_RIVAL_5:
    case TRAINER_TYPE_RIVAL_6:   /* Flame Orb on party[1] (the bird) when it is a Swellow with Guts */
        if (n_party < 2) return -1;
        if (party[1].species == SPECIES_ID_SWELLOW && has_ability(&party[1], ABILITY_ID_GUTS))
            give(out, &n, PR_ITEM_FLAME_ORB, -1, 1);
        break;
    default:   /* the dump fails when upstream adds a trainer with a genModifiersFunc */
        return 0;
    }
    return n;
}
