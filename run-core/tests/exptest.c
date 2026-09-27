#define _POSIX_C_SOURCE 200809L
/* C half of run-core's experience test: replays exp.producer.test.ts's cases. */
#include "pr/run.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool same(double c, double js) { return c == js || (c != c && js != js); }

int main(int argc, char **argv)
{
    FILE *f = fopen(argc > 1 ? argv[1] : "exp-cases.txt", "r");
    if (!f) { perror("cases"); return 2; }
    char line[256];
    int pass = 0, fail = 0;
    while (fgets(line, sizeof line, f)) {
        int rate, level;
        char total[64], rel[64];
        if (sscanf(line, "X %d %d %63s %63s", &rate, &level, total, rel) != 4) continue;
        double t = strtod(total, NULL), r = strtod(rel, NULL);
        if (same(pr_run_get_level_total_exp(level, rate), t) && same(pr_run_get_level_rel_exp(level, rate), r)) pass++;
        else if (++fail <= 10) printf("MISMATCH rate %d level %d: C %.17g %.17g | JS %s %s\n", rate, level,
                                      pr_run_get_level_total_exp(level, rate), pr_run_get_level_rel_exp(level, rate), total, rel);
    }
    printf("getLevelTotalExp + getLevelRelExp: pass %d  fail %d\n", pass, fail);
    return fail ? 1 : 0;
}
