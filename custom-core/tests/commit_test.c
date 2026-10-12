/* custom-core terminal transaction tests (task C04): cc_commit_terminal on the metadata.
 *
 * Expected values are literals worked out by hand (which id each insertion gets, which record a full roster gives up, what the
 * counters read afterwards). After every commit the state is validated and checked to survive an encode and decode. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cc/codec.h"
#include "builders.h"
#include "check.h"

#define NONCE 0x00000100u
#define UID(seq) (((uint64_t)NONCE << 32) | (seq))

static cc_meta g_meta, g_back;
static cc_history g_hist;
static uint8_t g_a[CC_WIRE_META_MAX + 64], g_b[CC_WIRE_META_MAX + 64];
static size_t g_alen, g_blen;

/* the state as bytes, to compare "unchanged" without reading padding */
static size_t snapshot(const cc_meta *m, uint8_t *buf)
{
    size_t n = 0;
    CHECK_STATUS(cc_meta_encode(m, buf, sizeof g_a, &n), CC_OK);
    return n;
}

static void fresh(void)
{
    cc_meta_init(&g_meta, NONCE);
    g_meta.counters.next_run_seq = 100;       /* runs 1..99 have been issued */
    memset(&g_hist, 0, sizeof g_hist);
    g_hist.n_turns = 1;
    g_hist.turns[0] = mk_turn(1, 1, 1, 2);
}

/* the terminal of run `run`, defeated at wave 27 by enemy pokemon `pid` (species 19), not yet committed */
static cc_terminal term_for(uint64_t run, uint32_t pid)
{
    cc_terminal t = mk_terminal(run, 0);
    t.finish_actor = mk_actor(pid, 19, CC_SIDE_ENEMY);
    return t;
}

static cc_nemesis cand_for(uint64_t run, uint32_t pid)
{
    cc_nemesis n = mk_nem(0, run, pid);
    return n;
}

/* validate, then check the state survives the codec */
static void check_state(const char *what)
{
    cc_status s = cc_meta_validate(&g_meta);
    if (s != CC_OK) printf("  (%s) invalid: %s\n", what, cc_status_str(s));
    CHECK_STATUS(s, CC_OK);
    g_alen = snapshot(&g_meta, g_a);
    CHECK_STATUS(cc_meta_decode(g_a, g_alen, &g_back), CC_OK);
    g_blen = snapshot(&g_back, g_b);
    CHECK(g_alen == g_blen && memcmp(g_a, g_b, g_alen) == 0);
}

static cc_status commit(uint64_t run, uint32_t pid, bool with_cand, cc_commit_result *res)
{
    cc_terminal t = term_for(run, pid);
    cc_nemesis c = cand_for(run, pid);
    return cc_commit_terminal(&g_meta, &t, &g_hist, with_cand ? &c : NULL, res);
}

