/* Behavior tests of the history reducer (C02): literal fixtures with hand-written expected values, then a differential
 * run against a deliberately naive model. Every state a test produces is also run through cc_reducer_validate.
 *
 * Species 25 and 19 are only labels. HP values are chosen so each expected delta can be read off the fixture. */
#include <string.h>

#include "cc/reducer.h"
#include "check.h"

#define RUN_A 0x0000000500000001ULL
#define RUN_B 0x0000000500000002ULL
#define RUN_C 0x0000000500000003ULL

/* The reducers and the batch array are big: static, as the header asks of real users. */
static cc_reducer R, R2, SAVED;
static cc_obs OBS[CC_MAX_BATCH_OBS + 8];

/* ------------------------------------------------------------------ builders */

static cc_obs base_obs(uint8_t kind, int pos, unsigned species, int hp)
{
    cc_obs o;
    memset(&o, 0, sizeof o);
    o.kind = kind;
    o.battler = (int8_t)pos;
    o.target = -1;
    o.battler_species = (uint16_t)species;
    o.hp_after = hp;
    o.hp_max_after = hp < 0 ? -1 : 200;
    return o;
}

static cc_obs o_dmg(int pos, unsigned sp, int shown, int hp, unsigned result)
{
    cc_obs o = base_obs(CC_OBS_DAMAGE, pos, sp, hp);
    o.id = (uint16_t)result;
    o.value = shown;
    return o;
}

static cc_obs o_heal(int pos, unsigned sp, int amount, int hp)
{
    cc_obs o = base_obs(CC_OBS_HP, pos, sp, hp);
    o.value = amount;
    return o;
}

static cc_obs o_loss(int pos, unsigned sp, int amount, int hp, unsigned status)
{
    cc_obs o = base_obs(CC_OBS_HP, pos, sp, hp);
    o.value = -amount;
    o.status_after = (uint8_t)status;
    return o;
}

static cc_obs o_faint(int pos, unsigned sp, int hp) { return base_obs(CC_OBS_FAINT, pos, sp, hp); }
static cc_obs o_out(int pos, unsigned sp, int hp) { return base_obs(CC_OBS_SWITCH_OUT, pos, sp, hp); }

static cc_obs o_in(int pos, unsigned sp, int hp, int slot)
{
    cc_obs o = base_obs(CC_OBS_SWITCH_IN, pos, sp, hp);
    o.value = slot;
    return o;
}

static cc_obs o_anim(int user, unsigned user_sp, int target, unsigned target_sp, unsigned move)
{
    cc_obs o = base_obs(CC_OBS_MOVE_ANIM, user, user_sp, -1);
    o.target = (int8_t)target;
    o.target_species = (uint16_t)target_sp;
    o.id = (uint16_t)move;
    return o;
}

static cc_batch mk(uint32_t id, uint32_t battle, unsigned wave, unsigned turn, const cc_obs *obs, unsigned n)
{
    cc_batch b;
    memset(&b, 0, sizeof b);
    b.run_uid = RUN_A;
    b.batch_id = id;
    b.battle_seq = battle;
    b.wave = (uint16_t)wave;
    b.turn = (uint16_t)turn;
    b.n_obs = (uint16_t)n;
    b.obs = obs;
    return b;
}

static void fresh(cc_reducer *r, cc_run_uid uid)
{
    CHECK_STATUS(cc_reducer_init(r), CC_OK);
    CHECK_STATUS(cc_reducer_begin_run(r, uid, false), CC_OK);
}

/* accept a batch and re-validate the state */
static cc_reduce_result go_(int line, cc_reducer *r, const cc_batch *b)
{
    cc_reduce_result res;
    memset(&res, 0x5A, sizeof res);
    const cc_status s = cc_reduce(r, b, &res);
    cc_checks++;
    if (s != CC_OK) {
        cc_failures++;
        printf("FAIL %s:%d: cc_reduce returned %d (%s)\n", __FILE__, line, (int)s, cc_status_str(s));
    }
    const cc_status v = cc_reducer_validate(r);
    cc_checks++;
    if (v != CC_OK) {
        cc_failures++;
        printf("FAIL %s:%d: state invalid after cc_reduce: %d (%s)\n", __FILE__, line, (int)v, cc_status_str(v));
    }
    return res;
}
#define GO(r, b) go_(__LINE__, (r), &(b))

static bool result_clear(const cc_reduce_result *x)
{
    return !x->duplicate && !x->new_turn && !x->turn_evicted && !x->gap && !x->events_added && !x->ignored &&
           !x->evicted && !x->sign_conflicts;
}

/* a rejected batch: the status, the index of the bad observation, an all-zero result and a byte-identical reducer */
#define REJECT(r_, b_, want_, bad_) \
    do { \
        cc_reduce_result rr_; \
        memset(&rr_, 0x5A, sizeof rr_); \
        memcpy(&SAVED, (r_), sizeof SAVED); \
        CHECK_STATUS(cc_reduce((r_), &(b_), &rr_), (want_)); \
        CHECK(memcmp(&SAVED, (r_), sizeof SAVED) == 0); \
        CHECK_EQ(rr_.bad_obs, (bad_)); \
        CHECK(result_clear(&rr_)); \
    } while (0)

static void expect_event(int line, const cc_event *e, uint32_t seq, unsigned kind, unsigned side, unsigned species,
                         unsigned known, int32_t shown, int32_t hp_delta, int32_t hp_after)
{
    int bad = 0;
    bad |= e->seq != seq;
    bad |= e->kind != kind;
    bad |= e->target_side != side;
    bad |= e->target_species != species;
    bad |= e->known != known;
    bad |= e->shown != shown;
    bad |= e->hp_delta != hp_delta;
    bad |= e->hp_after != hp_after;
    cc_checks++;
    if (bad) {
        cc_failures++;
        printf("FAIL %s:%d: event seq %u kind %u side %u species %u known 0x%x shown %d delta %d after %d; expected seq %u "
               "kind %u side %u species %u known 0x%x shown %d delta %d after %d\n",
               __FILE__, line, (unsigned)e->seq, (unsigned)e->kind, (unsigned)e->target_side, (unsigned)e->target_species,
               (unsigned)e->known, (int)e->shown, (int)e->hp_delta, (int)e->hp_after, (unsigned)seq, kind, side, species,
               known, (int)shown, (int)hp_delta, (int)hp_after);
    }
}
#define EXPECT(e, seq, kind, side, sp, known, shown, delta, after) \
    expect_event(__LINE__, &(e), (seq), (kind), (side), (sp), (known), (shown), (delta), (after))

#define TURN(r, i) ((r)->hist.turns[i])

/* ------------------------------------------------------------------ runs, empty state */

static void test_runs_and_empty(void)
{
    cc_reducer *r = &R;
    CHECK_STATUS(cc_reducer_init(NULL), CC_E_NULL);
    CHECK_STATUS(cc_reducer_init(r), CC_OK);
    CHECK_STATUS(cc_reducer_validate(r), CC_OK);
    CHECK_STATUS(cc_reducer_validate(NULL), CC_E_NULL);
    CHECK_EQ(r->run_uid, 0);
    CHECK_EQ(r->next_seq, 1);
    CHECK_EQ(r->hist.n_turns, 0);
    CHECK_EQ(r->last_batch_id, 0);

    /* nothing can be reduced before a run is begun, or ended without one */
    cc_obs o = o_dmg(2, 19, 5, 95, 3);
    cc_batch b = mk(1, 1, 1, 1, &o, 1);
    REJECT(r, b, CC_E_LIFECYCLE, -1);
    CHECK_STATUS(cc_reducer_end_run(r), CC_E_LIFECYCLE);
    CHECK_STATUS(cc_reducer_end_run(NULL), CC_E_NULL);
    CHECK_STATUS(cc_reducer_begin_run(NULL, RUN_A, false), CC_E_NULL);
    CHECK_STATUS(cc_reducer_begin_run(r, 0, false), CC_E_RANGE);
    CHECK_STATUS(cc_reducer_validate(r), CC_OK);

    CHECK_STATUS(cc_reducer_begin_run(r, RUN_A, false), CC_OK);
    CHECK_EQ(r->run_uid, RUN_A);
    CHECK_EQ(r->hist.flags, 0);
    CHECK_STATUS(cc_reducer_validate(r), CC_OK);
    CHECK_STATUS(cc_reducer_begin_run(r, RUN_A, false), CC_E_DUPLICATE);     /* the same run again: refused, unchanged */
    CHECK_STATUS(cc_reducer_begin_run(r, RUN_A, true), CC_E_DUPLICATE);
    CHECK_EQ(r->hist.flags, 0);

    /* an accepted batch with nothing in it still opens its turn: empty turn 0, "before the first turn" */
    cc_batch e = mk(1, 1, 1, 0, NULL, 0);
    cc_reduce_result res = GO(r, e);
    CHECK_EQ(res.new_turn, 1);
    CHECK_EQ(res.events_added, 0);
    CHECK_EQ(res.bad_obs, -1);
    CHECK_EQ(r->hist.n_turns, 1);
    CHECK_EQ(TURN(r, 0).battle_seq, 1);
    CHECK_EQ(TURN(r, 0).wave, 1);
    CHECK_EQ(TURN(r, 0).turn, 0);
    CHECK_EQ(TURN(r, 0).n_events, 0);
    CHECK_EQ(TURN(r, 0).flags, 0);
    CHECK_EQ(r->next_seq, 1);
    CHECK_EQ(r->last_batch_id, 1);

    /* a run resumed from a save: the history says older turns could not be seen */
    CHECK_STATUS(cc_reducer_begin_run(r, RUN_B, true), CC_OK);
    CHECK_EQ(r->run_uid, RUN_B);
    CHECK_EQ(r->hist.n_turns, 0);
    CHECK_EQ(r->hist.flags, CC_HF_BEGAN_MID);
    CHECK_EQ(r->last_batch_id, 0);
    CHECK_EQ(r->next_seq, 1);
    CHECK_STATUS(cc_reducer_validate(r), CC_OK);

    /* ending: refused afterwards, idempotent, and the uid is not reusable */
    CHECK_STATUS(cc_reducer_end_run(r), CC_OK);
    CHECK_EQ(r->closed, 1);
    CHECK_STATUS(cc_reducer_end_run(r), CC_OK);
    CHECK_STATUS(cc_reducer_validate(r), CC_OK);
    b = mk(1, 1, 1, 1, &o, 1);
    b.run_uid = RUN_B;
    REJECT(r, b, CC_E_LIFECYCLE, -1);
    CHECK_STATUS(cc_reducer_begin_run(r, RUN_B, false), CC_E_DUPLICATE);
    CHECK_EQ(r->closed, 1);
    CHECK_STATUS(cc_reducer_begin_run(r, RUN_C, false), CC_OK);              /* a new run replaces it entirely */
    CHECK_EQ(r->closed, 0);
    CHECK_EQ(r->hist.flags, 0);
    b.run_uid = RUN_C;
    cc_reduce_result r2 = GO(r, b);
    CHECK_EQ(r2.new_turn, 1);
}

/* ------------------------------------------------------------------ rejected batches change nothing */

