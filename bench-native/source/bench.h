/* Native battle-core speed test: shared declarations between the platform driver (bench.c)
 * and the wrapped whole-turn test (bench_turntest.c). */
#ifndef BENCH_H
#define BENCH_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "pr/engine.h"

uint64_t bench_ticks(void);
double bench_ms(uint64_t ticks);

/* Called right after each timed pr_run_turn. */
void bench_record(uint64_t ticks, const pr_battle *bt, const pr_engine *e);
/* Before and after each pr_run_turn, outside the timed span: stack depth probe (3DS only). */
void bench_before_turn(void);
void bench_after_turn(void);

/* turntest.c's printf and exit, redirected to the log file and to a clean abort. */
int bench_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void bench_exit(int code) __attribute__((noreturn));

/* Called after each timed damage calculation in the damage test. */
void bench_record_damage(uint64_t ticks);

/* battle-core/tests/turntest.c's and difftest.c's main, renamed. */
int turntest_main(int argc, char **argv);
int difftest_main(int argc, char **argv);
#endif