static void test_first_insertion(void)
{
    fresh();
    cc_commit_result r;
    CHECK_STATUS(commit(UID(1), 900, true, &r), CC_OK);
    CHECK(!r.replayed);
    CHECK_EQ(r.outcome, CC_TXO_INSERTED);
    CHECK_EQ(r.refusal, CC_SUPPORT_OK);
    CHECK_EQ(r.txn_id, 1);
    CHECK_EQ(r.record_id, 1);
    CHECK_EQ(r.overwritten, 0);
    CHECK_EQ(g_meta.roster.count, 1);
    CHECK_EQ(g_meta.roster.rec[0].record_id, 1);
    CHECK_EQ(g_meta.roster.rec[0].life, CC_LIFE_STORED);
    CHECK_EQ(g_meta.roster.rec[0].source_pokemon_id, 900);
    CHECK_EQ(g_meta.roster.rec[0].source_run_uid, UID(1));
    CHECK_EQ(g_meta.counters.next_txn_id, 2);
    CHECK_EQ(g_meta.counters.next_record_id, 2);
    CHECK_EQ(g_meta.counters.next_run_seq, 100);
    CHECK_EQ(g_meta.ledger.n, 1);
    CHECK_EQ(g_meta.ledger.e[0].txn_id, 1);
    CHECK_EQ(g_meta.ledger.e[0].run_uid, UID(1));
    CHECK_EQ(g_meta.ledger.e[0].record_id, 1);
    CHECK_EQ(g_meta.ledger.e[0].kind, CC_TXN_TERMINAL);
    CHECK_EQ(g_meta.ledger.e[0].outcome, CC_TXO_INSERTED);
    CHECK_EQ(g_meta.has_report, 1);
    CHECK_EQ(g_meta.report.txn_id, 1);                   /* the candidate had 0 */
    CHECK_EQ(g_meta.report.run_uid, UID(1));
    CHECK_EQ(g_meta.history.n_turns, 1);
    CHECK_EQ(g_meta.history.turns[0].n_events, 2);
    check_state("first insertion");

    /* a second run: the next record id and txn id, the report replaced */
    g_hist.turns[0] = mk_turn(2, 4, 50, 5);
    CHECK_STATUS(commit(UID(2), 901, true, &r), CC_OK);
    CHECK_EQ(r.record_id, 2);
    CHECK_EQ(r.txn_id, 2);
    CHECK_EQ(g_meta.roster.count, 2);
    CHECK_EQ(g_meta.roster.rec[1].record_id, 2);
    CHECK_EQ(g_meta.report.run_uid, UID(2));
    CHECK_EQ(g_meta.history.turns[0].n_events, 5);
    CHECK_EQ(g_meta.ledger.n, 2);
    check_state("second insertion");
}

static void test_replay(void)
{
    fresh();
    cc_commit_result r;
    CHECK_STATUS(commit(UID(1), 900, true, &r), CC_OK);
    g_alen = snapshot(&g_meta, g_a);
    /* the same run ends again (a retried callback, the result screen reopened, a restart before the screen): nothing changes */
    for (int i = 0; i < 3; i++) {
        CHECK_STATUS(commit(UID(1), 900, true, &r), CC_OK);
        CHECK(r.replayed);
        CHECK_EQ(r.outcome, CC_TXO_INSERTED);
        CHECK_EQ(r.record_id, 1);
        CHECK_EQ(r.txn_id, 1);
        CHECK_EQ(r.refusal, CC_SUPPORT_OK);
        g_blen = snapshot(&g_meta, g_b);
        CHECK(g_alen == g_blen && memcmp(g_a, g_b, g_alen) == 0);
    }
    /* even with a different candidate or none: the run already has its transaction */
    CHECK_STATUS(commit(UID(1), 901, true, &r), CC_OK);
    CHECK(r.replayed);
    CHECK_STATUS(commit(UID(1), 900, false, &r), CC_OK);
    CHECK(r.replayed);
    CHECK_EQ(g_meta.roster.count, 1);
    g_blen = snapshot(&g_meta, g_b);
    CHECK(g_alen == g_blen && memcmp(g_a, g_b, g_alen) == 0);
    CHECK_EQ(g_meta.counters.next_txn_id, 2);
    CHECK_EQ(g_meta.counters.next_record_id, 2);

    /* a report-only run replays too, with no reason (the ledger does not keep it) */
    CHECK_STATUS(commit(UID(2), 901, false, &r), CC_OK);
    CHECK(!r.replayed);
    g_alen = snapshot(&g_meta, g_a);
    CHECK_STATUS(commit(UID(2), 901, false, &r), CC_OK);
    CHECK(r.replayed);
    CHECK_EQ(r.outcome, CC_TXO_REPORT_ONLY);
    CHECK_EQ(r.refusal, -1);
    CHECK_EQ(r.record_id, 0);
    g_blen = snapshot(&g_meta, g_b);
    CHECK(g_alen == g_blen && memcmp(g_a, g_b, g_alen) == 0);
    check_state("replay");
}