static void test_rejections(void)
{
    cc_reducer *r = &R;
    fresh(r, RUN_A);
    cc_obs ok[3] = {o_in(2, 19, 100, 0), o_dmg(2, 19, 10, 90, 3), o_dmg(2, 19, 10, 80, 3)};
    cc_batch b = mk(1, 1, 1, 1, ok, 3);
    cc_reduce_result res = GO(r, b);
    CHECK_EQ(res.events_added, 3);

    /* arguments */
    cc_reduce_result rr;
    memset(&rr, 0x5A, sizeof rr);
    CHECK_STATUS(cc_reduce(NULL, &b, &rr), CC_E_NULL);
    CHECK(result_clear(&rr) && rr.bad_obs == -1);
    memset(&rr, 0x5A, sizeof rr);
    CHECK_STATUS(cc_reduce(r, NULL, &rr), CC_E_NULL);
    CHECK(result_clear(&rr) && rr.bad_obs == -1);
    CHECK_STATUS(cc_reduce(r, &b, NULL), CC_OK);                            /* the replay of the batch above: out may be NULL */
    cc_batch c = mk(2, 1, 1, 1, NULL, 3);
    REJECT(r, c, CC_E_NULL, -1);
    c = mk(2, 1, 1, 1, ok, 3);
    c.flags = 4;
    REJECT(r, c, CC_E_UNKNOWN_BITS, -1);
    c.flags = 0x80;
    REJECT(r, c, CC_E_UNKNOWN_BITS, -1);
    c = mk(2, 1, 1, 1, OBS, CC_MAX_BATCH_OBS + 1);
    REJECT(r, c, CC_E_COUNT, -1);
    c = mk(2, 1, 1, 1, ok, 3);
    c.run_uid = 0;
    REJECT(r, c, CC_E_RANGE, -1);
    c = mk(0, 1, 1, 1, ok, 3);
    REJECT(r, c, CC_E_RANGE, -1);
    c = mk(2, 0, 1, 1, ok, 3);
    REJECT(r, c, CC_E_RANGE, -1);
    c = mk(2, 1, 0, 1, ok, 3);
    REJECT(r, c, CC_E_RANGE, -1);
    c = mk(2, 1, 1, 1, ok, 3);
    c.run_uid = RUN_B;                                                      /* a batch of another run */
    REJECT(r, c, CC_E_CONSISTENCY, -1);

    /* batch ids */
    c = mk(2, 1, 1, 1, ok, 3);                                              /* a fresh id: accepted below */
    c.batch_id = 1;
    c.n_obs = 2;                                                            /* the id of the last batch, other content */
    REJECT(r, c, CC_E_DUPLICATE, -1);
    c = mk(2, 1, 1, 1, ok, 1);
    b = mk(5, 1, 1, 2, ok, 1);
    res = GO(r, b);                                                         /* ids may skip (see test_gap) */
    c = mk(4, 1, 1, 3, ok, 1);                                              /* older than the last accepted (5) */
    REJECT(r, c, CC_E_ORDER, -1);

    /* turn keys */
    c = mk(6, 1, 1, 1, ok, 1);                                              /* an older turn of the same battle */
    REJECT(r, c, CC_E_ORDER, -1);
    c = mk(6, 1, 2, 2, ok, 1);                                              /* same battle, other wave */
    REJECT(r, c, CC_E_CONSISTENCY, -1);
    b = mk(6, 3, 5, 1, ok, 1);
    res = GO(r, b);
    c = mk(7, 2, 5, 1, ok, 1);
    REJECT(r, c, CC_E_ORDER, -1);                                           /* battle 2 after battle 3 */
    c = mk(7, 3, 4, 1, ok, 1);                                              /* battle 3 again with a lower wave */
    REJECT(r, c, CC_E_CONSISTENCY, -1);
    c = mk(7, 4, 4, 1, ok, 1);                                              /* battle 4 at a lower wave than battle 3 */
    REJECT(r, c, CC_E_ORDER, -1);
    c = mk(7, 3, 5, 1, ok, 1);
    c.flags = CC_BF_BEGAN_MID;                                              /* "began late" on a turn that already exists */
    REJECT(r, c, CC_E_CONSISTENCY, -1);

    /* observations: the first bad one is named */
    cc_obs bad[4] = {o_in(2, 19, 100, 0), o_dmg(2, 19, 10, 90, 3), o_dmg(2, 19, 10, 80, 3), o_dmg(2, 19, 10, 70, 3)};
    bad[2].kind = CC_OBS_NONE;
    bad[3].battler = 7;
    c = mk(7, 3, 5, 1, bad, 4);
    REJECT(r, c, CC_E_RANGE, 2);
    bad[2] = o_dmg(2, 19, 10, 80, 16);                                      /* HitResult out of range */
    REJECT(r, c, CC_E_RANGE, 2);
    bad[2] = o_dmg(2, 19, 10, 80, 3);
    bad[3] = o_dmg(2, 19, 10, 70, 3);
    bad[0] = o_loss(2, 19, 8, 90, 1);
    bad[0].src_known = CC_EK_MOVE;                                          /* a loss without a hit never names a source */
    bad[0].src_move = 33;
    REJECT(r, c, CC_E_CONSISTENCY, 0);
    bad[0] = o_in(2, 19, 100, 0);
    bad[1] = o_dmg(2, 19, 10, 90, 3);
    res = GO(r, c);                                                         /* with every observation valid it is accepted */
    CHECK_EQ(res.events_added, 4);

    /* a corrupted state must not be indexed */
    cc_batch d = mk(8, 3, 5, 2, ok, 1);
    memcpy(&SAVED, r, sizeof SAVED);
    r->hist.n_turns = CC_HISTORY_TURNS + 1;
    CHECK_STATUS(cc_reduce(r, &d, NULL), CC_E_CONSISTENCY);
    r->hist.n_turns = 200;                                                  /* far past the array: indexing it would be caught */
    CHECK_STATUS(cc_reduce(r, &d, NULL), CC_E_CONSISTENCY);
    r->hist.n_turns = SAVED.hist.n_turns;
    TURN(r, r->hist.n_turns - 1).n_events = CC_MAX_EVENTS_PER_TURN + 1;
    CHECK_STATUS(cc_reduce(r, &d, NULL), CC_E_CONSISTENCY);
    TURN(r, r->hist.n_turns - 1).n_events = SAVED.hist.turns[SAVED.hist.n_turns - 1].n_events;
    CHECK(memcmp(&SAVED, r, sizeof SAVED) == 0);
    cc_batch gapped = mk(20, 3, 5, 2, ok, 1);                               /* far past the last id: would be a gap */
    memset(&r->hist, 0, sizeof r->hist);                                    /* batches accepted, yet no turn */
    r->next_seq = 1;
    REJECT(r, gapped, CC_E_CONSISTENCY, -1);
    memcpy(r, &SAVED, sizeof *r);

    /* the sequence counter never wraps */
    fresh(r, RUN_A);
    r->next_seq = UINT32_MAX - 2;
    cc_obs three[3] = {o_dmg(2, 19, 1, -1, 3), o_dmg(2, 19, 2, -1, 3), o_dmg(2, 19, 3, -1, 3)};
    b = mk(1, 1, 1, 1, three, 3);
    REJECT(r, b, CC_E_OVERFLOW, -1);
    b = mk(1, 1, 1, 1, three, 2);
    res = GO(r, b);
    CHECK_EQ(res.events_added, 2);
    CHECK_EQ(r->next_seq, UINT32_MAX);
    CHECK_EQ(TURN(r, 0).events[1].seq, UINT32_MAX - 1);
    b = mk(2, 1, 1, 1, three, 1);
    REJECT(r, b, CC_E_OVERFLOW, -1);                                        /* UINT32_MAX itself is never handed out */
    b = mk(2, 1, 1, 1, three, 0);
    res = GO(r, b);                                                         /* an empty batch needs no number */
    CHECK_EQ(res.events_added, 0);
}

/* ------------------------------------------------------------------ what each raw event becomes */

