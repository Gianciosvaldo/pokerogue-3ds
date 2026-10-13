/* The app's combat observer (see combat_observer.h). */
#include "combat_observer.h"

#include <string.h>

#include "cc/model.h"

/* ~46 KB of static data: the reducer, one batch of observations and the report. None of it belongs on the 3DS stack. */
static struct {
  cc_reducer red;
  cc_obs obs[CC_MAX_BATCH_OBS];
  co_report report;
  co_stats st;
  bool active;            /* a run is being observed */
  bool finished;          /* ... and has ended */
  bool resumed;
  cc_run_uid uid;
  bool uid_durable;
  uint32_t batch_id;      /* the last id used; every batch built takes one, accepted or not, so a refused one shows as a gap */
  uint32_t drain_no;      /* the last drain number taken in */
  uint32_t battle_seq;    /* GC_EV_WAVE seen in this run */
  uint16_t wave;
  uint16_t turn;
  bool wave_ok, turn_ok;
  bool pending_loss;      /* events were lost and no stored turn records it yet: the next batch carries the flag */
  bool queue_overflow_seen;
} C;

static cc_run_uid g_next_uid = 1;   /* this session only */
static co_uid_source g_uid_source;

void co_set_uid_source(co_uid_source source) { g_uid_source = source; }

/* ------------------------------------------------------------------ one event -> one observation */

/* 1: obs made. 0: not an event this contract observes. -1: observed kind, but it cannot be represented (dropped, never invented). */
static int to_obs(const pr_event* e, cc_obs* o) {
  uint8_t kind;
  switch (e->type) {
    case PR_EV_DAMAGE:
      kind = CC_OBS_DAMAGE;
      break;
    case PR_EV_HP:
      kind = CC_OBS_HP;
      break;
    case PR_EV_FAINT:
      kind = CC_OBS_FAINT;
      break;
    case PR_EV_SWITCH_OUT:
      kind = CC_OBS_SWITCH_OUT;
      break;
    case PR_EV_SWITCH_IN:
      kind = CC_OBS_SWITCH_IN;
      break;
    default:
      return 0;
  }
  memset(o, 0, sizeof *o);
  if (e->battler < 0 || e->battler > 3) return -1;
  const pr_event_field* f = &e->field[e->battler];
  o->kind = kind;
  o->battler = e->battler;
  o->target = -1;
  o->battler_species = f->species;
  o->battler_form = f->form;
  if (f->max_hp > 0 && f->hp >= 0 && f->hp <= f->max_hp) {
    o->hp_after = f->hp;
    o->hp_max_after = f->max_hp;
  } else {
    o->hp_after = o->hp_max_after = -1; /* no usable snapshot: unknown, not a measured 0 */
  }
  o->status_after = f->status;
  if (kind == CC_OBS_DAMAGE) {
    o->id = e->id;
    o->value = e->value;
  } else if (kind == CC_OBS_HP || kind == CC_OBS_SWITCH_IN) {
    o->value = e->value;
  }
  return cc_obs_validate(o) == CC_OK ? 1 : -1;
}

/* ------------------------------------------------------------------ batches */

static void count_result(const cc_reduce_result* r) {
  C.st.accepted++;
  C.st.ignored += r->ignored;
  C.st.sign_conflicts += r->sign_conflicts;
  C.st.evicted += r->evicted;
}

/* A batch of the current wave and turn with `n` observations from C.obs. */
static cc_status reduce_now(uint8_t flags, int n, cc_reduce_result* r) {
  const cc_batch b = {.run_uid = C.uid,
                      .batch_id = ++C.batch_id,
                      .battle_seq = C.battle_seq,
                      .wave = C.wave,
                      .turn = C.turn,
                      .n_obs = (uint16_t)n,
                      .flags = flags,
                      .obs = n ? C.obs : NULL};
  C.st.batches++;
  return cc_reduce(&C.red, &b, r);
}

/* Something was lost: put it on the turn it belongs to with an empty flagged batch. If even that is refused, the next batch (or the
 * report) carries it. */
static void record_loss(void) {
  cc_reduce_result r;
  C.st.loss_flagged++;
  if (cc_reduce(&C.red,
                &(cc_batch){.run_uid = C.uid, .batch_id = ++C.batch_id, .battle_seq = C.battle_seq, .wave = C.wave, .turn = C.turn,
                            .n_obs = 0, .flags = CC_BF_SOURCE_LOSS, .obs = NULL},
                &r) == CC_OK) {
    C.st.batches++;
    count_result(&r);
    C.pending_loss = false;
  } else {
    C.st.batches++;
    C.st.rejected++;
    C.pending_loss = true;
  }
}

static void observe_range(int a, int b, bool drain_loss, const pr_event* be, int n_be) {
  C.st.ranges++;
  if (!C.wave_ok || !C.turn_ok) {
    C.st.unkeyed++; /* no turn to put it on */
    C.pending_loss = true;
    return;
  }
  if (a < 0 || b <= 0 || b > CC_MAX_BATCH_OBS || a > n_be || b > n_be - a) {
    C.st.bad_range++;
    record_loss();
    return;
  }
  int n = 0;
  bool lost = false;
  for (int k = 0; k < b; k++) {
    const int r = to_obs(&be[a + k], &C.obs[n]);
    if (r > 0)
      n++;
    else if (r < 0) {
      C.st.unrepresentable++;
      lost = true;
    }
  }
  uint8_t flags = (uint8_t)((drain_loss || lost || C.pending_loss) ? CC_BF_SOURCE_LOSS : 0);
  if (n == 0 && !flags) {
    C.st.empty_ranges++; /* only texts and animations: nothing observed, and an empty turn would push a real one out */
    return;
  }
  if (flags) C.st.loss_flagged++;
  cc_reduce_result r;
  const cc_status s = reduce_now(flags, n, &r);
  if (s != CC_OK) {
    C.st.rejected++;
    C.st.last_status = (int)s;
    record_loss();
    return;
  }
  count_result(&r);
  C.st.events_in += (uint32_t)n;
  C.pending_loss = false;
}

