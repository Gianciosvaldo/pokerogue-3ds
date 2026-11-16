/* Tests of the timed events (cc/tevents.h): the schedule arithmetic at every boundary, the combination of overlapping events, the
 * validators and the shipped draft table. The expected values are literals chosen by hand (a date and its day number are cross-checked
 * with an independent civil-calendar function below), or come from an oracle written apart from the library: a brute-force scan over
 * the days around the time asked, which never uses the library's closed form. */
#include <stdint.h>
#include <string.h>

#include "cc/tevents.h"
#include "check.h"

#define DAY 86400ll

/* days since 1970-01-01 of a proleptic Gregorian date (Howard Hinnant's algorithm), the independent calendar of these tests */
static int64_t days_from_civil(int y, unsigned m, unsigned d)
{
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (int64_t)doe - 719468;
}

static int64_t T(int y, unsigned mo, unsigned d, int h, int mi, int s)
{
    return days_from_civil(y, mo, d) * DAY + h * 3600 + mi * 60 + s;
}

static int64_t fl_mod(int64_t a, int64_t b)
{
    const int64_t m = a % b;
    return m < 0 ? m + b : m;
}
static int64_t fl_div(int64_t a, int64_t b) { return (a - fl_mod(a, b)) / b; }

/* ------------------------------------------------------------------ the oracle */

/* Is a window open at t: scan every opening day of the last 40 days (a window is at most 28 days long) and the next. */
static int oracle_open(const cc_tevent_schedule *s, int64_t t)
{
    const int64_t today = fl_div(t, DAY);
    for (int64_t d = today - 40; d <= today + 1; d++) {
        if (fl_mod(d, s->period_days) != s->phase_day) continue;
        const int64_t b = d * DAY + (int64_t)s->start_minute * 60, e = b + (int64_t)s->length_minutes * 60;
        if (b <= t && t < e) return 1;
    }
    return 0;
}

/* The first opening that begins after t (when none is open). */
static int64_t oracle_next(const cc_tevent_schedule *s, int64_t t)
{
    const int64_t today = fl_div(t, DAY);
    for (int64_t d = today - 1; d <= today + 60; d++) {
        if (fl_mod(d, s->period_days) != s->phase_day) continue;
        const int64_t b = d * DAY + (int64_t)s->start_minute * 60;
        if (b > t) return b;
    }
    return INT64_MIN;
}

static void expect_window(const cc_tevent_schedule *s, int64_t t, int want_active, int64_t want_start, int64_t want_end)
{
    int active = -1;
    int64_t b = 0, e = 0;
    CHECK_EQ(cc_tevent_window(s, t, &active, &b, &e), CC_OK);
    CHECK_EQ(active, want_active);
    CHECK_EQ(b, want_start);
    CHECK_EQ(e, want_end);
}

/* ------------------------------------------------------------------ the calendar helper itself */

static void test_calendar(void)
{
    CHECK_EQ(T(1970, 1, 1, 0, 0, 0), 0);
    CHECK_EQ(T(1969, 12, 31, 0, 0, 0), -86400);
    CHECK_EQ(T(2000, 2, 29, 0, 0, 0), 951782400ll);
    CHECK_EQ(T(2026, 11, 16, 0, 0, 0), 1794787200ll);
    CHECK_EQ(CC_TEVENTS_MAX, 16);
    CHECK_EQ(CC_TEVENT_PERIOD_MAX, 28);
    CHECK_EQ(CC_TEVENT_NAME_MAX, 24);
    CHECK_EQ(CC_TEVENT_MULT_MAX, 10);
    CHECK_EQ(CC_TEVENT_LUCK_MAX, 14);
    CHECK_EQ(T(9999, 12, 31, 23, 59, 59), CC_TEVENT_TIME_MAX);   /* the limit is the last second of year 9999 */
    CHECK_EQ(CC_TEVENT_TIME_MAX, 253402300799ll);
    CHECK_EQ(days_from_civil(2026, 11, 16), 20773);   /* odd: the every-two-days events with phase 1 open on it */
    CHECK_EQ(fl_div(-1, DAY), -1);
    CHECK_EQ(fl_mod(-1, DAY), DAY - 1);
}

/* ------------------------------------------------------------------ one hour each day */