static void test_mapping(void)
{
    cc_reducer *r = &R;
    fresh(r, RUN_A);
    cc_obs o[9];
    o[0] = o_in(0, 25, 100, 0);
    o[1] = o_in(2, 19, 40, 0);
    o[2] = o_anim(0, 25, 2, 19, 84);
    o[3] = o_dmg(2, 19, 12, 28, 3);
    o[4] = o_dmg(0, 25, 30, 70, 1);
    o[5] = o_heal(0, 25, 20, 80);
    o[6] = o_loss(0, 25, 8, 72, 2);
    o[7] = o_faint(2, 19, 0);
    o[8] = o_out(0, 25, 72);
    cc_batch b = mk(1, 1, 1, 1, o, 9);
    cc_reduce_result res = GO(r, b);
    CHECK_EQ(res.new_turn, 1);
    CHECK_EQ(res.events_added, 8);
    CHECK_EQ(res.ignored, 1);                                               /* the move animation */
    CHECK_EQ(res.evicted, 0);
    CHECK_EQ(res.sign_conflicts, 0);
    CHECK_EQ(res.duplicate, 0);
    CHECK_EQ(res.bad_obs, -1);
    const cc_turn *t = &TURN(r, 0);
    CHECK_EQ(t->n_events, 8);
    CHECK_EQ(t->flags, 0);
    CHECK_EQ(t->dropped, 0);
    EXPECT(t->events[0], 1, CC_EV_SWITCH_IN, CC_SIDE_PLAYER, 25, CC_EK_HP_AFTER, 0, 0, 100);
    EXPECT(t->events[1], 2, CC_EV_SWITCH_IN, CC_SIDE_ENEMY, 19, CC_EK_HP_AFTER, 0, 0, 40);
    EXPECT(t->events[2], 3, CC_EV_DAMAGE, CC_SIDE_ENEMY, 19,
           CC_EK_RESULT | CC_EK_SHOWN | CC_EK_HP_DELTA | CC_EK_HP_AFTER, 12, -12, 28);
    CHECK_EQ(t->events[2].result, 3);
    EXPECT(t->events[3], 4, CC_EV_DAMAGE, CC_SIDE_PLAYER, 25,
           CC_EK_RESULT | CC_EK_SHOWN | CC_EK_HP_DELTA | CC_EK_HP_AFTER, 30, -30, 70);
    CHECK_EQ(t->events[3].result, 1);
    /* the heal shows 20 although only 10 HP were gained: both numbers are kept, apart */
    EXPECT(t->events[4], 5, CC_EV_HEAL, CC_SIDE_PLAYER, 25, CC_EK_SHOWN | CC_EK_HP_DELTA | CC_EK_HP_AFTER, 20, 10, 80);
    /* status damage: the engine's own measurement is hp_delta, nothing is "shown", nothing is attributed */
    EXPECT(t->events[5], 6, CC_EV_HP_LOSS, CC_SIDE_PLAYER, 25, CC_EK_HP_DELTA | CC_EK_HP_AFTER, 0, -8, 72);
    EXPECT(t->events[6], 7, CC_EV_FAINT, CC_SIDE_ENEMY, 19, CC_EK_HP_AFTER, 0, 0, 0);
    EXPECT(t->events[7], 8, CC_EV_SWITCH_OUT, CC_SIDE_PLAYER, 25, CC_EK_HP_AFTER, 0, 0, 72);
    CHECK_EQ(r->next_seq, 9);
    for (unsigned p = 0; p <= CC_BATTLER_MAX; p++)
        CHECK_EQ(r->pos[p].species, 0);                                     /* everyone left or fainted */
    for (unsigned i = 0; i < 8; i++) {
        CHECK_EQ(t->events[i].source_side | t->events[i].source_species | t->events[i].move_id | t->events[i].target_id, 0);
        CHECK_EQ(t->events[i].known & (CC_EK_SOURCE_SIDE | CC_EK_SOURCE_ID | CC_EK_SOURCE_SPECIES | CC_EK_MOVE | CC_EK_TARGET_ID), 0);
    }

    /* a Pokemon id the adapter derived is kept as the target id; forms are carried */
    cc_obs f[2] = {o_dmg(2, 19, 5, -1, 0), o_dmg(3, 25, 6, -1, 0)};
    f[0].battler_id = 77;
    f[0].battler_form = 2;
    b = mk(2, 1, 1, 2, f, 2);
    res = GO(r, b);
    t = &TURN(r, 1);
    EXPECT(t->events[0], 9, CC_EV_DAMAGE, CC_SIDE_ENEMY, 19, CC_EK_TARGET_ID | CC_EK_RESULT | CC_EK_SHOWN, 5, 0, 0);
    CHECK_EQ(t->events[0].target_id, 77);
    CHECK_EQ(t->events[0].target_form, 2);
    CHECK_EQ(t->events[0].result, 0);                                       /* a known result of 0 is a measured 0 */
    EXPECT(t->events[1], 10, CC_EV_DAMAGE, CC_SIDE_ENEMY, 25, CC_EK_RESULT | CC_EK_SHOWN, 6, 0, 0);   /* position 3 is an enemy */
    CHECK_EQ(t->events[1].target_id, 0);
    CHECK_EQ(t->events[1].target_form, 0);

    /* positions 0 and 1 are the player, 2 and 3 the enemy */
    cc_obs s[4] = {o_dmg(0, 1, 1, -1, 1), o_dmg(1, 1, 1, -1, 1), o_dmg(2, 1, 1, -1, 1), o_dmg(3, 1, 1, -1, 1)};
    b = mk(3, 1, 1, 3, s, 4);
    res = GO(r, b);
    t = &TURN(r, 2);
    CHECK_EQ(t->events[0].target_side, CC_SIDE_PLAYER);
    CHECK_EQ(t->events[1].target_side, CC_SIDE_PLAYER);
    CHECK_EQ(t->events[2].target_side, CC_SIDE_ENEMY);
    CHECK_EQ(t->events[3].target_side, CC_SIDE_ENEMY);

    /* a PR_EV_HP of 0 cannot be told from a heal of 0: nothing is recorded, nothing is guessed */
    cc_obs z[3] = {o_heal(0, 25, 0, 50), o_dmg(0, 25, 4, 46, 2), o_heal(0, 25, 0, 46)};
    b = mk(4, 1, 1, 4, z, 3);
    res = GO(r, b);
    CHECK_EQ(res.events_added, 1);
    CHECK_EQ(res.ignored, 2);
    CHECK_EQ(TURN(r, 3).n_events, 1);
    /* ... and an ignored observation does not move the HP tracker: the 50 was never taken as a snapshot, so no delta */
    EXPECT(TURN(r, 3).events[0], 11 + 4, CC_EV_DAMAGE, CC_SIDE_PLAYER, 25, CC_EK_RESULT | CC_EK_SHOWN | CC_EK_HP_AFTER, 4, 0, 46);
    cc_obs y[4] = {o_dmg(1, 25, 10, 90, 3), o_heal(1, 25, 0, 80), o_anim(1, 25, 2, 19, 33), o_dmg(1, 25, 10, 70, 3)};
    b = mk(5, 1, 1, 5, y, 4);
    res = GO(r, b);
    CHECK_EQ(res.events_added, 2);
    CHECK_EQ(res.ignored, 2);
    CHECK_EQ(TURN(r, 4).events[0].known & CC_EK_HP_DELTA, 0);
    CHECK_EQ(TURN(r, 4).events[1].hp_delta, -20);                           /* 90 -> 70, not 80 -> 70 */
}

/* ------------------------------------------------------------------ attribution is never invented */

static void test_attribution(void)
{
    cc_reducer *r = &R;
    fresh(r, RUN_A);

    /* nothing in an event names an attacker: a move animation before a faint is not a cause */
    cc_obs a[4] = {o_in(0, 25, 50, 0), o_anim(2, 19, 0, 25, 33), o_loss(0, 25, 50, 0, 1), o_faint(0, 25, 0)};
    cc_batch b = mk(1, 1, 1, 1, a, 4);
    cc_reduce_result res = GO(r, b);
    CHECK_EQ(res.events_added, 3);
    CHECK_EQ(res.ignored, 1);
    const cc_turn *t = &TURN(r, 0);
    EXPECT(t->events[1], 2, CC_EV_HP_LOSS, CC_SIDE_PLAYER, 25, CC_EK_HP_DELTA | CC_EK_HP_AFTER, 0, -50, 0);
    EXPECT(t->events[2], 3, CC_EV_FAINT, CC_SIDE_PLAYER, 25, CC_EK_HP_AFTER, 0, 0, 0);
    CHECK_EQ(t->events[2].source_side | t->events[2].source_form | t->events[2].source_species | t->events[2].move_id |
                 t->events[2].source_id,
             0);

    /* an attribution the adapter asserts is copied exactly, bit by bit */
    cc_obs s[5];
    s[0] = o_dmg(2, 19, 9, -1, 2);
    s[0].src_known = CC_EK_SOURCE_SIDE | CC_EK_SOURCE_ID | CC_EK_SOURCE_SPECIES | CC_EK_MOVE;
    s[0].src_side = CC_SIDE_PLAYER;
    s[0].src_id = 4242;
    s[0].src_species = 6;
    s[0].src_form = 1;
    s[0].src_move = 52;
    s[1] = o_dmg(2, 19, 9, -1, 2);
    s[1].src_known = CC_EK_MOVE;
    s[1].src_move = 920;
    s[2] = o_heal(0, 25, 7, -1);
    s[2].src_known = CC_EK_SOURCE_SIDE;
    s[2].src_side = CC_SIDE_ENEMY;
    s[3] = o_faint(2, 19, -1);
    s[3].src_known = CC_EK_SOURCE_SIDE | CC_EK_SOURCE_SPECIES;
    s[3].src_side = CC_SIDE_PLAYER;
    s[3].src_species = 8901;
    s[4] = o_dmg(2, 19, 9, -1, 2);                                          /* no attribution at all */
    b = mk(2, 1, 1, 2, s, 5);
    res = GO(r, b);
    t = &TURN(r, 1);
    EXPECT(t->events[0], 4, CC_EV_DAMAGE, CC_SIDE_ENEMY, 19,
           CC_EK_RESULT | CC_EK_SHOWN | CC_EK_SOURCE_SIDE | CC_EK_SOURCE_ID | CC_EK_SOURCE_SPECIES | CC_EK_MOVE, 9, 0, 0);
    CHECK_EQ(t->events[0].source_side, CC_SIDE_PLAYER);
    CHECK_EQ(t->events[0].source_id, 4242);
    CHECK_EQ(t->events[0].source_species, 6);
    CHECK_EQ(t->events[0].source_form, 1);
    CHECK_EQ(t->events[0].move_id, 52);
    EXPECT(t->events[1], 5, CC_EV_DAMAGE, CC_SIDE_ENEMY, 19, CC_EK_RESULT | CC_EK_SHOWN | CC_EK_MOVE, 9, 0, 0);
    CHECK_EQ(t->events[1].move_id, 920);
    CHECK_EQ(t->events[1].source_side | t->events[1].source_species | t->events[1].source_id, 0);
    EXPECT(t->events[2], 6, CC_EV_HEAL, CC_SIDE_PLAYER, 25, CC_EK_SHOWN | CC_EK_SOURCE_SIDE, 7, 0, 0);
    CHECK_EQ(t->events[2].source_side, CC_SIDE_ENEMY);
    EXPECT(t->events[3], 7, CC_EV_FAINT, CC_SIDE_ENEMY, 19, CC_EK_SOURCE_SIDE | CC_EK_SOURCE_SPECIES, 0, 0, 0);
    CHECK_EQ(t->events[3].source_species, 8901);
    CHECK_EQ(t->events[3].source_form, 0);
    EXPECT(t->events[4], 8, CC_EV_DAMAGE, CC_SIDE_ENEMY, 19, CC_EK_RESULT | CC_EK_SHOWN, 9, 0, 0);
    CHECK_EQ(t->events[4].move_id | t->events[4].source_side | t->events[4].source_id | t->events[4].source_species, 0);

    /* the position that was hit and the one that hit are not confused: side comes from the battler, not from the source */
    CHECK_EQ(t->events[0].target_side, CC_SIDE_ENEMY);
}

/* ------------------------------------------------------------------ shown and measured stay apart */

