/*
 * Unit tests for egg hatch step counters, boss defeat voucher unlocks and voucher rewards.
 */
#include "gc/system.h"
#include "gc/run.h"
#include "../src/internal.h"
#include "pr_enums.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_egg_hatch_steps(void) {
    gc_system sys;
    gc_system_init_defaults(&sys);

    assert(sys.n_eggs == 0);
    bool added1 = gc_system_add_egg(&sys, 101, 10, 0, 1);
    bool added2 = gc_system_add_egg(&sys, 102, 1, 1, 4);
    bool added3 = gc_system_add_egg(&sys, 103, 0, 2, 7);

    assert(added1 && added2 && added3);
    assert(sys.n_eggs == 3);

    int updated = gc_system_increment_egg_steps(&sys);
    assert(updated == 2);
    assert(sys.eggs[0].hatch_waves == 9);
    assert(sys.eggs[1].hatch_waves == 0);
    assert(sys.eggs[2].hatch_waves == 0);

    updated = gc_system_increment_egg_steps(&sys);
    assert(updated == 1);
    assert(sys.eggs[0].hatch_waves == 8);

    printf("test_egg_hatch_steps... ok\n");
}

static void test_voucher_unlocks(void) {
    gc_system sys;
    gc_system_init_defaults(&sys);

    assert(!gc_system_has_voucher_unlock(&sys, 100));
    assert(sys.voucher_counts[GC_VOUCHER_PLUS] == 0);

    /* First unlock */
    bool unlocked = gc_system_unlock_voucher(&sys, 100, GC_VOUCHER_PLUS);
    assert(unlocked == true);
    assert(gc_system_has_voucher_unlock(&sys, 100));
    assert(sys.voucher_counts[GC_VOUCHER_PLUS] == 1);

    /* Second unlock attempt - should return false and not increment count */
    unlocked = gc_system_unlock_voucher(&sys, 100, GC_VOUCHER_PLUS);
    assert(unlocked == false);
    assert(sys.voucher_counts[GC_VOUCHER_PLUS] == 1);

    /* Unlock another trainer type as PREMIUM */
    unlocked = gc_system_unlock_voucher(&sys, 300, GC_VOUCHER_PREMIUM);
    assert(unlocked == true);
    assert(gc_system_has_voucher_unlock(&sys, 300));
    assert(sys.voucher_counts[GC_VOUCHER_PREMIUM] == 1);

    printf("test_voucher_unlocks... ok\n");
}

/* One defeat of `trainer_type` through the run's TrainerVictoryPhase. Returns the voucher key of the ModifierRewardPhase
 * it queued (NULL = none) after running that phase, as upstream does: AddVoucherModifier.apply adds the count. */
static const char *defeat_trainer(gc_run *run, int trainer_type) {
    gc_internal *in = gc_int(run);
    const char *key = NULL;
    gc_rq_reset(&in->rq);
    run->trainer_type = trainer_type;
    gc_phase_start(run, &(gc_rphase){.type = RP_TRAINER_VICTORY});
    gc_rphase q;
    while (gc_rq_next(&in->rq, &q)) {
        if (q.type == RP_MODIFIER_REWARD && q.key && !strncmp(q.key, "VOUCHER", 7)) {
            assert(!key);
            key = q.key;
            gc_phase_start(run, &q);
        }
    }
    return key;
}

static void new_run(gc_run *run, gc_system *sys) {
    gc_system_init_defaults(sys);
    memset(run, 0, sizeof(*run));
    run->sys = sys;
    run->run.mode = 0; /* GAME_MODES_CLASSIC */
}

/* TrainerVictoryPhase (upstream 8555c08, src/phases/trainer-victory-phase.ts): the first win over a boss with a voucher
 * is validateVoucher unlocking it, +1 of the voucher's own type and no reward; only the later wins queue the
 * ModifierRewardPhase, [VOUCHER, VOUCHER, VOUCHER_PLUS, VOUCHER_PREMIUM][voucherType], which adds a voucher count. */
