/* Ported move attribute classes (src/data/moves/move.ts). Each apply loop walks the
 * move's attrs in upstream order; a class without a case here is reported unported. */
#include "pr/battle.h"
#include <math.h>

#define ATTR(m, i) (&pr_attrs[pr_moves[m].attr0 + (i)])
#define P(T, a) ((const pr_attr_##T *)(a)->p)

static int count_positive_stat_stages(const pr_pokemon *p)
{
    int n = 0;
    for (int i = 0; i < 7; i++) if (p->stat_stages[i] > 0) n += p->stat_stages[i];
    return n;
}

static double to_dmg_value(double v)
{
    v = floor(v);
    return v > 1 ? v : 1;
}

void pr_apply_variable_power(pr_battle *bt, const pr_pokemon *user, const pr_pokemon *target, int move,
                             double *power)
{
    for (unsigned i = 0; i < pr_moves[move].nattr; i++) {
        const pr_attr *a = ATTR(move, i);
        if (!pr_attr_is(a->cls, PR_AC_VariablePowerAttr)) continue;
        switch (a->cls) {
        case PR_AC_MovePowerMultiplierAttr:
            *power *= pr_closure_call(bt, P(MovePowerMultiplierAttr, a)->powerMultiplierFunc, a->cls, 1,
                                      user, target, move);
            break;
        case PR_AC_WeightPowerAttr: {
            double w = pr_get_weight(bt, target);
            static const double th[5] = {10, 25, 50, 100, 200};
            int k = 0;
            while (w >= th[k]) if (++k == 5) break;
            *power = (k + 1) * 20;
            break;
        }
        case PR_AC_CompareWeightPowerAttr: {
            double uw = pr_get_weight(bt, user), tw = pr_get_weight(bt, target);
            if (!uw) break;
            double rel = (tw / uw) * 100;
            *power = rel < 20.01 ? 120 : rel < 25.01 ? 100 : rel < 33.35 ? 80 : rel < 50.01 ? 60 : 40;
            break;
        }
        case PR_AC_HpPowerAttr:
            *power = to_dmg_value(150 * pr_get_hp_ratio(user, false));
            break;
        case PR_AC_LowHpPowerAttr: {
            double r = pr_get_hp_ratio(user, false);
            *power = r < 0.0417 ? 200 : r < 0.1042 ? 150 : r < 0.2083 ? 100 : r < 0.3542 ? 80 : r < 0.6875 ? 40 : 20;
            break;
        }
        case PR_AC_OpponentHighHpPowerAttr:
            *power = to_dmg_value(P(OpponentHighHpPowerAttr, a)->maxBasePower * pr_get_hp_ratio(target, false));
            break;
        case PR_AC_PositiveStatStagePowerAttr:
            *power += count_positive_stat_stages(user) * 20;
            break;
        case PR_AC_PunishmentPowerAttr: {
            double v = 60 + count_positive_stat_stages(target) * 20;
            *power = v < 200 ? v : 200;
            break;
        }
        case PR_AC_GyroBallPowerAttr: {
            /* user.getEffectiveStat(SPD): no opponent, no move */
            double us = pr_get_effective_stat(bt, user, STAT_SPD, NULL, -1, false, false);
            if (us < 1) { *power = 1; break; }
            double v = (25 * pr_get_effective_stat(bt, target, STAT_SPD, NULL, -1, false, false)) / us + 1;
            *power = floor(v < 150 ? v : 150);
            break;
        }
        case PR_AC_ElectroBallPowerAttr: {
            double ratio = (double)pr_get_effective_stat(bt, target, STAT_SPD, NULL, -1, false, false)
                           / pr_get_effective_stat(bt, user, STAT_SPD, NULL, -1, false, false);
            static const double th[5] = {0.25, 1.0 / 3, 0.5, 1, -1};
            static const double pw[5] = {150, 120, 80, 60, 40};
            int k = 0;
            while (k < 4 && ratio > th[k]) if (++k == 5) break;
            *power = pw[k];
            break;
        }
        case PR_AC_AntiSunlightPowerDecreaseAttr:
            switch (pr_get_effective_weather_for_move(bt, user)) {
            case WEATHER_TYPE_RAIN: case WEATHER_TYPE_SANDSTORM: case WEATHER_TYPE_HAIL:
            case WEATHER_TYPE_SNOW: case WEATHER_TYPE_FOG: case WEATHER_TYPE_HEAVY_RAIN:
                *power *= 0.5;
            }
            break;
        case PR_AC_TeraBlastPowerAttr:
            if (user->terastallized && pr_get_tera_type(user) == POKEMON_TYPE_STELLAR) *power = 100;
            break;
        default:
            pr_note_unported(bt, a->cls);
        }
    }
}