static void test_shown_vs_measured(void)
{
    cc_reducer *r = &R;
    cc_turn_summary sm;

    /* hits whose snapshots were taken in order: 100 -> 90 -> 78 -> 67 */
    fresh(r, RUN_A);
    cc_obs seq[4] = {o_in(2, 19, 100, 0), o_dmg(2, 19, 10, 90, 3), o_dmg(2, 19, 12, 78, 3), o_dmg(2, 19, 11, 67, 3)};
    cc_batch b = mk(1, 1, 1, 1, seq, 4);
    GO(r, b);
    const cc_turn *t = &TURN(r, 0);
    EXPECT(t->events[1], 2, CC_EV_DAMAGE, CC_SIDE_ENEMY, 19, CC_EK_RESULT | CC_EK_SHOWN | CC_EK_HP_DELTA | CC_EK_HP_AFTER, 10, -10, 90);
    EXPECT(t->events[2], 3, CC_EV_DAMAGE, CC_SIDE_ENEMY, 19, CC_EK_RESULT | CC_EK_SHOWN | CC_EK_HP_DELTA | CC_EK_HP_AFTER, 12, -12, 78);
    EXPECT(t->events[3], 4, CC_EV_DAMAGE, CC_SIDE_ENEMY, 19, CC_EK_RESULT | CC_EK_SHOWN | CC_EK_HP_DELTA | CC_EK_HP_AFTER, 11, -11, 67);
    CHECK_STATUS(cc_turn_summarize(t, &sm), CC_OK);
    CHECK_EQ(sm.exact, 1);
    CHECK_EQ(sm.damage[CC_SIDE_ENEMY].n, 3);
    CHECK_EQ(sm.damage[CC_SIDE_ENEMY].n_shown, 3);
    CHECK_EQ(sm.damage[CC_SIDE_ENEMY].n_measured, 3);
    CHECK_EQ(sm.damage[CC_SIDE_ENEMY].shown, 33);
    CHECK_EQ(sm.damage[CC_SIDE_ENEMY].measured, 33);                        /* two views of the same 33 HP, never 66 */
    CHECK_EQ(sm.damage[CC_SIDE_PLAYER].n, 0);
    CHECK_EQ(sm.n_switch_in[CC_SIDE_ENEMY], 1);

    /* all three snapshots were taken after the last hit (the event is added when the animation runs, section 1.1) */
    fresh(r, RUN_A);
    cc_obs late[4] = {o_in(2, 19, 100, 0), o_dmg(2, 19, 10, 67, 3), o_dmg(2, 19, 12, 67, 3), o_dmg(2, 19, 11, 67, 3)};
    b = mk(1, 1, 1, 1, late, 4);
    GO(r, b);
    t = &TURN(r, 0);
    CHECK_EQ(t->events[1].shown, 10);
    CHECK_EQ(t->events[2].shown, 12);
    CHECK_EQ(t->events[3].shown, 11);
    CHECK_EQ(t->events[1].hp_delta, -33);                                   /* per event it is not an attribution ... */
    CHECK_EQ(t->events[2].hp_delta, 0);                                     /* ... and a measured 0 is still known */
    CHECK_EQ(t->events[3].hp_delta, 0);
    CHECK_EQ(t->events[2].known & CC_EK_HP_DELTA, CC_EK_HP_DELTA);
    CHECK_STATUS(cc_turn_summarize(t, &sm), CC_OK);
    CHECK_EQ(sm.damage[CC_SIDE_ENEMY].shown, 33);                           /* ... but the totals agree */
    CHECK_EQ(sm.damage[CC_SIDE_ENEMY].measured, 33);

    /* a heal shows more than it gained; status damage is a measurement and has no shown amount */
    fresh(r, RUN_A);
    cc_obs hl[4] = {o_in(0, 25, 70, 0), o_heal(0, 25, 20, 80), o_loss(0, 25, 8, 72, 2), o_loss(0, 25, 5, 67, 2)};
    b = mk(1, 1, 1, 1, hl, 4);
    GO(r, b);
    CHECK_STATUS(cc_turn_summarize(&TURN(r, 0), &sm), CC_OK);
    CHECK_EQ(sm.heal[CC_SIDE_PLAYER].n, 1);
    CHECK_EQ(sm.heal[CC_SIDE_PLAYER].shown, 20);
    CHECK_EQ(sm.heal[CC_SIDE_PLAYER].measured, 10);
    CHECK_EQ(sm.hp_loss[CC_SIDE_PLAYER].n, 2);
    CHECK_EQ(sm.hp_loss[CC_SIDE_PLAYER].n_shown, 0);
    CHECK_EQ(sm.hp_loss[CC_SIDE_PLAYER].shown, 0);
    CHECK_EQ(sm.hp_loss[CC_SIDE_PLAYER].n_measured, 2);
    CHECK_EQ(sm.hp_loss[CC_SIDE_PLAYER].measured, 13);
    CHECK_EQ(sm.damage[CC_SIDE_PLAYER].n, 0);

    /* HP that moved the wrong way for the kind: no value is invented, the conflict is counted */
    fresh(r, RUN_A);
    cc_obs cf[5] = {o_in(2, 19, 50, 0), o_dmg(2, 19, 10, 60, 3), o_dmg(2, 19, 5, 55, 3), o_heal(2, 19, 9, 40), o_heal(2, 19, 9, 49)};
    b = mk(1, 1, 1, 1, cf, 5);
    cc_reduce_result res = GO(r, b);
    CHECK_EQ(res.sign_conflicts, 2);                                        /* the 60 after 50, the 40 after 55 */
    t = &TURN(r, 0);
    EXPECT(t->events[1], 2, CC_EV_DAMAGE, CC_SIDE_ENEMY, 19, CC_EK_RESULT | CC_EK_SHOWN | CC_EK_HP_AFTER, 10, 0, 60);
    EXPECT(t->events[2], 3, CC_EV_DAMAGE, CC_SIDE_ENEMY, 19, CC_EK_RESULT | CC_EK_SHOWN | CC_EK_HP_DELTA | CC_EK_HP_AFTER, 5, -5, 55);
    EXPECT(t->events[3], 4, CC_EV_HEAL, CC_SIDE_ENEMY, 19, CC_EK_SHOWN | CC_EK_HP_AFTER, 9, 0, 40);
    EXPECT(t->events[4], 5, CC_EV_HEAL, CC_SIDE_ENEMY, 19, CC_EK_SHOWN | CC_EK_HP_DELTA | CC_EK_HP_AFTER, 9, 9, 49);
    CHECK_STATUS(cc_turn_summarize(t, &sm), CC_OK);
    CHECK_EQ(sm.damage[CC_SIDE_ENEMY].n, 2);
    CHECK_EQ(sm.damage[CC_SIDE_ENEMY].n_measured, 1);                       /* the sums say how much they cover */
    CHECK_EQ(sm.damage[CC_SIDE_ENEMY].measured, 5);
    CHECK_EQ(sm.damage[CC_SIDE_ENEMY].shown, 15);
    CHECK_EQ(sm.heal[CC_SIDE_ENEMY].n_measured, 1);
}

/* ------------------------------------------------------------------ the HP tracker */

static void test_tracker(void)
{
    cc_reducer *r = &R;
    cc_reduce_result res;
    const cc_turn *t;

    /* a field position reused by another Pokemon of the same species: the faint and the switch-in reset it */
    fresh(r, RUN_A);
    cc_obs reuse[7] = {o_in(2, 19, 50, 0), o_dmg(2, 19, 20, 30, 3), o_faint(2, 19, 0), o_in(2, 19, 100, 1),
                       o_dmg(2, 19, 30, 70, 3), o_out(2, 19, 70), o_in(2, 19, 90, 2)};
    cc_batch b = mk(1, 1, 1, 1, reuse, 7);
    res = GO(r, b);
    CHECK_EQ(res.sign_conflicts, 0);
    t = &TURN(r, 0);
    CHECK_EQ(t->events[1].hp_delta, -20);
    CHECK_EQ(t->events[4].hp_delta, -30);                                   /* against 100, not against the fainted 0 or 30 */
    CHECK_EQ(r->pos[2].species, 19);
    CHECK_EQ(r->pos[2].hp, 90);

    /* no snapshot: the HP is unknown and so is the next delta */
    fresh(r, RUN_A);
    cc_obs nosnap[4] = {o_in(2, 19, 100, 0), o_dmg(2, 19, 10, -1, 3), o_dmg(2, 19, 10, 70, 3), o_dmg(2, 19, 10, 60, 3)};
    b = mk(1, 1, 1, 1, nosnap, 4);
    GO(r, b);
    t = &TURN(r, 0);
    CHECK_EQ(t->events[1].known & (CC_EK_HP_DELTA | CC_EK_HP_AFTER), 0);
    CHECK_EQ(t->events[2].known & CC_EK_HP_DELTA, 0);
    CHECK_EQ(t->events[3].hp_delta, -10);

    /* another species or another form at the position is another Pokemon: no delta across them */
    fresh(r, RUN_A);
    cc_obs who[5] = {o_in(2, 19, 100, 0), o_dmg(2, 20, 10, 60, 3), o_dmg(2, 20, 10, 50, 3), o_dmg(2, 20, 10, 40, 3), o_dmg(2, 20, 10, 30, 3)};
    who[3].battler_form = 1;
    who[4].battler_form = 1;
    b = mk(1, 1, 1, 1, who, 5);
    GO(r, b);
    t = &TURN(r, 0);
    CHECK_EQ(t->events[1].known & CC_EK_HP_DELTA, 0);                       /* 19 then 20 */
    CHECK_EQ(t->events[2].hp_delta, -10);
    CHECK_EQ(t->events[3].known & CC_EK_HP_DELTA, 0);                       /* form 0 then form 1 */
    CHECK_EQ(t->events[4].hp_delta, -10);

    /* a hit that leaves 0 HP is still a snapshot: the next event compares with it */
    fresh(r, RUN_A);
    cc_obs low[3] = {o_in(2, 19, 30, 0), o_dmg(2, 19, 30, 0, 3), o_heal(2, 19, 5, 5)};
    b = mk(1, 1, 1, 1, low, 3);
    GO(r, b);
    CHECK_EQ(TURN(r, 0).events[1].hp_delta, -30);
    CHECK_EQ(TURN(r, 0).events[2].hp_delta, 5);

    /* a measured 0 is a measurement */
    fresh(r, RUN_A);
    cc_obs zero[3] = {o_in(2, 19, 50, 0), o_dmg(2, 19, 5, 50, 3), o_heal(2, 19, 5, 50)};
    b = mk(1, 1, 1, 1, zero, 3);
    res = GO(r, b);
    CHECK_EQ(res.sign_conflicts, 0);
    EXPECT(TURN(r, 0).events[1], 2, CC_EV_DAMAGE, CC_SIDE_ENEMY, 19, CC_EK_RESULT | CC_EK_SHOWN | CC_EK_HP_DELTA | CC_EK_HP_AFTER, 5, 0, 50);
    EXPECT(TURN(r, 0).events[2], 3, CC_EV_HEAL, CC_SIDE_ENEMY, 19, CC_EK_SHOWN | CC_EK_HP_DELTA | CC_EK_HP_AFTER, 5, 0, 50);

    /* after a faint or a switch-out nothing of the old occupant is compared with what comes next at the position */
    fresh(r, RUN_A);
    cc_obs gone[6] = {o_in(2, 19, 40, 0), o_faint(2, 19, 0), o_dmg(2, 19, 5, 50, 3),
                      o_out(3, 20, 70), o_dmg(3, 20, 5, 60, 3), o_heal(3, 20, 1, 61)};
    b = mk(1, 1, 1, 1, gone, 6);
    res = GO(r, b);
    CHECK_EQ(res.sign_conflicts, 0);                                        /* 0 -> 50 across the faint is not a heal */
    t = &TURN(r, 0);
    CHECK_EQ(t->events[2].known & CC_EK_HP_DELTA, 0);
    CHECK_EQ(t->events[4].known & CC_EK_HP_DELTA, 0);                       /* 70 -> 60 across the switch-out is not a hit */
    CHECK_EQ(t->events[5].hp_delta, 1);                                     /* the chain restarts from the new snapshot */

    /* the chain across batches is real for the conflict count too, and a flagged batch cuts it */
    fresh(r, RUN_A);
    cc_obs c1[1] = {o_in(2, 19, 50, 0)}, c2[1] = {o_dmg(2, 19, 5, 60, 3)}, c3[1] = {o_dmg(2, 19, 5, 70, 3)};
    b = mk(1, 1, 1, 1, c1, 1);
    GO(r, b);
    b = mk(2, 1, 1, 1, c2, 1);
    res = GO(r, b);
    CHECK_EQ(res.sign_conflicts, 1);                                        /* 50 -> 60 on a hit */
    b = mk(3, 1, 1, 1, c3, 1);
    b.flags = CC_BF_SOURCE_LOSS;
    res = GO(r, b);
    CHECK_EQ(res.sign_conflicts, 0);                                        /* 60 -> 70 across lost events: nothing to compare */
    CHECK_EQ(TURN(r, 0).events[2].known & CC_EK_HP_DELTA, 0);

    /* the tracker carries across batches and turns of one battle, and is cleared by a new battle */
    fresh(r, RUN_A);
    cc_obs t1[2] = {o_in(2, 19, 100, 0), o_dmg(2, 19, 10, 90, 3)};
    cc_obs t2[1] = {o_dmg(2, 19, 15, 75, 3)};
    cc_obs t3[1] = {o_dmg(2, 19, 15, 60, 3)};
    cc_obs t4[1] = {o_dmg(2, 19, 15, 45, 3)};
    b = mk(1, 1, 1, 1, t1, 2);
    GO(r, b);
    b = mk(2, 1, 1, 1, t2, 1);                                              /* the rest of the same turn */
    GO(r, b);
    b = mk(3, 1, 1, 2, t3, 1);                                              /* the next turn */
    GO(r, b);
    CHECK_EQ(TURN(r, 0).events[2].hp_delta, -15);
    CHECK_EQ(TURN(r, 1).events[0].hp_delta, -15);
    b = mk(4, 2, 2, 1, t4, 1);                                              /* next battle: other Pokemon */
    GO(r, b);
    CHECK_EQ(TURN(r, 2).events[0].known & CC_EK_HP_DELTA, 0);

    /* positions are independent */
    fresh(r, RUN_A);
    cc_obs two[5] = {o_in(2, 19, 100, 0), o_in(3, 20, 80, 1), o_dmg(3, 20, 10, 70, 3), o_dmg(2, 19, 10, 90, 3), o_dmg(3, 20, 10, 60, 3)};
    b = mk(1, 1, 1, 1, two, 5);
    GO(r, b);
    t = &TURN(r, 0);
    CHECK_EQ(t->events[2].hp_delta, -10);
    CHECK_EQ(t->events[3].hp_delta, -10);
    CHECK_EQ(t->events[4].hp_delta, -10);
}

