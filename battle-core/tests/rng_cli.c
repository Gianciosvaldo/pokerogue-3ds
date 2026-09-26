/* Prints the same stream as tests/rng_ref.cjs for a given seed. */
#include "pr/rng.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv)
{
    pr_seedstr s, sh;
    pr_rng r;
    pr_seed_from_ascii(&s, argc > 1 ? argv[1] : "abc");
    pr_seed_shift(&sh, &s, argc > 2 ? atoi(argv[2]) : 0);
    pr_rng_sow(&r, &sh);
    printf("%.17g %.17g %.17g %.17g\n", r.c, r.s0, r.s1, r.s2);
    for (int i = 0; i < 2000; i++) {
        switch (i % 3) {
        case 0: printf("%d\n", pr_rng_seed_int(&r, 100 + i, -i)); break;
        case 1: printf("%.17g\n", pr_rng_frac(&r)); break;
        case 2: printf("%d\n", pr_rng_integer_in_range(&r, 85, 100)); break;
        }
    }
    return 0;
}
