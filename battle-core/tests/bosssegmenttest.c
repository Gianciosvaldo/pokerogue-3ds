/* C half of the boss health-segment differential test. */
#include "pr/battle.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static int same_double(double a, double b)
{
    return a == b || fabs(a - b) < 1e-9;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s <cases.txt>\n", argv[0]);
        return 2;
    }
    FILE *f = fopen(argv[1], "r");
    if (!f) {
        perror(argv[1]);
        return 2;
    }

    int cases = 0, fail = 0;
    for (;;) {
        int segments, max_hp, current_hp, min_segment, segment_index, damage, exp_cleared;
        double exp_adjusted;
        int n = fscanf(f, "%d %d %d %d %d %d %lf %d",
                       &segments, &max_hp, &current_hp, &min_segment, &segment_index,
                       &damage, &exp_adjusted, &exp_cleared);
        if (n == EOF) break;
        if (n != 8) {
            fprintf(stderr, "malformed case after %d records\n", cases);
            fail++;
            break;
        }
        (void)segments;
        double segment_hp = (double)max_hp / segments;
        int got_cleared = -99;
        double got_adjusted = pr_calculate_boss_segment_damage(
            damage, current_hp, segment_hp, min_segment, segment_index, &got_cleared);
        cases++;
        if (!same_double(got_adjusted, exp_adjusted) || got_cleared != exp_cleared) {
            if (fail < 20) {
                fprintf(stderr,
                        "FAIL case %d: max=%d hp=%d segs=%d idx=%d min=%d damage=%d "
                        "got %.17g/%d expected %.17g/%d\n",
                        cases, max_hp, current_hp, segments, segment_index, min_segment, damage,
                        got_adjusted, got_cleared, exp_adjusted, exp_cleared);
            }
            fail++;
        }
    }
    fclose(f);
    printf("bosssegment summary: %d cases, %d mismatches\n", cases, fail);
    return fail ? 1 : 0;
}