static void test_hourly(void)
{
    const cc_tevent_schedule s = {1, 0, 1200, 60};   /* 20:00 to 21:00 */
    const int64_t open = T(2026, 11, 16, 20, 0, 0), close = T(2026, 11, 16, 21, 0, 0), next_day = T(2026, 11, 17, 20, 0, 0);
    expect_window(&s, T(2026, 11, 16, 19, 59, 59), 0, open, close);
    expect_window(&s, open, 1, open, close);
    expect_window(&s, T(2026, 11, 16, 20, 30, 0), 1, open, close);
    expect_window(&s, T(2026, 11, 16, 20, 59, 59), 1, open, close);
    expect_window(&s, close, 0, next_day, next_day + 3600);
    expect_window(&s, T(2026, 11, 16, 23, 59, 59), 0, next_day, next_day + 3600);
    expect_window(&s, T(2026, 11, 17, 0, 0, 0), 0, next_day, next_day + 3600);
    expect_window(&s, T(2026, 11, 16, 0, 0, 0), 0, open, close);
}

/* ------------------------------------------------------------------ daily, all day */

static void test_daily_all_day(void)
{
    const cc_tevent_schedule s = {1, 0, 0, 1440};
    const int64_t d0 = T(2026, 11, 16, 0, 0, 0);
    expect_window(&s, d0, 1, d0, d0 + DAY);
    expect_window(&s, d0 + DAY - 1, 1, d0, d0 + DAY);
    expect_window(&s, d0 + DAY, 1, d0 + DAY, d0 + 2 * DAY);   /* the next window begins the second this one ends: never a gap */
    expect_window(&s, d0 - 1, 1, d0 - DAY, d0);
}

/* ------------------------------------------------------------------ every two days */

static void test_every_two_days(void)
{
    const cc_tevent_schedule even = {2, 0, 0, 1440}, odd = {2, 1, 0, 1440};
    const int64_t d16 = T(2026, 11, 16, 0, 0, 0), d17 = d16 + DAY, d18 = d17 + DAY, d19 = d18 + DAY;   /* day numbers 20773 (odd), 20774, 20775, 20776 */
    expect_window(&odd, d16, 1, d16, d17);
    expect_window(&even, d16, 0, d17, d18);
    expect_window(&odd, d17, 0, d18, d19);
    expect_window(&even, d17, 1, d17, d18);
    expect_window(&even, d18 - 1, 1, d17, d18);
    expect_window(&even, d18, 0, d19, d19 + DAY);
    expect_window(&odd, d18, 1, d18, d19);

    /* every third day: day 20775 (2026-11-18) is the first multiple of 3 after 2026-11-16 */
    const cc_tevent_schedule third = {3, 0, 0, 1440};
    expect_window(&third, d16, 0, d18, d19);
    expect_window(&third, d17 + DAY - 1, 0, d18, d19);
    expect_window(&third, d18, 1, d18, d19);
    expect_window(&third, d19 - 1, 1, d18, d19);
    expect_window(&third, d19, 0, d18 + 3 * DAY, d19 + 3 * DAY);
}

/* ------------------------------------------------------------------ a window across midnight, a window as long as its period */

static void test_midnight_and_full_period(void)
{
    const cc_tevent_schedule s = {1, 0, 1410, 120};   /* 23:30 to 01:30 the next day */
    const int64_t open = T(2026, 11, 16, 23, 30, 0), close = T(2026, 11, 17, 1, 30, 0);
    expect_window(&s, open - 1, 0, open, close);
    expect_window(&s, open, 1, open, close);
    expect_window(&s, T(2026, 11, 17, 0, 0, 0), 1, open, close);
    expect_window(&s, T(2026, 11, 17, 0, 29, 59), 1, open, close);
    expect_window(&s, close - 1, 1, open, close);
    expect_window(&s, close, 0, T(2026, 11, 17, 23, 30, 0), T(2026, 11, 18, 1, 30, 0));

    const cc_tevent_schedule three = {3, 1, 0, 4320};   /* three whole days, every third day: always open */
    for (int64_t t = T(2026, 11, 16, 0, 0, 0); t < T(2026, 11, 28, 0, 0, 0); t += 3607) {
        int active = 0;
        int64_t b = 0, e = 0;
        CHECK_EQ(cc_tevent_window(&three, t, &active, &b, &e), CC_OK);
        CHECK_EQ(active, 1);
        CHECK(b <= t && t < e);
        CHECK_EQ(e - b, 3 * DAY);
        CHECK_EQ(fl_mod(fl_div(b, DAY), 3), 1);
    }

    const cc_tevent_schedule week = {7, 3, 1380, 2880};   /* 23:00 on day-of-week 3 to 23:00 two days later */
    const int64_t wopen = (fl_div(T(2026, 11, 16, 0, 0, 0), DAY) + fl_mod(3 - fl_div(T(2026, 11, 16, 0, 0, 0), DAY), 7)) * DAY + 1380 * 60;
    expect_window(&week, wopen - 1, 0, wopen, wopen + 2880 * 60);
    expect_window(&week, wopen, 1, wopen, wopen + 2880 * 60);
    expect_window(&week, wopen + 2 * DAY - 1, 1, wopen, wopen + 2880 * 60);
    expect_window(&week, wopen + 2 * DAY, 0, wopen + 7 * DAY, wopen + 7 * DAY + 2880 * 60);
}

