/* Tests of the rematch policy (task C05): the custom stream, the draw, the windows, the answers, the lifecycle moves and the
 * once-per-record reward. Expected values are literals chosen by hand, or printed by tools/golden_rematch.py (written separately
 * from the C code). Every mutating call that fails is also checked to have changed nothing, byte for byte. */
#include <stdlib.h>

#include "builders.h"
#include "cc/scheduler.h"
#include "check.h"

/* ------------------------------------------------------------------ fixtures */

static cc_roster R;
static cc_schedule S;

#define RUN_A 0x0000000500000001ull
#define RUN_B 0x0000000500000002ull

/* n records with the given record ids (strictly ascending), all STORED; their enemies' sources are distinct */
static void make_roster(cc_roster *r, unsigned n, const cc_record_id *ids)
{
    memset(r, 0, sizeof *r);
    r->count = (uint8_t)n;
    for (unsigned i = 0; i < n; i++) r->rec[i] = mk_nem(ids[i], 0x0000000500000100ull + i, 1000u + i);
}

static void make_sequential(cc_roster *r, unsigned n)
{
    cc_record_id ids[CC_MAX_NEMESES];
    for (unsigned i = 0; i < n; i++) ids[i] = (cc_record_id)(3 + 2 * i);       /* 3, 5, 7 ... 33: not contiguous, not from 1 */
    make_roster(r, n, ids);
}

/* The slot of a record that must be in the roster (a miss is a failed check and slot 0, never an out-of-range index). */
static unsigned slot_of(const cc_roster *r, cc_record_id id)
{
    for (unsigned i = 0; i < r->count; i++)
        if (r->rec[i].record_id == id) return i;
    CHECK(!"record is in the roster");
    return 0;
}

static unsigned held(const cc_roster *r)
{
    unsigned n = 0;
    for (unsigned i = 0; i < r->count; i++)
        if (r->rec[i].life == CC_LIFE_SCHEDULED || r->rec[i].life == CC_LIFE_CHALLENGED) n++;
    return n;
}

static cc_encounter enc(uint16_t wave, uint8_t kind, uint8_t flags)
{
    cc_encounter e;
    e.wave = wave;
    e.kind = kind;
    e.flags = flags;
    return e;
}

static cc_schedule g_s_before;
static cc_roster g_r_before;
static void snap(void)
{
    g_s_before = S;
    g_r_before = R;
}
static bool unchanged(void)
{
    return memcmp(&g_s_before, &S, sizeof S) == 0 && memcmp(&g_r_before, &R, sizeof R) == 0;
}

/* a call that must fail with `want` and leave the schedule and the roster byte for byte as they were */
#define REFUSED(call, want) \
    do { \
        snap(); \
        CHECK_STATUS((call), (want)); \
        CHECK(unchanged()); \
    } while (0)

static void both_valid(void)
{
    CHECK_STATUS(cc_schedule_validate(&S), CC_OK);
    CHECK_STATUS(cc_roster_validate(&R), CC_OK);
    CHECK(held(&R) <= 1);
}

/* ------------------------------------------------------------------ the stream */

static void test_stream(void)
{
    /* the first outputs of SplitMix64 for seed 0 and seed 1234567, as printed by tools/golden_rematch.py (and as published) */
    static const uint64_t seed0[5] = {0xE220A8397B1DCDAFull, 0x6E789E6AA1B965F4ull, 0x06C45D188009454Full, 0xF88BB8A8724C81ECull,
                                      0x1B39896A51A8749Bull};
    static const uint64_t seed1234567[5] = {6457827717110365317ull, 3203168211198807973ull, 9817491932198370423ull,
                                            4593380528125082431ull, 16408922859458223821ull};
    uint64_t st = 0;
    for (int i = 0; i < 5; i++) CHECK(cc_rng_v1_next(&st) == seed0[i]);
    CHECK(st == 5 * 0x9E3779B97F4A7C15ull);
    st = 1234567;
    for (int i = 0; i < 5; i++) CHECK(cc_rng_v1_next(&st) == seed1234567[i]);

    /* seed, state after one draw, first output, and output % n for n = 1..16 */
    static const struct {
        uint64_t seed, after, first;
        uint8_t idx[16];
    } G[] = {
        {0x0000000000000000ULL, 0x9E3779B97F4A7C15ULL, 0xE220A8397B1DCDAFULL, {0, 1, 1, 3, 0, 1, 2, 7, 7, 5, 1, 7, 9, 9, 10, 15}},
        {0x0000000000000001ULL, 0x9E3779B97F4A7C16ULL, 0x910A2DEC89025CC1ULL, {0, 1, 2, 1, 0, 5, 2, 1, 5, 5, 9, 5, 6, 9, 5, 1}},
        {0x0000000000000002ULL, 0x9E3779B97F4A7C17ULL, 0x975835DE1C9756CEULL, {0, 0, 1, 2, 0, 4, 4, 6, 4, 0, 6, 10, 8, 4, 10, 14}},
        {0x0000000000003039ULL, 0x9E3779B97F4AAC4EULL, 0x22118258A9D111A0ULL, {0, 0, 2, 0, 4, 2, 5, 0, 8, 4, 3, 8, 1, 12, 14, 0}},
        {0x0123456789ABCDEFULL, 0x9F5ABF2108F64A04ULL, 0x157A3807A48FAA9DULL, {0, 1, 0, 1, 0, 3, 2, 5, 0, 5, 3, 9, 1, 9, 0, 13}},
        {0xFFFFFFFFFFFFFFFFULL, 0x9E3779B97F4A7C14ULL, 0xE4D971771B652C20ULL, {0, 0, 2, 0, 1, 2, 0, 0, 8, 6, 1, 8, 4, 0, 11, 0}},
    };
    for (unsigned g = 0; g < sizeof G / sizeof G[0]; g++) {
        st = G[g].seed;
        CHECK(cc_rng_v1_next(&st) == G[g].first);
        CHECK(st == G[g].after);
        for (unsigned n = 1; n <= 16; n++) {
            st = G[g].seed;
            CHECK_EQ(cc_rng_v1_pick(&st, n), G[g].idx[n - 1]);
            CHECK(st == G[g].after);                       /* one pick is exactly one step of the stream */
        }
    }
    st = 77;
    CHECK_EQ(cc_rng_v1_pick(&st, 0), 0);                   /* no range: 0, never a division by zero */
}

/* ------------------------------------------------------------------ encounters */

static void test_classify(void)
{
    static const struct { uint8_t kind, flags, want; } T[] = {
        {CC_ENC_ORDINARY_WILD, 0, CC_SKIP_NONE},
        {CC_ENC_ORDINARY_WILD, CC_ENF_FUSED, CC_SKIP_UNSUPPORTED},
        {CC_ENC_ORDINARY_WILD, CC_ENF_TERA, CC_SKIP_UNSUPPORTED},
        {CC_ENC_ORDINARY_WILD, CC_ENF_DOUBLES, CC_SKIP_UNSUPPORTED},
        {CC_ENC_ORDINARY_WILD, CC_ENF_ALL, CC_SKIP_UNSUPPORTED},
        {CC_ENC_TRAINER, 0, CC_SKIP_TRAINER},
        {CC_ENC_TRAINER, CC_ENF_DOUBLES, CC_SKIP_TRAINER},
        {CC_ENC_FIXED_OR_SCRIPTED, 0, CC_SKIP_FIXED_OR_SCRIPTED},
        {CC_ENC_MYSTERY, 0, CC_SKIP_MYSTERY},
        {CC_ENC_BOSS, 0, CC_SKIP_BOSS},
        {CC_ENC_BOSS, CC_ENF_TERA, CC_SKIP_BOSS},
        {CC_ENC_FINAL, 0, CC_SKIP_FINAL},
        {CC_ENC_FINAL, CC_ENF_FUSED, CC_SKIP_FINAL},
        {CC_ENC_UNSUPPORTED, 0, CC_SKIP_UNSUPPORTED},
    };
    for (unsigned i = 0; i < sizeof T / sizeof T[0]; i++) {
        uint8_t reason = 0xEE;
        const cc_encounter e = enc(27, T[i].kind, T[i].flags);
        CHECK_STATUS(cc_encounter_classify(&e, &reason), CC_OK);
        CHECK_EQ(reason, T[i].want);
    }
    uint8_t reason = 0xEE;
    cc_encounter e = enc(27, CC_ENC_COUNT_, 0);
    CHECK_STATUS(cc_encounter_classify(&e, &reason), CC_E_RANGE);
    e = enc(27, CC_ENC_ORDINARY_WILD, 8);
    CHECK_STATUS(cc_encounter_classify(&e, &reason), CC_E_RANGE);
    e = enc(27, 255, 0);
    CHECK_STATUS(cc_encounter_classify(&e, &reason), CC_E_RANGE);
    CHECK_EQ(reason, 0xEE);                                // nothing is written on an error
    CHECK_STATUS(cc_encounter_classify(NULL, &reason), CC_E_NULL);
    CHECK_STATUS(cc_encounter_classify(&e, NULL), CC_E_NULL);
}

