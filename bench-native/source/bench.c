/* Native battle-core speed test. Replays whole turns that upstream recorded (the same cases
 * battle-core's turntest checks on the PC) through the C turn engine, times each pr_run_turn,
 * and checks every covered turn still matches upstream line for line on this machine.
 * Builds for the 3DS (devkitARM + libctru) and for the PC (Makefile.host). */
#include "bench.h"
#include <stdarg.h>
#include <string.h>
#include <malloc.h>

#ifdef __3DS__
#include <3ds.h>
#include <sys/stat.h>
u32 __stacksize__ = 1024 * 1024;
#define CASES_DIR "romfs:/cases"
#define RESULT_PATH "sdmc:/3ds/pokerogue-native-bench-result.txt"
#define LOG_PATH "sdmc:/3ds/pokerogue-native-bench-log.txt"
static PrintConsole top_con, bot_con;
#else
#include <time.h>
#define RESULT_PATH "bench-result.txt"
#define LOG_PATH "bench-log.txt"
#endif

#ifndef BENCH_REV
#define BENCH_REV "unknown"
#endif

static const char *const MODES[] = {"plain", "RICH", "STATUS", "ONHIT", "WEATHER", "CALLED", "PROTECT", "ARENA"};
#define N_MODES ((int)(sizeof MODES / sizeof MODES[0]))
#define DAMAGE N_MODES   /* sums[] slot of the damage test */
#define MAX_RECS 16384

typedef struct { float ms; uint8_t mode, covered; uint16_t phases; } rec_t;
typedef struct {
    bool present, got_cases, got_covered;
    int rc, cases, skipped, parse_fail, stat_fail, covered, pass, fail, not_covered;
    double wall_ms;
} mode_sum_t;

static rec_t recs[MAX_RECS];
static int n_recs, cur_mode;
static mode_sum_t sums[N_MODES + 1];
static float dmg_ms[MAX_RECS];
static int n_dmg;
static FILE *log_f;
static bool timing_pass = true;   /* false during the stack pass: nothing is recorded */
static uint64_t main_at, first_turn_at;
static double first_turn_ms = -1, init_ms = -1;
static size_t turn_stack_max;

/* ------------------------------------------------------------------ platform */

uint64_t bench_ticks(void)
{
#ifdef __3DS__
    return svcGetSystemTick();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000u + (uint64_t)ts.tv_nsec;
#endif
}

double bench_ms(uint64_t ticks)
{
#ifdef __3DS__
    return (double)ticks / (SYSCLOCK_ARM11 / 1000.0);
#else
    return (double)ticks / 1e6;
#endif
}

/* Screen and log. On the 3DS, "say" goes to the top screen; the result file is written at the end. */
static const char *kind_name(int m) { return m == DAMAGE ? "damage" : MODES[m]; }

