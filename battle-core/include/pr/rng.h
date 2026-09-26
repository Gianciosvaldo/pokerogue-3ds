/* Bit-exact port of Phaser 3's RandomDataGenerator (the RNG behind every
 * seeded roll in PokeRogue) plus the seed helpers PokeRogue layers on top.
 * All arithmetic is IEEE double, exactly like the JS original. */
#ifndef PR_RNG_H
#define PR_RNG_H
#include <stdint.h>
#include <stddef.h>

typedef struct {
    double c, s0, s1, s2, n;
} pr_rng;

/* A JS string as UTF-16 code units (seeds are plain ASCII in practice, but
 * shiftCharCodes can push them past 0xFFFF, where fromCharCode wraps). */
typedef struct {
    uint16_t u[64];
    size_t len;
} pr_seedstr;

void pr_seed_from_ascii(pr_seedstr *out, const char *s);
/* utils/common.ts shiftCharCodes: every code unit + shift, wrapped to 16 bits. */
void pr_seed_shift(pr_seedstr *out, const pr_seedstr *in, int shift);

/* RandomDataGenerator.sow([seed]) with a single string seed. */
void pr_rng_sow(pr_rng *r, const pr_seedstr *seed);
double pr_rng_rnd(pr_rng *r);
double pr_rng_frac(pr_rng *r);
double pr_rng_real_in_range(pr_rng *r, double min, double max);
/* RandomDataGenerator.integerInRange(min, max), inclusive. */
int32_t pr_rng_integer_in_range(pr_rng *r, int32_t min, int32_t max);

/* utils/common.ts randSeedInt(range, min): min..min+range-1, no roll when range <= 1. */
int32_t pr_rng_seed_int(pr_rng *r, int32_t range, int32_t min);
/* randSeedFloat() */
static inline double pr_rng_seed_float(pr_rng *r) { return pr_rng_frac(r); }

#endif
