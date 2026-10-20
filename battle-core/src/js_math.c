/* Math.pow exactly as the JS engine the reference runs on computes it.
 *
 * C libraries don't agree on pow's last bit (glibc and newlib's current pow give a
 * different result from V8's for about 1 in 10 inputs, e.g. 1.05 ** 10), and upstream
 * uses Math.pow on the damage path (the enemy's damage tokens). This is V8's pow
 * (src/base/ieee754.cc as shipped with Node 22 / V8 12.4), itself fdlibm's e_pow.c.
 * Only integer and bit operations plus + - * /, so it gives the same bits on every
 * IEEE-754 target.
 *
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 *
 * The original source code covered by the above license above has been
 * modified significantly by Google Inc.
 * Copyright 2016 the V8 project authors. All rights reserved.
 * (V8's BSD license: see https://github.com/v8/v8/blob/main/LICENSE)
 */
#include "pr/battle.h"
#include <math.h>
#include <string.h>

static uint64_t bits_of(double d) { uint64_t b; memcpy(&b, &d, 8); return b; }
static double of_bits(uint64_t b) { double d; memcpy(&d, &b, 8); return d; }
#define EXTRACT_WORDS(hi, lo, d) do { uint64_t b_ = bits_of(d); (hi) = (int32_t)(b_ >> 32); (lo) = (uint32_t)b_; } while (0)
#define GET_HIGH_WORD(i, d) do { (i) = (int32_t)(bits_of(d) >> 32); } while (0)
#define SET_HIGH_WORD(d, v) do { (d) = of_bits((bits_of(d) & 0xFFFFFFFFull) | ((uint64_t)(uint32_t)(v) << 32)); } while (0)
#define SET_LOW_WORD(d, v) do { (d) = of_bits((bits_of(d) & 0xFFFFFFFF00000000ull) | (uint32_t)(v)); } while (0)