/* ------------------------------------------------------------------ before 1970 */

static void test_negative_time(void)
{
    const cc_tevent_schedule s = {1, 0, 1380, 60};   /* 23:00 to 24:00 */
    const int64_t open = -3600, close = 0;           /* 1969-12-31 23:00:00 to 1970-01-01 00:00:00 */
    expect_window(&s, -3601, 0, open, close);
    expect_window(&s, -3600, 1, open, close);
    expect_window(&s, -1, 1, open, close);
    expect_window(&s, 0, 0, DAY - 3600, DAY);
    const cc_tevent_schedule two = {2, 1, 0, 1440};   /* 1969-12-31 is day -1, which is 1 mod 2 */
    expect_window(&two, -86400, 1, -86400, 0);
    expect_window(&two, -1, 1, -86400, 0);
    expect_window(&two, 0, 0, DAY, 2 * DAY);
}

/* ------------------------------------------------------------------ errors change nothing */

static void test_window_errors(void)
{
    const cc_tevent_schedule ok = {1, 0, 0, 60};
    int active = 7;
    int64_t b = 11, e = 22;
    CHECK_EQ(cc_tevent_window(NULL, 0, &active, &b, &e), CC_E_NULL);
    CHECK_EQ(cc_tevent_window(&ok, 0, NULL, &b, &e), CC_E_NULL);
    CHECK_EQ(cc_tevent_window(&ok, 0, &active, NULL, &e), CC_E_NULL);
    CHECK_EQ(cc_tevent_window(&ok, 0, &active, &b, NULL), CC_E_NULL);
    const cc_tevent_schedule bad = {0, 0, 0, 60};
    CHECK_EQ(cc_tevent_window(&bad, 0, &active, &b, &e), CC_E_RANGE);
    CHECK_EQ(cc_tevent_window(&ok, CC_TEVENT_TIME_MAX + 1, &active, &b, &e), CC_E_RANGE);
    CHECK_EQ(cc_tevent_window(&ok, -CC_TEVENT_TIME_MAX - 1, &active, &b, &e), CC_E_RANGE);
    CHECK(active == 7 && b == 11 && e == 22);
    CHECK_EQ(cc_tevent_window(&ok, CC_TEVENT_TIME_MAX, &active, &b, &e), CC_OK);   /* the limit itself is fine and cannot overflow */
    CHECK_EQ(cc_tevent_window(&ok, -CC_TEVENT_TIME_MAX, &active, &b, &e), CC_OK);
}

/* ------------------------------------------------------------------ validators */

static cc_tevent good(void)
{
    cc_tevent e = {"golden_hour", "Golden Hour", {1, 0, 1200, 60}, {0, 0, 0, 1, 0, 0}};
    return e;
}