/* ------------------------------------------------------------------ eviction */

static void fill_dmg(cc_obs *o, unsigned n, int first_shown)
{
    for (unsigned i = 0; i < n; i++)
        o[i] = o_dmg(2, 19, first_shown + (int)i, -1, 3);
}

static void test_eviction(void)
{
    cc_reducer *r = &R;
    cc_reduce_result res;
    cc_batch b;
    const cc_turn *t;

    /* 64 events fit exactly */
    fresh(r, RUN_A);
    fill_dmg(OBS, 64, 1);
    b = mk(1, 1, 1, 1, OBS, 64);
    res = GO(r, b);
    t = &TURN(r, 0);
    CHECK_EQ(res.events_added, 64);
    CHECK_EQ(res.evicted, 0);
    CHECK_EQ(t->n_events, 64);
    CHECK_EQ(t->dropped, 0);
    CHECK_EQ(t->flags, 0);
    CHECK_EQ(t->events[0].seq, 1);
    CHECK_EQ(t->events[63].seq, 64);
    cc_turn_summary full;
    CHECK_STATUS(cc_turn_summarize(t, &full), CC_OK);
    CHECK_EQ(full.exact, 1);                                                /* 64 events, nothing dropped: complete */
    CHECK_EQ(full.damage[CC_SIDE_ENEMY].n, 64);

    /* the 65th drops the oldest, visibly */
    fresh(r, RUN_A);
    fill_dmg(OBS, 65, 1);
    b = mk(1, 1, 1, 1, OBS, 65);
    res = GO(r, b);
    t = &TURN(r, 0);
    CHECK_EQ(res.events_added, 65);
    CHECK_EQ(res.evicted, 1);
    CHECK_EQ(t->n_events, 64);
    CHECK_EQ(t->dropped, 1);
    CHECK_EQ(t->flags, CC_TF_EVICTED);
    CHECK_EQ(t->events[0].seq, 2);                                          /* oldest first: 1 is gone */
    CHECK_EQ(t->events[0].shown, 2);
    CHECK_EQ(t->events[63].seq, 65);
    CHECK_EQ(t->events[63].shown, 65);
    CHECK_EQ(r->next_seq, 66);
    cc_turn_summary sm;
    CHECK_STATUS(cc_turn_summarize(t, &sm), CC_OK);
    CHECK_EQ(sm.exact, 0);                                                  /* a truncated turn is never a complete total */
    CHECK_EQ(sm.dropped, 1);
    CHECK_EQ(sm.damage[CC_SIDE_ENEMY].n, 64);
    CHECK_EQ(sm.damage[CC_SIDE_ENEMY].shown, 2144);                         /* 2 + ... + 65: a lower bound, flagged inexact */

    /* the same result when the 65 arrive as 64 + 1 (and as 40 + 25) */
    memcpy(&R2, r, sizeof R2);
    fresh(r, RUN_A);
    b = mk(1, 1, 1, 1, OBS, 64);
    GO(r, b);
    b = mk(2, 1, 1, 1, OBS + 64, 1);
    res = GO(r, b);
    CHECK_EQ(res.evicted, 1);
    CHECK_EQ(res.new_turn, 0);
    CHECK(memcmp(&r->hist, &R2.hist, sizeof r->hist) == 0);
    fresh(r, RUN_A);
    b = mk(1, 1, 1, 1, OBS, 40);
    GO(r, b);
    b = mk(2, 1, 1, 1, OBS + 40, 25);
    GO(r, b);
    CHECK(memcmp(&r->hist, &R2.hist, sizeof r->hist) == 0);

    /* 60 present, 10 more: the six oldest present events go, none of the new ones */
    fresh(r, RUN_A);
    fill_dmg(OBS, 70, 1);
    b = mk(1, 1, 1, 1, OBS, 60);
    GO(r, b);
    b = mk(2, 1, 1, 1, OBS + 60, 10);
    res = GO(r, b);
    t = &TURN(r, 0);
    CHECK_EQ(res.evicted, 6);
    CHECK_EQ(t->n_events, 64);
    CHECK_EQ(t->dropped, 6);
    CHECK_EQ(t->events[0].shown, 7);
    CHECK_EQ(t->events[53].shown, 60);
    CHECK_EQ(t->events[54].shown, 61);
    CHECK_EQ(t->events[63].shown, 70);

    /* a batch of 384 alone: only its own newest 64 survive, the numbering still counts all 384 */
    fresh(r, RUN_A);
    fill_dmg(OBS, CC_MAX_BATCH_OBS, 1);
    b = mk(1, 1, 1, 1, OBS, CC_MAX_BATCH_OBS);
    res = GO(r, b);
    t = &TURN(r, 0);
    CHECK_EQ(res.evicted, 320);
    CHECK_EQ(t->dropped, 320);
    CHECK_EQ(t->events[0].seq, 321);
    CHECK_EQ(t->events[0].shown, 321);
    CHECK_EQ(t->events[63].seq, 384);
    CHECK_EQ(r->next_seq, 385);

    /* and on top of a full turn the whole old content goes */
    b = mk(2, 1, 1, 1, OBS, CC_MAX_BATCH_OBS);
    res = GO(r, b);
    t = &TURN(r, 0);
    CHECK_EQ(res.evicted, 384);
    CHECK_EQ(t->dropped, 704);
    CHECK_EQ(t->events[0].seq, 385 + 320);
    CHECK_EQ(t->events[63].seq, 768);

    /* eviction does not cross turns: the next turn starts empty and unflagged */
    b = mk(3, 1, 1, 2, OBS, 3);
    GO(r, b);
    CHECK_EQ(TURN(r, 1).n_events, 3);
    CHECK_EQ(TURN(r, 1).flags, 0);
    CHECK_EQ(TURN(r, 1).dropped, 0);
    CHECK_EQ(TURN(r, 0).flags, CC_TF_EVICTED);

    /* the dropped counter saturates instead of wrapping */
    fresh(r, RUN_A);
    fill_dmg(OBS, CC_MAX_BATCH_OBS, 1);
    for (uint32_t id = 1; id <= 171; id++) {
        b = mk(id, 1, 1, 1, OBS, CC_MAX_BATCH_OBS);
        GO(r, b);
    }
    CHECK_EQ(TURN(r, 0).dropped, 65535);
    CHECK_EQ(TURN(r, 0).n_events, 64);
    CHECK_EQ(TURN(r, 0).flags, CC_TF_EVICTED);
    CHECK_EQ(r->next_seq, 1u + 171u * 384u);

    /* five turns fit, the sixth drops the oldest (a whole turn, not an event) */
    fresh(r, RUN_A);
    for (unsigned k = 1; k <= 5; k++) {
        cc_obs one = o_dmg(2, 19, (int)k, -1, 3);
        b = mk(k, 1, 1, k, &one, 1);
        res = GO(r, b);
        CHECK_EQ(res.new_turn, 1);
        CHECK_EQ(res.turn_evicted, 0);
    }
    CHECK_EQ(r->hist.n_turns, 5);
    for (unsigned k = 0; k < 5; k++) {
        CHECK_EQ(TURN(r, k).turn, k + 1);
        CHECK_EQ(TURN(r, k).events[0].shown, k + 1);
    }
    {
        cc_obs one = o_dmg(2, 19, 6, -1, 3);
        b = mk(6, 1, 1, 6, &one, 1);
        res = GO(r, b);
        CHECK_EQ(res.turn_evicted, 1);
        CHECK_EQ(res.new_turn, 1);
    }
    CHECK_EQ(r->hist.n_turns, 5);
    for (unsigned k = 0; k < 5; k++) {
        CHECK_EQ(TURN(r, k).turn, k + 2);
        CHECK_EQ(TURN(r, k).events[0].shown, k + 2);
        CHECK_EQ(TURN(r, k).events[0].seq, k + 2);
        CHECK_EQ(TURN(r, k).n_events, 1);
    }
    /* a turn that fell off cannot be reopened */
    {
        cc_obs one = o_dmg(2, 19, 9, -1, 3);
        b = mk(7, 1, 1, 1, &one, 1);
        REJECT(r, b, CC_E_ORDER, -1);
    }
    CHECK_EQ(r->hist.turns[0].flags, 0);                                    /* losing an old turn is the window, not an incompleteness */
}

/* ------------------------------------------------------------------ a batch counts once */