static void test_report_only(void)
{
    cc_commit_result r;
    /* no candidate: the finishing enemy is not known; the report is kept with an unknown cause and no record appears */
    fresh();
    cc_terminal t = term_for(UID(1), 900);
    t.known = CC_TK_FALLEN_ACTOR;                        /* what the app can say: the fallen Pokemon only */
    t.finish_move = 0;
    memset(&t.finish_actor, 0, sizeof t.finish_actor);
    CHECK_STATUS(cc_commit_terminal(&g_meta, &t, &g_hist, NULL, &r), CC_OK);
    CHECK_EQ(r.outcome, CC_TXO_REPORT_ONLY);
    CHECK_EQ(r.refusal, CC_REFUSE_NOT_ATTRIBUTED);
    CHECK_EQ(r.record_id, 0);
    CHECK_EQ(r.txn_id, 1);
    CHECK_EQ(g_meta.roster.count, 0);
    CHECK_EQ(g_meta.counters.next_record_id, 1);         /* no record id was spent */
    CHECK_EQ(g_meta.counters.next_txn_id, 2);
    CHECK_EQ(g_meta.has_report, 1);
    CHECK_EQ(g_meta.report.known, CC_TK_FALLEN_ACTOR);
    CHECK_EQ(g_meta.ledger.e[0].outcome, CC_TXO_REPORT_ONLY);
    CHECK_EQ(g_meta.ledger.e[0].record_id, 0);
    check_state("no candidate");

    /* a candidate that is not the finisher (another enemy id) is not inserted */
    fresh();
    t = term_for(UID(1), 900);
    cc_nemesis c = cand_for(UID(1), 777);
    CHECK_STATUS(cc_commit_terminal(&g_meta, &t, &g_hist, &c, &r), CC_OK);
    CHECK_EQ(r.refusal, CC_REFUSE_NOT_ATTRIBUTED);
    CHECK_EQ(g_meta.roster.count, 0);
    check_state("wrong enemy");

    /* a clear is a report, never a record, even with a candidate */
    fresh();
    t = mk_terminal(UID(1), 0);
    t.outcome = CC_OUT_CLEAR;
    t.known = 0;
    t.finish_move = 0;
    memset(&t.finish_actor, 0, sizeof t.finish_actor);
    memset(&t.fallen_actor, 0, sizeof t.fallen_actor);
    c = cand_for(UID(1), 900);
    CHECK_STATUS(cc_commit_terminal(&g_meta, &t, &g_hist, &c, &r), CC_OK);
    CHECK_EQ(r.refusal, CC_REFUSE_NOT_DEFEAT);
    CHECK_EQ(r.outcome, CC_TXO_REPORT_ONLY);
    CHECK_EQ(g_meta.roster.count, 0);
    CHECK_EQ(g_meta.report.outcome, CC_OUT_CLEAR);
    check_state("clear");

    /* abandon and reset (not reachable in the app today) behave the same way */
    static const uint8_t others[] = {CC_OUT_ABANDON, CC_OUT_RESET};
    for (unsigned i = 0; i < 2; i++) {
        fresh();
        t.outcome = others[i];
        CHECK_STATUS(cc_commit_terminal(&g_meta, &t, &g_hist, &c, &r), CC_OK);
        CHECK_EQ(r.refusal, CC_REFUSE_NOT_DEFEAT);
        CHECK_EQ(g_meta.roster.count, 0);
        check_state("abandon/reset");
    }
}

