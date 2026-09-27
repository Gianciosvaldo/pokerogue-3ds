#define _POSIX_C_SOURCE 200809L
/* C half of run-core's shop test: replays the cases written by shop.producer.test.ts and
 * compares wave money, the money reward, the shop status, the shop options as shown and as
 * charged, and the reroll cost. */
#include "pr/run.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { C_MONEY, C_REWARD, C_STATUS, C_SHOWN, C_CHARGED, C_REROLL, C_COUNT_ };
static const char *const check_names[C_COUNT_] = {
    "getWaveMoneyAmount", "money reward (Amulet Coin, Happy Hour)", "getShopStatus",
    "shop options as shown (Black Sludge)", "shop options as charged", "getRerollCost",
};
static int pass[C_COUNT_], fail[C_COUNT_], uncovered[C_COUNT_];

static char *tok(char **save)
{
    char *t = strtok_r(NULL, " \n", save);
    if (!t) { fprintf(stderr, "short line\n"); exit(2); }
    return t;
}
static double num(char **save) { return strtod(tok(save), NULL); }
static bool same(double c, double js) { return c == js || (c != c && js != js); }

static void record(int check, pr_run *run, bool ok, int line_no)
{
    if (run->unported) { uncovered[check]++; run->unported = 0; return; }
    if (ok) { pass[check]++; return; }
    if (++fail[check] <= 10) printf("MISMATCH line %d: %s\n", line_no, check_names[check]);
}

/* Compares an options line ("V"/"B" n (name cost)*n) with run-core's options and costs */
static bool same_options(char **save, const pr_shop_option *mine, int n, const double *costs)
{
    int js_n = (int)num(save);
    bool ok = js_n == n;
    for (int i = 0; i < js_n; i++) {
        const char *name = tok(save);
        double cost = num(save);
        if (i < n) ok = ok && !strcmp(name, pr_shop_item_name[mine[i].item]) && same(costs[i], cost);
    }
    return ok;
}

int main(int argc, char **argv)
{
    FILE *f = fopen(argc > 1 ? argv[1] : "shop-cases.txt", "r");
    if (!f) { perror("cases"); return 2; }
    static char line[8192];
    int line_no = 0, cases = 0;
    pr_run run;
    int wave = 0;
    double sludge = 0;
    while (fgets(line, sizeof line, f)) {
        line_no++;
        char *save;
        char *t = strtok_r(line, " \n", &save);
        if (!t) continue;
        if (!strcmp(t, "S")) {
            cases++;
            memset(&run, 0, sizeof run);
            run.mode = (int)num(&save);
            wave = (int)num(&save);
            sludge = num(&save);
            int n_coins = (int)num(&save), coins[8];
            if (n_coins < 0 || n_coins > 8) { fprintf(stderr, "bad coin count\n"); return 2; }
            for (int i = 0; i < n_coins; i++) coins[i] = (int)num(&save);
            bool happy = num(&save) != 0;
            double mult = num(&save), money = num(&save), reward = num(&save), status = num(&save);
            record(C_MONEY, &run, same(pr_run_get_wave_money_amount(wave, mult), money), line_no);
            record(C_REWARD, &run, same(pr_run_get_money_reward(wave, mult, coins, n_coins, happy), reward), line_no);
            record(C_STATUS, &run, same(pr_run_get_shop_status(&run), status), line_no);
            bool lock = num(&save) != 0;
            int rerolls = (int)num(&save);
            double custom = num(&save);
            int n_tiers = (int)num(&save), tiers[16];
            if (n_tiers < 0 || n_tiers > 16) { fprintf(stderr, "bad tier count\n"); return 2; }
            for (int i = 0; i < n_tiers; i++) tiers[i] = (int)num(&save);
            double reroll = num(&save);
            record(C_REROLL, &run,
                   same(pr_run_get_reroll_cost(wave, lock, tiers, n_tiers, rerolls, custom, sludge), reroll), line_no);
        } else if (!strcmp(t, "V")) {
            /* shown: options built from Black Sludge applied to getWaveMoneyAmount(1) */
            pr_shop_option opts[PR_SHOP_ITEM_COUNT_];
            double base = pr_run_apply_heal_shop_cost(pr_run_get_wave_money_amount(wave, 1), sludge), costs[PR_SHOP_ITEM_COUNT_];
            int n = pr_run_get_player_shop_options(&run, wave, base, opts);
            for (int i = 0; i < n; i++) costs[i] = opts[i].cost;
            record(C_SHOWN, &run, same_options(&save, opts, n, costs), line_no);
        } else if (!strcmp(t, "B")) {
            /* charged: options built from getWaveMoneyAmount(1), then Black Sludge on each cost */
            pr_shop_option opts[PR_SHOP_ITEM_COUNT_];
            double costs[PR_SHOP_ITEM_COUNT_];
            int n = pr_run_get_player_shop_options(&run, wave, pr_run_get_wave_money_amount(wave, 1), opts);
            for (int i = 0; i < n; i++) costs[i] = pr_run_apply_heal_shop_cost(opts[i].cost, sludge);
            record(C_CHARGED, &run, same_options(&save, opts, n, costs), line_no);
        }
    }
    printf("cases %d\n", cases);
    int bad = 0;
    for (int c = 0; c < C_COUNT_; c++) {
        printf("  %-40s pass %6d  fail %d  not covered %d\n", check_names[c], pass[c], fail[c], uncovered[c]);
        bad |= fail[c];
    }
    return bad ? 1 : 0;
}
