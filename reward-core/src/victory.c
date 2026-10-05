/* The fixed rewards of a won wave: VictoryPhase.start (src/phases/victory-phase.ts) from the point the enemy side
 * is beaten. Timed events' fixed battle rewards (timedEventManager.getFixedBattleEventRewards) aren't ported: the
 * port runs with no event, as upstream does between events. */
#include "pr/reward.h"
#include "pr_enums.h"
#include "reward_internal.h"

int pr_reward_trainer_victory_rewards(int trainer_type, bool is_boss, bool has_voucher_config, bool has_voucher_unlocked, bool upgrade_unlocked_vouchers, double money_multiplier, uint16_t out[8])
{
    int n = 0;
    if (trainer_type == TRAINER_TYPE_RIVAL) {
        out[n++] = PR_ITEM_SUPER_EXP_CHARM;
        out[n++] = PR_ITEM_EXP_SHARE;
    } else if (trainer_type == TRAINER_TYPE_RIVAL_2) {
        out[n++] = PR_ITEM_EXP_SHARE;
    } else if (trainer_type == TRAINER_TYPE_RIVAL_4) {
        out[n++] = PR_ITEM_TERA_ORB;
    }

    if (is_boss && has_voucher_config && !has_voucher_unlocked) {
        /* Determine voucher type. In upstream trainerConfigs, moneyMultiplier < 10 => PLUS (1), else PREMIUM (2) */
        int vtype = (money_multiplier < 10.0) ? 1 : 2;
        if (upgrade_unlocked_vouchers) {
            uint16_t map[4] = { PR_ITEM_VOUCHER_PLUS, PR_ITEM_VOUCHER_PLUS, PR_ITEM_VOUCHER_PLUS, PR_ITEM_VOUCHER_PREMIUM };
            out[n++] = map[vtype];
        } else {
            uint16_t map[4] = { PR_ITEM_VOUCHER, PR_ITEM_VOUCHER, PR_ITEM_VOUCHER_PLUS, PR_ITEM_VOUCHER_PREMIUM };
            out[n++] = map[vtype];
        }
    }
    return n;
}

int pr_reward_fixed_battle_custom_settings(int wave, pr_reward_custom *custom)
{
    if (!custom) return 0;
    custom->n_tiers = 0;
    custom->n_options = 0;
    custom->n_funcs = 0;
    custom->fill_remaining = false;
    custom->reroll_multiplier = 1.0;
    custom->allow_luck_upgrades = true;

    /* Fixed battle custom settings from fixed-battle-configs.ts */
    if (wave == 25) { /* RIVAL_2 */
        custom->tiers[0] = 3; custom->tiers[1] = 2; custom->tiers[2] = 2; /* ULTRA, GREAT, GREAT */
        custom->n_tiers = 3;
        custom->allow_luck_upgrades = false;
        return 1;
    } else if (wave == 55) { /* RIVAL_3 */
        custom->tiers[0] = 3; custom->tiers[1] = 3; custom->tiers[2] = 2; custom->tiers[3] = 2;
        custom->n_tiers = 4;
        custom->allow_luck_upgrades = false;
        return 1;
    } else if (wave == 95) { /* RIVAL_4 */
        custom->tiers[0] = 3; custom->tiers[1] = 3; custom->tiers[2] = 3; custom->tiers[3] = 3;
        custom->n_tiers = 4;
        custom->allow_luck_upgrades = false;
        return 1;
    } else if (wave == 115) { /* EVIL_BOSS_1 */
        custom->tiers[0] = 4; custom->tiers[1] = 4; custom->tiers[2] = 3; custom->tiers[3] = 3; custom->tiers[4] = 3;
        custom->n_tiers = 5;
        custom->allow_luck_upgrades = false;
        return 1;
    } else if (wave == 145) { /* RIVAL_5 */
        custom->tiers[0] = 4; custom->tiers[1] = 4; custom->tiers[2] = 4; custom->tiers[3] = 3; custom->tiers[4] = 3;
        custom->n_tiers = 5;
        custom->allow_luck_upgrades = false;
        return 1;
    } else if (wave == 165) { /* EVIL_BOSS_2 */
        custom->tiers[0] = 4; custom->tiers[1] = 4; custom->tiers[2] = 3; custom->tiers[3] = 3; custom->tiers[4] = 3; custom->tiers[5] = 3;
        custom->n_tiers = 6;
        custom->allow_luck_upgrades = false;
        return 1;
    } else if (wave == 195) { /* RIVAL_6 */
        custom->tiers[0] = 4; custom->tiers[1] = 4; custom->tiers[2] = 3; custom->tiers[3] = 3; custom->tiers[4] = 2; custom->tiers[5] = 2;
        custom->n_tiers = 6;
        custom->allow_luck_upgrades = false;
        return 1;
    }
    return 0;
}

int pr_reward_victory_rewards(const pr_run *run, int wave, uint16_t out[8], bool *select_screen)
{
    int n = 0;
    *select_screen = false;
    const bool endless = run->mode == GAME_MODES_ENDLESS || run->mode == GAME_MODES_SPLICED_ENDLESS;
    if (!endless && pr_run_is_wave_final(run->mode, wave)) return 0;   /* GameOverPhase(true) */
    if (pr_reward_is_classic(run) && wave == 165) out[n++] = PR_ITEM_LOCK_CAPSULE;   /* ClassicFixedBossWaves.EVIL_BOSS_2 */
    if (wave % 10) {
        *select_screen = true;   /* SelectModifierPhase with the fixed battle's customModifierRewardSettings */
    } else if (run->mode == GAME_MODES_DAILY) {
        out[n++] = PR_ITEM_EXP_CHARM;
        if (wave > 10 && !pr_run_is_wave_final(run->mode, wave)) out[n++] = PR_ITEM_GOLDEN_POKEBALL;
    } else {
        const int super_exp_wave = endless ? 10 : run->offset_gym ? 0 : 20;
        if (endless && wave == 10) out[n++] = PR_ITEM_EXP_SHARE;
        if (pr_reward_is_classic(run) && wave == 10) out[n++] = PR_ITEM_EXP_CHARM;
        if (wave <= 750 && (wave <= 500 || wave % 30 == super_exp_wave))
            out[n++] = wave % 30 != super_exp_wave || wave > 250 ? PR_ITEM_EXP_CHARM : PR_ITEM_SUPER_EXP_CHARM;
        if (wave <= 150 && !(wave % 50)) out[n++] = PR_ITEM_GOLDEN_POKEBALL;
        if (endless && !(wave % 50)) out[n++] = wave % 250 ? PR_ITEM_VOUCHER_PLUS : PR_ITEM_VOUCHER_PREMIUM;
    }
    return n;
}
