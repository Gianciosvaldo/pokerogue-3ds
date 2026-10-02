/* C half of Task 34 challenge validation differential tests. */
#include "pr/run.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s <cases.txt>\n", argv[0]);
        return 2;
    }
    FILE *f = fopen(argv[1], "r");
    if (!f) { perror(argv[1]); return 2; }

    int cases = 0, fail = 0;
    for (;;) {
        unsigned flags;
        int gen, type, fresh;
        int sp1, form1, fsp1, fform1, sp2, form2, fsp2, fform2;
        int exp_starter, exp_mon, exp_capture, exp_party;
        int n = fscanf(f, "%u %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
                       &flags, &gen, &type, &fresh,
                       &sp1, &form1, &fsp1, &fform1, &sp2, &form2, &fsp2, &fform2,
                       &exp_starter, &exp_mon, &exp_capture, &exp_party);
        if (n == EOF) break;
        if (n != 16) { fprintf(stderr, "malformed case after %d records\n", cases); return 2; }

        pr_run run;
        memset(&run, 0, sizeof run);
        run.mode = GAME_MODES_CHALLENGE;
        run.has_challenges = true;
        run.challenge_flags = flags;
        run.challenge_generation = (uint8_t)gen;
        run.challenge_type = (uint8_t)type;
        run.challenge_fresh_start = (uint8_t)fresh;

        pr_challenge_mon party[2] = {
            {(uint16_t)sp1, (uint8_t)form1, (uint16_t)fsp1, (uint8_t)fform1},
            {(uint16_t)sp2, (uint8_t)form2, (uint16_t)fsp2, (uint8_t)fform2},
        };
        int got_starter = pr_run_challenge_starter_allowed(&run, sp1, form1);
        int got_mon = pr_run_challenge_mon_allowed(&run, &party[0]);
        int got_capture = pr_run_challenge_capture_allowed(&run, &party[0]);
        int got_party = pr_run_challenge_party_allowed(&run, party, 2);

        cases++;
        if (got_starter != exp_starter || got_mon != exp_mon
            || got_capture != exp_capture || got_party != exp_party
            || pr_run_has_unported_challenges(&run)) {
            if (fail < 20) {
                fprintf(stderr,
                        "FAIL case %d flags=%u g=%d t=%d fresh=%d sp=%d/%d fusion=%d/%d "
                        "got S/M/C/P=%d/%d/%d/%d exp=%d/%d/%d/%d unported=%d\n",
                        cases, flags, gen, type, fresh, sp1, form1, fsp1, fform1,
                        got_starter, got_mon, got_capture, got_party,
                        exp_starter, exp_mon, exp_capture, exp_party,
                        pr_run_has_unported_challenges(&run));
            }
            fail++;
        }
    }
    fclose(f);
    printf("challenge summary: %d cases, %d mismatches\n", cases, fail);
    return fail ? 1 : 0;
}