static void test_validate(void)
{
    cc_tevent e = good();
    CHECK_EQ(cc_tevent_validate(&e), CC_OK);
    CHECK_EQ(cc_tevent_validate(NULL), CC_E_NULL);

    e = good(); e.id = NULL;                                    CHECK_EQ(cc_tevent_validate(&e), CC_E_NULL);
    e = good(); e.name = NULL;                                  CHECK_EQ(cc_tevent_validate(&e), CC_E_NULL);
    e = good(); e.id = "";                                      CHECK_EQ(cc_tevent_validate(&e), CC_E_RANGE);
    e = good(); e.name = "";                                    CHECK_EQ(cc_tevent_validate(&e), CC_E_RANGE);
    e = good(); e.id = "Golden";                                CHECK_EQ(cc_tevent_validate(&e), CC_E_RANGE);   /* upper case */
    e = good(); e.id = "az_09";                                 CHECK_EQ(cc_tevent_validate(&e), CC_OK);        /* the first and last letter, a digit, '_' */
    e = good(); e.id = "golden hour";                           CHECK_EQ(cc_tevent_validate(&e), CC_E_RANGE);
    e = good(); e.id = "golden-hour";                           CHECK_EQ(cc_tevent_validate(&e), CC_E_RANGE);
    e = good(); e.id = "abcdefghijklmnopqrstuvwx";              CHECK_EQ(cc_tevent_validate(&e), CC_OK);        /* 24 characters */
    e = good(); e.id = "abcdefghijklmnopqrstuvwxy";             CHECK_EQ(cc_tevent_validate(&e), CC_E_RANGE);   /* 25 */
    e = good(); e.name = "ABCDEFGHIJKLMNOPQRSTUVWX";            CHECK_EQ(cc_tevent_validate(&e), CC_OK);
    e = good(); e.name = "ABCDEFGHIJKLMNOPQRSTUVWXY";           CHECK_EQ(cc_tevent_validate(&e), CC_E_RANGE);

    e = good(); e.when.period_days = 0;                         CHECK_EQ(cc_tevent_validate(&e), CC_E_RANGE);
    e = good(); e.when.period_days = CC_TEVENT_PERIOD_MAX;      CHECK_EQ(cc_tevent_validate(&e), CC_OK);
    e = good(); e.when.period_days = CC_TEVENT_PERIOD_MAX + 1;  CHECK_EQ(cc_tevent_validate(&e), CC_E_RANGE);
    e = good(); e.when.phase_day = 1;                           CHECK_EQ(cc_tevent_validate(&e), CC_E_RANGE);   /* period 1: only phase 0 */
    e = good(); e.when.period_days = 2; e.when.phase_day = 1;   CHECK_EQ(cc_tevent_validate(&e), CC_OK);
    e = good(); e.when.period_days = 2; e.when.phase_day = 2;   CHECK_EQ(cc_tevent_validate(&e), CC_E_RANGE);
    e = good(); e.when.start_minute = 1439;                     CHECK_EQ(cc_tevent_validate(&e), CC_OK);
    e = good(); e.when.start_minute = 1440;                     CHECK_EQ(cc_tevent_validate(&e), CC_E_RANGE);
    e = good(); e.when.length_minutes = 0;                      CHECK_EQ(cc_tevent_validate(&e), CC_E_RANGE);
    e = good(); e.when.length_minutes = 1440;                   CHECK_EQ(cc_tevent_validate(&e), CC_OK);        /* as long as the period */
    e = good(); e.when.length_minutes = 1441;                   CHECK_EQ(cc_tevent_validate(&e), CC_E_RANGE);   /* longer: two windows would overlap */
    e = good(); e.when.period_days = 2; e.when.length_minutes = 2880; CHECK_EQ(cc_tevent_validate(&e), CC_OK);
    e = good(); e.when.period_days = 2; e.when.length_minutes = 2881; CHECK_EQ(cc_tevent_validate(&e), CC_E_RANGE);

    e = good(); e.fx.luck_boost = 0; e.fx.shiny_encounter_mult = CC_TEVENT_MULT_MAX;      CHECK_EQ(cc_tevent_validate(&e), CC_OK);
    e = good(); e.fx.luck_boost = 0; e.fx.shiny_encounter_mult = CC_TEVENT_MULT_MAX + 1;  CHECK_EQ(cc_tevent_validate(&e), CC_E_RANGE);
    e = good(); e.fx.luck_boost = 0; e.fx.shiny_catch_mult = CC_TEVENT_MULT_MAX;          CHECK_EQ(cc_tevent_validate(&e), CC_OK);
    e = good(); e.fx.luck_boost = 0; e.fx.shiny_catch_mult = CC_TEVENT_MULT_MAX + 1;      CHECK_EQ(cc_tevent_validate(&e), CC_E_RANGE);
    e = good(); e.fx.luck_boost = 0; e.fx.candy_mult = CC_TEVENT_MULT_MAX;                CHECK_EQ(cc_tevent_validate(&e), CC_OK);
    e = good(); e.fx.luck_boost = 0; e.fx.candy_mult = CC_TEVENT_MULT_MAX + 1;            CHECK_EQ(cc_tevent_validate(&e), CC_E_RANGE);
    e = good(); e.fx.luck_boost = CC_TEVENT_LUCK_MAX;                                     CHECK_EQ(cc_tevent_validate(&e), CC_OK);
    e = good(); e.fx.luck_boost = CC_TEVENT_LUCK_MAX + 1;                                 CHECK_EQ(cc_tevent_validate(&e), CC_E_RANGE);
    e = good(); e.fx.luck_boost = 0; e.fx.trainer_shiny_chance = 65535;                   CHECK_EQ(cc_tevent_validate(&e), CC_OK);
    e = good(); e.fx.luck_boost = 0; e.fx.upgrade_vouchers = 1;                           CHECK_EQ(cc_tevent_validate(&e), CC_OK);
    e = good(); e.fx.luck_boost = 0; e.fx.upgrade_vouchers = 2;                           CHECK_EQ(cc_tevent_validate(&e), CC_E_RANGE);
    e = good(); e.fx.luck_boost = 0;                                                      CHECK_EQ(cc_tevent_validate(&e), CC_E_CONSISTENCY);   /* changes nothing */

    CHECK_EQ(cc_tevent_schedule_validate(NULL), CC_E_NULL);
    CHECK_EQ(cc_tevent_effects_validate(NULL), CC_E_NULL);

    /* one id that is the start of another is not a duplicate, in either order */
    {
        cc_tevent pre[2] = {good(), good()};
        pre[0].id = "gold"; pre[1].id = "golden";
        CHECK_EQ(cc_tevents_validate_in(pre, 2), CC_OK);
        pre[0].id = "golden"; pre[1].id = "gold";
        CHECK_EQ(cc_tevents_validate_in(pre, 2), CC_OK);
        pre[1].id = "golden";
        CHECK_EQ(cc_tevents_validate_in(pre, 2), CC_E_DUPLICATE);
    }

    /* a table */
    cc_tevent t[CC_TEVENTS_MAX + 1];
    for (unsigned i = 0; i < CC_TEVENTS_MAX + 1; i++) {
        static const char *ids[] = {"a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7", "a8", "a9", "b0", "b1", "b2", "b3", "b4", "b5", "b6"};
        t[i] = good();
        t[i].id = ids[i];
    }
    CHECK_EQ(cc_tevents_validate_in(t, 0), CC_OK);
    CHECK_EQ(cc_tevents_validate_in(t, CC_TEVENTS_MAX), CC_OK);
    CHECK_EQ(cc_tevents_validate_in(t, CC_TEVENTS_MAX + 1), CC_E_COUNT);
    CHECK_EQ(cc_tevents_validate_in(NULL, 0), CC_E_NULL);
    t[5].id = "a2";
    CHECK_EQ(cc_tevents_validate_in(t, 5), CC_OK);
    CHECK_EQ(cc_tevents_validate_in(t, 6), CC_E_DUPLICATE);
    t[5].id = "a5";
    t[3].fx.luck_boost = 0;
    CHECK_EQ(cc_tevents_validate_in(t, 6), CC_E_CONSISTENCY);
}