/* ------------------------------------------------------------------ the draw at the start of a run */

static void test_start_no_candidate(void)
{
    cc_roster r0;
    make_roster(&r0, 0, NULL);
    R = r0;
    memset(&S, 0x5A, sizeof S);
    CHECK_STATUS(cc_rematch_start(&S, &R, RUN_A, 12345, NULL), CC_OK);
    CHECK_EQ(S.state, CC_SCHED_NO_CANDIDATE);
    CHECK(S.run_uid == RUN_A && S.n_frozen == 0 && S.chosen_id == 0 && S.rng_algo == CC_RNG_ALGO_NONE && S.rng_seed == 0 &&
          S.rng_state == 0 && S.n_windows == 0);
    CHECK(memcmp(&R, &r0, sizeof R) == 0);
    both_valid();

    /* every window answers "no candidate" and nothing moves */
    for (int i = 0; i < CC_NUM_WINDOWS; i++) {
        cc_proposal p;
        const cc_encounter e = enc(cc_window_waves[i], CC_ENC_ORDINARY_WILD, 0);
        cc_schedule before = S;
        CHECK_STATUS(cc_rematch_window(&S, &R, &e, &p), CC_OK);
        CHECK_EQ(p.kind, CC_PROP_NONE);
        CHECK_EQ(p.why, CC_WHY_NO_CANDIDATE);
        CHECK_EQ(p.opportunity, i + 1);
        CHECK(p.record_id == 0 && p.reward_vouchers == 0 && p.level_rule == 0);
        CHECK(memcmp(&before, &S, sizeof S) == 0);
    }

    /* every record is in some other state: still no candidate */
    cc_record_id ids[3] = {1, 2, 3};
    make_roster(&R, 3, ids);
    R.rec[0].life = CC_LIFE_DEFEATED;
    R.rec[0].sched_run_uid = RUN_B;
    R.rec[0].challenge_wave = 77;
    R.rec[1].life = CC_LIFE_REWARD_CLAIMED;
    R.rec[1].sched_run_uid = RUN_B;
    R.rec[1].challenge_wave = 127;
    R.rec[1].reward_txn = 4;
    R.rec[2].life = CC_LIFE_DEFEATED;
    R.rec[2].sched_run_uid = RUN_B;
    R.rec[2].challenge_wave = 27;
    CHECK_STATUS(cc_roster_validate(&R), CC_OK);
    cc_roster r3 = R;
    CHECK_STATUS(cc_rematch_start(&S, &R, RUN_A, 999, NULL), CC_OK);
    CHECK_EQ(S.state, CC_SCHED_NO_CANDIDATE);
    CHECK(memcmp(&R, &r3, sizeof R) == 0);
}

static void test_start_draws(void)
{
    static const uint64_t seeds[6] = {0x0ull, 0x1ull, 0x2ull, 0x3039ull, 0x0123456789ABCDEFull, 0xFFFFFFFFFFFFFFFFull};
    static const uint8_t idx[6][16] = {
        {0, 1, 1, 3, 0, 1, 2, 7, 7, 5, 1, 7, 9, 9, 10, 15},   {0, 1, 2, 1, 0, 5, 2, 1, 5, 5, 9, 5, 6, 9, 5, 1},
        {0, 0, 1, 2, 0, 4, 4, 6, 4, 0, 6, 10, 8, 4, 10, 14},  {0, 0, 2, 0, 4, 2, 5, 0, 8, 4, 3, 8, 1, 12, 14, 0},
        {0, 1, 0, 1, 0, 3, 2, 5, 0, 5, 3, 9, 1, 9, 0, 13},    {0, 0, 2, 0, 1, 2, 0, 0, 8, 6, 1, 8, 4, 0, 11, 0},
    };
    static const uint64_t after[6] = {0x9E3779B97F4A7C15ull, 0x9E3779B97F4A7C16ull, 0x9E3779B97F4A7C17ull,
                                      0x9E3779B97F4AAC4Eull, 0x9F5ABF2108F64A04ull, 0x9E3779B97F4A7C14ull};
    unsigned distinct_for_16 = 0;
    bool seen[CC_MAX_NEMESES] = {false};
    for (unsigned g = 0; g < 6; g++) {
        for (unsigned n = 1; n <= CC_MAX_NEMESES; n++) {
            make_sequential(&R, n);
            cc_roster r0 = R;
            CHECK_STATUS(cc_rematch_start(&S, &R, RUN_A, seeds[g], NULL), CC_OK);
            const cc_record_id want = (cc_record_id)(3 + 2 * idx[g][n - 1]);
            CHECK_EQ(S.state, CC_SCHED_ACTIVE);
            CHECK_EQ(S.chosen_id, want);
            CHECK_EQ(S.n_frozen, n);
            for (unsigned i = 0; i < n; i++) CHECK_EQ(S.frozen_ids[i], 3 + 2 * i);
            CHECK(S.rng_algo == CC_RNG_ALGO_V1 && S.rng_seed == seeds[g] && S.rng_state == after[g] && S.run_uid == RUN_A);
            CHECK_EQ(S.n_windows, CC_NUM_WINDOWS);
            for (int w = 0; w < CC_NUM_WINDOWS; w++) {
                CHECK(S.windows[w].wave == cc_window_waves[w] && S.windows[w].state == CC_WIN_PENDING && S.windows[w].reason == 0);
            }
            both_valid();
            /* exactly the chosen record changed: STORED -> SCHEDULED by this run */
            for (unsigned i = 0; i < n; i++) {
                if (r0.rec[i].record_id == want) {
                    CHECK_EQ(R.rec[i].life, CC_LIFE_SCHEDULED);
                    CHECK(R.rec[i].sched_run_uid == RUN_A && R.rec[i].challenge_wave == 0);
                    cc_nemesis back = R.rec[i];
                    back.life = CC_LIFE_STORED;
                    back.sched_run_uid = 0;
                    CHECK(memcmp(&back, &r0.rec[i], sizeof back) == 0);
                } else {
                    CHECK(memcmp(&R.rec[i], &r0.rec[i], sizeof R.rec[i]) == 0);
                }
            }
            if (n == CC_MAX_NEMESES) {
                if (!seen[idx[g][n - 1]]) distinct_for_16++;
                seen[idx[g][n - 1]] = true;
            }
        }
    }
    CHECK(distinct_for_16 >= 5);                           // different seeds choose different records

    /* the same inputs make the same schedule, byte for byte (nothing reads a clock or ambient random state) */
    static cc_schedule s1, s2;
    static cc_roster r1, r2;
    make_sequential(&r1, 9);
    r2 = r1;
    memset(&s1, 0xA5, sizeof s1);
    memset(&s2, 0x3C, sizeof s2);
    CHECK_STATUS(cc_rematch_start(&s1, &r1, RUN_A, 4242, NULL), CC_OK);
    CHECK_STATUS(cc_rematch_start(&s2, &r2, RUN_A, 4242, NULL), CC_OK);
    CHECK(memcmp(&s1, &s2, sizeof s1) == 0 && memcmp(&r1, &r2, sizeof r1) == 0);
}