double js_pow(double x, double y)
{
    static const double
        bp[] = {1.0, 1.5},
        dp_h[] = {0.0, 5.84962487220764160156e-01},
        dp_l[] = {0.0, 1.35003920212974897128e-08},
        zero = 0.0, one = 1.0, two = 2.0,
        two53 = 9007199254740992.0,
        huge = 1.0e300, tiny = 1.0e-300,
        L1 = 5.99999999999994648725e-01,
        L2 = 4.28571428578550184252e-01,
        L3 = 3.33333329818377432918e-01,
        L4 = 2.72728123808534006489e-01,
        L5 = 2.30660745775561754067e-01,
        L6 = 2.06975017800338417784e-01,
        P1 = 1.66666666666666019037e-01,
        P2 = -2.77777777770155933842e-03,
        P3 = 6.61375632143793436117e-05,
        P4 = -1.65339022054652515390e-06,
        P5 = 4.13813679705723846039e-08,
        lg2 = 6.93147180559945286227e-01,
        lg2_h = 6.93147182464599609375e-01,
        lg2_l = -1.90465429995776804525e-09,
        ovt = 8.0085662595372944372e-0017,
        cp = 9.61796693925975554329e-01,
        cp_h = 9.61796700954437255859e-01,
        cp_l = -7.02846165095275826516e-09,
        ivln2 = 1.44269504088896338700e+00,
        ivln2_h = 1.44269502162933349609e+00,
        ivln2_l = 1.92596299112661746887e-08;

    double z, ax, z_h, z_l, p_h, p_l;
    double y1, t1, t2, r, s, t, u, v, w;
    int32_t i, j, k, yisint, n;
    int32_t hx, hy, ix, iy;
    uint32_t lx, ly;

    EXTRACT_WORDS(hx, lx, x);
    EXTRACT_WORDS(hy, ly, y);
    ix = hx & 0x7fffffff;
    iy = hy & 0x7fffffff;

    if ((iy | (int32_t)ly) == 0) return one;
    if (ix > 0x7ff00000 || ((ix == 0x7ff00000) && (lx != 0)) || iy > 0x7ff00000 ||
        ((iy == 0x7ff00000) && (ly != 0)))
        return x + y;

    yisint = 0;
    if (hx < 0) {
        if (iy >= 0x43400000) {
            yisint = 2;
        } else if (iy >= 0x3ff00000) {
            k = (iy >> 20) - 0x3ff;
            if (k > 20) {
                j = (int32_t)(ly >> (52 - k));
                if (((uint32_t)j << (52 - k)) == ly) yisint = 2 - (j & 1);
            } else if (ly == 0) {
                j = iy >> (20 - k);
                if ((j << (20 - k)) == iy) yisint = 2 - (j & 1);
            }
        }
    }

    if (ly == 0) {
        if (iy == 0x7ff00000) {
            if (((ix - 0x3ff00000) | (int32_t)lx) == 0) return y - y;
            else if (ix >= 0x3ff00000) return (hy >= 0) ? y : zero;
            else return (hy < 0) ? -y : zero;
        }
        if (iy == 0x3ff00000) return hy < 0 ? one / x : x;
        if (hy == 0x40000000) return x * x;
        if (hy == 0x3fe00000 && hx >= 0) return sqrt(x);
    }

    ax = fabs(x);
    if (lx == 0) {
        if (ix == 0x7ff00000 || ix == 0 || ix == 0x3ff00000) {
            z = ax;
            if (hy < 0) z = one / z;
            if (hx < 0) {
                if (((ix - 0x3ff00000) | yisint) == 0) z = NAN;
                else if (yisint == 1) z = -z;
            }
            return z;
        }
    }

    n = hx < 0 ? 0 : 1; /* (hx >> 31) + 1 */
    if ((n | yisint) == 0) return NAN;

    s = one;
    if ((n | (yisint - 1)) == 0) s = -one;

    if (iy > 0x41e00000) {
        if (iy > 0x43f00000) {
            if (ix <= 0x3fefffff) return (hy < 0) ? huge * huge : tiny * tiny;
            if (ix >= 0x3ff00000) return (hy > 0) ? huge * huge : tiny * tiny;
        }
        if (ix < 0x3fefffff) return (hy < 0) ? s * huge * huge : s * tiny * tiny;
        if (ix > 0x3ff00000) return (hy > 0) ? s * huge * huge : s * tiny * tiny;
        t = ax - one;
        w = (t * t) * (0.5 - t * (0.3333333333333333333333 - t * 0.25));
        u = ivln2_h * t;
        v = t * ivln2_l - w * ivln2;
        t1 = u + v;
        SET_LOW_WORD(t1, 0);
        t2 = v - (t1 - u);
    } else {
        double ss, s2, s_h, s_l, t_h, t_l;
        n = 0;
        if (ix < 0x00100000) {
            ax *= two53;
            n -= 53;
            GET_HIGH_WORD(ix, ax);
        }
        n += ((ix) >> 20) - 0x3ff;
        j = ix & 0x000fffff;
        ix = j | 0x3ff00000;
        if (j <= 0x3988E) {
            k = 0;
        } else if (j < 0xBB67A) {
            k = 1;
        } else {
            k = 0;
            n += 1;
            ix -= 0x00100000;
        }
        SET_HIGH_WORD(ax, ix);

        u = ax - bp[k];
        v = one / (ax + bp[k]);
        ss = u * v;
        s_h = ss;
        SET_LOW_WORD(s_h, 0);
        t_h = zero;
        SET_HIGH_WORD(t_h, ((ix >> 1) | 0x20000000) + 0x00080000 + (k << 18));
        t_l = ax - (t_h - bp[k]);
        s_l = v * ((u - s_h * t_h) - s_h * t_l);
        s2 = ss * ss;
        r = s2 * s2 * (L1 + s2 * (L2 + s2 * (L3 + s2 * (L4 + s2 * (L5 + s2 * L6)))));
        r += s_l * (s_h + ss);
        s2 = s_h * s_h;
        t_h = 3.0 + s2 + r;
        SET_LOW_WORD(t_h, 0);
        t_l = r - ((t_h - 3.0) - s2);
        u = s_h * t_h;
        v = s_l * t_h + t_l * ss;
        p_h = u + v;
        SET_LOW_WORD(p_h, 0);
        p_l = v - (p_h - u);
        z_h = cp_h * p_h;
        z_l = cp_l * p_h + p_l * cp + dp_l[k];
        t = (double)n;
        t1 = (((z_h + z_l) + dp_h[k]) + t);
        SET_LOW_WORD(t1, 0);
        t2 = z_l - (((t1 - t) - dp_h[k]) - z_h);
    }

    y1 = y;
    SET_LOW_WORD(y1, 0);
    p_l = (y - y1) * t1 + y * t2;
    p_h = y1 * t1;
    z = p_l + p_h;
    {
        uint32_t zl;
        EXTRACT_WORDS(j, zl, z);
        i = (int32_t)zl;
    }
    if (j >= 0x40900000) {
        if (((j - 0x40900000) | i) != 0) return s * huge * huge;
        if (p_l + ovt > z - p_h) return s * huge * huge;
    } else if ((j & 0x7fffffff) >= 0x4090cc00) {
        if (((j - (int32_t)0xc090cc00) | i) != 0) return s * tiny * tiny;
        if (p_l <= z - p_h) return s * tiny * tiny;
    }
    i = j & 0x7fffffff;
    k = (i >> 20) - 0x3ff;
    n = 0;
    if (i > 0x3fe00000) {
        n = j + (0x00100000 >> (k + 1));
        k = ((n & 0x7fffffff) >> 20) - 0x3ff;
        t = zero;
        SET_HIGH_WORD(t, n & ~(0x000fffff >> k));
        n = ((n & 0x000fffff) | 0x00100000) >> (20 - k);
        if (j < 0) n = -n;
        p_h -= t;
    }
    t = p_l + p_h;
    SET_LOW_WORD(t, 0);
    u = t * lg2_h;
    v = (p_l - (t - p_h)) * lg2 + t * lg2_l;
    z = u + v;
    w = v - (z - u);
    t = z * z;
    t1 = z - t * (P1 + t * (P2 + t * (P3 + t * (P4 + t * P5))));
    r = (z * t1) / ((t1 - two) - (w + z * w));
    z = one - (r - z);
    GET_HIGH_WORD(j, z);
    j += (int32_t)((uint32_t)n << 20);
    if ((j >> 20) <= 0) {
        z = scalbn(z, n);
    } else {
        int32_t tmp;
        GET_HIGH_WORD(tmp, z);
        SET_HIGH_WORD(z, tmp + (int32_t)((uint32_t)n << 20));
    }
    return s * z;
}

/* Preserve the exact operation order of BattleScene.getWaveMoneyAmount. */
double pr_wave_money_amount(int wave, double multiplier)
{
    double set = ceil(wave / 10.0) - 1;
    double value = js_pow((set + 1 + (0.75 + (fmod(wave - 1, 10) + 1) / 10)) * 100,
                          1 + 0.005 * set) * multiplier;
    return floor(value / 10) * 10;
}
