/* battle-core's whole-turn test (battle-core/tests/turntest.c), compiled as it is, with four
 * names redirected: its only pr_run_turn call goes through bench_run_turn, which times it; its
 * printf goes to the log file; exit becomes a clean abort; main becomes turntest_main.
 * battle-core stays read-only: nothing in it is copied or changed. */
#define _POSIX_C_SOURCE 200809L
#include "bench.h"

static void bench_run_turn(pr_battle *bt, pr_engine *e);
#define pr_run_turn bench_run_turn
#define printf bench_printf
#define exit bench_exit
#define main turntest_main
#include "turntest.c"
#undef main
#undef exit
#undef printf
#undef pr_run_turn

static void bench_run_turn(pr_battle *bt, pr_engine *e)
{
    bench_before_turn();
    uint64_t t0 = bench_ticks();
    pr_run_turn(bt, e);
    uint64_t t1 = bench_ticks();
    bench_after_turn();
    bench_record(t1 - t0, bt, e);
}
