/* custom-core: timed events (see cc/tevents.h). Pure: no clock; the time is always an argument. */
#include "cc/tevents.h"

#define MINUTE 60ll
#define DAY 86400ll
#define MINUTES_PER_DAY 1440u

/* floor division and modulo for a possibly negative numerator and a positive divisor */
static int64_t floor_div(int64_t a, int64_t b)
{
    int64_t q = a / b;
    if (a % b != 0 && a < 0) q--;
    return q;
}

static int64_t floor_mod(int64_t a, int64_t b)
{
    int64_t m = a % b;
    return m < 0 ? m + b : m;
}

static int time_ok(int64_t t) { return t >= -CC_TEVENT_TIME_MAX && t <= CC_TEVENT_TIME_MAX; }

/* the length of a C string up to `max` + 1 characters (the module calls no libc) */
static unsigned bounded_len(const char *s, unsigned max)
{
    unsigned n = 0;
    while (n <= max && s[n]) n++;
    return n;
}

static int id_char_ok(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
}

static int same_string(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

cc_status cc_tevent_schedule_validate(const cc_tevent_schedule *s)
{
    if (!s) return CC_E_NULL;
    if (s->period_days < 1 || s->period_days > CC_TEVENT_PERIOD_MAX) return CC_E_RANGE;
    if (s->phase_day >= s->period_days) return CC_E_RANGE;
    if (s->start_minute >= MINUTES_PER_DAY) return CC_E_RANGE;
    if (s->length_minutes < 1 || s->length_minutes > s->period_days * MINUTES_PER_DAY) return CC_E_RANGE;
    return CC_OK;
}

cc_status cc_tevent_effects_validate(const cc_tevent_effects *f)
{
    if (!f) return CC_E_NULL;
    if (f->shiny_encounter_mult > CC_TEVENT_MULT_MAX) return CC_E_RANGE;
    if (f->shiny_catch_mult > CC_TEVENT_MULT_MAX) return CC_E_RANGE;
    if (f->candy_mult > CC_TEVENT_MULT_MAX) return CC_E_RANGE;
    if (f->luck_boost > CC_TEVENT_LUCK_MAX) return CC_E_RANGE;
    if (f->upgrade_vouchers > 1) return CC_E_RANGE;
    return CC_OK;
}

cc_status cc_tevent_validate(const cc_tevent *e)
{
    if (!e) return CC_E_NULL;
    if (!e->id || !e->name) return CC_E_NULL;
    unsigned idlen = bounded_len(e->id, CC_TEVENT_NAME_MAX);
    if (idlen == 0 || idlen > CC_TEVENT_NAME_MAX) return CC_E_RANGE;
    for (unsigned i = 0; i < idlen; i++)
        if (!id_char_ok(e->id[i])) return CC_E_RANGE;
    unsigned namelen = bounded_len(e->name, CC_TEVENT_NAME_MAX);
    if (namelen == 0 || namelen > CC_TEVENT_NAME_MAX) return CC_E_RANGE;
    cc_status st = cc_tevent_schedule_validate(&e->when);
    if (st != CC_OK) return st;
    st = cc_tevent_effects_validate(&e->fx);
    if (st != CC_OK) return st;
    if (!e->fx.shiny_encounter_mult && !e->fx.shiny_catch_mult && !e->fx.candy_mult && !e->fx.luck_boost && !e->fx.trainer_shiny_chance &&
        !e->fx.upgrade_vouchers)
        return CC_E_CONSISTENCY;
    return CC_OK;
}

cc_status cc_tevents_validate_in(const cc_tevent *ev, unsigned count)
{
    if (!ev) return CC_E_NULL;
    if (count > CC_TEVENTS_MAX) return CC_E_COUNT;
    for (unsigned i = 0; i < count; i++) {
        cc_status st = cc_tevent_validate(&ev[i]);
        if (st != CC_OK) return st;
        for (unsigned j = 0; j < i; j++)
            if (same_string(ev[i].id, ev[j].id)) return CC_E_DUPLICATE;
    }
    return CC_OK;
}

cc_status cc_tevents_validate(void) { return cc_tevents_validate_in(cc_tevents, cc_tevent_count); }

cc_status cc_tevent_window(const cc_tevent_schedule *s, int64_t local_s, int *active, int64_t *start_s, int64_t *end_s)
{
    if (!s || !active || !start_s || !end_s) return CC_E_NULL;
    if (cc_tevent_schedule_validate(s) != CC_OK || !time_ok(local_s)) return CC_E_RANGE;
    const int64_t period = s->period_days, len_s = (int64_t)s->length_minutes * MINUTE, off_s = (int64_t)s->start_minute * MINUTE;
    /* An opening on day D is open during [D*DAY + off_s, D*DAY + off_s + len_s). One that began up to `back` days ago can still be open
     * (len_s <= period*DAY, but the first opening of a period may be the one that has not ended). The first opening whose end is after
     * local_s is the open one or the next one: openings are in order and all have the same length. */
    const int64_t today = floor_div(local_s, DAY);
    const int64_t back = ((int64_t)s->length_minutes + MINUTES_PER_DAY - 1) / MINUTES_PER_DAY;
    const int64_t from = today - back;
    int64_t d = from + floor_mod((int64_t)s->phase_day - from, period);
    for (int64_t guard = 0; guard < back / period + 3; guard++, d += period) {
        const int64_t b = d * DAY + off_s, e = b + len_s;
        if (local_s < e) {
            *active = local_s >= b;
            *start_s = b;
            *end_s = e;
            return CC_OK;
        }
    }
    return CC_E_CONSISTENCY;   /* unreachable: the opening of day today + period begins after local_s */
}

uint32_t cc_tevents_active_in(const cc_tevent *ev, unsigned count, int64_t local_s)
{
    if (cc_tevents_validate_in(ev, count) != CC_OK || !time_ok(local_s)) return 0;
    uint32_t mask = 0;
    for (unsigned i = 0; i < count; i++) {
        int active = 0;
        int64_t b = 0, e = 0;
        if (cc_tevent_window(&ev[i].when, local_s, &active, &b, &e) == CC_OK && active) mask |= 1u << i;
    }
    return mask;
}

cc_tevent_effects cc_tevents_effects_in(const cc_tevent *ev, unsigned count, int64_t local_s)
{
    cc_tevent_effects out = {0, 0, 0, 0, 0, 0};
    const uint32_t mask = cc_tevents_active_in(ev, count, local_s);
    for (unsigned i = 0; i < count && i < CC_TEVENTS_MAX; i++) {
        if (!(mask >> i & 1u)) continue;
        const cc_tevent_effects *f = &ev[i].fx;
        if (f->shiny_encounter_mult > out.shiny_encounter_mult) out.shiny_encounter_mult = f->shiny_encounter_mult;
        if (f->shiny_catch_mult > out.shiny_catch_mult) out.shiny_catch_mult = f->shiny_catch_mult;
        if (f->candy_mult > out.candy_mult) out.candy_mult = f->candy_mult;
        if (f->luck_boost > out.luck_boost) out.luck_boost = f->luck_boost;
        if (f->trainer_shiny_chance > out.trainer_shiny_chance) out.trainer_shiny_chance = f->trainer_shiny_chance;
        if (f->upgrade_vouchers > out.upgrade_vouchers) out.upgrade_vouchers = f->upgrade_vouchers;
    }
    return out;
}

uint32_t cc_tevents_active(int64_t local_s) { return cc_tevents_active_in(cc_tevents, cc_tevent_count, local_s); }

cc_tevent_effects cc_tevents_effects(int64_t local_s) { return cc_tevents_effects_in(cc_tevents, cc_tevent_count, local_s); }
