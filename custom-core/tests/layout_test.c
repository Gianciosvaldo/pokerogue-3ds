/* Layout and limit guards for custom-core: the wire-size constants against widths summed here by hand, the storage budgets
 * as compile-time assertions, and the ID ceilings against battle-core's generated enum header (read-only).
 *
 *   layout_test [path/to/pr_enums.h]     default ../battle-core/gen/out/pr_enums.h, relative to custom-core/
 *   layout_test --report                 print sizeof and size bounds, no checks
 *
 * The enum check parses the generated header at run time, so a regenerated enum that no longer matches a CC_*_MAX
 * ceiling fails here instead of silently validating a wrong range. Nothing is skipped when the file is missing. */
#include <stdlib.h>
#include <string.h>

#include "cc/model.h"
#include "cc/reducer.h"
#include "check.h"

/* C99 compile-time assertion: a negative array size is a compile error. */
#define CC_STATIC_ASSERT(name, cond) typedef char cc_assert_##name[(cond) ? 1 : -1]

CC_STATIC_ASSERT(meta_fits_budget, CC_WIRE_META_MAX <= CC_META_BUDGET_BYTES);
/* the persisted state, the run's schedule, the reducer (with its own copy of the last turns) and the caller's largest batch */
CC_STATIC_ASSERT(runtime_fits_budget,
                 sizeof(cc_meta) + sizeof(cc_schedule) + sizeof(cc_reducer) + CC_MAX_BATCH_OBS * sizeof(cc_obs) <=
                     CC_RUNTIME_BUDGET_BYTES);
CC_STATIC_ASSERT(frozen_matches_roster, CC_MAX_FROZEN == CC_MAX_NEMESES);
CC_STATIC_ASSERT(windows_match_table, CC_NUM_WINDOWS == 4);
CC_STATIC_ASSERT(key_fits_wire_item, CC_ITEM_KEY_BYTES == 33 && CC_WIRE_ITEM == CC_ITEM_KEY_BYTES + 4);

static size_t sum(const unsigned *w, size_t n)
{
    size_t s = 0;
    while (n--)
        s += *w++;
    return s;
}

#define SUM(arr) sum(arr, sizeof(arr) / sizeof((arr)[0]))

static void test_wire_widths(void)
{
    /* The field widths are written out again here, independent of wire.h, so a change to either side is caught. */
    static const unsigned actor[] = {4, 2, 1, 1, 1};
    static const unsigned event[] = {4, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 4, 4, 4, 4, 4};
    static const unsigned turn_header[] = {4, 2, 2, 2, 1, 1};
    static const unsigned history_header[] = {1, 1};
    static const unsigned item[] = {33, 2, 2};
    static const unsigned counters[] = {4, 4, 4, 8};
    static const unsigned ledger_entry[] = {8, 8, 4, 1, 1};
    static const unsigned envelope[] = {4, 2, 2, 4};
    /* terminal: run_uid txn_id outcome known biome wave finish_move finish_actor fallen_actor */
    static const unsigned terminal[] = {8, 8, 1, 1, 1, 2, 2, 9, 9};
    /* nemesis, group by group as in wire.h */
    static const unsigned nemesis[] = {4, 1, 1, 2,           /* record_id life traits known */
                                       8, 4, 2, 1,           /* source run, pokemon, wave, biome */
                                       2, 1, 1, 1, 1, 1,     /* species form gender shiny orig_level nature */
                                       2, 2,                 /* ability passive */
                                       6,                    /* ivs */
                                       1, 8,                 /* n_moves, 4 moves x 2 */
                                       1, 8 * 37,            /* n_items, 8 items */
                                       2, 2, 1,              /* finish_move fallen_species fallen_form */
                                       8, 2, 8};             /* sched_run_uid challenge_wave reward_txn */

    CHECK_EQ(SUM(actor), 9);
    CHECK_EQ(CC_WIRE_ACTOR, 9);
    CHECK_EQ(SUM(event), 38);
    CHECK_EQ(CC_WIRE_EVENT, 38);
    CHECK_EQ(SUM(turn_header), 12);
    CHECK_EQ(CC_WIRE_TURN_HEADER, 12);
    CHECK_EQ(SUM(history_header), 2);
    CHECK_EQ(CC_WIRE_HISTORY_HEADER, 2);
    CHECK_EQ(SUM(item), 37);
    CHECK_EQ(CC_WIRE_ITEM, 37);
    CHECK_EQ(SUM(counters), 20);
    CHECK_EQ(CC_WIRE_COUNTERS, 20);
    CHECK_EQ(SUM(ledger_entry), 22);
    CHECK_EQ(CC_WIRE_LEDGER_ENTRY, 22);
    CHECK_EQ(SUM(envelope), 12);
    CHECK_EQ(CC_WIRE_ENVELOPE_HEADER, 12);
    CHECK_EQ(SUM(terminal), 41);
    CHECK_EQ(CC_WIRE_TERMINAL, 41);
    CHECK_EQ(SUM(nemesis), 369);
    CHECK_EQ(CC_WIRE_NEMESIS, 369);

    /* derived maxima, by hand: 12 + 64*38; 2 + 5*2444; 12 + 20 + (1+8*22) + (1+16*369) + 1 + 41 + 12222 */
    CHECK_EQ(CC_WIRE_TURN_MAX, 2444);
    CHECK_EQ(CC_WIRE_HISTORY_MAX, 12222);
    CHECK_EQ(CC_WIRE_META_MAX, 18378);
}