static void test_start_eligibility(void)
{
    /* only STORED records are candidates, whatever else the roster holds */
    cc_record_id ids[6] = {10, 11, 12, 13, 14, 15};
    make_roster(&R, 6, ids);
    R.rec[1].life = CC_LIFE_DEFEATED;
    R.rec[1].sched_run_uid = RUN_B;
    R.rec[1].challenge_wave = 27;
    R.rec[3].life = CC_LIFE_REWARD_CLAIMED;
    R.rec[3].sched_run_uid = RUN_B;
    R.rec[3].challenge_wave = 77;
    R.rec[3].reward_txn = 2;
    CHECK_STATUS(cc_roster_validate(&R), CC_OK);
    for (uint64_t seed = 0; seed < 40; seed++) {
        cc_roster r = R;
        CHECK_STATUS(cc_rematch_start(&S, &r, RUN_A, seed, NULL), CC_OK);
        CHECK_EQ(S.n_frozen, 4);
        CHECK(S.frozen_ids[0] == 10 && S.frozen_ids[1] == 12 && S.frozen_ids[2] == 14 && S.frozen_ids[3] == 15);
        CHECK(S.chosen_id == 10 || S.chosen_id == 12 || S.chosen_id == 14 || S.chosen_id == 15);
        CHECK(r.rec[slot_of(&r, S.chosen_id)].life == CC_LIFE_SCHEDULED);
        CHECK(r.rec[1].life == CC_LIFE_DEFEATED && r.rec[3].life == CC_LIFE_REWARD_CLAIMED);
    }

    /* a snapshot the adapter cannot rebuild stays out of the candidates and is never silently reshaped */
    make_sequential(&R, 8);
    uint8_t rep[8] = {1, 0, 1, 1, 0, 1, 1, 1};
    cc_roster r = R;
    CHECK_STATUS(cc_rematch_start(&S, &r, RUN_A, 7, rep), CC_OK);
    CHECK_EQ(S.n_frozen, 6);
    static const cc_record_id want[6] = {3, 7, 9, 13, 15, 17};
    for (unsigned i = 0; i < 6; i++) CHECK_EQ(S.frozen_ids[i], want[i]);
    uint64_t st = 7;
    const unsigned pick = cc_rng_v1_pick(&st, 6);
    CHECK_EQ(S.chosen_id, want[pick]);
    uint8_t none[8] = {0};
    r = R;
    CHECK_STATUS(cc_rematch_start(&S, &r, RUN_A, 7, none), CC_OK);
    CHECK_EQ(S.state, CC_SCHED_NO_CANDIDATE);
    CHECK(memcmp(&r, &R, sizeof R) == 0);
}

static void test_start_refusals(void)
{
    make_sequential(&R, 4);
    memset(&S, 0x11, sizeof S);
    snap();
    CHECK_STATUS(cc_rematch_start(NULL, &R, RUN_A, 1, NULL), CC_E_NULL);
    CHECK_STATUS(cc_rematch_start(&S, NULL, RUN_A, 1, NULL), CC_E_NULL);
    CHECK(unchanged());
    REFUSED(cc_rematch_start(&S, &R, 0, 1, NULL), CC_E_RANGE);
    {
        const cc_roster four = R;
        make_sequential(&R, 0);                             // no candidate either: uid 0 is still refused, nothing is started
        REFUSED(cc_rematch_start(&S, &R, 0, 1, NULL), CC_E_RANGE);
        R = four;
    }

    cc_roster good = R;
    R.rec[2].record_id = R.rec[1].record_id;               // a duplicate id: the roster is not valid
    snap();
    CHECK_STATUS(cc_rematch_start(&S, &R, RUN_A, 1, NULL), CC_E_DUPLICATE);
    CHECK(unchanged());
    R = good;
    R.count = CC_MAX_NEMESES + 1;
    snap();
    CHECK_STATUS(cc_rematch_start(&S, &R, RUN_A, 1, NULL), CC_E_COUNT);
    CHECK(unchanged());

    /* a record still held by a run (any run, even this one) must be released first */
    R = good;
    R.rec[1].life = CC_LIFE_SCHEDULED;
    R.rec[1].sched_run_uid = RUN_B;
    REFUSED(cc_rematch_start(&S, &R, RUN_A, 1, NULL), CC_E_CONSISTENCY);
    REFUSED(cc_rematch_start(&S, &R, RUN_B, 1, NULL), CC_E_CONSISTENCY);
    R.rec[1].life = CC_LIFE_CHALLENGED;
    R.rec[1].challenge_wave = 127;
    REFUSED(cc_rematch_start(&S, &R, RUN_A, 1, NULL), CC_E_CONSISTENCY);
}

/* ------------------------------------------------------------------ the windows */

static cc_proposal ask(uint16_t wave, uint8_t kind, uint8_t flags)
{
    cc_proposal p;
    memset(&p, 0xCC, sizeof p);
    const cc_encounter e = enc(wave, kind, flags);
    CHECK_STATUS(cc_rematch_window(&S, &R, &e, &p), CC_OK);
    both_valid();
    return p;
}

static void start_with(unsigned n, uint64_t seed)
{
    make_sequential(&R, n);
    CHECK_STATUS(cc_rematch_start(&S, &R, RUN_A, seed, NULL), CC_OK);
}

static void test_windows_offer_at_first_eligible(void)
{
    /* the first window is an ordinary wild encounter: offered there, with the fixed reward and no scaling */
    start_with(5, 1);
    const cc_record_id chosen = S.chosen_id;
    cc_proposal p = ask(27, CC_ENC_ORDINARY_WILD, 0);
    CHECK_EQ(p.kind, CC_PROP_OFFER);
    CHECK_EQ(p.why, CC_WHY_OFFER);
    CHECK(p.skip == 0 && p.opportunity == 1 && p.wave == 27 && p.record_id == chosen);
    CHECK(p.level_rule == CC_LEVEL_ORDINARY_WAVE && p.reward_vouchers == 1);
    CHECK_EQ(S.state, CC_SCHED_OFFERED);
    CHECK_EQ(S.windows[0].state, CC_WIN_OFFERED);
    for (int i = 1; i < CC_NUM_WINDOWS; i++) CHECK_EQ(S.windows[i].state, CC_WIN_PENDING);
    CHECK_EQ(R.rec[slot_of(&R, chosen)].life, CC_LIFE_SCHEDULED);

    /* asked again (a reload at the prompt): the same offer, the schedule untouched */
    cc_schedule before = S;
    cc_roster rbefore = R;
    cc_proposal q = ask(27, CC_ENC_ORDINARY_WILD, 0);
    CHECK(memcmp(&p, &q, sizeof p) == 0);
    CHECK(memcmp(&before, &S, sizeof S) == 0 && memcmp(&rbefore, &R, sizeof R) == 0);
    /* even if the adapter's classification changed since: the answer was fixed when the window was first reported */
    q = ask(27, CC_ENC_TRAINER, 0);
    CHECK(memcmp(&p, &q, sizeof p) == 0);
    CHECK(memcmp(&before, &S, sizeof S) == 0);
}

static void test_windows_each_skip(void)
{
    static const struct {
        uint8_t kind, flags, reason;
    } SK[] = {
        {CC_ENC_TRAINER, 0, CC_SKIP_TRAINER},       {CC_ENC_FIXED_OR_SCRIPTED, 0, CC_SKIP_FIXED_OR_SCRIPTED},
        {CC_ENC_MYSTERY, 0, CC_SKIP_MYSTERY},       {CC_ENC_BOSS, 0, CC_SKIP_BOSS},
        {CC_ENC_FINAL, 0, CC_SKIP_FINAL},           {CC_ENC_UNSUPPORTED, 0, CC_SKIP_UNSUPPORTED},
        {CC_ENC_ORDINARY_WILD, CC_ENF_FUSED, CC_SKIP_UNSUPPORTED},
        {CC_ENC_ORDINARY_WILD, CC_ENF_TERA, CC_SKIP_UNSUPPORTED},
        {CC_ENC_ORDINARY_WILD, CC_ENF_DOUBLES, CC_SKIP_UNSUPPORTED},
    };
    /* each skip reason at each of the first three windows: the next window is tried, and offered when it is eligible */
    for (unsigned k = 0; k < sizeof SK / sizeof SK[0]; k++) {
        for (int skipped = 1; skipped <= 3; skipped++) {
            start_with(4, k + 100u * (unsigned)skipped);
            const cc_record_id chosen = S.chosen_id;
            for (int w = 0; w < skipped; w++) {
                cc_proposal p = ask(cc_window_waves[w], SK[k].kind, SK[k].flags);
                CHECK_EQ(p.kind, CC_PROP_NONE);
                CHECK_EQ(p.why, CC_WHY_SKIPPED);
                CHECK_EQ(p.skip, SK[k].reason);
                CHECK_EQ(p.opportunity, w + 1);
                CHECK(p.record_id == 0 && p.reward_vouchers == 0);
                CHECK_EQ(S.windows[w].state, CC_WIN_SKIPPED);
                CHECK_EQ(S.windows[w].reason, SK[k].reason);
                CHECK_EQ(S.state, CC_SCHED_ACTIVE);
            }
            cc_proposal o = ask(cc_window_waves[skipped], CC_ENC_ORDINARY_WILD, 0);
            CHECK_EQ(o.kind, CC_PROP_OFFER);
            CHECK(o.wave == cc_window_waves[skipped] && o.opportunity == skipped + 1 && o.record_id == chosen);
            CHECK_EQ(S.state, CC_SCHED_OFFERED);
            CHECK_EQ(R.rec[slot_of(&R, chosen)].life, CC_LIFE_SCHEDULED);   // still scheduled, not yet challenged
            /* a replayed skipped window answers as it did */
            cc_proposal again = ask(cc_window_waves[0], CC_ENC_ORDINARY_WILD, 0);
            CHECK(again.kind == CC_PROP_NONE && again.why == CC_WHY_SKIPPED && again.skip == SK[k].reason);
        }
    }
}

