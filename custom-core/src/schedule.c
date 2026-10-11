/* custom-core: structural validation of a run's frozen rematch schedule. The scheduler itself (the draw and the
 * eligibility of a window) is task C05; nothing here draws a number. */
#include "cc/model.h"

cc_status cc_schedule_validate(const cc_schedule *s)
{
    if (!s) return CC_E_NULL;
    if (s->state == CC_SCHED_NONE || s->state >= CC_SCHED_COUNT_) return CC_E_RANGE;
    if (s->run_uid == 0) return CC_E_RANGE;
    if (s->n_frozen > CC_MAX_FROZEN) return CC_E_COUNT;
    for (unsigned i = 0; i < s->n_frozen; i++) {
        if (s->frozen_ids[i] == 0) return CC_E_RANGE;
        if (i > 0) {
            if (s->frozen_ids[i] == s->frozen_ids[i - 1]) return CC_E_DUPLICATE;
            if (s->frozen_ids[i] < s->frozen_ids[i - 1]) return CC_E_ORDER;
        }
    }

    if (s->state == CC_SCHED_NO_CANDIDATE) {
        if (s->n_frozen || s->chosen_id || s->n_windows || s->rng_algo != CC_RNG_ALGO_NONE || s->rng_seed ||
            s->rng_state)
            return CC_E_CONSISTENCY;
        for (int i = 0; i < CC_NUM_WINDOWS; i++)
            if (s->windows[i].wave || s->windows[i].state || s->windows[i].reason) return CC_E_NONCANONICAL;
        return CC_OK;
    }

    /* a record was chosen: it is one of the frozen candidates (so at least one was frozen) and the draw is recorded */
    if (s->rng_algo != CC_RNG_ALGO_V1 || s->n_windows != CC_NUM_WINDOWS) return CC_E_CONSISTENCY;
    bool chosen_frozen = false;
    for (unsigned i = 0; i < s->n_frozen; i++)
        if (s->frozen_ids[i] == s->chosen_id) chosen_frozen = true;
    if (!chosen_frozen) return CC_E_CONSISTENCY;

    for (int i = 0; i < CC_NUM_WINDOWS; i++) {
        const cc_window *w = &s->windows[i];
        if (w->wave != cc_window_waves[i]) return CC_E_RANGE;
        if (w->state >= CC_WIN_COUNT_) return CC_E_RANGE;
        if (w->reason >= CC_SKIP_COUNT_) return CC_E_RANGE;
        if ((w->state == CC_WIN_SKIPPED) != (w->reason != CC_SKIP_NONE)) return CC_E_CONSISTENCY;
    }

    /* skipped windows form a prefix, then the state's own window, then pending windows */
    int i = 0;
    while (i < CC_NUM_WINDOWS && s->windows[i].state == CC_WIN_SKIPPED) i++;
    uint8_t expect;
    switch (s->state) {
    case CC_SCHED_EXHAUSTED:
        return i == CC_NUM_WINDOWS ? CC_OK : CC_E_CONSISTENCY;
    case CC_SCHED_ACTIVE: expect = CC_WIN_PENDING; break;
    case CC_SCHED_OFFERED: expect = CC_WIN_OFFERED; break;
    case CC_SCHED_DECLINED: expect = CC_WIN_DECLINED; break;
    case CC_SCHED_ACCEPTED: expect = CC_WIN_ACCEPTED; break;
    default: return CC_E_RANGE;
    }
    if (i == CC_NUM_WINDOWS || s->windows[i].state != expect) return CC_E_CONSISTENCY;
    for (int j = (expect == CC_WIN_PENDING ? i : i + 1); j < CC_NUM_WINDOWS; j++)
        if (s->windows[j].state != CC_WIN_PENDING) return CC_E_CONSISTENCY;
    return CC_OK;
}