static void test_refusal_table(void)
{
    static const struct { uint8_t trait; int refusal; } traits[] = {
        {CC_TR_BOSS, CC_REFUSE_BOSS}, {CC_TR_FUSED, CC_REFUSE_FUSED}, {CC_TR_TERA, CC_REFUSE_TERA},
        {CC_TR_DOUBLES, CC_REFUSE_DOUBLES}, {CC_TR_TRAINER_OWNED, CC_REFUSE_TRAINER_OWNED},
        {CC_TR_MYSTERY, CC_REFUSE_MYSTERY}, {CC_TR_FIXED_SCRIPTED, CC_REFUSE_FIXED_SCRIPTED}, {CC_TR_REMATCH, CC_REFUSE_REMATCH},
    };
    for (unsigned i = 0; i < sizeof traits / sizeof traits[0]; i++) {
        fresh();
        cc_terminal t = term_for(UID(1), 900);
        cc_nemesis c = cand_for(UID(1), 900);
        c.traits = traits[i].trait;
        cc_commit_result r;
        CHECK_STATUS(cc_commit_terminal(&g_meta, &t, &g_hist, &c, &r), CC_OK);
        CHECK_EQ(r.refusal, traits[i].refusal);
        CHECK_EQ(r.outcome, CC_TXO_REPORT_ONLY);
        CHECK_EQ(g_meta.roster.count, 0);
        CHECK_EQ(g_meta.counters.next_record_id, 1);
        check_state("trait");
    }
    /* unverified finisher, unknown ability, unknown moves */
    static const struct { uint16_t drop; int refusal; } known[] = {
        {CC_NK_FINISHER_VERIFIED, CC_REFUSE_NOT_ATTRIBUTED}, {CC_NK_ABILITY, CC_REFUSE_ABILITY_UNKNOWN},
        {CC_NK_MOVES, CC_REFUSE_MOVES_UNKNOWN},
    };
    for (unsigned i = 0; i < 3; i++) {
        fresh();
        cc_terminal t = term_for(UID(1), 900);
        cc_nemesis c = cand_for(UID(1), 900);
        c.known &= (uint16_t)~known[i].drop;
        if (known[i].drop == CC_NK_ABILITY) c.ability = 0;
        if (known[i].drop == CC_NK_MOVES) { c.n_moves = 0; memset(c.moves, 0, sizeof c.moves); }
        cc_commit_result r;
        CHECK_STATUS(cc_commit_terminal(&g_meta, &t, &g_hist, &c, &r), CC_OK);
        CHECK_EQ(r.refusal, known[i].refusal);
        CHECK_EQ(g_meta.roster.count, 0);
        check_state("known");
    }
}

/* inserts runs 1..n, each a different enemy */
static void insert_runs(unsigned n)
{
    cc_commit_result r;
    for (unsigned i = 1; i <= n; i++) CHECK_STATUS(commit(UID(i), 900 + i, true, &r), CC_OK);
}

static void test_zero_sixteen_seventeen(void)
{
    cc_commit_result r;
    fresh();
    check_state("0 insertions");
    CHECK_EQ(g_meta.roster.count, 0);

    insert_runs(16);
    CHECK_EQ(g_meta.roster.count, 16);
    for (unsigned i = 0; i < 16; i++) CHECK_EQ(g_meta.roster.rec[i].record_id, 1 + i);
    CHECK_EQ(g_meta.counters.next_record_id, 17);
    CHECK_EQ(g_meta.ledger.n, 8);                        /* the ledger keeps the last 8 */
    CHECK_EQ(g_meta.ledger.e[0].run_uid, UID(9));
    CHECK_EQ(g_meta.ledger.e[7].run_uid, UID(16));
    check_state("16 insertions");
    const size_t len16 = g_alen;

    /* the 17th gives up the oldest (record 1); nothing grows */
    CHECK_STATUS(commit(UID(17), 917, true, &r), CC_OK);
    CHECK_EQ(r.record_id, 17);
    CHECK_EQ(r.overwritten, 1);
    CHECK_EQ(g_meta.roster.count, 16);
    for (unsigned i = 0; i < 16; i++) CHECK_EQ(g_meta.roster.rec[i].record_id, 2 + i);   /* ascending, oldest first */
    CHECK_EQ(g_meta.roster.rec[0].source_pokemon_id, 902);
    CHECK_EQ(g_meta.roster.rec[15].source_pokemon_id, 917);
    CHECK_EQ(g_meta.counters.next_record_id, 18);
    check_state("17 insertions");
    CHECK_EQ(g_alen, len16);                             /* the blob is exactly as large as with 16 */

    /* 18th, 19th, ... keep going */
    for (unsigned i = 18; i <= 40; i++) {
        CHECK_STATUS(commit(UID(i), 900 + i, true, &r), CC_OK);
        CHECK_EQ(r.overwritten, i - 16);
    }
    CHECK_EQ(g_meta.roster.count, 16);
    CHECK_EQ(g_meta.roster.rec[0].record_id, 25);
    check_state("40 insertions");
    CHECK_EQ(g_alen, len16);
}