static void test_windows_exhausted(void)
{
    start_with(3, 5);
    const cc_record_id chosen = S.chosen_id;
    for (int w = 0; w < CC_NUM_WINDOWS; w++) {
        cc_proposal p = ask(cc_window_waves[w], w % 2 ? CC_ENC_BOSS : CC_ENC_TRAINER, 0);
        CHECK(p.kind == CC_PROP_NONE && p.why == CC_WHY_SKIPPED);
        CHECK_EQ(S.state, w == CC_NUM_WINDOWS - 1 ? CC_SCHED_EXHAUSTED : CC_SCHED_ACTIVE);
    }
    CHECK_EQ(S.windows[0].reason, CC_SKIP_TRAINER);
    CHECK_EQ(S.windows[1].reason, CC_SKIP_BOSS);
    CHECK_EQ(S.windows[3].reason, CC_SKIP_BOSS);
    /* nothing this run: the record is free again and the schedule offers nothing more */
    const cc_nemesis *n = &R.rec[slot_of(&R, chosen)];
    CHECK(n->life == CC_LIFE_STORED && n->sched_run_uid == 0 && n->challenge_wave == 0);
    CHECK_EQ(held(&R), 0);
    cc_proposal p = ask(177, CC_ENC_ORDINARY_WILD, 0);
    CHECK(p.kind == CC_PROP_NONE && p.why == CC_WHY_SKIPPED);
    cc_rematch_view v;
    CHECK_STATUS(cc_rematch_view_of(&S, &v), CC_OK);
    CHECK(v.state == CC_SCHED_EXHAUSTED && v.n_pending == 0 && v.next_opportunity == 0 && v.next_wave == 0 && v.record_id == chosen);
}

static void test_windows_not_a_window_and_order(void)
{
    start_with(4, 3);
    for (uint16_t wave = 0; wave < 300; wave++) {
        bool is = wave == 27 || wave == 77 || wave == 127 || wave == 177;
        if (is) continue;
        cc_schedule before = S;
        cc_proposal p = ask(wave, CC_ENC_ORDINARY_WILD, 0);
        CHECK(p.kind == CC_PROP_NONE && p.why == CC_WHY_NOT_A_WINDOW && p.opportunity == 0 && p.wave == wave);
        CHECK(memcmp(&before, &S, sizeof S) == 0);
    }
    /* a later window before the earlier one was reported: refused, nothing changes */
    cc_proposal p;
    const cc_encounter e77 = enc(77, CC_ENC_ORDINARY_WILD, 0);
    REFUSED(cc_rematch_window(&S, &R, &e77, &p), CC_E_ORDER);
    const cc_encounter e177 = enc(177, CC_ENC_ORDINARY_WILD, 0);
    REFUSED(cc_rematch_window(&S, &R, &e177, &p), CC_E_ORDER);
    ask(27, CC_ENC_TRAINER, 0);
    REFUSED(cc_rematch_window(&S, &R, &e177, &p), CC_E_ORDER);        // 77 and 127 are still pending
    ask(77, CC_ENC_ORDINARY_WILD, 0);                                  // offered at 77
    REFUSED(cc_rematch_window(&S, &R, &e177, &p), CC_E_ORDER);        // offered and not answered: no later window
    const cc_encounter e127 = enc(127, CC_ENC_ORDINARY_WILD, 0);
    REFUSED(cc_rematch_window(&S, &R, &e127, &p), CC_E_ORDER);
    /* an encounter that does not make sense */
    const cc_encounter bad = enc(27, 99, 0);
    REFUSED(cc_rematch_window(&S, &R, &bad, &p), CC_E_RANGE);
    const cc_encounter badf = enc(27, CC_ENC_ORDINARY_WILD, 0x80);
    REFUSED(cc_rematch_window(&S, &R, &badf, &p), CC_E_RANGE);
    REFUSED(cc_rematch_window(NULL, &R, &e77, &p), CC_E_NULL);
    REFUSED(cc_rematch_window(&S, NULL, &e77, &p), CC_E_NULL);
    REFUSED(cc_rematch_window(&S, &R, NULL, &p), CC_E_NULL);
    REFUSED(cc_rematch_window(&S, &R, &e77, NULL), CC_E_NULL);
}

static void test_windows_disagreement(void)
{
    /* the roster does not hold the schedule's record as scheduled by this run: the schedule is not trusted */
    start_with(3, 8);
    cc_proposal p;
    const cc_encounter e = enc(27, CC_ENC_ORDINARY_WILD, 0);
    const unsigned slot = slot_of(&R, S.chosen_id);
    R.rec[slot].life = CC_LIFE_STORED;
    R.rec[slot].sched_run_uid = 0;
    REFUSED(cc_rematch_window(&S, &R, &e, &p), CC_E_CONSISTENCY);
    R.rec[slot].life = CC_LIFE_SCHEDULED;
    R.rec[slot].sched_run_uid = RUN_B;                    // scheduled by another run
    REFUSED(cc_rematch_window(&S, &R, &e, &p), CC_E_CONSISTENCY);
    R.rec[slot].sched_run_uid = RUN_A;
    snap();
    CHECK_STATUS(cc_rematch_window(&S, &R, &e, &p), CC_OK);   // restored: it works again
    CHECK_EQ(p.kind, CC_PROP_OFFER);

    /* the chosen record is gone from the roster */
    start_with(3, 8);
    cc_roster r2 = R;
    r2.count = 0;
    snap();
    const cc_roster rb = R;
    CHECK_STATUS(cc_rematch_window(&S, &r2, &e, &p), CC_E_CONSISTENCY);
    CHECK(memcmp(&R, &rb, sizeof R) == 0);

    /* a schedule that is not valid */
    start_with(3, 8);
    S.windows[1].state = CC_WIN_SKIPPED;                   // a skipped window without a reason, after a pending one
    REFUSED(cc_rematch_window(&S, &R, &e, &p), CC_E_CONSISTENCY);

    /* an offered schedule is held to the same agreement: the offer, the answer and the replay all stop */
    bool rp;
    start_with(3, 8);
    ask(27, CC_ENC_ORDINARY_WILD, 0);
    const unsigned os = slot_of(&R, S.chosen_id);
    R.rec[os].life = CC_LIFE_STORED;
    R.rec[os].sched_run_uid = 0;
    REFUSED(cc_rematch_window(&S, &R, &e, &p), CC_E_CONSISTENCY);
    REFUSED(cc_rematch_decide(&S, &R, true, &rp), CC_E_CONSISTENCY);
    REFUSED(cc_rematch_decide(&S, &R, false, &rp), CC_E_CONSISTENCY);

    /* a roster that is not valid, a schedule that is not valid: each entry point refuses with the validator's own status */
    start_with(3, 8);
    ask(27, CC_ENC_ORDINARY_WILD, 0);
    cc_roster good = R;
    R.count = CC_MAX_NEMESES + 1;
    REFUSED(cc_rematch_window(&S, &R, &e, &p), CC_E_COUNT);
    REFUSED(cc_rematch_decide(&S, &R, true, &rp), CC_E_COUNT);
    R = good;
    CHECK_STATUS(cc_rematch_decide(&S, &R, true, NULL), CC_OK);
    good = R;
    R.count = CC_MAX_NEMESES + 1;
    REFUSED(cc_rematch_result(&S, &R, true, &rp), CC_E_COUNT);
    R = good;
    const cc_schedule sgood = S;
    S.rng_algo = 77;                                       // an algorithm id this version does not know
    const cc_status bad_sched = cc_schedule_validate(&S);
    CHECK(bad_sched != CC_OK);
    REFUSED(cc_rematch_window(&S, &R, &e, &p), bad_sched);
    REFUSED(cc_rematch_decide(&S, &R, true, &rp), bad_sched);
    REFUSED(cc_rematch_result(&S, &R, true, &rp), bad_sched);
    S = sgood;
}

/* ------------------------------------------------------------------ the answers, the result, the reward */

