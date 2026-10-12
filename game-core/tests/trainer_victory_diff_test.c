#define _POSIX_C_SOURCE 200809L
/* C half of game-core's boss voucher replay (tools/trainervictorydiff.sh; trainer-victory.producer.test.ts is the
 * upstream half). Every case is upstream's TrainerVictoryPhase for one trainer, with its voucher unlocked or not, and
 * the voucher ModifierRewardPhases it queued run. Here the same trainer is defeated through gc_phase_start(
 * RP_TRAINER_VICTORY) on a gc_system with the same unlock, and the queued voucher rewards are run through
 * gc_phase_start(RP_MODIFIER_REWARD). Compared per case:
 *  - the queued phases in order: the money multiplier, then the config's modifierRewardFuncs (game-core queues none of
 *    them yet: it counts GC_STUB_TRAINER_REWARDS once instead, which is compared too), then at most the voucher
 *  - the voucher reward's key (VOUCHER for a Plus voucher, VOUCHER_PLUS for a Premium one: the table in
 *    TrainerVictoryPhase)
 *  - whether the trainer's voucher is unlocked afterwards and the four voucherCounts changes, after the rewards ran
 * Prints the totals and what the cases exercised. Exit 1 on any mismatch, 2 on a malformed or cut file (the last line
 * counts the cases), 3 when a kind of case went unexercised. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>

static gc_run run;
static gc_system sys;
static int line_no;

static char *tok(char **save)
{
    char *t = strtok_r(NULL, " \n", save);
    if (!t) { fprintf(stderr, "line %d: short line\n", line_no); exit(2); }
    return t;
}
static double num(char **save)
{
    char *t = tok(save), *end;
    double v = strtod(t, &end);
    if (*end) { fprintf(stderr, "line %d: not a number: %s\n", line_no, t); exit(2); }
    return v;
}

static char why[256];
#define MISMATCH(...) do { if (ok) snprintf(why, sizeof why, __VA_ARGS__); ok = false; } while (0)

int main(int argc, char **argv)
{
    FILE *fp;
    if (argc != 2 || !(fp = fopen(argv[1], "r"))) { fprintf(stderr, "usage: trainer_victory_diff_test <trace>\n"); return 2; }
    char line[1024];
    int cases = 0, bad = 0, declared = -1;
    /* what the cases exercised */
    int first_plus = 0, first_premium = 0, later_voucher = 0, later_voucher_plus = 0, no_voucher = 0, funcs = 0,
        boss_no_voucher = 0, shown = 0;
    while (fgets(line, sizeof line, fp)) {
        line_no++;
        char *save = NULL, *t = strtok_r(line, " \n", &save);
        if (!t) continue;
        if (!strcmp(t, "#")) {
            tok(&save);   /* "cases" */
            declared = (int)num(&save);
            continue;
        }
        if (strcmp(t, "T")) { fprintf(stderr, "line %d: unknown record %s\n", line_no, t); return 2; }
        cases++;
        bool ok = true;
        const int tt = (int)num(&save), unlocked_before = (int)num(&save), unlocked_after = (int)num(&save);
        int delta[4];
        for (int k = 0; k < 4; k++) delta[k] = (int)num(&save);
        const int n = (int)num(&save);
        if (tt < 0 || tt >= 512) { fprintf(stderr, "line %d: TrainerType %d is out of voucher_unlocks\n", line_no, tt); return 2; }

        /* the same defeat here, on counts that are not zero so that a write that replaces the count is seen */
        gc_system_init_defaults(&sys);
        static const uint32_t base[4] = {10, 20, 30, 40};
        memcpy(sys.voucher_counts, base, sizeof base);
        sys.voucher_unlocks[tt] = (uint8_t)unlocked_before;
        memset(&run, 0, sizeof run);
        run.sys = &sys;
        run.trainer_type = tt;
        run.run.mode = GAME_MODES_CLASSIC;
        gc_internal *in = gc_int(&run);
        gc_rq_reset(&in->rq);
        gc_rphase start = gc_rp(RP_TRAINER_VICTORY, 0, 0, 0);
        gc_phase_start(&run, &start);
        const gc_trainer_config *c = gc_trainer_config_of(tt);
        const int n_funcs = c ? c->n_reward_funcs : 0;

        /* what is queued, in the order it runs; a voucher reward is run, as upstream's producer ran it */
        double money = -1;
        const char *voucher = NULL;
        int n_mine = 0;
        gc_rphase q;
        while (gc_rq_next(&in->rq, &q)) {
            n_mine++;
            if (q.type == RP_MONEY_REWARD && n_mine == 1) {
                money = q.d;
            } else if (q.type == RP_MODIFIER_REWARD && n_mine == 2 && q.key && !strncmp(q.key, "VOUCHER", 7)) {
                voucher = q.key;
                gc_phase_start(&run, &q);
            } else {
                MISMATCH("trainer %d: unexpected phase %d at position %d", tt, q.type, n_mine);
            }
        }

        /* upstream: M, the config's reward funcs, then the voucher's reward when there is one */
        const char *up_voucher = NULL;
        if (n != 1 + n_funcs && n != 2 + n_funcs) MISMATCH("trainer %d: upstream queued %d phases, config has %d reward funcs", tt, n, n_funcs);
        for (int i = 0; i < n; i++) {
            const char *what = tok(&save);
            if (!strcmp(what, "M")) {
                const double m = num(&save);
                if (i != 0 || m != money) MISMATCH("trainer %d: money phase %d x%g, port x%g", tt, i, m, money);
            } else if (!strcmp(what, "R")) {
                const char *key = tok(&save);
                if (i == n_funcs + 1) up_voucher = key;
                else if (i < 1 || i > n_funcs) MISMATCH("trainer %d: upstream phase %d is a reward", tt, i);
            } else {
                fprintf(stderr, "line %d: unknown entry %s\n", line_no, what);
                return 2;
            }
        }
        if (!up_voucher != !voucher || (voucher && strcmp(voucher, up_voucher)))
            MISMATCH("trainer %d: voucher reward port %s, upstream %s", tt, voucher ? voucher : "none", up_voucher ? up_voucher : "none");
        if (run.stubs[GC_STUB_TRAINER_REWARDS] != (n_funcs ? 1u : 0u))
            MISMATCH("trainer %d: trainer rewards stub counted %u, config has %d reward funcs", tt, run.stubs[GC_STUB_TRAINER_REWARDS], n_funcs);
        if ((sys.voucher_unlocks[tt] != 0) != (unlocked_after != 0))
            MISMATCH("trainer %d: voucher unlocked after %d, upstream %d", tt, sys.voucher_unlocks[tt] != 0, unlocked_after);
        for (int k = 0; k < 4; k++) {
            const int mine = (int)sys.voucher_counts[k] - (int)base[k];
            if (mine != delta[k]) MISMATCH("trainer %d: voucher count %d +%d, upstream +%d", tt, k, mine, delta[k]);
        }

        if (unlocked_after && !unlocked_before) {
            first_plus += delta[GC_VOUCHER_PLUS] == 1;
            first_premium += delta[GC_VOUCHER_PREMIUM] == 1;
        }
        later_voucher += up_voucher && !strcmp(up_voucher, "VOUCHER");
        later_voucher_plus += up_voucher && !strcmp(up_voucher, "VOUCHER_PLUS");
        no_voucher += !unlocked_after;
        funcs += n_funcs > 0;
        boss_no_voucher += !unlocked_after && c && c->is_boss;
        if (!ok) {
            bad++;
            if (shown++ < 10) printf("FAIL line %d: %s\n", line_no, why);
        }
    }
    fclose(fp);
    if (declared != cases || cases < 1) { fprintf(stderr, "cut or malformed trace: %d cases read, %d declared\n", cases, declared); return 2; }
    printf("cases %d: mismatching %d\n", cases, bad);
    printf("first win unlocks: Plus %d, Premium %d; later wins: VOUCHER %d, VOUCHER_PLUS %d; no voucher %d (%d of them boss trainers); with reward funcs %d\n",
           first_plus, first_premium, later_voucher, later_voucher_plus, no_voucher, boss_no_voucher, funcs);
    printf("TOTAL mismatches: %d\n", bad);
    if (bad) return 1;
    if (!first_plus || !first_premium || !later_voucher || !later_voucher_plus || !no_voucher || !funcs) {
        fprintf(stderr, "a kind of case went unexercised\n");
        return 3;
    }
    return 0;
}
