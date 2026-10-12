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

/* The trainer's own rewards (config.modifierRewardFuncs, upstream 8555c08 trainer-config.ts): RIVAL gives a Super Exp.
 * Charm then an Exp. Share, RIVAL_2 an Exp. Share, RIVAL_4 a Tera Orb. Each is a ModifierRewardPhase run through
 * addModifier: PersistentModifier.add stacks it and incrementStack refuses past getMaxStackCount() (Exp. Share 5;
 * addModifier then queues itemStackFull and gives the COMMON tier's default item, 5 Poke Balls); the Tera Orb has no match(), so it is added again each time with no cap. */
static int defeat_trainer_rewards(gc_run *run, int trainer_type, const char *keys[4]) {
    gc_internal *in = gc_int(run);
    int n = 0;
    gc_rq_reset(&in->rq);
    run->trainer_type = trainer_type;
    gc_phase_start(run, &(gc_rphase){.type = RP_TRAINER_VICTORY});
    gc_rphase q;
    while (gc_rq_next(&in->rq, &q)) {
        if (q.type == RP_MODIFIER_REWARD && q.key && strncmp(q.key, "VOUCHER", 7)) {
            assert(n < 4);
            keys[n++] = q.key;
            gc_phase_start(run, &q);
        }
    }
    return n;
}

static void test_trainer_reward_items(void) {
    static gc_system sys;
    static gc_run run;
    const char *keys[4];
    new_run(&run, &sys);

    assert(defeat_trainer_rewards(&run, TRAINER_TYPE_RIVAL, keys) == 2);
    assert(!strcmp(keys[0], "SUPER_EXP_CHARM") && !strcmp(keys[1], "EXP_SHARE"));
    assert(gc_item_stack(&run, "SUPER_EXP_CHARM") == 1 && gc_item_stack(&run, "EXP_SHARE") == 1);
    assert(defeat_trainer_rewards(&run, TRAINER_TYPE_RIVAL_2, keys) == 1 && !strcmp(keys[0], "EXP_SHARE"));
    assert(gc_item_stack(&run, "EXP_SHARE") == 2);
    for (int i = 0; i < 6; i++) defeat_trainer_rewards(&run, TRAINER_TYPE_RIVAL_2, keys);
    assert(gc_item_stack(&run, "EXP_SHARE") == 5);          /* getMaxStackCount(): the sixth is refused */
    assert(run.pokeballs[POKEBALL_TYPE_POKEBALL] == 15);    /* ...and itemStackFull gives 5 Poke Balls each time (3 times) */
    assert(gc_item_stack(&run, "SUPER_EXP_CHARM") == 1);

    assert(defeat_trainer_rewards(&run, TRAINER_TYPE_RIVAL_4, keys) == 1 && !strcmp(keys[0], "TERA_ORB"));
    assert(gc_item_stack(&run, "TERA_ORB") == 1);
    defeat_trainer_rewards(&run, TRAINER_TYPE_RIVAL_4, keys);
    assert(gc_item_stack(&run, "TERA_ORB") == 2);           /* a modifier of its own each time */
    assert(run.stubs[GC_STUB_TRAINER_REWARDS] == 0);

    /* a trainer without reward funcs gives none, and the voucher rule is untouched */
    assert(defeat_trainer_rewards(&run, TRAINER_TYPE_BROCK, keys) == 0);
    printf("test_trainer_reward_items... ok\n");
}

/* A voucher taken from the reward screen (SelectModifierPhase.applyModifier -> addModifier): AddVoucherModifier.apply adds
 * one to the count of its type. No text, no run item, not a stand-in. */
static void test_voucher_reward_screen(void) {
    static gc_system sys;
    static gc_run run;
    new_run(&run, &sys);
    const char *keys[] = {"VOUCHER", "VOUCHER_PLUS", "VOUCHER_PREMIUM", "VOUCHER_PLUS"};
    const int types[] = {GC_VOUCHER_REGULAR, GC_VOUCHER_PLUS, GC_VOUCHER_PREMIUM, GC_VOUCHER_PLUS};
    uint32_t want[4] = {0, 0, 0, 0};
    for (int i = 0; i < 4; i++) {
        gc_offer o;
        memset(&o, 0, sizeof o);
        o.key = keys[i];
        o.arg = -1;
        o.target = gc_reward_target(keys[i]);
        const char *why = "";
        assert(gc_port_apply_reward(&run, &o, -1, -1, &why));
        want[types[i]]++;
        assert(!memcmp(sys.voucher_counts, want, sizeof want));
    }
    assert(run.n_items == 0);
    assert(run.stubs[GC_STUB_REWARD_APPLY] == 0);
    gc_event ev;
    while (gc_run_next_event(&run, &ev)) assert(ev.kind != GC_EV_MESSAGE);

    /* no system data: taken, nothing counted, nothing dereferenced */
    run.sys = NULL;
    gc_offer o = {.key = "VOUCHER", .arg = -1, .target = gc_reward_target("VOUCHER")};
    const char *why = "";
    assert(gc_port_apply_reward(&run, &o, -1, -1, &why));
    assert(run.n_items == 0);
    printf("test_voucher_reward_screen... ok\n");
}

int main(void) {
    test_egg_hatch_steps();
    test_voucher_unlocks();
    test_run_boss_defeat_voucher();
    test_voucher_reward_phase();
    test_trainer_reward_items();
    test_voucher_reward_screen();

    printf("All voucher and egg tests passed successfully!\n");
    return 0;
}