static void test_decline(void)
{
    start_with(6, 21);
    const cc_record_id chosen = S.chosen_id;
    ask(27, CC_ENC_TRAINER, 0);
    cc_proposal p = ask(77, CC_ENC_ORDINARY_WILD, 0);
    CHECK_EQ(p.kind, CC_PROP_OFFER);
    bool replayed = true;
    CHECK_STATUS(cc_rematch_decide(&S, &R, false, &replayed), CC_OK);
    CHECK(!replayed);
    both_valid();
    CHECK_EQ(S.state, CC_SCHED_DECLINED);
    CHECK_EQ(S.windows[1].state, CC_WIN_DECLINED);
    CHECK_EQ(S.windows[2].state, CC_WIN_PENDING);
    const cc_nemesis *n = &R.rec[slot_of(&R, chosen)];
    CHECK(n->life == CC_LIFE_STORED && n->sched_run_uid == 0);
    CHECK_EQ(held(&R), 0);

    /* consumed: no later window is ever offered, scouting or reloading does not reroll */
    for (int w = 2; w < CC_NUM_WINDOWS; w++) {
        cc_proposal q = ask(cc_window_waves[w], CC_ENC_ORDINARY_WILD, 0);
        CHECK(q.kind == CC_PROP_NONE && q.why == CC_WHY_DECLINED && q.opportunity == w + 1);
    }
    cc_proposal again = ask(77, CC_ENC_ORDINARY_WILD, 0);
    CHECK(again.kind == CC_PROP_NONE && again.why == CC_WHY_DECLINED);
    CHECK_EQ(S.chosen_id, chosen);
    /* the same answer again is a replay; the other answer is refused; nothing changes either way */
    snap();
    CHECK_STATUS(cc_rematch_decide(&S, &R, false, &replayed), CC_OK);
    CHECK(replayed && unchanged());
    REFUSED(cc_rematch_decide(&S, &R, true, &replayed), CC_E_LIFECYCLE);
    bool r2;
    REFUSED(cc_rematch_result(&S, &R, true, &r2), CC_E_LIFECYCLE);
    cc_rematch_view v;
    CHECK_STATUS(cc_rematch_view_of(&S, &v), CC_OK);
    CHECK(v.state == CC_SCHED_DECLINED && v.n_pending == 0 && v.next_opportunity == 0);
}

static void test_accept_and_win(void)
{
    cc_meta m;
    cc_meta_init(&m, 5);
    cc_run_uid uid = 0;
    CHECK_STATUS(cc_alloc_run_uid(&m.counters, &uid), CC_OK);          // 0x0000000500000001 = RUN_A
    CHECK(uid == RUN_A);
    CHECK_STATUS(cc_alloc_run_uid(&m.counters, &uid), CC_OK);          // RUN_B
    m.counters.next_record_id = 50;
    m.counters.next_txn_id = 9;
    for (unsigned i = 0; i < 4; i++) {
        cc_nemesis n = mk_nem(10 + i, RUN_B, 100 + i);
        m.roster.rec[m.roster.count++] = n;
    }
    CHECK_STATUS(cc_meta_validate(&m), CC_OK);
    R = m.roster;
    CHECK_STATUS(cc_rematch_start(&S, &R, RUN_A, 31, NULL), CC_OK);
    const cc_record_id chosen = S.chosen_id;
    ask(27, CC_ENC_MYSTERY, 0);
    ask(77, CC_ENC_ORDINARY_WILD, 0);
    bool replayed = true;
    CHECK_STATUS(cc_rematch_decide(&S, &R, true, &replayed), CC_OK);
    CHECK(!replayed);
    both_valid();
    CHECK_EQ(S.state, CC_SCHED_ACCEPTED);
    CHECK_EQ(S.windows[1].state, CC_WIN_ACCEPTED);
    cc_nemesis *n = &R.rec[slot_of(&R, chosen)];
    CHECK(n->life == CC_LIFE_CHALLENGED && n->sched_run_uid == RUN_A && n->challenge_wave == 77);
    cc_reward_entitlement e0;
    CHECK_STATUS(cc_reward_entitlement_of(n, &e0), CC_OK);        // fighting is not winning: nothing is owed yet
    CHECK(e0.state == CC_RW_NOT_WON && e0.vouchers == 0);

    /* a reload at the accepted window rebuilds the same rematch; the other windows are resolved */
    cc_proposal p = ask(77, CC_ENC_ORDINARY_WILD, 0);
    CHECK(p.kind == CC_PROP_RESUME && p.why == CC_WHY_OFFER && p.record_id == chosen && p.opportunity == 2 && p.wave == 77);
    CHECK(p.level_rule == CC_LEVEL_ORDINARY_WAVE && p.reward_vouchers == 1);
    p = ask(127, CC_ENC_ORDINARY_WILD, 0);
    CHECK(p.kind == CC_PROP_NONE && p.why == CC_WHY_RESOLVED);
    p = ask(177, CC_ENC_ORDINARY_WILD, 0);
    CHECK(p.kind == CC_PROP_NONE && p.why == CC_WHY_RESOLVED);
    snap();
    CHECK_STATUS(cc_rematch_decide(&S, &R, true, &replayed), CC_OK);
    CHECK(replayed && unchanged());
    REFUSED(cc_rematch_decide(&S, &R, false, &replayed), CC_E_LIFECYCLE);

    /* the result: a different outcome than the first is refused, the same is a replay */
    bool rep = true;
    CHECK_STATUS(cc_rematch_result(&S, &R, true, &rep), CC_OK);
    CHECK(!rep);
    both_valid();
    n = &R.rec[slot_of(&R, chosen)];
    CHECK(n->life == CC_LIFE_DEFEATED && n->sched_run_uid == RUN_A && n->challenge_wave == 77 && n->reward_txn == 0);
    CHECK_EQ(S.state, CC_SCHED_ACCEPTED);
    snap();
    CHECK_STATUS(cc_rematch_result(&S, &R, true, &rep), CC_OK);
    CHECK(rep && unchanged());
    REFUSED(cc_rematch_result(&S, &R, false, &rep), CC_E_LIFECYCLE);

    /* the reward: owed once, from the lifecycle alone */
    cc_reward_entitlement e;
    CHECK_STATUS(cc_reward_entitlement_of(n, &e), CC_OK);
    CHECK(e.state == CC_RW_OWED && e.vouchers == 1);
    m.roster = R;
    cc_meta before = m;
    CHECK_STATUS(cc_meta_validate(&m), CC_OK);
    cc_claim_result c;
    CHECK_STATUS(cc_reward_claim(&m, chosen, &c), CC_OK);
    CHECK(!c.replayed && c.txn_id == 9);
    CHECK_EQ(m.counters.next_txn_id, 10);
    CHECK_EQ(m.ledger.n, 1);
    CHECK(m.ledger.e[0].txn_id == 9 && m.ledger.e[0].kind == CC_TXN_REWARD_CLAIM && m.ledger.e[0].outcome == CC_TXO_CLAIMED &&
          m.ledger.e[0].record_id == chosen && m.ledger.e[0].run_uid == RUN_A);
    const cc_nemesis *cn = &m.roster.rec[slot_of(&m.roster, chosen)];
    CHECK(cn->life == CC_LIFE_REWARD_CLAIMED && cn->reward_txn == 9 && cn->sched_run_uid == RUN_A && cn->challenge_wave == 77);
    for (unsigned i = 0; i < m.roster.count; i++)
        if (m.roster.rec[i].record_id != chosen) CHECK(memcmp(&m.roster.rec[i], &before.roster.rec[i], sizeof m.roster.rec[i]) == 0);
    CHECK_STATUS(cc_meta_validate(&m), CC_OK);
    CHECK_STATUS(cc_reward_entitlement_of(cn, &e), CC_OK);
    CHECK(e.state == CC_RW_CLAIMED && e.vouchers == 0);

    /* once per record: claiming again, or after a retry, pays nothing and changes nothing */
    cc_meta claimed = m;
    CHECK_STATUS(cc_reward_claim(&m, chosen, &c), CC_OK);
    CHECK(c.replayed && c.txn_id == 9 && memcmp(&claimed, &m, sizeof m) == 0);
    /* the ledger forgot the claim (it keeps 8): the record still says so */
    m.ledger.n = 0;
    memset(&m.ledger.e, 0, sizeof m.ledger.e);
    cc_meta forgotten = m;
    CHECK_STATUS(cc_reward_claim(&m, chosen, &c), CC_OK);
    CHECK(c.replayed && c.txn_id == 9 && memcmp(&forgotten, &m, sizeof m) == 0);

    /* the result reported again after the claim is still the same win; the other outcome, or another run's record, is not */
    cc_roster claimed_roster = m.roster;
    cc_schedule claimed_sched = S;
    CHECK_STATUS(cc_rematch_result(&claimed_sched, &claimed_roster, true, &rep), CC_OK);
    CHECK(rep && memcmp(&claimed_roster, &m.roster, sizeof claimed_roster) == 0 && memcmp(&claimed_sched, &S, sizeof S) == 0);
    CHECK_STATUS(cc_rematch_result(&claimed_sched, &claimed_roster, false, &rep), CC_E_LIFECYCLE);
    claimed_roster.rec[slot_of(&claimed_roster, chosen)].sched_run_uid = RUN_B;
    cc_roster other_run = claimed_roster;
    CHECK_STATUS(cc_rematch_result(&claimed_sched, &claimed_roster, true, &rep), CC_E_LIFECYCLE);
    CHECK(memcmp(&other_run, &claimed_roster, sizeof other_run) == 0);
}