/* moves record `idx` through the lifecycle to `to` (the only mutator) */
static void to_life(unsigned idx, uint8_t to)
{
    cc_nemesis *n = &g_meta.roster.rec[idx];
    if (to >= CC_LIFE_SCHEDULED && n->life == CC_LIFE_STORED) CHECK_STATUS(cc_nemesis_transition(n, CC_LIFE_SCHEDULED, UID(50), 0, 0), CC_OK);
    if (to >= CC_LIFE_CHALLENGED && n->life == CC_LIFE_SCHEDULED) CHECK_STATUS(cc_nemesis_transition(n, CC_LIFE_CHALLENGED, UID(50), 27, 0), CC_OK);
    if (to >= CC_LIFE_DEFEATED && n->life == CC_LIFE_CHALLENGED) CHECK_STATUS(cc_nemesis_transition(n, CC_LIFE_DEFEATED, 0, 0, 0), CC_OK);
    if (to >= CC_LIFE_REWARD_CLAIMED && n->life == CC_LIFE_DEFEATED) CHECK_STATUS(cc_nemesis_transition(n, CC_LIFE_REWARD_CLAIMED, 0, 0, 5), CC_OK);
    CHECK_EQ(n->life, to);
}

static void test_protected_records(void)
{
    cc_commit_result r;
    /* SCHEDULED and CHALLENGED are not given up: the next oldest goes */
    fresh();
    insert_runs(16);
    to_life(0, CC_LIFE_SCHEDULED);
    CHECK_STATUS(commit(UID(17), 917, true, &r), CC_OK);
    CHECK_EQ(r.overwritten, 2);
    CHECK_EQ(g_meta.roster.rec[0].record_id, 1);         /* still there, still first */
    CHECK_EQ(g_meta.roster.rec[0].life, CC_LIFE_SCHEDULED);
    CHECK_EQ(g_meta.roster.rec[1].record_id, 3);
    CHECK_EQ(g_meta.roster.rec[15].record_id, 17);
    check_state("scheduled kept");

    fresh();
    insert_runs(16);
    to_life(0, CC_LIFE_CHALLENGED);
    CHECK_STATUS(commit(UID(17), 917, true, &r), CC_OK);
    CHECK_EQ(r.overwritten, 2);
    check_state("challenged kept");

    /* DEFEATED (reward not claimed) is kept; REWARD_CLAIMED may go, and the oldest eligible is chosen, not the oldest */
    fresh();
    insert_runs(16);
    to_life(0, CC_LIFE_DEFEATED);
    to_life(1, CC_LIFE_DEFEATED);
    to_life(2, CC_LIFE_REWARD_CLAIMED);
    to_life(3, CC_LIFE_REWARD_CLAIMED);
    CHECK_STATUS(commit(UID(17), 917, true, &r), CC_OK);
    CHECK_EQ(r.overwritten, 3);                          /* record 3 = slot 2: the oldest that may be overwritten */
    CHECK_EQ(g_meta.roster.rec[0].record_id, 1);
    CHECK_EQ(g_meta.roster.rec[1].record_id, 2);
    CHECK_EQ(g_meta.roster.rec[2].record_id, 4);
    CHECK_EQ(g_meta.roster.rec[15].record_id, 17);
    check_state("oldest eligible");

    /* every slot protected: the report is kept, no record, no record id spent, the roster is untouched */
    fresh();
    insert_runs(16);
    for (unsigned i = 0; i < 16; i++) to_life(i, CC_LIFE_DEFEATED);
    const cc_txn_id txn_before = g_meta.counters.next_txn_id;
    CHECK_STATUS(commit(UID(17), 917, true, &r), CC_OK);
    CHECK_EQ(r.outcome, CC_TXO_REPORT_ONLY);
    CHECK_EQ(r.refusal, CC_REFUSE_ROSTER_FULL);
    CHECK_EQ(r.record_id, 0);
    CHECK_EQ(r.overwritten, 0);
    CHECK_EQ(g_meta.roster.count, 16);
    CHECK_EQ(g_meta.roster.rec[0].record_id, 1);
    CHECK_EQ(g_meta.roster.rec[15].record_id, 16);
    CHECK_EQ(g_meta.counters.next_record_id, 17);
    CHECK_EQ(g_meta.counters.next_txn_id, txn_before + 1);
    CHECK_EQ(g_meta.report.run_uid, UID(17));
    check_state("all protected");

    /* a full roster of one scheduled record and fifteen defeated: still no slot */
    fresh();
    insert_runs(16);
    for (unsigned i = 1; i < 16; i++) to_life(i, CC_LIFE_DEFEATED);
    to_life(0, CC_LIFE_SCHEDULED);
    CHECK_STATUS(commit(UID(17), 917, true, &r), CC_OK);
    CHECK_EQ(r.refusal, CC_REFUSE_ROSTER_FULL);
    check_state("one scheduled, rest defeated");
}

