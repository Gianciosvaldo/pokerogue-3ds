/* The app's timed-events source: see events_host.h. */
#include "events_host.h"

#include <string.h>

#include "cc/tevents.h"

/* Days from 1970-01-01 to a civil date (proleptic Gregorian; y 1970..9999, m 1..12, d 1..31: the caller has checked them, so y is never
 * negative here and the era is a plain division). The standard day-count algorithm, written out here so the app does not depend on
 * timegm, which the 3DS C library may not have. */
static int64_t days_from_civil(int64_t y, int m, int d)
{
    y -= m <= 2;
    const int64_t era = y / 400;
    const int64_t yoe = y - era * 400;
    const int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

int64_t eh_local_seconds(const struct tm *tm)
{
    if (!tm) return -1;
    const int64_t y = (int64_t)tm->tm_year + 1900;
    if (y < 1970 || y > 9999) return -1;
    if (tm->tm_mon < 0 || tm->tm_mon > 11 || tm->tm_mday < 1 || tm->tm_mday > 31) return -1;
    if (tm->tm_hour < 0 || tm->tm_hour > 23 || tm->tm_min < 0 || tm->tm_min > 59 || tm->tm_sec < 0 || tm->tm_sec > 60) return -1;
    return days_from_civil(y, tm->tm_mon + 1, tm->tm_mday) * 86400 + tm->tm_hour * 3600 + tm->tm_min * 60 + tm->tm_sec;
}

static int fixed_on;
static int64_t fixed_s;

void eh_use_fixed_clock(int64_t local_s)
{
    fixed_on = 1;
    fixed_s = local_s;
}

void eh_use_live_clock(void) { fixed_on = 0; }

int64_t eh_now(void)
{
    if (fixed_on) return fixed_s;
    const time_t t = time(NULL);   /* (time_t)-1 when it cannot be read: that is 1969, which eh_local_seconds refuses */
    struct tm tm;
    memset(&tm, 0, sizeof tm);
    if (!localtime_r(&t, &tm)) return -1;
    return eh_local_seconds(&tm);
}

void eh_effects_at(int64_t local_s, gc_event_effects *out)
{
    memset(out, 0, sizeof *out);
    if (local_s < 0) return;
    const cc_tevent_effects fx = cc_tevents_effects(local_s);
    out->shiny_encounter_mult = fx.shiny_encounter_mult;
    out->shiny_catch_mult = fx.shiny_catch_mult;
    out->candy_mult = fx.candy_mult;
    out->luck_boost = fx.luck_boost;
    out->trainer_shiny_chance = fx.trainer_shiny_chance;
    out->upgrade_vouchers = fx.upgrade_vouchers;
}

static void ask(void *user, gc_event_effects *out)
{
    (void)user;
    eh_effects_at(eh_now(), out);
}

static const gc_event_source SOURCE = {NULL, ask};

const gc_event_source *eh_source(void) { return &SOURCE; }

void eh_install(gc_run *run) { gc_run_set_events(run, &SOURCE); }