static void test_repeats(void)
{
    cc_reducer *r = &R;
    cc_reduce_result res;
    fresh(r, RUN_A);
    cc_obs o[4] = {o_in(2, 19, 100, 0), o_dmg(2, 19, 10, 90, 3), o_anim(0, 25, 2, 19, 33), o_heal(2, 19, 5, 95)};
    cc_batch b = mk(1, 1, 1, 1, o, 4);
    res = GO(r, b);
    CHECK_EQ(res.duplicate, 0);
    CHECK_EQ(res.events_added, 3);
    memcpy(&SAVED, r, sizeof SAVED);

    /* retries, an animation replay, a redraw: any number of exact repeats change nothing and say so */
    for (int i = 0; i < 5; i++) {
        res = GO(r, b);
        CHECK_EQ(res.duplicate, 1);
        CHECK_EQ(res.events_added, 0);
        CHECK_EQ(res.new_turn, 0);
        CHECK_EQ(res.ignored, 0);
        CHECK(memcmp(&SAVED, r, sizeof SAVED) == 0);
    }
    CHECK_EQ(r->hist.n_turns, 1);
    CHECK_EQ(TURN(r, 0).n_events, 3);
    CHECK_EQ(r->next_seq, 4);

    /* the same id with ANY other content is a conflict, not a replay: header ... */
    cc_batch v;
#define VARIANT(expr, want) \
    do { \
        v = b; \
        expr; \
        REJECT(r, v, (want), -1); \
    } while (0)
    VARIANT(v.turn = 2, CC_E_DUPLICATE);
    VARIANT(v.battle_seq = 2, CC_E_DUPLICATE);
    VARIANT(v.wave = 2, CC_E_DUPLICATE);
    VARIANT(v.flags = CC_BF_SOURCE_LOSS, CC_E_DUPLICATE);
    VARIANT(v.flags = CC_BF_BEGAN_MID, CC_E_DUPLICATE);
    VARIANT(v.n_obs = 3, CC_E_DUPLICATE);
    VARIANT(v.run_uid = RUN_B, CC_E_CONSISTENCY);
    /* ... and every byte of every field of an observation (the digest covers them all; the last observation is varied) */
    cc_obs w[4];
    memcpy(w, o, sizeof w);
    v = b;
    v.obs = w;
#define OBS_FLIP(field, mask) \
    do { \
        memcpy(w, o, sizeof w); \
        w[3].field ^= (mask); \
        REJECT(r, v, CC_E_DUPLICATE, -1); \
    } while (0)
#define FLIP8(field) OBS_FLIP(field, 0x01)
#define FLIP16(field) do { OBS_FLIP(field, 0x0001); OBS_FLIP(field, 0x0100); } while (0)
#define FLIP32(field) do { OBS_FLIP(field, 0x00000001); OBS_FLIP(field, 0x00000100); \
                           OBS_FLIP(field, 0x00010000); OBS_FLIP(field, 0x01000000); } while (0)
    FLIP8(kind);
    FLIP8(battler);
    FLIP8(target);
    FLIP8(battler_form);
    FLIP8(target_form);
    FLIP16(battler_species);
    FLIP16(target_species);
    FLIP32(battler_id);
    FLIP32(target_id);
    FLIP16(id);
    FLIP32(value);
    FLIP32(hp_after);
    FLIP32(hp_max_after);
    FLIP8(status_after);
    FLIP16(src_known);
    FLIP8(src_side);
    FLIP8(src_form);
    FLIP16(src_species);
    FLIP16(src_move);
    FLIP32(src_id);
    /* the header: every byte of the battle number, the wave and the turn */
    {
        cc_batch hv;
        hv = b; hv.battle_seq ^= 0x00000002; REJECT(r, hv, CC_E_DUPLICATE, -1);
        hv = b; hv.battle_seq ^= 0x00000100; REJECT(r, hv, CC_E_DUPLICATE, -1);
        hv = b; hv.battle_seq ^= 0x00010000; REJECT(r, hv, CC_E_DUPLICATE, -1);
        hv = b; hv.battle_seq ^= 0x01000000; REJECT(r, hv, CC_E_DUPLICATE, -1);
        hv = b; hv.wave ^= 0x0002; REJECT(r, hv, CC_E_DUPLICATE, -1);
        hv = b; hv.wave ^= 0x0100; REJECT(r, hv, CC_E_DUPLICATE, -1);
        hv = b; hv.turn ^= 0x0001; REJECT(r, hv, CC_E_DUPLICATE, -1);
        hv = b; hv.turn ^= 0x0100; REJECT(r, hv, CC_E_DUPLICATE, -1);
    }
    /* the first observation too, and an order swap */
    memcpy(w, o, sizeof w);
    w[0].value = 1;
    REJECT(r, v, CC_E_DUPLICATE, -1);
    memcpy(w, o, sizeof w);
    w[1] = o[3];
    w[3] = o[1];
    REJECT(r, v, CC_E_DUPLICATE, -1);
    memcpy(w, o, sizeof w);
    CHECK_STATUS(cc_reduce(r, &v, NULL), CC_OK);                            /* identical content is again a plain replay */

    /* an older id is never applied, replay or not */
    cc_obs one = o_dmg(2, 19, 1, -1, 3);
    cc_batch n = mk(2, 1, 1, 1, &one, 1);
    res = GO(r, n);
    CHECK_EQ(res.events_added, 1);
    memcpy(&SAVED, r, sizeof SAVED);
    REJECT(r, b, CC_E_ORDER, -1);                                           /* batch 1 again after batch 2 */
    res = GO(r, n);
    CHECK_EQ(res.duplicate, 1);
    CHECK(memcmp(&SAVED, r, sizeof SAVED) == 0);

    /* a rejected batch leaves no trace: the same id may be sent again, corrected */
    cc_obs badone[2] = {o_dmg(2, 19, 1, -1, 3), o_dmg(2, 19, 1, -1, 99)};
    cc_batch bad = mk(3, 1, 1, 1, badone, 2);
    REJECT(r, bad, CC_E_RANGE, 1);
    badone[1] = o_dmg(2, 19, 2, -1, 3);
    res = GO(r, bad);
    CHECK_EQ(res.duplicate, 0);
    CHECK_EQ(res.events_added, 2);
    CHECK_EQ(r->last_batch_id, 3);

    /* after the end of the run a replay (an animation still playing) cannot change the captured history */
    memcpy(&SAVED, r, sizeof SAVED);
    CHECK_STATUS(cc_reducer_end_run(r), CC_OK);
    SAVED.closed = 1;
    REJECT(r, bad, CC_E_LIFECYCLE, -1);
    CHECK(memcmp(&SAVED.hist, &r->hist, sizeof r->hist) == 0);
}

/* ------------------------------------------------------------------ wave and battle boundaries, order */

static void test_boundaries(void)
{
    cc_reducer *r = &R;
    cc_reduce_result res;
    cc_obs a = o_dmg(2, 19, 1, -1, 3), c = o_dmg(2, 19, 2, -1, 3), d = o_dmg(2, 19, 3, -1, 3), e = o_dmg(2, 19, 4, -1, 3);
    fresh(r, RUN_A);

    /* turn 3 of wave 5 and turn 3 of wave 6 are two turns */
    cc_batch b = mk(1, 1, 5, 3, &a, 1);
    GO(r, b);
    b = mk(2, 2, 6, 3, &c, 1);
    res = GO(r, b);
    CHECK_EQ(res.new_turn, 1);
    CHECK_EQ(r->hist.n_turns, 2);
    CHECK_EQ(TURN(r, 0).wave, 5);
    CHECK_EQ(TURN(r, 1).wave, 6);
    CHECK_EQ(TURN(r, 0).n_events, 1);
    CHECK_EQ(TURN(r, 1).n_events, 1);
    /* the same key again appends */
    b = mk(3, 2, 6, 3, &d, 1);
    res = GO(r, b);
    CHECK_EQ(res.new_turn, 0);
    CHECK_EQ(TURN(r, 1).n_events, 2);
    /* a later battle at the same wave number is its own turns (a second battle in one wave) */
    b = mk(4, 3, 6, 3, &e, 1);
    res = GO(r, b);
    CHECK_EQ(res.new_turn, 1);
    CHECK_EQ(TURN(r, 2).battle_seq, 3);
    /* turn 0 ("before the first turn") sorts before turn 1 of the next battle, after nothing of its own battle */
    b = mk(5, 4, 7, 0, &a, 1);
    GO(r, b);
    b = mk(6, 4, 7, 1, &a, 1);
    GO(r, b);
    b = mk(7, 4, 7, 0, &a, 1);                                              /* turn 0 again after turn 1 */
    REJECT(r, b, CC_E_ORDER, -1);

    /* malformed order */
    fresh(r, RUN_A);
    b = mk(10, 5, 9, 4, &a, 1);
    GO(r, b);
    b = mk(11, 5, 9, 3, &a, 1);                                             /* turn goes back */
    REJECT(r, b, CC_E_ORDER, -1);
    b = mk(11, 4, 9, 9, &a, 1);                                             /* battle goes back */
    REJECT(r, b, CC_E_ORDER, -1);
    b = mk(9, 5, 9, 5, &a, 1);                                              /* batch id goes back */
    REJECT(r, b, CC_E_ORDER, -1);
    b = mk(11, 5, 8, 5, &a, 1);                                             /* the wave changes inside a battle */
    REJECT(r, b, CC_E_CONSISTENCY, -1);
    b = mk(11, 6, 8, 1, &a, 1);                                             /* the wave goes back across battles */
    REJECT(r, b, CC_E_ORDER, -1);
    b = mk(11, 5, 9, 4, &c, 1);                                             /* the same turn, a new id: fine */
    res = GO(r, b);
    CHECK_EQ(res.new_turn, 0);
}

/* ------------------------------------------------------------------ incomplete observation is visible */

static void test_incomplete(void)
{
    cc_reducer *r = &R;
    cc_reduce_result res;
    cc_turn_summary sm;
    cc_obs two[2] = {o_in(2, 19, 100, 0), o_dmg(2, 19, 10, 90, 3)};
    cc_obs hit[1] = {o_dmg(2, 19, 10, 80, 3)};

    /* a clean turn is exact */
    fresh(r, RUN_A);
    cc_batch b = mk(1, 1, 1, 1, two, 2);
    GO(r, b);
    CHECK_STATUS(cc_turn_summarize(&TURN(r, 0), &sm), CC_OK);
    CHECK_EQ(sm.exact, 1);
    CHECK_EQ(sm.flags, 0);

    /* the game's own buffer overflowed: the turn says so, and the HP chain restarts */
    b = mk(2, 1, 1, 1, hit, 1);
    b.flags = CC_BF_SOURCE_LOSS;
    res = GO(r, b);
    CHECK_EQ(res.gap, 0);
    CHECK_EQ(TURN(r, 0).flags, CC_TF_SOURCE_LOSS);
    CHECK_EQ(TURN(r, 0).events[2].known & CC_EK_HP_DELTA, 0);
    CHECK_STATUS(cc_turn_summarize(&TURN(r, 0), &sm), CC_OK);
    CHECK_EQ(sm.exact, 0);
    CHECK_EQ(sm.flags, CC_TF_SOURCE_LOSS);
    /* the flag is not undone by later clean batches */
    cc_obs next[1] = {o_dmg(2, 19, 10, 70, 3)};
    b = mk(3, 1, 1, 1, next, 1);
    GO(r, b);
    CHECK_EQ(TURN(r, 0).flags, CC_TF_SOURCE_LOSS);
    CHECK_EQ(TURN(r, 0).events[3].hp_delta, -10);                           /* ... but the chain resumed */

    /* observation started inside the turn: only the batch that opens a turn may say so */
    fresh(r, RUN_A);
    b = mk(1, 1, 1, 1, hit, 1);
    b.flags = CC_BF_BEGAN_MID;
    res = GO(r, b);
    CHECK_EQ(TURN(r, 0).flags, CC_TF_BEGAN_MID);
    CHECK_STATUS(cc_turn_summarize(&TURN(r, 0), &sm), CC_OK);
    CHECK_EQ(sm.exact, 0);
    b = mk(2, 1, 1, 1, hit, 1);
    b.flags = CC_BF_BEGAN_MID;
    REJECT(r, b, CC_E_CONSISTENCY, -1);
    b = mk(2, 1, 1, 2, hit, 1);
    b.flags = CC_BF_BEGAN_MID | CC_BF_SOURCE_LOSS;
    GO(r, b);
    CHECK_EQ(TURN(r, 1).flags, CC_TF_BEGAN_MID | CC_TF_SOURCE_LOSS);
    CHECK_EQ(TURN(r, 0).flags, CC_TF_BEGAN_MID);

    /* a resumed run: history flag plus the first batch's own flag */
    CHECK_STATUS(cc_reducer_begin_run(r, RUN_B, true), CC_OK);
    b = mk(7, 12, 40, 3, hit, 1);
    b.run_uid = RUN_B;
    b.flags = CC_BF_BEGAN_MID;
    res = GO(r, b);
    CHECK_EQ(res.gap, 0);                                                   /* the first batch may have any id */
    CHECK_EQ(r->hist.flags, CC_HF_BEGAN_MID);
    CHECK_EQ(TURN(r, 0).flags, CC_TF_BEGAN_MID);
    CHECK_EQ(r->last_batch_id, 7);

    /* a skipped batch id: both neighbouring turns are marked and the HP chain is cut */
    fresh(r, RUN_A);
    cc_obs s1[2] = {o_in(2, 19, 100, 0), o_dmg(2, 19, 10, 90, 3)};
    cc_obs s2[1] = {o_dmg(2, 19, 10, 80, 3)};
    cc_obs s3[1] = {o_dmg(2, 19, 10, 70, 3)};
    cc_obs s4[1] = {o_dmg(2, 19, 10, 60, 3)};
    b = mk(1, 1, 1, 1, s1, 2);
    GO(r, b);
    b = mk(2, 1, 1, 2, s2, 1);
    GO(r, b);
    CHECK_EQ(TURN(r, 0).flags | TURN(r, 1).flags, 0);
    b = mk(4, 1, 1, 3, s3, 1);                                              /* id 3 never arrived */
    res = GO(r, b);
    CHECK_EQ(res.gap, 1);
    CHECK_EQ(res.new_turn, 1);
    CHECK_EQ(TURN(r, 0).flags, 0);                                          /* only the two neighbours */
    CHECK_EQ(TURN(r, 1).flags, CC_TF_SOURCE_LOSS);
    CHECK_EQ(TURN(r, 2).flags, CC_TF_SOURCE_LOSS);
    CHECK_EQ(TURN(r, 2).events[0].known & CC_EK_HP_DELTA, 0);
    b = mk(5, 1, 1, 3, s4, 1);                                              /* consecutive again: no new gap */
    res = GO(r, b);
    CHECK_EQ(res.gap, 0);
    CHECK_EQ(TURN(r, 2).events[1].hp_delta, -10);
    /* a gap inside one turn marks that turn once */
    b = mk(9, 1, 1, 3, s4, 1);
    res = GO(r, b);
    CHECK_EQ(res.gap, 1);
    CHECK_EQ(res.new_turn, 0);
    CHECK_EQ(TURN(r, 2).flags, CC_TF_SOURCE_LOSS);
    CHECK_EQ(TURN(r, 1).flags, CC_TF_SOURCE_LOSS);                          /* untouched since */
}