/* ------------------------------------------------------------------ overlap and combination */

static void test_combine(void)
{
    /* a: daily all day (shiny x2, catch x3), b: the same days, one hour (shiny x5, luck 2, vouchers), c: every other day (candy 4, trainer 100) */
    const cc_tevent t[] = {
        {"a", "A", {1, 0, 0, 1440}, {2, 3, 0, 0, 0, 0}},
        {"b", "B", {1, 0, 600, 60}, {5, 0, 0, 2, 0, 1}},
        {"c", "C", {2, 1, 0, 1440}, {0, 0, 4, 1, 100, 0}},
        {"d", "D", {2, 1, 0, 1440}, {0, 1, 0, 0, 200, 0}},
        {"e", "E", {2, 1, 0, 1440}, {0, 0, 2, 0, 0, 1}},     /* candy and vouchers again: b and e both upgrade vouchers, c and e both set candy */
    };
    const unsigned n = 5;
    CHECK_EQ(cc_tevents_validate_in(t, n), CC_OK);

    /* 2026-11-16 (odd day, so c, d and e are on) at 10:30: everything is open */
    const int64_t all = T(2026, 11, 16, 10, 30, 0);
    CHECK_EQ(cc_tevents_active_in(t, n, all), 0x1Fu);
    cc_tevent_effects f = cc_tevents_effects_in(t, n, all);
    CHECK_EQ(f.shiny_encounter_mult, 5);   /* the strongest wins: 5, not 2 * 5 */
    CHECK_EQ(f.shiny_catch_mult, 3);       /* the strongest of 3 and 1 */
    CHECK_EQ(f.candy_mult, 4);             /* the strongest of 4 and 2 */
    CHECK_EQ(f.luck_boost, 2);             /* the maximum of 2 and 1: no sum */
    CHECK_EQ(f.trainer_shiny_chance, 200);
    CHECK_EQ(f.upgrade_vouchers, 1);       /* two events upgrade them: still 1 */

    /* the same day at 09:59:59 and 11:00:00: b is closed */
    CHECK_EQ(cc_tevents_active_in(t, n, T(2026, 11, 16, 9, 59, 59)), 0x1Du);
    CHECK_EQ(cc_tevents_active_in(t, n, T(2026, 11, 16, 11, 0, 0)), 0x1Du);
    f = cc_tevents_effects_in(t, n, T(2026, 11, 16, 11, 0, 0));
    CHECK_EQ(f.shiny_encounter_mult, 2);
    CHECK_EQ(f.candy_mult, 4);
    CHECK_EQ(f.luck_boost, 1);
    CHECK_EQ(f.upgrade_vouchers, 1);       /* e alone keeps it on */

    /* the next day (even): c and d are off, b is on at 10:00 */
    CHECK_EQ(cc_tevents_active_in(t, n, T(2026, 11, 17, 10, 0, 0)), 0x3u);
    f = cc_tevents_effects_in(t, n, T(2026, 11, 17, 12, 0, 0));
    CHECK_EQ(f.shiny_encounter_mult, 2);
    CHECK_EQ(f.shiny_catch_mult, 3);
    CHECK_EQ(f.candy_mult, 0);
    CHECK_EQ(f.luck_boost, 0);
    CHECK_EQ(f.trainer_shiny_chance, 0);
    CHECK_EQ(f.upgrade_vouchers, 0);

    /* the table of one event that is closed gives nothing */
    CHECK_EQ(cc_tevents_active_in(t + 1, 1, T(2026, 11, 16, 9, 0, 0)), 0u);
    f = cc_tevents_effects_in(t + 1, 1, T(2026, 11, 16, 9, 0, 0));
    CHECK(f.shiny_encounter_mult == 0 && f.shiny_catch_mult == 0 && f.candy_mult == 0 && f.luck_boost == 0 && f.trainer_shiny_chance == 0 &&
          f.upgrade_vouchers == 0);
    CHECK_EQ(cc_tevents_active_in(t, 0, all), 0u);
}