/* ------------------------------------------------------------------ the calls */

void co_begin_run(bool resumed) {
  memset(&C.st, 0, sizeof C.st);
  memset(&C.report, 0, sizeof C.report);
  C.finished = false;
  C.resumed = resumed;
  C.batch_id = 0;
  C.drain_no = 0;
  C.battle_seq = 0;
  C.wave = C.turn = 0;
  C.wave_ok = C.turn_ok = false;
  C.pending_loss = false;
  C.queue_overflow_seen = false;
  C.uid = g_uid_source ? g_uid_source() : 0;
  C.uid_durable = C.uid != 0;
  if (!C.uid_durable) C.uid = g_next_uid++;     /* only this observation needs a name; nothing stores it */
  cc_reducer_init(&C.red);
  C.active = cc_reducer_begin_run(&C.red, C.uid, resumed) == CC_OK;
}

bool co_carry_get(co_carry* out) {
  if (!C.active || C.finished) return false;
  out->hist = C.red.hist;
  out->next_seq = C.red.next_seq;
  out->battle_seq = C.battle_seq;
  return true;
}

bool co_begin_run_carry(const co_carry* carry) {
  co_begin_run(true);
  if (!C.active || !carry) return false;
  C.red.hist = carry->hist;
  C.red.hist.flags |= CC_HF_BEGAN_MID;
  C.red.next_seq = carry->next_seq;
  if (cc_reducer_validate(&C.red) != CC_OK) {
    cc_reducer_init(&C.red);   /* the carry is refused: an empty, began-mid observation */
    C.active = cc_reducer_begin_run(&C.red, C.uid, true) == CC_OK;
    return false;
  }
  C.battle_seq = carry->battle_seq;
  return true;
}

void co_observe(uint32_t drain_no, const gc_event* q, int nq, const pr_event* be, int n_be, bool battle_overflow, bool queue_overflow) {
  if (!C.active || C.finished) return;
  if (drain_no <= C.drain_no) {
    C.st.replayed_drains++;
    return;
  }
  if (C.drain_no && drain_no > C.drain_no + 1) {
    C.st.skipped_drains += drain_no - C.drain_no - 1;
    C.pending_loss = true;
  }
  C.drain_no = drain_no;
  C.st.drains++;

  bool drain_loss = battle_overflow;
  if (queue_overflow && !C.queue_overflow_seen) {
    C.queue_overflow_seen = true;
    drain_loss = true;
  }
  if (nq >= GC_MAX_EVENTS) drain_loss = true; /* the queue was full: the next event may never have been queued */
  bool batch_seen = false;
  for (int i = 0; i < nq; i++) {
    const gc_event* e = &q[i];
    switch (e->kind) {
      case GC_EV_WAVE:
        C.battle_seq++;
        C.wave_ok = e->a >= 1 && e->a <= 0xFFFF;
        C.wave = C.wave_ok ? (uint16_t)e->a : 0;
        C.turn = 0; /* what happens before the first turn of a battle is turn 0 */
        C.turn_ok = true;
        break;
      case GC_EV_TURN:
        C.turn_ok = e->a >= 0 && e->a <= 0xFFFF;
        C.turn = C.turn_ok ? (uint16_t)e->a : 0;
        break;
      case GC_EV_BATTLE:
        batch_seen = true;
        observe_range(e->a, e->b, drain_loss, be, n_be);
        break;
      default:
        break;
    }
  }
  if (drain_loss && !batch_seen) C.pending_loss = true; /* nothing to attach it to in this drain */
}

bool co_finish(bool victory, int biome, int wave) {
  if (!C.active) return false;
  if (C.finished) return true;
  cc_reducer_end_run(&C.red);
  C.finished = true;
  co_report* r = &C.report;
  memset(r, 0, sizeof *r);
  r->valid = true;
  r->run_uid = C.uid;
  r->uid_durable = C.uid_durable;
  r->outcome = victory ? CC_OUT_CLEAR : CC_OUT_DEFEAT;
  r->biome = (int16_t)biome;
  r->wave = (uint16_t)(wave < 0 ? 0 : wave > 0xFFFF ? 0xFFFF : wave);
  r->began_mid = C.resumed;
  r->untracked_loss = C.pending_loss;
  r->history = C.red.hist;
  r->stats = C.st;
  if (!victory) {
    for (int t = r->history.n_turns - 1; t >= 0 && !r->fallen_known; t--) {
      const cc_turn* turn = &r->history.turns[t];
      for (int k = turn->n_events - 1; k >= 0; k--) {
        const cc_event* ev = &turn->events[k];
        if (ev->kind == CC_EV_FAINT && ev->target_side == CC_SIDE_PLAYER) {
          r->fallen_known = true;
          r->fallen_species = ev->target_species;
          r->fallen_form = ev->target_form;
          break;
        }
      }
    }
  }
  return true;
}

const co_report* co_report_get(void) { return C.finished && C.report.valid ? &C.report : NULL; }
const co_stats* co_stats_get(void) { return &C.st; }
