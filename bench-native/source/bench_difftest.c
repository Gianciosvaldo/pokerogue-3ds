/* battle-core's damage differential test (battle-core/tests/difftest.c), compiled as it is,
 * with each pr_get_attack_damage call it makes timed (one damage calculation each), its printf
 * sent to the log file, exit turned into a clean abort and main renamed. The enemy AI isn't
 * ported yet; upstream's getNextMove runs one simulated damage calculation per attack move and
 * opposing target, so this timing sizes that part of it. battle-core stays read-only. */
#define _POSIX_C_SOURCE 200809L
#include "bench.h"

static uint64_t dmg_t0;
static pr_damage_result bench_damage_done(pr_damage_result r)
{
    uint64_t t1 = bench_ticks();
    bench_record_damage(t1 - dmg_t0);
    return r;
}
#define pr_get_attack_damage(...) (dmg_t0 = bench_ticks(), bench_damage_done(pr_get_attack_damage(__VA_ARGS__)))
#define printf bench_printf
#define exit bench_exit
#define main difftest_main
#include "difftest.c"