static void test_distinct_pairs_and_partial_roster(void)
{
    cc_commit_result r;
    /* (run, pokemon) is the identity: the same run with another enemy, and the same enemy id in another run, are both new */
    fresh();
    CHECK_STATUS(commit(UID(1), 900, true, &r), CC_OK);
    g_meta.ledger.n = 0;                                 /* the ledger forgot run 1, so it is not a replay */
    CHECK_STATUS(commit(UID(1), 901, true, &r), CC_OK);
    CHECK_EQ(r.outcome, CC_TXO_INSERTED);
    CHECK_STATUS(commit(UID(2), 900, true, &r), CC_OK);
    CHECK_EQ(r.outcome, CC_TXO_INSERTED);
    CHECK_EQ(g_meta.roster.count, 3);
    check_state("distinct pairs");

    /* a roster with a free slot never refuses for lack of room, even if everything in it is protected */
    fresh();
    insert_runs(15);
    for (unsigned i = 0; i < 15; i++) to_life(i, CC_LIFE_DEFEATED);
    CHECK_STATUS(commit(UID(16), 916, true, &r), CC_OK);
    CHECK_EQ(r.outcome, CC_TXO_INSERTED);
    CHECK_EQ(r.overwritten, 0);
    CHECK_EQ(g_meta.roster.count, 16);
    check_state("fifteen protected");
}

/* every error leaves the state exactly as it was */
static void expect_unchanged(cc_status want, uint64_t run, const cc_terminal *t, const cc_history *h, const cc_nemesis *c)
{
    (void)run;
    static cc_meta before;
    memcpy(&before, &g_meta, sizeof before);              /* raw bytes: an error must not write anything, padding included */
    cc_commit_result r;
    memset(&r, 0x77, sizeof r);
    CHECK_STATUS(cc_commit_terminal(&g_meta, t, h, c, &r), want);
    CHECK(memcmp(&before, &g_meta, sizeof before) == 0);
    CHECK(!r.replayed && r.record_id == 0 && r.txn_id == 0);   /* the result is cleared, never left half-filled */
}

