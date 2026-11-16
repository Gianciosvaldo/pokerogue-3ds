/* The app's timed-events source (source/events_host.c): the console's wall clock as the module's seconds, and the shipped table's effects at
 * known instants. The clock conversion is checked against the C library's own timegm on 20000 random instants from 1970 to 2100, so the
 * expectation does not come from the conversion under test; the effects are checked at hand-picked instants of the shipped table.
 * make events_host_test && build/events_host_test */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "events_host.h"

static int checks;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "events_host FAIL line %d: %s\n", __LINE__, #x); exit(1); } checks++; } while (0)
#define CHECK_EQ(a, b) do { long long va_ = (long long)(a), vb_ = (long long)(b); if (va_ != vb_) { \
    fprintf(stderr, "events_host FAIL line %d: %s == %s (%lld vs %lld)\n", __LINE__, #a, #b, va_, vb_); exit(1); } checks++; } while (0)

/* what eh_install hands to game-core */
static const gc_event_source *installed;
void gc_run_set_events(struct gc_run *run, const gc_event_source *src) { (void)run; installed = src; }

static int64_t at(int y, int mo, int d, int h, int mi, int s)
{
    struct tm tm;
    memset(&tm, 0, sizeof tm);
    tm.tm_year = y - 1900; tm.tm_mon = mo - 1; tm.tm_mday = d; tm.tm_hour = h; tm.tm_min = mi; tm.tm_sec = s;
    return eh_local_seconds(&tm);
}

static gc_event_effects fx_at(int64_t t)
{
    gc_event_effects fx;
    eh_effects_at(t, &fx);
    return fx;
}

int main(void)
{
    /* known dates */
    CHECK_EQ(at(1970, 1, 1, 0, 0, 0), 0);
    CHECK_EQ(at(1970, 1, 1, 0, 0, 59), 59);
    CHECK_EQ(at(1970, 1, 2, 0, 0, 0), 86400);
    CHECK_EQ(at(2000, 3, 1, 0, 0, 0), 951868800);      /* after the 29th of February of a leap year divisible by 400 */
    CHECK_EQ(at(2024, 2, 29, 12, 0, 0), 1709208000);   /* a leap day */
    CHECK_EQ(at(2026, 11, 16, 12, 0, 0), 20773ll * 86400 + 43200);
    CHECK_EQ(at(2100, 3, 1, 0, 0, 0) - at(2100, 2, 28, 0, 0, 0), 86400);   /* 2100 is not a leap year */
    CHECK_EQ(at(2038, 1, 19, 3, 14, 8), 2147483648ll);                      /* past the 32-bit time_t */
    CHECK_EQ(at(9999, 12, 31, 23, 59, 59), 253402300799ll);

    /* not a date: refused, never wrapped */
    CHECK_EQ(at(1969, 12, 31, 23, 0, 0), -1);   /* -3600 if it were computed */
    CHECK_EQ(at(1960, 6, 1, 0, 0, 0), -1);
    CHECK_EQ(at(10000, 1, 1, 0, 0, 0), -1);
    CHECK_EQ(at(2026, 0, 1, 0, 0, 0), -1);
    CHECK_EQ(at(2026, 13, 1, 0, 0, 0), -1);
    CHECK_EQ(at(2026, 1, 0, 0, 0, 0), -1);
    CHECK_EQ(at(2026, 1, 32, 0, 0, 0), -1);
    CHECK_EQ(at(2026, 1, 1, 24, 0, 0), -1);
    CHECK_EQ(at(2026, 1, 1, 0, 60, 0), -1);
    CHECK_EQ(at(2026, 1, 1, 0, 0, 61), -1);
    CHECK_EQ(at(2026, 1, 1, -1, 0, 0), -1);
    CHECK_EQ(at(2026, 1, 1, 0, -1, 0), -1);
    CHECK_EQ(at(2026, 1, 1, 0, 0, -1), -1);
    CHECK(eh_local_seconds(NULL) == -1);

    /* against the C library, on 20000 instants (a fixed generator, so a failure repeats) */
    uint64_t x = 0x9E3779B97F4A7C15ull;
    for (int i = 0; i < 20000; i++) {
        x ^= x << 13; x ^= x >> 7; x ^= x << 17;
        const time_t t = (time_t)(x % 4102444800ull);   /* 1970 .. 2100 */
        struct tm tm;
        CHECK(gmtime_r(&t, &tm) != NULL);
        CHECK_EQ(eh_local_seconds(&tm), (int64_t)t);
        CHECK_EQ(eh_local_seconds(&tm), (int64_t)timegm(&tm));
    }

    /* the effects of the shipped table (custom-core's tevents_table.c) at hand-picked instants.
     * Day numbers: 2026-11-16 is day 20773 (odd: Voucher Plus Day, every second day, is on); Shiny Day (every third day) is on days divisible by 3. */
    CHECK_EQ(20773 % 2, 1);
    CHECK_EQ(20775 % 3, 0);
    /* 2026-11-16 (odd day, not a third day) 12:00: Lucky Catch, Sweet Tooth and Voucher Plus Day only */
    gc_event_effects fx = fx_at(at(2026, 11, 16, 12, 0, 0));
    CHECK_EQ(fx.shiny_catch_mult, 3);
    CHECK_EQ(fx.candy_mult, 4);
    CHECK_EQ(fx.upgrade_vouchers, 1);
    CHECK_EQ(fx.shiny_encounter_mult, 0);
    CHECK_EQ(fx.luck_boost, 0);
    CHECK_EQ(fx.trainer_shiny_chance, 0);
    /* 13:00 the same day: Shiny Trainers opens (13:00 to 14:00) */
    CHECK_EQ(fx_at(at(2026, 11, 16, 12, 59, 59)).trainer_shiny_chance, 0);
    CHECK_EQ(fx_at(at(2026, 11, 16, 13, 0, 0)).trainer_shiny_chance, 6554);
    CHECK_EQ(fx_at(at(2026, 11, 16, 13, 59, 59)).trainer_shiny_chance, 6554);
    CHECK_EQ(fx_at(at(2026, 11, 16, 14, 0, 0)).trainer_shiny_chance, 0);
    /* 20:00 to 21:00: Golden Hour */
    CHECK_EQ(fx_at(at(2026, 11, 16, 19, 59, 59)).luck_boost, 0);
    CHECK_EQ(fx_at(at(2026, 11, 16, 20, 0, 0)).luck_boost, 1);
    CHECK_EQ(fx_at(at(2026, 11, 16, 20, 59, 59)).luck_boost, 1);
    CHECK_EQ(fx_at(at(2026, 11, 16, 21, 0, 0)).luck_boost, 0);
    /* 2026-11-17 (even day): no Voucher Plus Day; 2026-11-18 (a third day, odd): Shiny Day and Voucher Plus Day */
    CHECK_EQ(fx_at(at(2026, 11, 17, 12, 0, 0)).upgrade_vouchers, 0);
    CHECK_EQ(fx_at(at(2026, 11, 17, 12, 0, 0)).shiny_encounter_mult, 0);
    CHECK_EQ(fx_at(at(2026, 11, 18, 0, 0, 0)).shiny_encounter_mult, 2);
    CHECK_EQ(fx_at(at(2026, 11, 18, 23, 59, 59)).shiny_encounter_mult, 2);
    CHECK_EQ(fx_at(at(2026, 11, 19, 0, 0, 0)).shiny_encounter_mult, 0);
    CHECK_EQ(fx_at(at(2026, 11, 18, 12, 0, 0)).upgrade_vouchers, 1);
    /* a negative instant: nothing, even though two events are open all day */
    fx = fx_at(-1);
    CHECK_EQ(fx.shiny_catch_mult, 0);
    CHECK_EQ(fx.candy_mult, 0);

    /* the source game-core asks follows the pinned clock, live otherwise */
    gc_event_effects out;
    memset(&out, 0xAA, sizeof out);
    eh_use_fixed_clock(at(2026, 11, 18, 20, 30, 0));
    CHECK(eh_source() != NULL && eh_source()->effects != NULL);
    memset(&out, 0, sizeof out);
    eh_source()->effects(eh_source()->user, &out);
    CHECK_EQ(out.shiny_encounter_mult, 2);
    CHECK_EQ(out.luck_boost, 1);
    CHECK_EQ(out.shiny_catch_mult, 3);
    CHECK_EQ(out.upgrade_vouchers, 1);
    eh_use_fixed_clock(at(2026, 11, 17, 3, 0, 0));   /* asked again: the clock is not read once */
    memset(&out, 0, sizeof out);
    eh_source()->effects(eh_source()->user, &out);
    CHECK_EQ(out.shiny_encounter_mult, 0);
    CHECK_EQ(out.luck_boost, 0);
    CHECK_EQ(out.shiny_catch_mult, 3);
    eh_use_fixed_clock(-1);   /* a clock that cannot be read: no event */
    memset(&out, 0, sizeof out);
    eh_source()->effects(eh_source()->user, &out);
    CHECK_EQ(out.shiny_catch_mult, 0);
    CHECK_EQ(out.candy_mult, 0);
    eh_use_live_clock();
    const int64_t now = eh_now();
    CHECK(now > at(2025, 1, 1, 0, 0, 0));   /* this machine's clock reads as a date */
    CHECK(now < at(2200, 1, 1, 0, 0, 0) || now == -1);

    installed = NULL;
    eh_install(NULL);
    CHECK(installed == eh_source());

    printf("events_host: %d checks, 0 failures\n", checks);
    return 0;
}
