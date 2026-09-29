#define _POSIX_C_SOURCE 200809L
/* C half of reward-core's victory reward test: replays victory.producer.test.ts's cases through
 * pr_reward_victory_rewards and compares the items (in order) and whether a reward screen follows. */
#include "pr/reward.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *tok(char **save)
{
    char *t = strtok_r(NULL, " \n", save);
    if (!t) { fprintf(stderr, "short line\n"); exit(2); }
    return t;
}

int main(int argc, char **argv)
{
    FILE *f = fopen(argc > 1 ? argv[1] : "victory-cases.txt", "r");
    if (!f) { perror("cases"); return 2; }
    static char line[4096];
    int cases = 0, pass = 0, fail = 0, threw = 0, items = 0, selects = 0;
    while (fgets(line, sizeof line, f)) {
        char *save;
        char *t = strtok_r(line, " \n", &save);
        if (!t || t[0] == '#') continue;
        if (!strcmp(t, "X")) { threw++; continue; }
        if (strcmp(t, "V")) continue;
        cases++;
        pr_run run;
        memset(&run, 0, sizeof run);
        run.mode = atoi(tok(&save));
        const int wave = atoi(tok(&save));
        run.offset_gym = atoi(tok(&save)) != 0;
        const bool select = atoi(tok(&save)) != 0;
        const int n = atoi(tok(&save));
        uint16_t out[8];
        bool my_select;
        const int mine = pr_reward_victory_rewards(&run, wave, out, &my_select);
        bool ok = mine == n && my_select == select;
        for (int i = 0; i < n; i++) {
            const char *key = tok(&save);
            if (ok && strcmp(pr_item_keys[out[i]], key)) ok = false;
        }
        items += n;
        selects += select;
        if (ok) pass++;
        else if (fail++ < 10) fprintf(stderr, "mode %d wave %d: %d items (select %d), upstream %d (select %d)\n", run.mode, wave, mine, my_select, n, select);
    }
    printf("cases %d: pass %d, fail %d, upstream threw %d; %d items, %d reward screens\n", cases, pass, fail, threw, items, selects);
    return fail ? 1 : 0;
}
