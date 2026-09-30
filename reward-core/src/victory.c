/* The fixed rewards of a won wave: VictoryPhase.start (src/phases/victory-phase.ts) from the point the enemy side
 * is beaten. Timed events' fixed battle rewards (timedEventManager.getFixedBattleEventRewards) aren't ported: the
 * port runs with no event, as upstream does between events. */
#include "pr/reward.h"
#include "reward_internal.h"

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
