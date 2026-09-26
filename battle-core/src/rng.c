#include "pr/rng.h"
#include <math.h>

#define TWO_M32 2.3283064365386963e-10 /* 2^-32 */
#define TWO_32 4294967296.0

/* JS ToUint32 for the finite values that occur here. */
static double to_uint32(double x)
{
    double t = trunc(x);
    t = fmod(t, TWO_32);
    if (t < 0) t += TWO_32;
    return t;
}

void pr_seed_from_ascii(pr_seedstr *out, const char *s)
{
    out->len = 0;
    while (*s && out->len < sizeof out->u / sizeof out->u[0])
        out->u[out->len++] = (uint8_t)*s++;
}

void pr_seed_shift(pr_seedstr *out, const pr_seedstr *in, int shift)
{
    out->len = in->len;
    for (size_t i = 0; i < in->len; i++)
        out->u[i] = (uint16_t)(in->u[i] + shift);
}

static double hash_units(pr_rng *r, const uint16_t *u, size_t len)
{
    double n = r->n, h;
    for (size_t i = 0; i < len; i++) {
        n += u[i];
        h = 0.02519603282416938 * n;
        n = to_uint32(h);
        h -= n;
        h *= n;
        n = to_uint32(h);
        h -= n;
        n += h * TWO_32;
    }
    r->n = n;
    return to_uint32(n) * TWO_M32;
}

void pr_rng_sow(pr_rng *r, const pr_seedstr *seed)
{
    static const uint16_t space = ' ';
    r->n = 0xefc8249d;
    r->s0 = hash_units(r, &space, 1);
    r->s1 = hash_units(r, &space, 1);
    r->s2 = hash_units(r, &space, 1);
    r->c = 1;
    if (!seed) return;
    r->s0 -= hash_units(r, seed->u, seed->len);
    r->s0 += (r->s0 < 0);
    r->s1 -= hash_units(r, seed->u, seed->len);
    r->s1 += (r->s1 < 0);
    r->s2 -= hash_units(r, seed->u, seed->len);
    r->s2 += (r->s2 < 0);
}

double pr_rng_rnd(pr_rng *r)
{
    double t = 2091639 * r->s0 + r->c * TWO_M32;
    r->c = (double)(int32_t)(int64_t)t; /* t | 0; t is always in [0, 2^31) */
    r->s0 = r->s1;
    r->s1 = r->s2;
    r->s2 = t - r->c;
    return r->s2;
}

double pr_rng_frac(pr_rng *r)
{
    double a = pr_rng_rnd(r);
    double b = pr_rng_rnd(r) * 0x200000;
    return a + (double)(int32_t)(int64_t)b * 1.1102230246251565e-16;
}

double pr_rng_real_in_range(pr_rng *r, double min, double max)
{
    return pr_rng_frac(r) * (max - min) + min;
}

int32_t pr_rng_integer_in_range(pr_rng *r, int32_t min, int32_t max)
{
    return (int32_t)floor(pr_rng_real_in_range(r, 0, (double)max - min + 1) + min);
}

int32_t pr_rng_seed_int(pr_rng *r, int32_t range, int32_t min)
{
    if (range <= 1) return min;
    return pr_rng_integer_in_range(r, min, range - 1 + min);
}