/* ------------------------------------------------------------------ replaying a recorded run */

static void test_save_replay(void)
{
    /* a recorded session, literal */
    cc_obs b1[3] = {o_in(0, 25, 100, 0), o_in(2, 19, 60, 0), o_anim(0, 25, 2, 19, 84)};
    cc_obs b2[2] = {o_dmg(2, 19, 25, 35, 3), o_dmg(0, 25, 18, 82, 1)};
    cc_obs b3[2] = {o_loss(0, 25, 6, 76, 2), o_heal(0, 25, 10, 86)};
    cc_obs b4[2] = {o_dmg(0, 25, 90, 0, 3), o_faint(0, 25, 0)};
    cc_obs b5[1] = {o_in(0, 26, 70, 1)};
    cc_batch rec[6];
    rec[0] = mk(1, 1, 3, 0, b1, 3);
    rec[1] = mk(2, 1, 3, 1, b2, 2);
    rec[2] = mk(3, 1, 3, 1, b3, 2);
    rec[3] = mk(4, 1, 3, 2, b4, 2);
    rec[4] = mk(5, 1, 3, 2, b5, 1);
    rec[5] = mk(6, 2, 4, 1, b2, 2);

    fresh(&R, RUN_A);
    for (int i = 0; i < 6; i++)
        GO(&R, rec[i]);
    CHECK_EQ(R.hist.n_turns, 4);
    CHECK_EQ(TURN(&R, 0).n_events, 2);
    CHECK_EQ(TURN(&R, 1).n_events, 4);                                      /* two batches, one turn */
    CHECK_EQ(TURN(&R, 2).n_events, 3);
    CHECK_EQ(TURN(&R, 3).n_events, 2);
    CHECK_EQ(TURN(&R, 1).events[0].hp_delta, -25);                          /* 60 -> 35 (the animation gave no snapshot) */
    CHECK_EQ(TURN(&R, 1).events[1].hp_delta, -18);
    CHECK_EQ(TURN(&R, 1).events[2].hp_delta, -6);
    CHECK_EQ(TURN(&R, 1).events[3].hp_delta, 10);
    CHECK_EQ(TURN(&R, 2).events[0].hp_delta, -86);
    CHECK_EQ(TURN(&R, 3).events[0].known & CC_EK_HP_DELTA, 0);              /* battle 2: nothing carried over */

    /* the same input into a fresh reducer gives the same bytes */
    fresh(&R2, RUN_A);
    for (int i = 0; i < 6; i++)
        GO(&R2, rec[i]);
    CHECK(memcmp(&R, &R2, sizeof R) == 0);

    /* feeding the recording again without a new run: every earlier batch is refused, the last is a plain replay */
    memcpy(&SAVED, &R, sizeof SAVED);
    for (int i = 0; i < 5; i++)
        REJECT(&R, rec[i], CC_E_ORDER, -1);
    cc_reduce_result res = GO(&R, rec[5]);
    CHECK_EQ(res.duplicate, 1);
    CHECK(memcmp(&SAVED, &R, sizeof SAVED) == 0);

    /* a new run resets, and replaying then reproduces the history (the uid is not part of it) */
    CHECK_STATUS(cc_reducer_begin_run(&R, RUN_B, false), CC_OK);
    CHECK_EQ(R.hist.n_turns, 0);
    CHECK_EQ(R.next_seq, 1);
    for (int i = 0; i < 6; i++) {
        rec[i].run_uid = RUN_B;
        GO(&R, rec[i]);
    }
    CHECK(memcmp(&R.hist, &R2.hist, sizeof R.hist) == 0);
    CHECK(memcmp(&R, &R2, sizeof R) != 0);                                  /* the run uid differs ... */
    R.run_uid = RUN_A;
    CHECK(memcmp(&R, &R2, sizeof R) == 0);                                  /* ... and nothing else */
}

/* ------------------------------------------------------------------ totals of a turn */

static void test_summary(void)
{
    cc_reducer *r = &R;
    cc_turn_summary sm;
    fresh(r, RUN_A);
    cc_obs o[8] = {o_in(0, 25, 100, 0), o_in(2, 19, 100, 0), o_dmg(2, 19, 30, 70, 3), o_dmg(0, 25, 20, 80, 3),
                   o_loss(0, 25, 5, 75, 2), o_heal(0, 25, 12, 87), o_faint(2, 19, 0), o_out(0, 25, 87)};
    o[7].battler = 1;                                                       /* a different position: no tracker carry-over */
    cc_batch b = mk(1, 1, 1, 1, o, 8);
    GO(r, b);
    CHECK_STATUS(cc_turn_summarize(&TURN(r, 0), &sm), CC_OK);
    CHECK_EQ(sm.exact, 1);
    CHECK_EQ(sm.n_events, 8);
    CHECK_EQ(sm.dropped, 0);
    CHECK_EQ(sm.damage[CC_SIDE_ENEMY].n, 1);
    CHECK_EQ(sm.damage[CC_SIDE_ENEMY].shown, 30);
    CHECK_EQ(sm.damage[CC_SIDE_ENEMY].measured, 30);
    CHECK_EQ(sm.damage[CC_SIDE_PLAYER].shown, 20);
    CHECK_EQ(sm.damage[CC_SIDE_PLAYER].measured, 20);
    CHECK_EQ(sm.hp_loss[CC_SIDE_PLAYER].measured, 5);
    CHECK_EQ(sm.heal[CC_SIDE_PLAYER].shown, 12);
    CHECK_EQ(sm.heal[CC_SIDE_PLAYER].measured, 12);
    CHECK_EQ(sm.n_faint[CC_SIDE_ENEMY], 1);
    CHECK_EQ(sm.n_faint[CC_SIDE_PLAYER], 0);
    CHECK_EQ(sm.n_switch_in[CC_SIDE_PLAYER], 1);
    CHECK_EQ(sm.n_switch_in[CC_SIDE_ENEMY], 1);
    CHECK_EQ(sm.n_switch_out[CC_SIDE_PLAYER], 1);
    CHECK_EQ(sm.n_status[CC_SIDE_PLAYER] + sm.n_status[CC_SIDE_ENEMY], 0);

    /* arguments and invalid turns */
    cc_turn_summary keep;
    memset(&keep, 0xA5, sizeof keep);
    CHECK_STATUS(cc_turn_summarize(NULL, &keep), CC_E_NULL);
    CHECK_STATUS(cc_turn_summarize(&TURN(r, 0), NULL), CC_E_NULL);
    cc_turn bad = TURN(r, 0);
    bad.events[1].kind = CC_EV_NONE;
    CHECK_STATUS(cc_turn_summarize(&bad, &keep), CC_E_RANGE);
    bad = TURN(r, 0);
    bad.flags = CC_TF_EVICTED;
    CHECK_STATUS(cc_turn_summarize(&bad, &keep), CC_E_CONSISTENCY);
    CHECK_EQ(((const unsigned char *)&keep)[0], 0xA5);                      /* untouched on an error */

    /* a status event (not produced by the reducer, but a valid stored kind) is counted */
    cc_turn st = TURN(r, 0);
    memset(&st.events[2], 0, sizeof st.events[2]);
    st.events[2].seq = 3;
    st.events[2].kind = CC_EV_STATUS;
    st.events[2].target_side = CC_SIDE_ENEMY;
    st.events[2].target_species = 19;
    st.events[2].known = CC_EK_SHOWN;
    st.events[2].shown = 2;
    CHECK_STATUS(cc_turn_summarize(&st, &sm), CC_OK);
    CHECK_EQ(sm.n_status[CC_SIDE_ENEMY], 1);
    CHECK_EQ(sm.damage[CC_SIDE_ENEMY].n, 0);

    /* sums are 64-bit: 64 hits of INT32_MAX do not wrap */
    cc_turn big;
    memset(&big, 0, sizeof big);
    big.battle_seq = 1;
    big.wave = 1;
    big.n_events = 64;
    for (unsigned i = 0; i < 64; i++) {
        big.events[i].seq = i + 1;
        big.events[i].kind = CC_EV_DAMAGE;
        big.events[i].target_side = CC_SIDE_PLAYER;
        big.events[i].target_species = 25;
        big.events[i].known = CC_EK_SHOWN | CC_EK_HP_DELTA;
        big.events[i].shown = INT32_MAX;
        big.events[i].hp_delta = -INT32_MAX;
    }
    big.events[63].hp_delta = INT32_MIN;                                    /* the one value whose negation overflows int32 */
    CHECK_STATUS(cc_turn_summarize(&big, &sm), CC_OK);
    CHECK_EQ(sm.damage[CC_SIDE_PLAYER].shown, 64ULL * INT32_MAX);
    CHECK_EQ(sm.damage[CC_SIDE_PLAYER].measured, 63ULL * INT32_MAX + 2147483648ULL);
}

/* ------------------------------------------------------------------ the structural check of a state */