static void test_accept_and_lose(void)
{
    start_with(3, 2);
    const cc_record_id chosen = S.chosen_id;
    ask(27, CC_ENC_ORDINARY_WILD, 0);
    CHECK_STATUS(cc_rematch_decide(&S, &R, true, NULL), CC_OK);
    bool rep = true;
    CHECK_STATUS(cc_rematch_result(&S, &R, false, &rep), CC_OK);
    CHECK(!rep);
    both_valid();
    const cc_nemesis *n = &R.rec[slot_of(&R, chosen)];
    CHECK(n->life == CC_LIFE_STORED && n->sched_run_uid == 0 && n->challenge_wave == 0);
    CHECK_EQ(S.state, CC_SCHED_ACCEPTED);                  // no second try this run
    cc_proposal p = ask(77, CC_ENC_ORDINARY_WILD, 0);
    CHECK(p.kind == CC_PROP_NONE && p.why == CC_WHY_RESOLVED);
    snap();
    CHECK_STATUS(cc_rematch_result(&S, &R, false, &rep), CC_OK);
    CHECK(rep && unchanged());
    REFUSED(cc_rematch_result(&S, &R, true, &rep), CC_E_LIFECYCLE);
    cc_reward_entitlement e;
    CHECK_STATUS(cc_reward_entitlement_of(n, &e), CC_OK);
    CHECK(e.state == CC_RW_NOT_WON && e.vouchers == 0);
}

static void test_answer_refusals(void)
{
    start_with(3, 2);
    bool rep;
    REFUSED(cc_rematch_decide(&S, &R, true, &rep), CC_E_LIFECYCLE);    // nothing offered yet
    REFUSED(cc_rematch_decide(&S, &R, false, &rep), CC_E_LIFECYCLE);
    REFUSED(cc_rematch_result(&S, &R, true, &rep), CC_E_LIFECYCLE);    // nothing accepted yet
    ask(27, CC_ENC_ORDINARY_WILD, 0);
    REFUSED(cc_rematch_result(&S, &R, true, &rep), CC_E_LIFECYCLE);    // offered, not accepted
    REFUSED(cc_rematch_decide(NULL, &R, true, &rep), CC_E_NULL);
    REFUSED(cc_rematch_decide(&S, NULL, true, &rep), CC_E_NULL);
    REFUSED(cc_rematch_result(NULL, &R, true, &rep), CC_E_NULL);
    REFUSED(cc_rematch_result(&S, NULL, true, &rep), CC_E_NULL);
    /* no candidate, exhausted */
    make_sequential(&R, 0);
    CHECK_STATUS(cc_rematch_start(&S, &R, RUN_A, 5, NULL), CC_OK);
    REFUSED(cc_rematch_decide(&S, &R, true, &rep), CC_E_LIFECYCLE);
    REFUSED(cc_rematch_result(&S, &R, true, &rep), CC_E_LIFECYCLE);
    /* the result of a rematch whose record is not challenged by this run */
    start_with(2, 4);
    ask(27, CC_ENC_ORDINARY_WILD, 0);
    CHECK_STATUS(cc_rematch_decide(&S, &R, true, NULL), CC_OK);
    R.rec[slot_of(&R, S.chosen_id)].life = CC_LIFE_STORED;
    R.rec[slot_of(&R, S.chosen_id)].sched_run_uid = 0;
    R.rec[slot_of(&R, S.chosen_id)].challenge_wave = 0;
    REFUSED(cc_rematch_result(&S, &R, true, &rep), CC_E_LIFECYCLE);   // it was lost (stored again): a win does not apply
    R.rec[slot_of(&R, S.chosen_id)].life = CC_LIFE_CHALLENGED;
    R.rec[slot_of(&R, S.chosen_id)].sched_run_uid = RUN_B;            // challenged by another run: not this rematch's to settle
    R.rec[slot_of(&R, S.chosen_id)].challenge_wave = 27;
    REFUSED(cc_rematch_result(&S, &R, true, &rep), CC_E_LIFECYCLE);
    REFUSED(cc_rematch_result(&S, &R, false, &rep), CC_E_LIFECYCLE);
    R.rec[slot_of(&R, S.chosen_id)].life = CC_LIFE_SCHEDULED;
    R.rec[slot_of(&R, S.chosen_id)].challenge_wave = 0;
    R.rec[slot_of(&R, S.chosen_id)].sched_run_uid = RUN_B;            // taken by another run since
    REFUSED(cc_rematch_result(&S, &R, false, &rep), CC_E_LIFECYCLE);
}

static void test_claim_refusals(void)
{
    cc_meta m;
    cc_meta_init(&m, 5);
    cc_run_uid uid;
    for (int i = 0; i < 3; i++) CHECK_STATUS(cc_alloc_run_uid(&m.counters, &uid), CC_OK);
    m.counters.next_record_id = 40;
    m.counters.next_txn_id = 20;
    static const uint8_t lives[4] = {CC_LIFE_STORED, CC_LIFE_SCHEDULED, CC_LIFE_CHALLENGED, CC_LIFE_DEFEATED};
    for (unsigned i = 0; i < 4; i++) {
        cc_nemesis n = mk_nem(10 + i, 0x0000000500000002ull, 100 + i);
        n.life = lives[i];
        if (n.life != CC_LIFE_STORED) n.sched_run_uid = 0x0000000500000003ull;
        if (n.life >= CC_LIFE_CHALLENGED) n.challenge_wave = 127;
        if (n.life == CC_LIFE_SCHEDULED) { /* at most one held record in a roster */ }
        m.roster.rec[m.roster.count++] = n;
    }
    m.roster.rec[2].life = CC_LIFE_STORED;                 // only one held record: keep SCHEDULED, make the CHALLENGED one stored
    m.roster.rec[2].sched_run_uid = 0;
    m.roster.rec[2].challenge_wave = 0;
    CHECK_STATUS(cc_meta_validate(&m), CC_OK);
    cc_claim_result c;
    cc_reward_entitlement c_en;
    cc_meta before = m;
    /* only a defeated record can be claimed */
    CHECK_STATUS(cc_reward_claim(&m, 10, &c), CC_E_LIFECYCLE);
    CHECK_STATUS(cc_reward_claim(&m, 11, &c), CC_E_LIFECYCLE);
    CHECK_STATUS(cc_reward_claim(&m, 12, &c), CC_E_LIFECYCLE);
    CHECK_STATUS(cc_reward_claim(&m, 99, &c), CC_E_RANGE);
    CHECK_STATUS(cc_reward_claim(&m, 0, &c), CC_E_RANGE);
    CHECK(memcmp(&before, &m, sizeof m) == 0);
    m.roster.rec[1].life = CC_LIFE_CHALLENGED;             // the held record is challenged instead of scheduled
    m.roster.rec[1].challenge_wave = 127;
    CHECK_STATUS(cc_meta_validate(&m), CC_OK);
    cc_meta challenged = m;
    CHECK_STATUS(cc_reward_claim(&m, 11, &c), CC_E_LIFECYCLE);
    CHECK(memcmp(&challenged, &m, sizeof m) == 0);
    CHECK_STATUS(cc_reward_entitlement_of(&m.roster.rec[1], &c_en), CC_OK);
    CHECK(c_en.state == CC_RW_NOT_WON && c_en.vouchers == 0);
    m = before;
    CHECK_STATUS(cc_reward_claim(NULL, 13, &c), CC_E_NULL);
    CHECK_STATUS(cc_reward_claim(&m, 13, NULL), CC_E_NULL);
    /* an invalid state is not trusted */
    m.counters.next_txn_id = 0;
    cc_meta bad = m;
    CHECK_STATUS(cc_reward_claim(&m, 13, &c), CC_E_RANGE);
    CHECK(memcmp(&bad, &m, sizeof m) == 0);
    m.counters.next_txn_id = 20;
    /* the ledger says claimed, the record says defeated: neither is trusted, nothing is paid */
    cc_ledger_entry e = mk_entry(5, 0x0000000500000003ull, 13, CC_TXN_REWARD_CLAIM, CC_TXO_CLAIMED);
    CHECK_STATUS(cc_ledger_append(&m.ledger, &e), CC_OK);
    cc_meta contradiction = m;
    CHECK_STATUS(cc_reward_claim(&m, 13, &c), CC_E_CONSISTENCY);
    CHECK(memcmp(&contradiction, &m, sizeof m) == 0);
    /* an exhausted transaction id space changes nothing */
    m = before;
    m.counters.next_txn_id = UINT64_MAX;
    m.roster.rec[3].reward_txn = 0;
    cc_meta full = m;
    CHECK_STATUS(cc_reward_claim(&m, 13, &c), CC_E_OVERFLOW);
    CHECK(memcmp(&full, &m, sizeof m) == 0);

    /* a record and a ledger that disagree about the transaction */
    m = before;
    CHECK_STATUS(cc_reward_claim(&m, 13, &c), CC_OK);
    CHECK_EQ(c.txn_id, 20);
    m.ledger.e[0].txn_id = 7;                              // a different transaction than the record's
    cc_meta wrong = m;
    CHECK_STATUS(cc_reward_claim(&m, 13, &c), CC_E_CONSISTENCY);
    CHECK(memcmp(&wrong, &m, sizeof m) == 0);

    /* entitlement of a record in each life */
    cc_reward_entitlement en;
    for (unsigned i = 0; i < 4; i++) {
        CHECK_STATUS(cc_reward_entitlement_of(&before.roster.rec[i], &en), CC_OK);
        CHECK_EQ(en.state, i == 3 ? CC_RW_OWED : CC_RW_NOT_WON);
        CHECK_EQ(en.vouchers, i == 3 ? 1 : 0);
    }
    CHECK_STATUS(cc_reward_entitlement_of(NULL, &en), CC_E_NULL);
    CHECK_STATUS(cc_reward_entitlement_of(&before.roster.rec[0], NULL), CC_E_NULL);
    cc_nemesis badn = before.roster.rec[0];
    badn.life = 99;
    CHECK_STATUS(cc_reward_entitlement_of(&badn, &en), cc_nemesis_validate(&badn));
    CHECK(cc_nemesis_validate(&badn) != CC_OK);
}