static void test_default_limits(void)
{
    /* the version-1 product defaults from docs/CUSTOM-CONTENT-ADVANCED-AGENT.md */
    CHECK_EQ(CC_SCHEMA_VERSION, 1);
    CHECK_EQ(CC_MAX_NEMESES, 16);
    CHECK_EQ(CC_HISTORY_TURNS, 5);
    CHECK_EQ(CC_MAX_EVENTS_PER_TURN, 64);
    CHECK_EQ(CC_MAX_BATCH_OBS, 384);
    CHECK_EQ(CC_NUM_WINDOWS, 4);
    CHECK_EQ(CC_MAX_MOVES, 4);
    CHECK_EQ(CC_META_BUDGET_BYTES, 65536);
    CHECK_EQ(CC_RUNTIME_BUDGET_BYTES, 131072);
    CHECK_EQ(cc_window_waves[0], 27);
    CHECK_EQ(cc_window_waves[1], 77);
    CHECK_EQ(cc_window_waves[2], 127);
    CHECK_EQ(cc_window_waves[3], 177);
    /* "PRCM" in the file, little-endian: bytes 50 52 43 4D */
    CHECK_EQ(CC_META_MAGIC, 0x4D435250);
}

static void test_size_function_bounds(void)
{
    unsigned full[CC_HISTORY_TURNS];
    unsigned i;
    size_t n = 0;

    for (i = 0; i < CC_HISTORY_TURNS; i++)
        full[i] = CC_MAX_EVENTS_PER_TURN;
    CHECK_STATUS(cc_wire_meta_size_for(CC_MAX_NEMESES, CC_LEDGER_ENTRIES, true, CC_HISTORY_TURNS, full, &n), CC_OK);
    CHECK_EQ(n, CC_WIRE_META_MAX);
    CHECK(n <= CC_META_BUDGET_BYTES);

    n = 123;
    CHECK_STATUS(cc_wire_meta_size_for(0, 0, false, 0, NULL, &n), CC_OK);
    CHECK_EQ(n, 35); /* 12 header + 20 counters + 1 ledger count + 1 record count + 1 has_report */

    /* one over each limit is refused and leaves the output alone */
    n = 123;
    CHECK_STATUS(cc_wire_meta_size_for(CC_MAX_NEMESES + 1, 0, false, 0, NULL, &n), CC_E_COUNT);
    CHECK_STATUS(cc_wire_meta_size_for(0, CC_LEDGER_ENTRIES + 1, false, 0, NULL, &n), CC_E_COUNT);
    CHECK_STATUS(cc_wire_meta_size_for(0, 0, true, CC_HISTORY_TURNS + 1, full, &n), CC_E_COUNT);
    full[0] = CC_MAX_EVENTS_PER_TURN + 1;
    CHECK_STATUS(cc_wire_meta_size_for(0, 0, true, 1, full, &n), CC_E_COUNT);
    CHECK_EQ(n, 123);
}

/* ---- ID ceilings against the generated enums ---- */

/* The largest value of any enumerator whose name starts with `prefix`; *count is how many were found. */
static long long enum_max(const char *text, const char *prefix, int *count)
{
    size_t plen = strlen(prefix);
    long long best = -1;
    const char *p = text;

    *count = 0;
    while (*p) {
        const char *eol = strchr(p, '\n');
        size_t len = eol ? (size_t)(eol - p) : strlen(p);
        const char *s = p;
        const char *end = p + len;

        while (s < end && (*s == ' ' || *s == '\t'))
            s++;
        if ((size_t)(end - s) > plen && strncmp(s, prefix, plen) == 0) {
            const char *eq = s + plen;
            while (eq < end && ((*eq >= 'A' && *eq <= 'Z') || (*eq >= '0' && *eq <= '9') || *eq == '_'))
                eq++;
            while (eq < end && *eq == ' ')
                eq++;
            if (eq < end && *eq == '=') {
                char *num_end;
                long long v = strtoll(eq + 1, &num_end, 10);
                if (num_end != eq + 1) {
                    (*count)++;
                    if (v > best)
                        best = v;
                }
            }
        }
        p += len + (eol ? 1 : 0);
    }
    return best;
}