static void test_refusals(void)
{
    const cc_tevent ok[] = {{"a", "A", {1, 0, 0, 1440}, {2, 0, 0, 0, 0, 0}}};
    const cc_tevent bad[] = {{"a", "A", {1, 0, 0, 1440}, {2, 0, 0, 0, 0, 0}}, {"b", "B", {0, 0, 0, 1440}, {2, 0, 0, 0, 0, 0}}};
    const cc_tevent dup[] = {{"a", "A", {1, 0, 0, 1440}, {2, 0, 0, 0, 0, 0}}, {"a", "B", {1, 0, 0, 1440}, {3, 0, 0, 0, 0, 0}}};
    const int64_t now = T(2026, 11, 16, 12, 0, 0);
    CHECK_EQ(cc_tevents_active_in(ok, 1, now), 1u);
    CHECK_EQ(cc_tevents_effects_in(ok, 1, now).shiny_encounter_mult, 2);
    CHECK_EQ(cc_tevents_active_in(NULL, 1, now), 0u);
    CHECK_EQ(cc_tevents_effects_in(NULL, 1, now).shiny_encounter_mult, 0);
    CHECK_EQ(cc_tevents_active_in(bad, 2, now), 0u);          /* one invalid event: the whole table is refused, not half applied */
    CHECK_EQ(cc_tevents_effects_in(bad, 2, now).shiny_encounter_mult, 0);
    CHECK_EQ(cc_tevents_active_in(dup, 2, now), 0u);
    CHECK_EQ(cc_tevents_effects_in(dup, 2, now).shiny_encounter_mult, 0);
    CHECK_EQ(cc_tevents_active_in(ok, CC_TEVENTS_MAX + 1, now), 0u);
    CHECK_EQ(cc_tevents_active_in(ok, 1, CC_TEVENT_TIME_MAX + 1), 0u);
    CHECK_EQ(cc_tevents_effects_in(ok, 1, CC_TEVENT_TIME_MAX + 1).shiny_encounter_mult, 0);
    CHECK_EQ(cc_tevents_effects_in(ok, 1, -CC_TEVENT_TIME_MAX - 1).shiny_encounter_mult, 0);
}

