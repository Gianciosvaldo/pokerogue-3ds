/*
 * Unit tests for egg hatch step counters and boss defeat voucher unlocks.
 */
#include "gc/system.h"
#include "gc/run.h"
#include "../src/internal.h"
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

static void test_run_boss_defeat_voucher(void) {
    gc_system sys;
    gc_system_init_defaults(&sys);

    gc_run run;
    memset(&run, 0, sizeof(run));
    run.sys = &sys;
    run.run.mode = 0; /* GAME_MODES_CLASSIC */
    run.trainer_type = 100; /* Brock (Gym Leader) */

    gc_internal *in = gc_int(&run);
    gc_rq_reset(&in->rq);

    /* Initial state: no voucher unlocked for Brock (100) */
    assert(!gc_system_has_voucher_unlock(&sys, 100));

    /* Simulate defeating Brock in trainer_victory */
    gc_phase_start(&run, &(gc_rphase){.type = RP_TRAINER_VICTORY});

    /* Brock's voucher should now be unlocked in sys */
    assert(gc_system_has_voucher_unlock(&sys, 100));
    assert(sys.voucher_counts[GC_VOUCHER_PLUS] == 1);

    /* Queue should contain RP_MODIFIER_REWARD with key VOUCHER_PLUS */
    bool found_voucher_reward = false;
    for (int l = 0; l < in->rq.n_levels; l++) {
        for (int k = 0; k < in->rq.n[l]; k++) {
            if (in->rq.q[l][k].type == RP_MODIFIER_REWARD && in->rq.q[l][k].key &&
                strcmp(in->rq.q[l][k].key, "VOUCHER_PLUS") == 0) {
                found_voucher_reward = true;
            }
        }
    }
    assert(found_voucher_reward);

    /* Clear queue and defeat Brock again - no new voucher reward should be queued */
    gc_rq_reset(&in->rq);
    gc_phase_start(&run, &(gc_rphase){.type = RP_TRAINER_VICTORY});

    found_voucher_reward = false;
    for (int l = 0; l < in->rq.n_levels; l++) {
        for (int k = 0; k < in->rq.n[l]; k++) {
            if (in->rq.q[l][k].type == RP_MODIFIER_REWARD && in->rq.q[l][k].key &&
                strcmp(in->rq.q[l][k].key, "VOUCHER_PLUS") == 0) {
                found_voucher_reward = true;
            }
        }
    }
    assert(!found_voucher_reward);
    assert(sys.voucher_counts[GC_VOUCHER_PLUS] == 1);

    printf("test_run_boss_defeat_voucher... ok\n");
}

int main(void) {
    test_egg_hatch_steps();
    test_voucher_unlocks();
    test_run_boss_defeat_voucher();

    printf("All voucher and egg tests passed successfully!\n");
    return 0;
}