static char *slurp(const char *path)
{
    FILE *f = fopen(path, "rb");
    char *buf;
    long n;

    if (!f)
        return NULL;
    if (fseek(f, 0, SEEK_END) != 0 || (n = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }
    buf = malloc((size_t)n + 1);
    if (buf && fread(buf, 1, (size_t)n, f) != (size_t)n) {
        free(buf);
        buf = NULL;
    }
    if (buf)
        buf[n] = '\0';
    fclose(f);
    return buf;
}

static void check_ceiling(const char *text, const char *prefix, long long ceiling, int min_count)
{
    int count = 0;
    long long max = enum_max(text, prefix, &count);

    cc_checks++;
    if (count < min_count || max != ceiling) {
        cc_failures++;
        printf("FAIL enum %s*: found %d enumerators, max %lld; limits.h says %lld (expected at least %d enumerators)\n",
               prefix, count, max, ceiling, min_count);
    }
}

static void test_id_ceilings(const char *path)
{
    char *text = slurp(path);

    cc_checks++;
    if (!text) {
        cc_failures++;
        printf("FAIL cannot read %s (run from custom-core/ or pass the path)\n", path);
        return;
    }
    /* min_count guards against a prefix that stops matching anything (a rename would otherwise read as max -1) */
    check_ceiling(text, "SPECIES_ID_", CC_SPECIES_ID_MAX, 1000);
    check_ceiling(text, "MOVE_ID_", CC_MOVE_ID_MAX, 900);
    check_ceiling(text, "ABILITY_ID_", CC_ABILITY_ID_MAX, 300);
    check_ceiling(text, "BIOME_ID_", CC_BIOME_ID_MAX, 30);
    check_ceiling(text, "NATURE_", CC_NATURE_MAX, 25);
    check_ceiling(text, "STATUS_EFFECT_", CC_STATUS_MAX, 8);
    check_ceiling(text, "HIT_RESULT_", CC_HIT_RESULT_MAX, 15);
    check_ceiling(text, "BATTLER_INDEX_", CC_BATTLER_MAX, 5);
    free(text);
}

static void report(void)
{
    printf("sizeof (this build, %u-bit pointers; not an Old 3DS measurement)\n", (unsigned)(sizeof(void *) * 8));
    printf("  cc_obs            %zu\n", sizeof(cc_obs));
    printf("  cc_actor          %zu\n", sizeof(cc_actor));
    printf("  cc_event          %zu\n", sizeof(cc_event));
    printf("  cc_turn           %zu\n", sizeof(cc_turn));
    printf("  cc_history        %zu\n", sizeof(cc_history));
    printf("  cc_terminal       %zu\n", sizeof(cc_terminal));
    printf("  cc_item           %zu\n", sizeof(cc_item));
    printf("  cc_nemesis        %zu\n", sizeof(cc_nemesis));
    printf("  cc_roster         %zu\n", sizeof(cc_roster));
    printf("  cc_schedule       %zu\n", sizeof(cc_schedule));
    printf("  cc_counters       %zu\n", sizeof(cc_counters));
    printf("  cc_ledger         %zu\n", sizeof(cc_ledger));
    printf("  cc_meta           %zu\n", sizeof(cc_meta));
    printf("  cc_reducer        %zu\n", sizeof(cc_reducer));
    printf("  cc_turn_summary   %zu\n", sizeof(cc_turn_summary));
    printf("  batch of %d obs    %zu\n", CC_MAX_BATCH_OBS, CC_MAX_BATCH_OBS * sizeof(cc_obs));
    printf("  meta+schedule+reducer+batch %zu (budget %u)\n",
           sizeof(cc_meta) + sizeof(cc_schedule) + sizeof(cc_reducer) + CC_MAX_BATCH_OBS * sizeof(cc_obs),
           (unsigned)CC_RUNTIME_BUDGET_BYTES);
    printf("encoded size (version 1, bytes)\n");
    printf("  actor %u event %u turn max %u history max %u terminal %u nemesis %u\n", (unsigned)CC_WIRE_ACTOR,
           (unsigned)CC_WIRE_EVENT, (unsigned)CC_WIRE_TURN_MAX, (unsigned)CC_WIRE_HISTORY_MAX,
           (unsigned)CC_WIRE_TERMINAL, (unsigned)CC_WIRE_NEMESIS);
    printf("  metadata min 35, max %u (budget %u)\n", (unsigned)CC_WIRE_META_MAX, (unsigned)CC_META_BUDGET_BYTES);
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--report") == 0) {
        report();
        return 0;
    }
    test_wire_widths();
    test_default_limits();
    test_size_function_bounds();
    test_id_ceilings(argc > 1 ? argv[1] : "../battle-core/gen/out/pr_enums.h");
    return cc_summary("layout_test");
}