static void test_errors_change_nothing(void)
{
    fresh();
    insert_runs(3);
    cc_terminal t = term_for(UID(10), 950);
    cc_nemesis c = cand_for(UID(10), 950);
    cc_commit_result r;

    CHECK_STATUS(cc_commit_terminal(NULL, &t, &g_hist, &c, &r), CC_E_NULL);
    CHECK_STATUS(cc_commit_terminal(&g_meta, NULL, &g_hist, &c, &r), CC_E_NULL);
    CHECK_STATUS(cc_commit_terminal(&g_meta, &t, NULL, &c, &r), CC_E_NULL);
    CHECK_STATUS(cc_commit_terminal(&g_meta, &t, &g_hist, &c, NULL), CC_E_NULL);

    cc_terminal bad = t;
    bad.txn_id = 5;                                      /* a candidate carries none */
    expect_unchanged(CC_E_RANGE, 10, &bad, &g_hist, &c);
    bad = t; bad.run_uid = 0;
    expect_unchanged(CC_E_RANGE, 0, &bad, &g_hist, &c);
    bad = t; bad.outcome = CC_OUT_NONE;
    expect_unchanged(CC_E_RANGE, 10, &bad, &g_hist, &c);
    bad = t; bad.wave = 0;
    expect_unchanged(CC_E_RANGE, 10, &bad, &g_hist, &c);
    bad = t; bad.run_uid = UID(100);                     /* never issued: the counter says runs 1..99 exist */
    expect_unchanged(CC_E_CONSISTENCY, 100, &bad, &g_hist, &c);
    bad = t; bad.run_uid = ((uint64_t)(NONCE + 1) << 32) | 5;
    expect_unchanged(CC_E_CONSISTENCY, 5, &bad, &g_hist, &c);

    cc_history bh = g_hist;
    bh.turns[0].events[1].seq = bh.turns[0].events[0].seq;   /* a duplicate sequence number */
    expect_unchanged(CC_E_DUPLICATE, 10, &t, &bh, &c);
    bh = g_hist; bh.n_turns = 6;
    expect_unchanged(CC_E_COUNT, 10, &t, &bh, &c);

    cc_nemesis bc = c;
    bc.record_id = 7;                                    /* the id is the store's to give */
    expect_unchanged(CC_E_RANGE, 10, &t, &g_hist, &bc);
    bc = c; bc.species = 0;
    expect_unchanged(CC_E_RANGE, 10, &t, &g_hist, &bc);
    bc = c; bc.life = CC_LIFE_SCHEDULED; bc.sched_run_uid = UID(9);
    expect_unchanged(CC_E_LIFECYCLE, 10, &t, &g_hist, &bc);
    bc = c; bc.known |= CC_NK_ALL + 1;
    expect_unchanged(CC_E_UNKNOWN_BITS, 10, &t, &g_hist, &bc);
    bc = c; bc.n_moves = 2; bc.moves[1] = bc.moves[0];
    expect_unchanged(CC_E_DUPLICATE, 10, &t, &g_hist, &bc);
    /* the same enemy of the same run already on the roster, but under a different run's ledger entry */
    cc_terminal t2 = term_for(UID(1), 901);              /* run 1 is already in the ledger: this is a replay, not an error */
    CHECK_STATUS(cc_commit_terminal(&g_meta, &t2, &g_hist, &c, &r), CC_OK);
    CHECK(r.replayed);
    g_meta.ledger.n = 0;                                 /* the ledger forgot run 1 (it holds 8 runs; this is the 9th-oldest) */
    cc_terminal t3 = term_for(UID(1), 901);
    cc_nemesis c3 = cand_for(UID(1), 901);               /* record 1's twin: same run, same enemy */
    expect_unchanged(CC_E_DUPLICATE, 1, &t3, &g_hist, &c3);

    /* the counters running out: txn ids, record ids */
    fresh();
    insert_runs(2);
    g_meta.counters.next_txn_id = UINT64_MAX;
    g_meta.ledger.n = 0; g_meta.has_report = 0;
    memset(&g_meta.report, 0, sizeof g_meta.report); memset(&g_meta.history, 0, sizeof g_meta.history);
    t = term_for(UID(10), 950); c = cand_for(UID(10), 950);
    expect_unchanged(CC_E_OVERFLOW, 10, &t, &g_hist, &c);
    expect_unchanged(CC_E_OVERFLOW, 10, &t, &g_hist, NULL);
    g_meta.counters.next_txn_id = 50;
    g_meta.counters.next_record_id = UINT32_MAX;
    g_meta.roster.count = 0;
    expect_unchanged(CC_E_OVERFLOW, 10, &t, &g_hist, &c);
    CHECK_STATUS(cc_commit_terminal(&g_meta, &t, &g_hist, NULL, &r), CC_OK);     /* a report-only commit needs no record id */
    CHECK_EQ(r.outcome, CC_TXO_REPORT_ONLY);

    /* a stored state that is not valid is refused before anything is read from it */
    fresh();
    insert_runs(2);
    g_meta.roster.rec[1].record_id = 1;
    t = term_for(UID(10), 950);
    expect_unchanged(CC_E_DUPLICATE, 10, &t, &g_hist, NULL);
}