static void say(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void say(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
#ifdef __3DS__
    consoleSelect(&top_con);
#endif
    vprintf(fmt, ap);
    fflush(stdout);
    va_end(ap);
}

static void show_status(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void show_status(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
#ifdef __3DS__
    consoleSelect(&bot_con);
    vprintf(fmt, ap);
    fflush(stdout);
#else
    vfprintf(stderr, fmt, ap);
#endif
    va_end(ap);
}

static void wait_for_start(void)
{
#ifdef __3DS__
    show_status("\nPress START to exit.\n");
    while (aptMainLoop()) {
        hidScanInput();
        if (hidKeysDown() & KEY_START) break;
        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }
#endif
}

static void bench_shutdown(void)
{
    if (log_f) fclose(log_f);
    log_f = NULL;
#ifdef __3DS__
    romfsExit();
    gfxExit();
#endif
}

/* ------------------------------------------------------------------ hooks for turntest.c */

int bench_printf(const char *fmt, ...)
{
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    char head[256];
    vsnprintf(head, sizeof head, fmt, ap2);
    va_end(ap2);
    /* turntest's two summary lines carry the per-file results */
    mode_sum_t *m = &sums[cur_mode];
    if (timing_pass) {
        if (sscanf(head, "cases %d (skipped %d: records not read yet), parse mismatches %d, stats mismatches %d",
                   &m->cases, &m->skipped, &m->parse_fail, &m->stat_fail) == 4)
            m->got_cases = true;
        if (sscanf(head, "cases %d, stats mismatches %d, carried-state mismatches %d", &m->cases, &m->stat_fail,
                   &m->parse_fail) == 3)
            m->got_cases = true;   /* the damage test's first summary line */
        if (sscanf(head, "covered %d: pass %d, fail %d; not covered %d", &m->covered, &m->pass, &m->fail,
                   &m->not_covered) == 4)
            m->got_covered = true;
    }
    int n = log_f ? vfprintf(log_f, fmt, ap) : 0;
    va_end(ap);
    return n;
}

void bench_exit(int code)
{
    if (log_f) fprintf(log_f, "test stopped with exit(%d)\n", code);
    FILE *f = fopen(RESULT_PATH, "w");
    if (f) {
        fprintf(f, "native battle-core bench %s: test stopped with exit(%d) on %s; see the log file\n", BENCH_REV,
                code, kind_name(cur_mode));
        fclose(f);
    }
    show_status("\ntest stopped (exit %d) on %s.\nSee %s\n", code, kind_name(cur_mode), LOG_PATH);
    wait_for_start();
    bench_shutdown();
    exit(code);
}

/* Stack depth of one turn (3DS, stack pass only): paint below the caller, run, find the deepest
 * word written. The paint evicts the data cache, so the timing pass never paints. */
#define PAINT_WORDS (128 * 1024 / 4)
#define PAINT 0x5AA5C33Cu
#ifdef __3DS__
static volatile uint32_t *paint_lo, *paint_ref;
#endif

void bench_before_turn(void)
{
#ifdef __3DS__
    if (timing_pass) return;
    uintptr_t sp;
    __asm__ volatile("mov %0, sp" : "=r"(sp));
    paint_ref = (volatile uint32_t *)(sp & ~(uintptr_t)3);
    paint_lo = paint_ref - 64 - PAINT_WORDS;
    for (volatile uint32_t *p = paint_lo; p < paint_ref - 64; p++) *p = PAINT;
#endif
}

void bench_after_turn(void)
{
#ifdef __3DS__
    if (timing_pass) return;
    volatile uint32_t *p = paint_lo;
    while (p < paint_ref - 64 && *p == PAINT) p++;
    size_t used = (size_t)(paint_ref - p) * 4;
    if (used > turn_stack_max) turn_stack_max = used;
#endif
}

void bench_record_damage(uint64_t ticks)
{
    if (timing_pass && n_dmg < MAX_RECS) dmg_ms[n_dmg++] = (float)bench_ms(ticks);
}

void bench_record(uint64_t ticks, const pr_battle *bt, const pr_engine *e)
{
    if (!timing_pass) return;
    double ms = bench_ms(ticks);
    if (first_turn_ms < 0) {
        first_turn_ms = ms;
        first_turn_at = bench_ticks();
    }
    if (n_recs >= MAX_RECS) return;
    rec_t *r = &recs[n_recs++];
    r->ms = (float)ms;
    r->mode = (uint8_t)cur_mode;
    r->covered = !bt->n_unported && !bt->n_deferred;
    r->phases = (uint16_t)e->n_trace;
}

/* ------------------------------------------------------------------ statistics */

typedef struct { int n; double median, p95, max, mean, phases; } dist_t;

static int cmp_double(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return x < y ? -1 : x > y;
}

/* mode < 0: every mode; covered < 0: every turn. Percentiles by nearest rank. */
static dist_t dist(int mode, int covered)
{
    static double v[MAX_RECS];
    dist_t d = {0};
    double sum = 0, phases = 0;
    for (int i = 0; i < n_recs; i++) {
        if (mode >= 0 && recs[i].mode != mode) continue;
        if (covered >= 0 && recs[i].covered != covered) continue;
        v[d.n++] = recs[i].ms;
        sum += recs[i].ms;
        phases += recs[i].phases;
    }
    if (!d.n) return d;
    qsort(v, d.n, sizeof v[0], cmp_double);
    d.median = v[(d.n - 1) / 2];
    int k95 = (int)((95 * (long)d.n + 99) / 100) - 1;
    d.p95 = v[k95 < 0 ? 0 : k95];
    d.max = v[d.n - 1];
    d.mean = sum / d.n;
    d.phases = phases / d.n;
    return d;
}

static dist_t dist_damage(void)
{
    static double v[MAX_RECS];
    dist_t d = {0};
    double sum = 0;
    for (int i = 0; i < n_dmg; i++) sum += v[d.n++] = dmg_ms[i];
    if (!d.n) return d;
    qsort(v, d.n, sizeof v[0], cmp_double);
    d.median = v[(d.n - 1) / 2];
    int k95 = (int)((95 * (long)d.n + 99) / 100) - 1;
    d.p95 = v[k95 < 0 ? 0 : k95];
    d.max = v[d.n - 1];
    d.mean = sum / d.n;
    return d;
}

/* ------------------------------------------------------------------ main */

static void run_mode(int m, const char *dir)
{
    char path[256];
    snprintf(path, sizeof path, "%s/%s.txt", dir, kind_name(m));
    FILE *probe = fopen(path, "r");
    if (!probe) return;
    fclose(probe);
    cur_mode = m;
    if (timing_pass) sums[m].present = true;
    /* the tests keep their "not covered, by reason" tallies in statics, so those lists add up
     * from one kind to the next; the covered/pass/fail lines are per file */
    if (log_f) fprintf(log_f, "=== %s (%s pass)\n", kind_name(m), timing_pass ? "timing" : "stack");
    char *argv[] = {m == DAMAGE ? "difftest" : "turntest", path, NULL};
    uint64_t t0 = bench_ticks();
    int rc = m == DAMAGE ? difftest_main(2, argv) : turntest_main(2, argv);
    uint64_t t1 = bench_ticks();
    if (timing_pass) {
        sums[m].rc = rc;
        sums[m].wall_ms = bench_ms(t1 - t0);
    }
    if (log_f) fflush(log_f);
}

static void copy_manifest(FILE *out, const char *dir)
{
    char path[256], line[512];
    snprintf(path, sizeof path, "%s/MANIFEST.txt", dir);
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(out, "cases: no MANIFEST.txt\n");
        return;
    }
    while (fgets(line, sizeof line, f)) fprintf(out, "cases: %s", line);
    fclose(f);
}

static void write_report(FILE *o, const char *dir, const char *machine, double total_s, size_t heap_used,
                         size_t heap_arena, const char *mem_line)
{
    fprintf(o, "PokeRogue native battle-core speed test\n");
    fprintf(o, "build: %s, machine: %s\n", BENCH_REV, machine);
    copy_manifest(o, dir);
    fprintf(o, "timed: pr_run_turn only (TurnStartPhase to the end of the turn, both commands given).\n");
    fprintf(o, "not timed: enemy AI, command menus, switching, drawing, reading/checking the test records.\n");
    fprintf(o, "covered = the turn touched nothing unported; same = every record matched upstream here.\n\n");
    fprintf(o, "%-8s %5s %5s %5s | covered turns, ms       | all turns, ms           | phases\n", "kind", "turns",
            "cover", "same");
    fprintf(o, "%-8s %5s %5s %5s | %7s %7s %7s | %7s %7s %7s | %6s\n", "", "", "", "", "median", "p95", "max",
            "median", "p95", "max", "avg");
    int tot_cases = 0, tot_cov = 0, tot_pass = 0, tot_fail = sums[DAMAGE].fail, tot_parse = sums[DAMAGE].parse_fail;
    for (int m = 0; m < N_MODES; m++) {
        mode_sum_t *s = &sums[m];
        if (!s->present) continue;
        dist_t c = dist(m, 1), a = dist(m, -1);
        fprintf(o, "%-8s %5d %5d %5d | %7.3f %7.3f %7.3f | %7.3f %7.3f %7.3f | %6.1f\n", MODES[m], s->cases,
                s->covered, s->pass, c.median, c.p95, c.max, a.median, a.p95, a.max, a.phases);
        if (s->fail || s->parse_fail || s->stat_fail || !s->got_cases || !s->got_covered || s->rc)
            fprintf(o, "  !! %s: fail %d, parse mismatches %d, stats mismatches %d, summary %s, exit %d\n", MODES[m],
                    s->fail, s->parse_fail, s->stat_fail, s->got_cases && s->got_covered ? "read" : "MISSING",
                    s->rc);
        tot_cases += s->cases;
        tot_cov += s->covered;
        tot_pass += s->pass;
        tot_fail += s->fail;
        tot_parse += s->parse_fail;
    }
    dist_t c = dist(-1, 1), a = dist(-1, -1);
    fprintf(o, "%-8s %5d %5d %5d | %7.3f %7.3f %7.3f | %7.3f %7.3f %7.3f | %6.1f\n", "ALL", tot_cases, tot_cov,
            tot_pass, c.median, c.p95, c.max, a.median, a.p95, a.max, a.phases);
    fprintf(o, "mean ms per turn: covered %.3f, all %.3f; turns timed %d (bench counted %d covered)\n", c.mean,
            a.mean, a.n, c.n);
    fprintf(o, "result: %s\n\n",
            tot_fail || tot_parse ? "SOME COVERED CASES DIFFER FROM UPSTREAM ON THIS MACHINE (see log)"
                                  : "every covered turn and damage case matched upstream on this machine");
    mode_sum_t *ds = &sums[DAMAGE];
    if (ds->present) {
        dist_t d = dist_damage();
        fprintf(o, "damage calculations (pr_get_attack_damage, one call each): %d calls from %d cases, %d covered, %d same\n",
                d.n, ds->cases, ds->covered, ds->pass);
        fprintf(o, "  ms per call: median %.4f, p95 %.4f, max %.4f, mean %.4f\n", d.median, d.p95, d.max, d.mean);
        if (ds->fail || ds->parse_fail || ds->stat_fail || !ds->got_cases || !ds->got_covered || ds->rc)
            fprintf(o, "  !! damage: fail %d, carried-state mismatches %d, stats mismatches %d, summary %s, exit %d\n",
                    ds->fail, ds->parse_fail, ds->stat_fail, ds->got_cases && ds->got_covered ? "read" : "MISSING",
                    ds->rc);
        fprintf(o, "\n");
    }
    fprintf(o, "startup: closure registry %.1f ms (the only load step; the data tables are compiled in)\n", init_ms);
    fprintf(o, "first turn after launch: %.3f ms; launch to first turn: %.0f ms\n", first_turn_ms,
            bench_ms(first_turn_at - main_at));
    fprintf(o, "reading + checking records (not timed above), per case:");
    for (int m = 0; m <= DAMAGE; m++)
        if (sums[m].present && sums[m].cases)
            fprintf(o, " %s %.1f ms", kind_name(m), sums[m].wall_ms / sums[m].cases);
    fprintf(o, "\n");
    fprintf(o, "memory: pr_battle %u B, pr_engine %u B, pr_pokemon %u B\n", (unsigned)sizeof(pr_battle),
            (unsigned)sizeof(pr_engine), (unsigned)sizeof(pr_pokemon));
    if (turn_stack_max)
        fprintf(o, "memory: deepest stack in one turn %u B (probe paints %u KB)\n", (unsigned)turn_stack_max,
                (unsigned)(PAINT_WORDS * 4 / 1024));
    fprintf(o, "memory: malloc in use %u B, arena %u B\n", (unsigned)heap_used, (unsigned)heap_arena);
    if (mem_line) fprintf(o, "memory: %s\n", mem_line);
    fprintf(o, "total run: %.1f s\n", total_s);
}

int main(int argc, char **argv)
{
    main_at = bench_ticks();
    const char *dir;
    char machine[96];
#ifdef __3DS__
    gfxInitDefault();
    consoleInit(GFX_TOP, &top_con);
    consoleInit(GFX_BOTTOM, &bot_con);
    bool n3ds = false;
    APT_CheckNew3DS(&n3ds);
    /* the speedup stays off: a New 3DS runs at 268 MHz here, like an Old one (L2 cache aside) */
    snprintf(machine, sizeof machine, "%s, ARM11 268 MHz", n3ds ? "New 3DS (speedup off)" : "Old 3DS");
    Result rr = romfsInit();
    dir = CASES_DIR;
    show_status("PokeRogue native battle speed test\n%s\n\n", BENCH_REV);
    if (R_FAILED(rr)) {
        show_status("romfs failed (0x%08lx).\n", (unsigned long)rr);
        wait_for_start();
        bench_shutdown();
        return 1;
    }
    (void)argc;
    (void)argv;
#else
    dir = argc > 1 ? argv[1] : "romfs/cases";
    snprintf(machine, sizeof machine, "PC (host build)");
#endif
#ifdef __3DS__
    mkdir("sdmc:/3ds", 0777);   /* exists on any console with the Homebrew Launcher; not in a fresh emulator */
#endif
    log_f = fopen(LOG_PATH, "w");

    /* the one-time setup the first battle would pay: linking the hand-ported closures */
    uint64_t ti = bench_ticks();
    pr_closures_init();
    init_ms = bench_ms(bench_ticks() - ti);

    show_status("Timing %d kinds of turn and\nthe damage calculation.\nDon't press HOME until it says Done.\n\n",
                N_MODES);
    say("%-8s %5s %5s | median   p95    max (ms)\n", "kind", "cover", "same");
    for (int m = 0; m < N_MODES; m++) {
        show_status("  %s...\n", MODES[m]);
        run_mode(m, dir);
        if (!sums[m].present) {
            say("%-8s missing\n", MODES[m]);
            continue;
        }
        dist_t c = dist(m, 1);
        say("%-8s %5d %5d | %6.2f %6.2f %6.2f\n", MODES[m], sums[m].covered, sums[m].pass, c.median, c.p95, c.max);
    }
    dist_t all = dist(-1, 1);
    say("%-8s %5d       | %6.2f %6.2f %6.2f\n", "ALL", all.n, all.median, all.p95, all.max);
    show_status("  damage calculations...\n");
    run_mode(DAMAGE, dir);
    if (sums[DAMAGE].present) {
        dist_t d = dist_damage();
        say("\none damage calculation (%d calls), ms:\n%-20s | %6.3f %6.3f %6.3f\n", d.n, "", d.median, d.p95, d.max);
    }

#ifdef __3DS__
    /* second pass, not timed: the deepest stack one turn needs */
    show_status("\nStack pass (not timed)...\n");
    timing_pass = false;
    for (int m = 0; m < N_MODES; m++)
        if (sums[m].present) run_mode(m, dir);
    timing_pass = true;
#endif

    double total_s = bench_ms(bench_ticks() - main_at) / 1000.0;
    struct mallinfo mi = mallinfo();
    char mem_line[160] = "";
#ifdef __3DS__
    snprintf(mem_line, sizeof mem_line, "app heap %.1f MB, linear heap %.1f MB (all the app memory this launch got)",
             envGetHeapSize() / 1048576.0, envGetLinearHeapSize() / 1048576.0);
#endif
    FILE *out = fopen(RESULT_PATH, "w");
    if (out) {
        write_report(out, dir, machine, total_s, mi.uordblks, mi.arena, mem_line[0] ? mem_line : NULL);
        fclose(out);
    }
    int bad = 0;
    for (int m = 0; m <= DAMAGE; m++) bad += sums[m].fail + sums[m].parse_fail;
    say(bad ? "\n!! %d covered cases differ from upstream here\n" : "\nAll covered cases match upstream here.\n", bad);
    show_status("\nDone in %.0f s.\n%s %s\n", total_s, out ? "Saved" : "COULD NOT SAVE", RESULT_PATH);
#ifndef __3DS__
    if (out) {
        FILE *f = fopen(RESULT_PATH, "r");
        char line[512];
        while (f && fgets(line, sizeof line, f)) fputs(line, stdout);
        if (f) fclose(f);
    }
#endif
    wait_for_start();
    bench_shutdown();
    return bad ? 1 : 0;
}
