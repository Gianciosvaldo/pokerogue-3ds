/* custom-core: the JoJo roll (see cc/jojo.h). Pure: the only randomness is the explicit custom stream. */
#include "cc/jojo.h"
#include "cc/scheduler.h"

#define CC_JOJO_SALT 0x4A4F4A4F424F5A41ull   /* "JOJOBOZA": keeps this stream apart from the rematch stream of the same seed */

void cc_jojo_start(cc_jojo_state *s, uint64_t seed)
{
    if (!s) return;
    s->rng_state = seed ^ CC_JOJO_SALT;
    s->used = 0;
    s->fights = 0;
}

static unsigned popcount32(uint32_t v)
{
    unsigned n = 0;
    for (; v; v &= v - 1) n++;
    return n;
}

cc_status cc_jojo_validate_in(const cc_jojo_state *s, unsigned count)
{
    if (!s) return CC_E_NULL;
    if (count < 32 && (s->used >> count) != 0) return CC_E_RANGE;
    if (popcount32(s->used) != s->fights) return CC_E_CONSISTENCY;
    return CC_OK;
}

cc_status cc_jojo_validate(const cc_jojo_state *s) { return cc_jojo_validate_in(s, cc_jojo_count); }

int cc_jojo_roll_in(cc_jojo_state *s, const cc_jojo_char *chars, unsigned count, int biome)
{
    if (!s || !chars || count > 32 || cc_jojo_validate_in(s, count) != CC_OK) return CC_JOJO_NONE;
    if (biome < 0) return CC_JOJO_NONE;   /* biome -1 is the Pillar Men's: never dealt here */
    unsigned n = 0;
    for (unsigned i = 0; i < count; i++)
        if (chars[i].biome == biome && !(s->used >> i & 1u)) n++;
    if (!n) return CC_JOJO_NONE;
    if (cc_rng_v1_pick(&s->rng_state, 100) >= CC_JOJO_P_PERCENT) return CC_JOJO_NONE;
    unsigned pick = cc_rng_v1_pick(&s->rng_state, n);
    for (unsigned i = 0; i < count; i++) {
        if (chars[i].biome != biome || (s->used >> i & 1u)) continue;
        if (pick-- == 0) {
            s->used |= 1u << i;
            s->fights++;
            return (int)i;
        }
    }
    return CC_JOJO_NONE;   /* unreachable: pick < n */
}

int cc_jojo_roll(cc_jojo_state *s, int biome) { return cc_jojo_roll_in(s, cc_jojo_chars, cc_jojo_count, biome); }

int cc_jojo_find(const char *id)
{
    if (!id) return CC_JOJO_NONE;
    for (unsigned i = 0; i < cc_jojo_count; i++) {
        const char *a = cc_jojo_chars[i].id, *b = id;
        while (*a && *a == *b) { a++; b++; }
        if (*a == *b) return (int)i;
    }
    return CC_JOJO_NONE;
}
