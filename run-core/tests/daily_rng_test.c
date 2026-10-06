/* Daily generation is executeWithSeedOffset work: its temporary stream must not
 * alter the caller's outer RNG. Repeating a preview must return the same starters. */
#include "pr/run.h"
#include <stdio.h>
#include <string.h>

static int checks, failures;
static void check(bool ok, const char *seed, const char *what)
{
    checks++;
    if (!ok) { failures++; fprintf(stderr, "FAIL daily RNG %s: %s\n", seed, what); }
}
static bool same_rng(const pr_rng *a, const pr_rng *b)
{
    return a->c == b->c && a->s0 == b->s0 && a->s1 == b->s1
        && a->s2 == b->s2 && a->n == b->n;
}
int main(void)
{
    const char *seeds[] = {
        "MjAyNC0wNS0xMA==", "MjAyNC0wOS0zMA==", "MjAyNS0wMS0wMQ==",
        "MjAyNS0wNS0xNQ==", "MjAyNS0xMC0zMQ=="
    };
    for (unsigned i = 0; i < sizeof seeds / sizeof seeds[0]; i++) {
        pr_run run = {0};
        run.mode = GAME_MODES_DAILY;
        pr_seed_from_ascii(&run.seed, seeds[i]);
        pr_seedstr outer;
        pr_seed_from_ascii(&outer, "daily-preview-outer-stream");
        pr_rng_sow(&run.rng, &outer);
        pr_rng before = run.rng;
        pr_enemy_gen a[3] = {0}, b[3] = {0};
        int species_a[3] = {0}, species_b[3] = {0};
        int first = pr_run_get_daily_starters(&run, a, species_a);
        check(first == 0, seeds[i], "first generation accepted");
        check(same_rng(&before, &run.rng), seeds[i], "outer RNG preserved");
        int second = pr_run_get_daily_starters(&run, b, species_b);
        check(second == 0, seeds[i], "repeated generation accepted");
        check(first == 0 && second == 0 && !memcmp(a, b, sizeof a)
            && !memcmp(species_a, species_b, sizeof species_a), seeds[i], "preview is repeatable");
        check(same_rng(&before, &run.rng), seeds[i], "outer RNG preserved after repetition");
        if (i == 0) {
            check(species_a[0] == SPECIES_ID_FALINKS && species_a[1] == SPECIES_ID_PALDEA_TAUROS
                && species_a[2] == SPECIES_ID_CASTFORM, seeds[i], "natural starter sequence");
            check(a[1].nature == NATURE_SASSY, seeds[i], "Tauros nature after temporary form roll");
        }
    }
    printf("daily RNG regression: %d checks, %d passed, %d failed\n", checks, checks - failures, failures);
    return failures ? 1 : 0;
}