static void test_state_validation(void)
{
    cc_reducer *r = &R;
    fresh(r, RUN_A);
    cc_obs o[3] = {o_in(2, 19, 100, 0), o_dmg(2, 19, 10, 90, 3), o_dmg(2, 19, 10, 80, 3)};
    cc_batch b = mk(1, 1, 5, 1, o, 3);
    GO(r, b);
    b = mk(2, 1, 5, 2, o, 3);
    GO(r, b);
    CHECK_STATUS(cc_reducer_validate(r), CC_OK);
    memcpy(&R2, r, sizeof R2);
    cc_reducer *m = &R2;

#define MUTATE(stmt, want) \
    do { \
        memcpy(m, r, sizeof *m); \
        stmt; \
        CHECK_STATUS(cc_reducer_validate(m), (want)); \
    } while (0)
    MUTATE((void)0, CC_OK);
    MUTATE(m->hist.turns[0].events[0].kind = CC_EV_NONE, CC_E_RANGE);                 /* the history's own rules apply */
    MUTATE(m->hist.turns[0].events[1].seq = 1, CC_E_DUPLICATE);
    MUTATE(m->hist.flags = 2, CC_E_UNKNOWN_BITS);
    MUTATE(m->hist.turns[0].events[3].seq = 9, CC_E_NONCANONICAL);                    /* a stale event past the count */
    MUTATE(m->hist.turns[0].events[63].shown = 1, CC_E_NONCANONICAL);
    MUTATE(m->hist.turns[2].battle_seq = 1, CC_E_NONCANONICAL);                       /* an unused turn slot is zero */
    MUTATE(m->hist.turns[4].events[0].seq = 1, CC_E_NONCANONICAL);
    MUTATE(m->closed = 2, CC_E_RANGE);
    MUTATE(m->next_seq = 0, CC_E_RANGE);
    MUTATE(m->next_seq = 6, CC_E_CONSISTENCY);                                        /* not above the newest seq (6) */
    MUTATE(m->next_seq = 7, CC_OK);
    MUTATE(m->last_batch_id = 0, CC_E_CONSISTENCY);
    MUTATE(m->hist.n_turns = 0, CC_E_NONCANONICAL);                                   /* the turns are still there: not zeroed */
    MUTATE(m->run_uid = 0, CC_E_CONSISTENCY);
    MUTATE(m->hist.turns[1].wave = 4, CC_E_ORDER);                                    /* waves never go back */
    MUTATE(m->hist.turns[1].wave = 6, CC_E_CONSISTENCY);                              /* one battle, two waves */
    MUTATE((m->hist.turns[1].wave = 6, m->hist.turns[1].battle_seq = 2), CC_OK);
    MUTATE(m->pos[2].species = 0, CC_E_NONCANONICAL);                                 /* nobody there, yet an HP */
    MUTATE((m->pos[2].species = 0, m->pos[2].hp = 0, m->pos[2].form = 1), CC_E_NONCANONICAL);   /* ... or a form */
    MUTATE((m->pos[2].species = 0, m->pos[2].hp = 0, m->pos[2].form = 0), CC_OK);
    MUTATE(m->pos[2].species = 8902, CC_E_RANGE);
    MUTATE(m->pos[2].species = 8901, CC_OK);
    MUTATE(m->pos[2].hp = -1, CC_E_RANGE);
    MUTATE(m->pos[2].hp = 0, CC_OK);
    MUTATE(m->pos[0].form = 1, CC_E_NONCANONICAL);                                    /* an empty position holds nothing */
    MUTATE(m->pos[1].hp = 5, CC_E_NONCANONICAL);
    MUTATE(m->pos[3].form = 1, CC_E_NONCANONICAL);

    /* an initialised reducer is exactly the initial state; a begun one with nothing accepted is nearly so */
    CHECK_STATUS(cc_reducer_init(m), CC_OK);
    CHECK_STATUS(cc_reducer_validate(m), CC_OK);
    m->closed = 1;
    CHECK_STATUS(cc_reducer_validate(m), CC_E_CONSISTENCY);
    CHECK_STATUS(cc_reducer_init(m), CC_OK);
    m->next_seq = 2;
    CHECK_STATUS(cc_reducer_validate(m), CC_E_CONSISTENCY);
    CHECK_STATUS(cc_reducer_init(m), CC_OK);
    m->last_digest = 1;
    CHECK_STATUS(cc_reducer_validate(m), CC_E_CONSISTENCY);
    CHECK_STATUS(cc_reducer_init(m), CC_OK);
    m->hist.flags = CC_HF_BEGAN_MID;
    CHECK_STATUS(cc_reducer_validate(m), CC_E_CONSISTENCY);
    CHECK_STATUS(cc_reducer_init(m), CC_OK);
    m->pos[1].species = 4;
    CHECK_STATUS(cc_reducer_validate(m), CC_E_CONSISTENCY);
    CHECK_STATUS(cc_reducer_init(m), CC_OK);
    m->last_batch_id = 1;
    CHECK_STATUS(cc_reducer_validate(m), CC_E_CONSISTENCY);
    CHECK_STATUS(cc_reducer_init(m), CC_OK);
    m->hist.n_turns = 1;                                                    /* a well-formed turn, but no run */
    m->hist.turns[0].battle_seq = 1;
    m->hist.turns[0].wave = 1;
    CHECK_STATUS(cc_history_validate(&m->hist), CC_OK);
    CHECK_STATUS(cc_reducer_validate(m), CC_E_CONSISTENCY);

    CHECK_STATUS(cc_reducer_begin_run(m, RUN_A, true), CC_OK);
    CHECK_STATUS(cc_reducer_validate(m), CC_OK);
    m->last_digest = 1;
    CHECK_STATUS(cc_reducer_validate(m), CC_E_CONSISTENCY);
    CHECK_STATUS(cc_reducer_begin_run(m, RUN_B, true), CC_OK);
    m->next_seq = 3;
    CHECK_STATUS(cc_reducer_validate(m), CC_E_CONSISTENCY);
    CHECK_STATUS(cc_reducer_begin_run(m, RUN_C, true), CC_OK);
    m->pos[0].species = 3;
    CHECK_STATUS(cc_reducer_validate(m), CC_E_CONSISTENCY);
    CHECK_STATUS(cc_reducer_begin_run(m, RUN_A, false), CC_OK);
    m->hist.n_turns = 1;                                                    /* a turn, but no batch was accepted */
    m->hist.turns[0].battle_seq = 1;
    m->hist.turns[0].wave = 1;
    CHECK_STATUS(cc_reducer_validate(m), CC_E_CONSISTENCY);
    CHECK_STATUS(cc_reducer_begin_run(m, RUN_B, false), CC_OK);
    m->last_batch_id = 5;                                                   /* a batch was accepted, but it left no turn */
    CHECK_STATUS(cc_reducer_validate(m), CC_E_CONSISTENCY);
}

/* ------------------------------------------------------------------ differential run against a naive model */

typedef struct {
    uint32_t battle, wave, turn, dropped;
    unsigned n;
    uint32_t seq[CC_MAX_EVENTS_PER_TURN];
    int32_t shown[CC_MAX_EVENTS_PER_TURN];
} model_turn;

static struct {
    unsigned n;
    model_turn t[CC_HISTORY_TURNS];
    uint32_t next_seq;
} MODEL;

/* One event at a time, one shift per event over the limit: the obvious implementation the reducer must agree with. */
static void model_batch(uint32_t battle, unsigned wave, unsigned turn, const cc_obs *obs, unsigned n)
{
    model_turn *t;
    if (MODEL.n && MODEL.t[MODEL.n - 1].battle == battle && MODEL.t[MODEL.n - 1].turn == turn) {
        t = &MODEL.t[MODEL.n - 1];
    } else {
        if (MODEL.n == CC_HISTORY_TURNS) {
            for (unsigned i = 1; i < CC_HISTORY_TURNS; i++)
                MODEL.t[i - 1] = MODEL.t[i];
            MODEL.n--;
        }
        t = &MODEL.t[MODEL.n++];
        memset(t, 0, sizeof *t);
        t->battle = battle;
        t->wave = wave;
        t->turn = turn;
    }
    for (unsigned i = 0; i < n; i++) {
        if (obs[i].kind != CC_OBS_DAMAGE) continue;
        const uint32_t seq = MODEL.next_seq++;
        if (t->n == CC_MAX_EVENTS_PER_TURN) {
            for (unsigned k = 1; k < t->n; k++) {
                t->seq[k - 1] = t->seq[k];
                t->shown[k - 1] = t->shown[k];
            }
            t->n--;
            t->dropped++;
        }
        t->seq[t->n] = seq;
        t->shown[t->n] = obs[i].value;
        t->n++;
    }
}

static uint64_t rng_state = 0x9E3779B97F4A7C15ULL;

static unsigned rnd(unsigned n)
{
    rng_state = rng_state * 6364136223846793005ULL + 1442695040888963407ULL;
    return (unsigned)(rng_state >> 33) % n;
}

static void test_differential(void)
{
    cc_reducer *r = &R;
    fresh(r, RUN_A);
    memset(&MODEL, 0, sizeof MODEL);
    MODEL.next_seq = 1;
    uint32_t battle = 1, wave = 1, turn = 0, id = 0;
    unsigned compared = 0;
    for (int step = 0; step < 600; step++) {
        const unsigned c = rnd(12);
        if (c >= 6 && c < 10) {
            turn++;
        } else if (c >= 10) {
            battle++;
            wave += rnd(2);
            turn = rnd(2);
        }
        unsigned n = rnd(110);
        if (step % 17 == 0) n = CC_MAX_BATCH_OBS - rnd(3);
        for (unsigned i = 0; i < n; i++) {
            const unsigned k = rnd(20);
            if (k == 0) OBS[i] = o_anim(0, 25, 2, 19, 1 + rnd(900));
            else if (k == 1) OBS[i] = o_heal(2, 19, 0, -1);
            else OBS[i] = o_dmg((int)rnd(4), 19, (int)rnd(1000), -1, rnd(16));
        }
        cc_batch b = mk(++id, battle, wave, turn, OBS, n);
        GO(r, b);
        model_batch(battle, wave, turn, OBS, n);

        CHECK_EQ(r->hist.n_turns, MODEL.n);
        CHECK_EQ(r->next_seq, MODEL.next_seq);
        for (unsigned i = 0; i < MODEL.n && i < r->hist.n_turns; i++) {
            const cc_turn *t = &TURN(r, i);
            const model_turn *m = &MODEL.t[i];
            int bad = t->battle_seq != m->battle || t->wave != m->wave || t->turn != m->turn || t->n_events != m->n ||
                      t->dropped != m->dropped || t->flags != (m->dropped ? CC_TF_EVICTED : 0);
            for (unsigned k = 0; k < m->n && k < t->n_events; k++)
                bad |= t->events[k].seq != m->seq[k] || t->events[k].shown != m->shown[k] || t->events[k].kind != CC_EV_DAMAGE;
            CHECK(!bad);
            compared += m->n;
        }
    }
    CHECK(compared > 10000);                                                /* the run really exercised eviction */
    CHECK(MODEL.next_seq > 5000);
}

int main(void)
{
    test_runs_and_empty();
    test_rejections();
    test_mapping();
    test_attribution();
    test_shown_vs_measured();
    test_tracker();
    test_eviction();
    test_repeats();
    test_boundaries();
    test_incomplete();
    test_save_replay();
    test_summary();
    test_state_validation();
    test_differential();
    return cc_summary("reducer_test");
}