/* a long random run of ends, with lifecycle moves in between: invariants hold after every step */
static uint32_t g_rng = 0x9E3779B9u;
static uint32_t rnd(void)
{
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 17;
    g_rng ^= g_rng << 5;
    return g_rng;
}

static void test_random_sequence(void)
{
    fresh();
    g_meta.counters.next_run_seq = 20000;
    unsigned inserted = 0, report_only = 0, replays = 0, full_refused = 0, overwrites = 0;
    uint32_t run = 0;
    for (int step = 0; step < 6000; step++) {
        const unsigned dice = rnd() % 10;
        if (dice < 4 && g_meta.roster.count) {           /* move a random record forward, within the rules */
            /* the first record that has not been defeated yet moves on; failing that, a random one (a claim now and then) */
            cc_nemesis *n = &g_meta.roster.rec[rnd() % g_meta.roster.count];
            for (unsigned i = 0; i < g_meta.roster.count; i++)
                if (g_meta.roster.rec[i].life < CC_LIFE_DEFEATED) { n = &g_meta.roster.rec[i]; break; }
            int active = 0;
            for (unsigned i = 0; i < g_meta.roster.count; i++)
                active += g_meta.roster.rec[i].life == CC_LIFE_SCHEDULED || g_meta.roster.rec[i].life == CC_LIFE_CHALLENGED;
            switch (n->life) {
            case CC_LIFE_STORED: if (!active) cc_nemesis_transition(n, CC_LIFE_SCHEDULED, UID(1), 0, 0); break;
            case CC_LIFE_SCHEDULED: cc_nemesis_transition(n, CC_LIFE_CHALLENGED, n->sched_run_uid, 77, 0); break;
            case CC_LIFE_CHALLENGED: cc_nemesis_transition(n, rnd() % 4 ? CC_LIFE_DEFEATED : CC_LIFE_STORED, 0, 0, 0); break;
            case CC_LIFE_DEFEATED: if (rnd() % 12 == 0) cc_nemesis_transition(n, CC_LIFE_REWARD_CLAIMED, 0, 0, 1 + rnd() % 3); break;
            default: break;
            }
            continue;
        }
        if (dice == 4 && run) {                          /* the same recent run again (the ledger holds the last 8 ends) */
            cc_commit_result r;
            const uint32_t back = run > 8 ? 8 : run;
            const uint32_t again = run - rnd() % back;
            const cc_status s = commit(UID(again), 5000 + again, true, &r);
            CHECK_STATUS(s, CC_OK);
            CHECK(r.replayed);                                /* every one of the last 8 ends is still remembered */
            replays++;
            continue;
        }
        run++;
        cc_commit_result r;
        const bool attributed = rnd() % 4 != 0;
        cc_terminal t = term_for(UID(run), 5000 + run);
        cc_nemesis c = cand_for(UID(run), 5000 + run);
        if (rnd() % 7 == 0) c.traits = (uint8_t)(1u << (rnd() % 8));
        CHECK_STATUS(cc_commit_terminal(&g_meta, &t, &g_hist, attributed ? &c : NULL, &r), CC_OK);
        CHECK(!r.replayed);
        if (r.outcome == CC_TXO_INSERTED) {
            inserted++;
            if (r.overwritten) overwrites++;
        } else {
            report_only++;
            if (r.refusal == CC_REFUSE_ROSTER_FULL) full_refused++;
        }
        if (step % 25 == 0) check_state("random");
        else CHECK_STATUS(cc_meta_validate(&g_meta), CC_OK);
    }
    check_state("random end");
    CHECK(inserted > 200 && report_only > 500 && replays > 100 && overwrites > 100 && full_refused > 100);
    CHECK_EQ(g_meta.roster.count <= 16, 1);
    printf("random sequence: %u inserted (%u overwrote), %u report only (%u roster full), %u replays\n", inserted, overwrites,
           report_only, full_refused, replays);
}

int main(void)
{
    test_first_insertion();
    test_replay();
    test_report_only();
    test_refusal_table();
    test_zero_sixteen_seventeen();
    test_protected_records();
    test_distinct_pairs_and_partial_roster();
    test_errors_change_nothing();
    test_random_sequence();
    return cc_summary("commit_test");
}