/* ------------------------------------------------------------------ held records */

static void test_release(void)
{
    start_with(5, 12);
    cc_proposal p = ask(27, CC_ENC_ORDINARY_WILD, 0);
    CHECK_EQ(p.kind, CC_PROP_OFFER);
    const cc_record_id chosen = S.chosen_id;
    unsigned n = 77;
    cc_roster r0 = R;
    /* the run that holds it is not "another run" */
    CHECK_STATUS(cc_roster_release_except(&R, RUN_A, &n), CC_OK);
    CHECK(n == 0 && memcmp(&r0, &R, sizeof R) == 0);
    CHECK_STATUS(cc_roster_release(&R, RUN_B, &n), CC_OK);
    CHECK(n == 0 && memcmp(&r0, &R, sizeof R) == 0);
    /* but another run's uid frees it: what a crash left behind */
    CHECK_STATUS(cc_roster_release_except(&R, RUN_B, &n), CC_OK);
    CHECK_EQ(n, 1);
    CHECK_EQ(R.rec[slot_of(&R, chosen)].life, CC_LIFE_STORED);
    CHECK_EQ(R.rec[slot_of(&R, chosen)].sched_run_uid, 0);
    CHECK_STATUS(cc_roster_validate(&R), CC_OK);
    CHECK_STATUS(cc_roster_release_except(&R, RUN_B, &n), CC_OK);   // idempotent
    CHECK_EQ(n, 0);

    /* a challenged record is freed the same way, a defeated one never */
    R = r0;
    CHECK_STATUS(cc_rematch_decide(&S, &R, true, NULL), CC_OK);
    CHECK_EQ(R.rec[slot_of(&R, chosen)].life, CC_LIFE_CHALLENGED);
    cc_roster rc = R;
    CHECK_STATUS(cc_roster_release(&R, RUN_A, &n), CC_OK);
    CHECK_EQ(n, 1);
    CHECK_EQ(R.rec[slot_of(&R, chosen)].life, CC_LIFE_STORED);
    CHECK_EQ(R.rec[slot_of(&R, chosen)].challenge_wave, 0);
    R = rc;
    CHECK_STATUS(cc_rematch_result(&S, &R, true, NULL), CC_OK);
    rc = R;
    CHECK_STATUS(cc_roster_release(&R, RUN_A, &n), CC_OK);
    CHECK(n == 0 && memcmp(&rc, &R, sizeof R) == 0);                // its reward is owed
    CHECK_STATUS(cc_roster_release_except(&R, 0, &n), CC_OK);
    CHECK(n == 0 && memcmp(&rc, &R, sizeof R) == 0);

    /* "except 0" frees whatever is held, whichever run holds it */
    R = r0;
    CHECK_STATUS(cc_roster_release_except(&R, 0, &n), CC_OK);
    CHECK_EQ(n, 1);
    CHECK_EQ(held(&R), 0);

    /* refusals change nothing */
    R = r0;
    cc_roster r1 = R;
    CHECK_STATUS(cc_roster_release(&R, 0, &n), CC_E_RANGE);
    CHECK_EQ(n, 0);
    CHECK_STATUS(cc_roster_release(NULL, RUN_A, &n), CC_E_NULL);
    CHECK_STATUS(cc_roster_release_except(NULL, RUN_A, &n), CC_E_NULL);
    R.rec[1].record_id = R.rec[0].record_id;
    cc_roster r2 = R;
    CHECK_STATUS(cc_roster_release(&R, RUN_A, &n), CC_E_DUPLICATE);
    CHECK(memcmp(&r2, &R, sizeof R) == 0);
    CHECK_STATUS(cc_roster_release(&r1, RUN_A, NULL), CC_OK);        // the count is optional
    CHECK_EQ(held(&r1), 0);
}

/* ------------------------------------------------------------------ reload, scouting */

static void test_reload_and_scouting(void)
{
    /* at every step of a run the schedule and the roster are copied (what the adapter would persist) and the run continues from the
     * copies: the answers and the final states are the same as without the interruption, and nothing was drawn again */
    static cc_schedule s_a, s_b;
    static cc_roster r_a, r_b;
    make_sequential(&r_a, 11);
    CHECK_STATUS(cc_rematch_start(&s_a, &r_a, RUN_A, 0xDEADBEEFull, NULL), CC_OK);
    s_b = s_a;
    r_b = r_a;
    const uint8_t kinds[4] = {CC_ENC_TRAINER, CC_ENC_ORDINARY_WILD, CC_ENC_ORDINARY_WILD, CC_ENC_ORDINARY_WILD};
    for (int w = 0; w < 2; w++) {
        cc_proposal pa, pb;
        const cc_encounter e = enc(cc_window_waves[w], kinds[w], 0);
        CHECK_STATUS(cc_rematch_window(&s_a, &r_a, &e, &pa), CC_OK);
        /* reload: b is rebuilt from a validated copy, and is asked twice (the adapter re-enters the window) */
        static cc_schedule s_c;
        static cc_roster r_c;
        memcpy(&s_c, &s_a, sizeof s_c);
        memcpy(&r_c, &r_a, sizeof r_c);
        CHECK_STATUS(cc_schedule_validate(&s_c), CC_OK);
        CHECK_STATUS(cc_rematch_window(&s_b, &r_b, &e, &pb), CC_OK);
        CHECK(memcmp(&pa, &pb, sizeof pa) == 0);
        CHECK_STATUS(cc_rematch_window(&s_c, &r_c, &e, &pb), CC_OK);      // asked again on the reloaded copy
        if (pa.kind == CC_PROP_OFFER) CHECK(memcmp(&pa, &pb, sizeof pa) == 0);
        CHECK(memcmp(&s_a, &s_c, sizeof s_a) == 0 && memcmp(&r_a, &r_c, sizeof r_a) == 0);
        CHECK(s_c.rng_state == s_a.rng_state && s_c.rng_seed == 0xDEADBEEFull);
    }
    CHECK(memcmp(&s_a, &s_b, sizeof s_a) == 0 && memcmp(&r_a, &r_b, sizeof r_a) == 0);
    CHECK_EQ(s_a.state, CC_SCHED_OFFERED);

    /* scouting: any number of reads and non-window asks leave the schedule, the roster and the stream exactly as they were */
    S = s_a;
    R = r_a;
    snap();
    /* the process's ambient generator stands in for the game's: it must not move, whatever is asked (the objects also link
       against nothing but cc_* and mem*, see `make purity-check`) */
    int ambient[4];
    srand(31337);
    for (int i = 0; i < 4; i++) ambient[i] = rand();
    srand(31337);
    for (int i = 0; i < 1000; i++) {
        cc_rematch_view v;
        CHECK_STATUS(cc_rematch_view_of(&S, &v), CC_OK);
        CHECK(v.state == CC_SCHED_OFFERED && v.next_opportunity == 2 && v.next_wave == 77 && v.record_id == S.chosen_id &&
              v.n_candidates == 11 && v.n_pending == 0);
        cc_proposal p;
        const cc_encounter e = enc((uint16_t)(i % 300), CC_ENC_ORDINARY_WILD, 0);
        if (e.wave != 77 && e.wave != 127 && e.wave != 177 && e.wave != 27) {
            CHECK_STATUS(cc_rematch_window(&S, &R, &e, &p), CC_OK);
            CHECK_EQ(p.why, CC_WHY_NOT_A_WINDOW);
        }
        const cc_encounter again = enc(77, CC_ENC_BOSS, 0);
        CHECK_STATUS(cc_rematch_window(&S, &R, &again, &p), CC_OK);
        CHECK_EQ(p.kind, CC_PROP_OFFER);
    }
    CHECK(unchanged());
    for (int i = 0; i < 4; i++) CHECK_EQ(rand(), ambient[i]);
    CHECK_STATUS(cc_rematch_view_of(NULL, &(cc_rematch_view){0}), CC_E_NULL);
    cc_rematch_view vv;
    CHECK_STATUS(cc_rematch_view_of(&S, NULL), CC_E_NULL);
    cc_schedule bad = S;
    bad.state = CC_SCHED_NONE;
    CHECK_STATUS(cc_rematch_view_of(&bad, &vv), CC_E_RANGE);
}

