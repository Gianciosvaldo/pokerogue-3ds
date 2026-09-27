/* Experience curves (src/data/exp.ts). Below level 100 upstream reads its own table, whose results
 * are dumped into gen/out/run_exp.c; from 100 on it uses formulas, ported here. */
#include "pr/run.h"
#include <math.h>

/* getLevelTotalExp(level, growthRate):
 *   if (level < 100) → expLevels (non MEDIUM_FAST: Math.floor(levelExp * 0.325 + MEDIUM_FAST's * 0.675))
 *   ERRATIC (level^4 + level^3 * 2000) / 3500; FAST level^3 * 4 / 5; MEDIUM_FAST level^3;
 *   MEDIUM_SLOW level^3 * 6 / 5 - 15 * level^2 + 100 * level - 140; SLOW level^3 * 5 / 4;
 *   FLUCTUATING level^3 * (level / 2 + 8) * 4 / (100 + level);
 *   non MEDIUM_FAST: Math.floor(ret * 0.325 + getLevelTotalExp(level, MEDIUM_FAST) * 0.675); else Math.floor(ret)
 * A level below 1 reads past the table's start: undefined, NaN once blended; NaN here. */
double pr_run_get_level_total_exp(int level, int rate)
{
    if (rate < 0 || rate >= PR_GROWTH_RATE_COUNT) return NAN;
    if (level < 100) return level >= 1 ? pr_exp_table[rate][level - 1] : NAN;
    double l = level, ret = 0;
    switch (rate) {
    case GROWTH_RATE_ERRATIC: ret = (js_pow(l, 4) + js_pow(l, 3) * 2000) / 3500; break;
    case GROWTH_RATE_FAST: ret = (js_pow(l, 3) * 4) / 5; break;
    case GROWTH_RATE_MEDIUM_FAST: ret = js_pow(l, 3); break;
    case GROWTH_RATE_MEDIUM_SLOW: ret = (js_pow(l, 3) * 6) / 5 - 15 * js_pow(l, 2) + 100 * l - 140; break;
    case GROWTH_RATE_SLOW: ret = (js_pow(l, 3) * 5) / 4; break;
    case GROWTH_RATE_FLUCTUATING: ret = (js_pow(l, 3) * (l / 2 + 8) * 4) / (100 + l); break;
    }
    if (rate != GROWTH_RATE_MEDIUM_FAST)
        return floor(ret * 0.325 + pr_run_get_level_total_exp(level, GROWTH_RATE_MEDIUM_FAST) * 0.675);
    return floor(ret);
}

/* getLevelRelExp(level, growthRate): getLevelTotalExp(level) - getLevelTotalExp(level - 1) */
double pr_run_get_level_rel_exp(int level, int rate)
{
    return pr_run_get_level_total_exp(level, rate) - pr_run_get_level_total_exp(level - 1, rate);
}