static void test_run_boss_defeat_voucher(void) {
    static gc_system sys;
    static gc_run run;

    /* Brock, a Gym Leader: a Plus voucher (moneyMultiplier below 10) */
    new_run(&run, &sys);
    assert(!gc_system_has_voucher_unlock(&sys, TRAINER_TYPE_BROCK));
    assert(defeat_trainer(&run, TRAINER_TYPE_BROCK) == NULL);      /* the first win: unlocked, no reward */
    assert(gc_system_has_voucher_unlock(&sys, TRAINER_TYPE_BROCK));
    assert(sys.voucher_counts[GC_VOUCHER_PLUS] == 1);
    assert(sys.voucher_counts[GC_VOUCHER_REGULAR] == 0);
    const char *key = defeat_trainer(&run, TRAINER_TYPE_BROCK);    /* later wins: an Egg Voucher */
    assert(key && !strcmp(key, "VOUCHER"));
    assert(sys.voucher_counts[GC_VOUCHER_REGULAR] == 1);
    assert(sys.voucher_counts[GC_VOUCHER_PLUS] == 1);
    key = defeat_trainer(&run, TRAINER_TYPE_BROCK);
    assert(key && !strcmp(key, "VOUCHER"));
    assert(sys.voucher_counts[GC_VOUCHER_REGULAR] == 2);
    assert(sys.voucher_counts[GC_VOUCHER_PLUS] == 1);
    assert(sys.voucher_counts[GC_VOUCHER_PREMIUM] == 0 && sys.voucher_counts[GC_VOUCHER_GOLDEN] == 0);

    /* Red, a Champion: a Premium voucher (moneyMultiplier 10), its later wins give an Egg Voucher Plus */
    new_run(&run, &sys);
    assert(defeat_trainer(&run, TRAINER_TYPE_RED) == NULL);
    assert(gc_system_has_voucher_unlock(&sys, TRAINER_TYPE_RED));
    assert(sys.voucher_counts[GC_VOUCHER_PREMIUM] == 1);
    key = defeat_trainer(&run, TRAINER_TYPE_RED);
    assert(key && !strcmp(key, "VOUCHER_PLUS"));
    assert(sys.voucher_counts[GC_VOUCHER_PLUS] == 1);
    assert(sys.voucher_counts[GC_VOUCHER_PREMIUM] == 1);
    assert(sys.voucher_counts[GC_VOUCHER_REGULAR] == 0);

    /* each trainer's voucher is its own: Brock's unlock does not stand for another trainer's */
    new_run(&run, &sys);
    defeat_trainer(&run, TRAINER_TYPE_BROCK);
    assert(defeat_trainer(&run, TRAINER_TYPE_RED) == NULL);
    assert(sys.voucher_counts[GC_VOUCHER_PLUS] == 1 && sys.voucher_counts[GC_VOUCHER_PREMIUM] == 1);

    /* a boss that has no voucher, and a trainer that is no boss: no unlock, no reward, any number of wins. The boss
     * is found in the tables: reward-core says it has no voucher, game-core's config says it is a boss. */
    int boss_without = -1;
    for (int i = 0; i < gc_trainer_config_count && boss_without < 0; i++) {
        pr_reward_trainer_victory tv;
        const gc_trainer_config *c = &gc_trainer_configs[i];
        if (c->is_boss && pr_reward_trainer_victory_rewards(c->type, false, false, &tv) == 0 && tv.voucher_type < 0)
            boss_without = c->type;
    }
    assert(boss_without >= 0);
    const int plain[] = {boss_without, TRAINER_TYPE_ACE_TRAINER};
    for (int i = 0; i < 2; i++) {
        new_run(&run, &sys);
        for (int win = 0; win < 3; win++) assert(defeat_trainer(&run, plain[i]) == NULL);
        assert(!gc_system_has_voucher_unlock(&sys, plain[i]));
        for (int k = 0; k < 4; k++) assert(sys.voucher_counts[k] == 0);
    }

    /* no system data: nothing unlocks, nothing is given, nothing is dereferenced */
    new_run(&run, &sys);
    run.sys = NULL;
    assert(defeat_trainer(&run, TRAINER_TYPE_BROCK) == NULL);
    assert(defeat_trainer(&run, TRAINER_TYPE_BROCK) == NULL);

    printf("test_run_boss_defeat_voucher... ok\n");
}

/* ModifierRewardPhase with a voucher: AddVoucherModifier.apply adds one to the count of its VoucherType. Endless mode
 * queues Plus every 50 waves and Premium every 250 (VictoryPhase), and the count comes when the phase runs. */
static void test_voucher_reward_phase(void) {
    static gc_system sys;
    static gc_run run;
    new_run(&run, &sys);
    gc_rq_reset(&gc_int(&run)->rq);
    const char *keys[] = {"VOUCHER", "VOUCHER_PLUS", "VOUCHER_PREMIUM", "VOUCHER_PLUS"};
    const int types[] = {GC_VOUCHER_REGULAR, GC_VOUCHER_PLUS, GC_VOUCHER_PREMIUM, GC_VOUCHER_PLUS};
    uint32_t want[4] = {0, 0, 0, 0};
    for (int i = 0; i < 4; i++) {
        gc_rphase p = {.type = RP_MODIFIER_REWARD, .key = keys[i]};
        gc_phase_start(&run, &p);
        want[types[i]]++;
        assert(!memcmp(sys.voucher_counts, want, sizeof want));
    }
    assert(run.n_items == 0);   /* a voucher is system data, not a run item */
    printf("test_voucher_reward_phase... ok\n");
}

int main(void) {
    test_egg_hatch_steps();
    test_voucher_unlocks();
    test_run_boss_defeat_voucher();
    test_voucher_reward_phase();

    printf("All voucher and egg tests passed successfully!\n");
    return 0;
}