/* ------------------------------------------------------------------ a long random sequence of everything */

static uint32_t g_t = 0x12345678u;
static uint32_t trnd(void)
{
    g_t ^= g_t << 13;
    g_t ^= g_t >> 17;
    g_t ^= g_t << 5;
    return g_t;
}

static void test_random_runs(void)
{
    /* the test's own generator (not the policy stream). Each round is a run: release, start, windows, answer, result, claim, end.
     * Invariants after every call: both structures valid, at most one record held, a reward paid at most once per record. */
    static cc_meta m;
    cc_meta_init(&m, 9);
    static unsigned claims[8192];   // by record id: ids are never reused, so a second payment of one id is a bug
    memset(claims, 0, sizeof claims);
    unsigned offers = 0, accepts = 0, declines = 0, wins = 0, losses = 0, exhausted = 0, none = 0, paid = 0, replays = 0;
    for (int round = 0; round < 3000; round++) {
        cc_run_uid uid = 0;
        CHECK_STATUS(cc_alloc_run_uid(&m.counters, &uid), CC_OK);
        /* grow the roster with records from earlier runs (as terminal transactions would) */
        if (m.roster.count < CC_MAX_NEMESES && trnd() % 3 == 0) {
            cc_record_id id;
            CHECK_STATUS(cc_alloc_record_id(&m.counters, &id), CC_OK);
            cc_nemesis n = mk_nem(id, uid, 5000u + (uint32_t)round);
            m.roster.rec[m.roster.count++] = n;
            CHECK_STATUS(cc_alloc_run_uid(&m.counters, &uid), CC_OK);        // the run being played is a later one
        }
        R = m.roster;
        unsigned freed = 0;
        CHECK_STATUS(cc_roster_release_except(&R, uid, &freed), CC_OK);
        CHECK_EQ(freed, 0);
        const unsigned n_rep = R.count;
        uint8_t rep[CC_MAX_NEMESES];
        for (unsigned i = 0; i < n_rep; i++) rep[i] = trnd() % 5 != 0;
        const uint64_t seed_hi = trnd();      // sequenced on purpose: C leaves the order of two calls in one expression open
        const uint64_t seed = (seed_hi << 32) | trnd();
        const bool use_mask = trnd() % 2;
        CHECK_STATUS(cc_rematch_start(&S, &R, uid, seed, use_mask ? rep : NULL), CC_OK);
        both_valid();
        const uint64_t draw_state = S.rng_state;
        bool answered = false, accepted = false;
        for (int w = 0; w < CC_NUM_WINDOWS; w++) {
            cc_proposal p;
            const uint8_t kind = (uint8_t)(trnd() % 8 == 0 ? CC_ENC_ORDINARY_WILD : trnd() % CC_ENC_COUNT_);
            const cc_encounter e = enc(cc_window_waves[w], kind, (uint8_t)(trnd() % 4 == 0 ? trnd() % 8 : 0));
            CHECK_STATUS(cc_rematch_window(&S, &R, &e, &p), CC_OK);
            both_valid();
            if (p.kind == CC_PROP_OFFER) {
                offers++;
                CHECK(p.record_id == S.chosen_id && p.reward_vouchers == 1);
                bool rp;
                if (trnd() % 2) {
                    CHECK_STATUS(cc_rematch_decide(&S, &R, true, &rp), CC_OK);
                    CHECK_STATUS(cc_rematch_decide(&S, &R, true, &rp), CC_OK);
                    CHECK(rp);
                    accepts++;
                    accepted = true;
                } else {
                    CHECK_STATUS(cc_rematch_decide(&S, &R, false, &rp), CC_OK);
                    declines++;
                }
                answered = true;
                both_valid();
            } else if (p.why == CC_WHY_NO_CANDIDATE) {
                none++;
            }
            CHECK(S.rng_state == draw_state);                   // nothing ever draws again
            if (answered) break;
        }
        if (S.state == CC_SCHED_EXHAUSTED) exhausted++;
        if (accepted) {
            const bool won = trnd() % 2;
            bool rp;
            CHECK_STATUS(cc_rematch_result(&S, &R, won, &rp), CC_OK);
            CHECK_STATUS(cc_rematch_result(&S, &R, won, &rp), CC_OK);
            CHECK(rp);
            both_valid();
            if (won) wins++; else losses++;
        }
        m.roster = R;
        CHECK_STATUS(cc_meta_validate(&m), CC_OK);
        /* claims: any defeated record, sometimes twice */
        for (unsigned i = 0; i < m.roster.count; i++) {
            if (m.roster.rec[i].life != CC_LIFE_DEFEATED || trnd() % 3 == 0) continue;
            cc_claim_result c;
            const cc_record_id id = m.roster.rec[i].record_id;
            CHECK(id < 8192);
            CHECK_STATUS(cc_reward_claim(&m, id, &c), CC_OK);
            if (!c.replayed) {
                paid++;
                claims[id]++;
                CHECK(claims[id] == 1);
            } else {
                replays++;
            }
            CHECK_STATUS(cc_reward_claim(&m, id, &c), CC_OK);
            CHECK(c.replayed);
            replays++;
        }
        CHECK_STATUS(cc_meta_validate(&m), CC_OK);
        /* the run ends: whatever it still holds goes back; a full roster of finished records lets the oldest go */
        CHECK_STATUS(cc_roster_release(&m.roster, uid, NULL), CC_OK);
        CHECK_EQ(held(&m.roster), 0);
        if (m.roster.count == CC_MAX_NEMESES) {
            for (unsigned i = 0; i < m.roster.count; i++)
                if (cc_life_overwritable(m.roster.rec[i].life)) {
                    for (unsigned j = i + 1; j < m.roster.count; j++) m.roster.rec[j - 1] = m.roster.rec[j];
                    m.roster.count--;
                    break;
                }
        }
        CHECK_STATUS(cc_meta_validate(&m), CC_OK);
    }
    for (unsigned i = 0; i < 8192; i++) CHECK(claims[i] <= 1);
    /* the walk really reached every branch */
    CHECK(offers > 100 && accepts > 50 && declines > 50 && wins > 20 && losses > 20 && exhausted > 20 && none > 20 && paid > 10 && replays > 10);
    printf("random runs: %u offers (%u accepted, %u declined), %u won, %u lost, %u exhausted, %u without a candidate, %u rewards paid, %u replays\n",
           offers, accepts, declines, wins, losses, exhausted, none, paid, replays);
}

int main(void)
{
    test_stream();
    test_classify();
    test_start_no_candidate();
    test_start_draws();
    test_start_eligibility();
    test_start_refusals();
    test_windows_offer_at_first_eligible();
    test_windows_each_skip();
    test_windows_exhausted();
    test_windows_not_a_window_and_order();
    test_windows_disagreement();
    test_decline();
    test_accept_and_win();
    test_accept_and_lose();
    test_answer_refusals();
    test_claim_refusals();
    test_release();
    test_reload_and_scouting();
    test_random_runs();
    return cc_summary("scheduler_test");
}
