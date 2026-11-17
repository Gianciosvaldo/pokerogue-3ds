/* The title screen's events lines (source/events_view.c) on the shipped table (custom-core's tevents_table.c), at hand-picked instants.
 * Expectations are written from the table's design (which events are open on which day and hour), and the open set is cross-checked
 * against the module's own cc_tevents_active_in on random instants.
 * make events_view_test && build/events_view_test */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "events_view.h"

static int checks;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "events_view FAIL line %d: %s\n", __LINE__, #x); exit(1); } checks++; } while (0)
#define CHECK_EQ(a, b) do { long long va_ = (long long)(a), vb_ = (long long)(b); if (va_ != vb_) { \
    fprintf(stderr, "events_view FAIL line %d: %s == %s (%lld vs %lld)\n", __LINE__, #a, #b, va_, vb_); exit(1); } checks++; } while (0)
#define CHECK_STR(a, b) do { if (strcmp((a), (b))) { \
    fprintf(stderr, "events_view FAIL line %d: %s is \"%s\", wanted \"%s\"\n", __LINE__, #a, (a), (b)); exit(1); } checks++; } while (0)

/* local seconds of a day number (days since 1970-01-01) at h:m. 2026-11-16 is day 20773. */
static int64_t T(int64_t day, int h, int m) { return day * 86400 + h * 3600 + m * 60; }

static ev_view V;

static const ev_row *find(const ev_row *rows, int n, const char *name) {
    for (int i = 0; i < n; i++) if (!strcmp(rows[i].name, name)) return &rows[i];
    return NULL;
}

int main(void) {
    char t[96], s[96];

    /* durations */
    static const struct { int64_t secs; const char *text; } LEFT[] = {
        {1, "1 min"}, {59, "1 min"}, {60, "1 min"}, {61, "2 min"}, {1800, "30 min"}, {3540, "59 min"}, {3541, "1 h"}, {3600, "1 h"},
        {3601, "1 h 1 min"}, {12600, "3 h 30 min"}, {17400, "4 h 50 min"}, {43200, "12 h"}, {86400, "24 h"}, {0, "0 min"}, {-5, "0 min"},
    };
    for (unsigned i = 0; i < sizeof LEFT / sizeof LEFT[0]; i++) { ev_left(LEFT[i].secs, t, sizeof t); CHECK_STR(t, LEFT[i].text); }

    /* day 20773 (an odd day; 20773 % 3 == 1) at 12:00 */
    ev_build(T(20773, 12, 0), &V);
    CHECK(V.ok);
    /* open: the daily ones, Voucher Plus Day (odd days) and Delibirdy Day (day % 3 == 1); in the table's order, all closing at midnight */
    CHECK_EQ(V.n_on, 6);
    static const char *const ON[6] = {"Lucky Catch", "Sweet Tooth", "Voucher Plus Day", "Delibirdy Day", "Waiting Room", "Snowy Town"};
    for (int i = 0; i < 6; i++) { CHECK_STR(V.on[i].name, ON[i]); CHECK_EQ(V.on[i].secs, 43200); }
    CHECK(!find(V.on, V.n_on, "Shiny Day"));     /* day % 3 == 0 only */
    CHECK(!find(V.on, V.n_on, "Fusion Day"));    /* day % 3 == 2 only */
    CHECK(!find(V.on, V.n_on, "Candy Jar Day")); /* even days only */
    /* coming within a day: the four hours today, then the two that open at midnight (even day 20774, 20774 % 3 == 2) */
    CHECK_EQ(V.n_next, 6);
    static const struct { const char *name; int64_t secs; int day, h, m; } NEXT[6] = {
        {"Shiny Trainers", 3600, 0, 13, 0}, {"Lucky Species Hour", 14400, 0, 16, 0}, {"Charm Hour", 21600, 0, 18, 0},
        {"Golden Hour", 28800, 0, 20, 0}, {"Candy Jar Day", 43200, 1, 0, 0}, {"Fusion Day", 43200, 1, 0, 0},
    };
    for (int i = 0; i < 6; i++) {
        CHECK_STR(V.next[i].name, NEXT[i].name);
        CHECK_EQ(V.next[i].secs, NEXT[i].secs);
        CHECK_EQ(V.next[i].day_offset, NEXT[i].day);
        CHECK_EQ(V.next[i].start_hour, NEXT[i].h);
        CHECK_EQ(V.next[i].start_minute, NEXT[i].m);
    }
    ev_when(&V.next[0], t, sizeof t); CHECK_STR(t, "today 13:00");
    ev_when(&V.next[4], t, sizeof t); CHECK_STR(t, "tomorrow 00:00");
    /* Shiny Day (day 20775) is two days off: not listed */
    CHECK(!find(V.next, V.n_next, "Shiny Day"));
    ev_headline(&V, t, sizeof t, s, sizeof s);
    CHECK_STR(t, "Lucky Catch is on");
    CHECK_STR(s, "Shiny catch rate x3. 12 h left.");

    /* 13:30: Shiny Trainers is on with 30 minutes left and closes first */
    ev_build(T(20773, 13, 30), &V);
    CHECK_EQ(V.n_on, 7);
    CHECK_STR(V.on[0].name, "Shiny Trainers");
    CHECK_EQ(V.on[0].secs, 1800);
    ev_headline(&V, t, sizeof t, s, sizeof s);
    CHECK_STR(t, "Shiny Trainers is on");
    CHECK_STR(s, "Trainer shiny chance 1 in 10. 30 min left.");
    /* the minute it opens, and the second it closes */
    ev_build(T(20773, 13, 0), &V);
    CHECK_STR(V.on[0].name, "Shiny Trainers");
    CHECK_EQ(V.on[0].secs, 3600);
    ev_build(T(20773, 14, 0), &V);
    CHECK(!find(V.on, V.n_on, "Shiny Trainers"));
    CHECK(find(V.next, V.n_next, "Shiny Trainers") != NULL);   /* its next opening is tomorrow 13:00, 23 h away: listed */

    /* exactly a day before an opening is still "coming": day 20773 at 00:00 is 86400 s before Candy Jar Day and Fusion Day open (day 20774) */
    ev_build(T(20773, 0, 0), &V);
    CHECK(find(V.next, V.n_next, "Candy Jar Day") != NULL);
    CHECK(find(V.next, V.n_next, "Fusion Day") != NULL);
    CHECK_EQ(find(V.next, V.n_next, "Fusion Day")->secs, 86400);
    ev_left(86400, t, sizeof t); CHECK_STR(t, "24 h");
    ev_build(T(20773, 0, 1), &V);   /* a minute later it is a minute under a day */
    CHECK_EQ(find(V.next, V.n_next, "Fusion Day")->secs, 86340);

    /* the headline when nothing is on (no shipped instant has that: the daily ones are always open), and without a clock */
    memset(&V, 0, sizeof V);
    V.ok = true;
    V.n_next = 1;
    snprintf(V.next[0].name, sizeof V.next[0].name, "Golden Hour");
    snprintf(V.next[0].blurb, sizeof V.next[0].blurb, "Party luck +1");
    V.next[0].secs = 17400; V.next[0].day_offset = 0; V.next[0].start_hour = 20;
    ev_headline(&V, t, sizeof t, s, sizeof s);
    CHECK_STR(t, "Next: Golden Hour today 20:00");
    CHECK_STR(s, "In 4 h 50 min. Party luck +1.");
    V.n_next = 0;
    ev_headline(&V, t, sizeof t, s, sizeof s);
    CHECK_STR(t, "No event is on");
    V.ok = false;
    ev_headline(&V, t, sizeof t, s, sizeof s);
    CHECK_STR(t, "No event today");
    CHECK_STR(s, "The console clock cannot be read.");
    ev_build(-1, &V);
    CHECK(!V.ok && V.n_on == 0 && V.n_next == 0);
    ev_build(CC_TEVENT_TIME_MAX + 1, &V);
    CHECK(!V.ok && V.n_on == 0 && V.n_next == 0);
    ev_build(0, &V);
    CHECK(V.ok);   /* 1970-01-01 00:00 is a time */

    /* what every event says: never the fallback, short enough for the panel, numbers from the table */
    for (unsigned i = 0; i < cc_tevent_count; i++) {
        ev_blurb(&cc_tevents[i], t, sizeof t);
        CHECK(strcmp(t, EV_BLURB_NONE) != 0);
        CHECK(strlen(t) <= 34);
        CHECK(strlen(cc_tevents[i].name) <= 20);
    }
    static const struct { const char *id, *text; } SAYS[] = {
        {"lucky_catch", "Shiny catch rate x3"}, {"sweet_tooth", "Classic candy rate x4"}, {"shiny_day", "Wild shiny odds x2"},
        {"voucher_plus_day", "Bosses give the upgraded voucher"}, {"shiny_trainers", "Trainer shiny chance 1 in 10"}, {"golden_hour", "Party luck +1"},
        {"charm_hour", "Charms after waves 8 and 25"}, {"lucky_species_hour", "Eevee line: luck +1 each"}, {"candy_jar_day", "Candy Jar after wave 25"},
        {"delibirdy_day", "Delibird-y is common, +1 charm"}, {"fusion_day", "Fusions feed candy faster"}, {"waiting_room", "Mega + Dynamax items at wave 115"},
        {"snowy_town", "The Town is always snowing"},
    };
    for (unsigned k = 0; k < sizeof SAYS / sizeof SAYS[0]; k++) {
        const cc_tevent *e = NULL;
        for (unsigned i = 0; i < cc_tevent_count; i++) if (!strcmp(cc_tevents[i].id, SAYS[k].id)) e = &cc_tevents[i];
        CHECK(e != NULL);
        ev_blurb(e, t, sizeof t);
        CHECK_STR(t, SAYS[k].text);
    }
    /* a numeric event with two effects says both, and an unknown event with none says the fallback */
    cc_tevent two = {"x", "X", {1, 0, 0, 1440}, {.luck_boost = 2, .candy_mult = 5}};
    ev_blurb(&two, t, sizeof t); CHECK_STR(t, "Classic candy rate x5; Party luck +2");
    cc_tevent none = {"unknown_new_event", "New", {1, 0, 0, 1440}, {.flags = CC_TFX_TOWN_SNOW}};
    ev_blurb(&none, t, sizeof t); CHECK_STR(t, EV_BLURB_NONE);
    ev_blurb(NULL, t, sizeof t); CHECK_STR(t, EV_BLURB_NONE);

    /* pages of the 12-row view at 198 px (the panel under its header): 6 open rows and the first coming one, then the five others */
    ev_build(T(20773, 12, 0), &V);
    int start[EV_PAGES_MAX];
    CHECK_EQ(ev_layout(&V, 198, start, EV_PAGES_MAX), 2);
    CHECK_EQ(start[0], 0);
    CHECK_EQ(start[1], 7);
    bool on = false;
    CHECK_STR(ev_item(&V, 0, &on)->name, "Lucky Catch"); CHECK(on);
    CHECK_STR(ev_item(&V, 6, &on)->name, "Shiny Trainers"); CHECK(!on);
    CHECK_STR(ev_item(&V, 11, &on)->name, "Fusion Day"); CHECK(!on);
    CHECK(ev_item(&V, 12, &on) == NULL);
    CHECK(ev_item(&V, -1, &on) == NULL);
    /* the page invariants for any space: every row once, in order, a page is never empty, and a page that has more than one row fits */
    for (int avail = 20; avail <= 400; avail++) {
        const int pages = ev_layout(&V, avail, start, EV_PAGES_MAX);
        CHECK(pages >= 1);
        CHECK_EQ(start[0], 0);
        for (int p = 0; p < pages; p++) {
            const int end = p + 1 < pages ? start[p + 1] : V.n_on + V.n_next;
            CHECK(end > start[p]);
            int used = 0, group = -1;
            for (int i = start[p]; i < end; i++) {
                const int g = i < V.n_on ? 0 : 1;
                if (g != group) used += EV_LABEL_H;
                group = g;
                used += EV_ROW_H;
            }
            CHECK(end - start[p] == 1 || used <= avail);
            if (p + 1 < pages) {   /* the next row would not have fit: no page is left emptier than it must be */
                const int g = end < V.n_on ? 0 : 1;
                CHECK(used + (g != group ? EV_LABEL_H : 0) + EV_ROW_H > avail);
            }
        }
    }
    CHECK_EQ(ev_layout(&V, 198, start, 1), 1);   /* the cap on pages */
    memset(&V, 0, sizeof V);
    CHECK_EQ(ev_layout(&V, 198, start, EV_PAGES_MAX), 0);   /* not ok */
    V.ok = true;
    CHECK_EQ(ev_layout(&V, 198, start, EV_PAGES_MAX), 0);   /* no rows */

    /* random instants over three years: the open set is the module's own, the lists are in order and inside their limits, and a daily
     * all-day event closes at midnight */
    uint64_t x = 0x9E3779B97F4A7C15ull;
    for (int i = 0; i < 20000; i++) {
        x ^= x << 13; x ^= x >> 7; x ^= x << 17;
        const int64_t now = T(20000, 0, 0) + (int64_t)(x % (3 * 365 * 86400ull));
        ev_build(now, &V);
        CHECK(V.ok);
        const uint32_t mask = cc_tevents_active_in(cc_tevents, cc_tevent_count, now);
        CHECK_EQ(V.n_on, __builtin_popcount(mask));
        for (unsigned k = 0; k < cc_tevent_count; k++)
            CHECK_EQ(find(V.on, V.n_on, cc_tevents[k].name) != NULL, (mask >> k) & 1);
        for (int k = 0; k < V.n_on; k++) {
            CHECK(V.on[k].secs > 0 && V.on[k].secs <= 86400);
            if (k) CHECK(V.on[k - 1].secs <= V.on[k].secs);
        }
        for (int k = 0; k < V.n_next; k++) {
            CHECK(V.next[k].secs > 0 && V.next[k].secs <= EV_COMING_SECONDS);
            CHECK(V.next[k].day_offset == 0 || V.next[k].day_offset == 1);
            CHECK(V.next[k].start_hour >= 0 && V.next[k].start_hour < 24 && V.next[k].start_minute >= 0 && V.next[k].start_minute < 60);
            if (k) CHECK(V.next[k - 1].secs <= V.next[k].secs);
            CHECK(!find(V.on, V.n_on, V.next[k].name));   /* an event is open or coming, not both */
            /* the clock time it opens is now plus the wait */
            const int64_t at = now + V.next[k].secs;
            CHECK_EQ(V.next[k].start_hour, at % 86400 / 3600);
            CHECK_EQ(V.next[k].start_minute, at % 3600 / 60);
            CHECK_EQ(V.next[k].day_offset, at / 86400 - now / 86400);
        }
        const ev_row *lc = find(V.on, V.n_on, "Lucky Catch");
        CHECK(lc != NULL);
        CHECK_EQ(lc->secs, 86400 - now % 86400);
    }

    printf("events_view: %d checks, 0 failures\n", checks);
    return 0;
}