/* ------------------------------------------------------------------ the oracle against random schedules */

static uint64_t rng_state = 0x9E3779B97F4A7C15ull;
static uint64_t rng(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return rng_state;
}

static void test_random_schedules(void)
{
    for (int i = 0; i < 4000; i++) {
        cc_tevent_schedule s;
        s.period_days = (uint16_t)(1 + rng() % CC_TEVENT_PERIOD_MAX);
        s.phase_day = (uint16_t)(rng() % s.period_days);
        s.start_minute = (uint16_t)(rng() % 1440);
        switch (rng() % 4) {
        case 0: s.length_minutes = (uint16_t)(1 + rng() % 120); break;
        case 1: s.length_minutes = (uint16_t)(1 + rng() % 1440); break;
        case 2: s.length_minutes = (uint16_t)(s.period_days * 1440u); break;      /* the longest allowed */
        default: s.length_minutes = (uint16_t)(1 + rng() % (s.period_days * 1440u)); break;
        }
        CHECK_EQ(cc_tevent_schedule_validate(&s), CC_OK);
        for (int k = 0; k < 6; k++) {
            int64_t t;
            switch (rng() % 3) {
            case 0: t = (int64_t)(rng() % 4000000000ull) - 1000000000ll; break;                       /* 1938 to 2096 */
            case 1: t = T(2026, 11, 16, 0, 0, 0) + (int64_t)(rng() % (60 * DAY)); break;
            default: t = (int64_t)(rng() % 90000000ull) - 45000000ll; break;                          /* around 1970 */
            }
            int active = -1;
            int64_t b = 0, e = 0;
            CHECK_EQ(cc_tevent_window(&s, t, &active, &b, &e), CC_OK);
            CHECK_EQ(active, oracle_open(&s, t));
            CHECK_EQ(e - b, (int64_t)s.length_minutes * 60);
            CHECK_EQ(fl_mod(b - (int64_t)s.start_minute * 60, DAY), 0);                                   /* it opens at its minute of the day */
            CHECK_EQ(fl_mod(fl_div(b - (int64_t)s.start_minute * 60, DAY), s.period_days), s.phase_day);          /* on a day of its phase */
            if (active) {
                CHECK(b <= t && t < e);
            } else {
                CHECK(b > t);
                CHECK_EQ(b, oracle_next(&s, t));
            }
        }
    }
}

/* ------------------------------------------------------------------ the shipped draft table */

static void test_shipped_table(void)
{
    CHECK_EQ(cc_tevents_validate(), CC_OK);
    CHECK_EQ(cc_tevent_count, 6u);
    CHECK(cc_tevent_count <= CC_TEVENTS_MAX);
    static const char *const ids[] = {"lucky_catch", "sweet_tooth", "shiny_day", "voucher_plus_day", "shiny_trainers", "golden_hour"};
    for (unsigned i = 0; i < 6; i++) CHECK(strcmp(cc_tevents[i].id, ids[i]) == 0);

    /* hand-checked moments */
    cc_tevent_effects f = cc_tevents_effects(T(2026, 11, 16, 12, 0, 0));   /* day 20773, odd: Voucher Plus Day, not Shiny Day; between the two hours */
    CHECK_EQ(cc_tevents_active(T(2026, 11, 16, 12, 0, 0)), 0x1u | 0x2u | 0x8u);
    CHECK(f.shiny_encounter_mult == 0 && f.shiny_catch_mult == 3 && f.candy_mult == 4 && f.luck_boost == 0 && f.trainer_shiny_chance == 0 &&
          f.upgrade_vouchers == 1);
    f = cc_tevents_effects(T(2026, 11, 16, 20, 30, 0));                      /* Golden Hour */
    CHECK(f.shiny_encounter_mult == 0 && f.shiny_catch_mult == 3 && f.candy_mult == 4 && f.luck_boost == 1 && f.trainer_shiny_chance == 0 &&
          f.upgrade_vouchers == 1);
    f = cc_tevents_effects(T(2026, 11, 17, 12, 0, 0));                       /* day 20774: neither a Shiny Day (20774 mod 3 = 2) nor an odd day */
    CHECK_EQ(cc_tevents_active(T(2026, 11, 17, 12, 0, 0)), 0x1u | 0x2u);
    CHECK(f.shiny_encounter_mult == 0 && f.shiny_catch_mult == 3 && f.candy_mult == 4 && f.luck_boost == 0 && f.trainer_shiny_chance == 0 &&
          f.upgrade_vouchers == 0);
    f = cc_tevents_effects(T(2026, 11, 18, 13, 30, 0));                      /* day 20775: a Shiny Day (mod 3 = 0) and an odd day, at Shiny Trainers' hour */
    CHECK(f.shiny_encounter_mult == 2 && f.shiny_catch_mult == 3 && f.candy_mult == 4 && f.luck_boost == 0 && f.trainer_shiny_chance == 6554 &&
          f.upgrade_vouchers == 1);
    CHECK_EQ(cc_tevents_active(T(2026, 11, 18, 13, 30, 0)), 0x1u | 0x2u | 0x4u | 0x8u | 0x10u);
    CHECK_EQ(cc_tevents_active(T(2026, 11, 18, 12, 59, 59)), 0x1u | 0x2u | 0x4u | 0x8u);
    CHECK_EQ(cc_tevents_active(T(2026, 11, 18, 14, 0, 0)), 0x1u | 0x2u | 0x4u | 0x8u);

    /* every minute of twelve days against the table read as plain rules */
    const int64_t d0 = T(2026, 11, 16, 0, 0, 0);
    for (int64_t t = d0; t < d0 + 12 * DAY; t += 60) {
        const int64_t day = fl_div(t, DAY), minute = fl_mod(t, DAY) / 60;
        uint32_t want = 0x3u;                                    /* lucky catch and sweet tooth: every minute */
        if (fl_mod(day, 3) == 0) want |= 0x4u;                   /* shiny day: every third day */
        if (fl_mod(day, 2) == 1) want |= 0x8u;                   /* voucher plus day: every second day */
        if (minute >= 780 && minute < 840) want |= 0x10u;        /* 13:00 to 14:00 */
        if (minute >= 1200 && minute < 1260) want |= 0x20u;      /* 20:00 to 21:00 */
        CHECK_EQ(cc_tevents_active(t), want);
        f = cc_tevents_effects(t);
        CHECK_EQ(f.shiny_encounter_mult, (want & 0x4u) ? 2 : 0);
        CHECK_EQ(f.shiny_catch_mult, 3);
        CHECK_EQ(f.candy_mult, 4);
        CHECK_EQ(f.luck_boost, (want & 0x20u) ? 1 : 0);
        CHECK_EQ(f.trainer_shiny_chance, (want & 0x10u) ? 6554 : 0);
        CHECK_EQ(f.upgrade_vouchers, (want & 0x8u) ? 1 : 0);
    }

    /* what the title screen shows: Golden Hour from 19:00 */
    int active = -1;
    int64_t b = 0, e = 0;
    CHECK_EQ(cc_tevent_window(&cc_tevents[5].when, T(2026, 11, 16, 19, 0, 0), &active, &b, &e), CC_OK);
    CHECK_EQ(active, 0);
    CHECK_EQ(b - T(2026, 11, 16, 19, 0, 0), 3600);   /* opens in one hour */
    CHECK_EQ(e - b, 3600);
}

int main(void)
{
    test_calendar();
    test_hourly();
    test_daily_all_day();
    test_every_two_days();
    test_midnight_and_full_period();
    test_negative_time();
    test_window_errors();
    test_validate();
    test_combine();
    test_refusals();
    test_random_schedules();
    test_shipped_table();
    return cc_summary("tevents_test");
}
