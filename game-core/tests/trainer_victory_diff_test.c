#define _POSIX_C_SOURCE 200809L
/* C half of game-core's trainer victory replay (tools/trainervictorydiff.sh; trainer-victory.producer.test.ts is the
 * upstream half). Every case is upstream's TrainerVictoryPhase for one trainer, with its voucher unlocked or not, and
 * every ModifierRewardPhase it queued run (addModifier). Here the same trainer is defeated through gc_phase_start(
 * RP_TRAINER_VICTORY) on a gc_system with the same unlock and the same stacks of the reward items, and the queued
 * rewards are run through gc_phase_start(RP_MODIFIER_REWARD). Compared per case:
 *  - the queued phases in order: the money multiplier, the config's modifierRewardFuncs (rewards, rival battles'
 *    EXP. Share, Super EXP. Charm and Tera Orb), then at most the voucher's reward
 *  - the voucher reward's key (VOUCHER for a Plus voucher, VOUCHER_PLUS for a Premium one: the table in
 *    TrainerVictoryPhase)
 *  - the stack of each reward item after its phase, from the stack the player had before it: the scene keeps its
 *    modifiers from case to case, so the stacks reach getMaxStackCount() and addModifier adds nothing past it
 *  - the text each reward shows, the voucher's included, formatted from upstream's locale files (GC_LOCALES)
 *  - a reward whose stack was full: addModifier queued battle:itemStackFull and gave the tier's default item (a Poke
 *    Ball). Compared: that message's text, and the five Poke Ball counts after the rewards, from the counts before
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

/* upstream's locales/en folder (GC_LOCALES, set by tools/trainervictorydiff.sh): a reward's text is compared as text,
 * formatted by battle-core's pr_msg_format from the same files upstream's i18next read */
static const char *locale_file(void *ctx, const char *file, int fallback, size_t *len)
{
    (void)ctx;
    static struct { char name[64]; char *text; size_t len; } cache[8];
    static int n_cache;
    const char *dir = getenv("GC_LOCALES");
    if (fallback) return NULL;
    if (!dir) { fputs("GC_LOCALES must name upstream's locales/en folder (tools/trainervictorydiff.sh sets it)\n", stderr); exit(2); }
    for (int i = 0; i < n_cache; i++)
        if (!strcmp(cache[i].name, file)) { *len = cache[i].len; return cache[i].text; }
    if (n_cache >= 8) return NULL;
    char path[512];
    snprintf(path, sizeof path, "%s/%s.json", dir, file);
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", path); exit(2); }
    fseek(f, 0, SEEK_END);
    const long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *text = malloc((size_t)sz + 1);
    const size_t n = fread(text, 1, (size_t)sz, f);
    text[n] = 0;
    fclose(f);
    snprintf(cache[n_cache].name, sizeof cache[n_cache].name, "%s", file);
    cache[n_cache].text = text;
    cache[n_cache].len = n;
    n_cache++;
    *len = n;
    return text;
}

static char why[4096];
#define MISMATCH(...) do { if (ok) snprintf(why, sizeof why, __VA_ARGS__); ok = false; } while (0)
#define MAX_PHASES 8

int main(int argc, char **argv)
{
    FILE *fp;
    if (argc != 2 || !(fp = fopen(argv[1], "r"))) { fprintf(stderr, "usage: trainer_victory_diff_test <trace>\n"); return 2; }
    static const pr_text_source src = {locale_file, NULL, "en"};
    char line[2048];
    int cases = 0, bad = 0, declared = -1;
    /* what the cases exercised */
    int first_plus = 0, first_premium = 0, later_voucher = 0, later_voucher_plus = 0, no_voucher = 0, funcs = 0,
        boss_no_voucher = 0, texts_compared = 0, at_cap = 0, from_zero = 0, shown = 0, full_stacks = 0, ball_cases = 0;
    int by_key[3] = {0, 0, 0};
    static const char *const func_keys[3] = {"EXP_SHARE", "SUPER_EXP_CHARM", "TERA_ORB"};
    while (fgets(line, sizeof line, fp)) {
        line_no++;
        /* the texts follow the phases, each after a tab */
        char *texts = strchr(line, '\t');
        if (texts) *texts++ = 0;
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
        int balls_before[5], balls_after[5];
        for (int k = 0; k < 5; k++) balls_before[k] = (int)num(&save);
        for (int k = 0; k < 5; k++) balls_after[k] = (int)num(&save);
        const int n = (int)num(&save);
        if (tt < 0 || tt >= 512) { fprintf(stderr, "line %d: TrainerType %d is out of voucher_unlocks\n", line_no, tt); return 2; }
        if (n < 1 || n > MAX_PHASES) { fprintf(stderr, "line %d: %d phases\n", line_no, n); return 2; }

        /* upstream's phases: M, the config's reward funcs, then the voucher's reward when there is one */
        double up_money = -1;
        char up_key[MAX_PHASES][48];
        int up_before[MAX_PHASES], up_after[MAX_PHASES];
        for (int i = 0; i < n; i++) {
            const char *what = tok(&save);
            up_key[i][0] = 0;
            up_before[i] = up_after[i] = 0;
            if (!strcmp(what, "M")) {
                if (i != 0) { fprintf(stderr, "line %d: money phase %d\n", line_no, i); return 2; }
                up_money = num(&save);
            } else if (!strcmp(what, "R")) {
                snprintf(up_key[i], sizeof up_key[i], "%s", tok(&save));
                up_before[i] = (int)num(&save);
                up_after[i] = (int)num(&save);
            } else {
                fprintf(stderr, "line %d: unknown entry %s\n", line_no, what);
                return 2;
            }
        }
        char *up_text[MAX_PHASES] = {NULL}, *up_full[MAX_PHASES] = {NULL};
        if (texts) {
            texts[strcspn(texts, "\n")] = 0;
            char *s2 = NULL;
            int k = 0;
            for (char *x = strtok_r(texts, "\t", &s2); x && k < MAX_PHASES; x = strtok_r(NULL, "\t", &s2)) {
                char *full = strchr(x, '\x01');   /* the itemStackFull message follows the reward's text */
                if (full) *full++ = 0;
                for (char *c2 = x; *c2; c2++) if (*c2 == '|') *c2 = '\n';   /* the producer wrote a line break as | */
                for (char *c2 = full ? full : x + strlen(x); *c2; c2++) if (*c2 == '|') *c2 = '\n';
                up_full[k] = full;
                up_text[k++] = x;
            }
        }

        /* the same defeat here, on counts that are not zero so that a write that replaces the count is seen */
        gc_system_init_defaults(&sys);
        static const uint32_t base[4] = {10, 20, 30, 40};
        memcpy(sys.voucher_counts, base, sizeof base);
        sys.voucher_unlocks[tt] = (uint8_t)unlocked_before;
        memset(&run, 0, sizeof run);
        run.sys = &sys;
        run.trainer_type = tt;
        run.run.mode = GAME_MODES_CLASSIC;
        for (int k = 0; k < 5; k++) run.pokeballs[k] = (uint8_t)balls_before[k];
        const gc_trainer_config *c = gc_trainer_config_of(tt);
        const int n_funcs = c ? c->n_reward_funcs : 0;
        if (n != 1 + n_funcs && n != 2 + n_funcs) MISMATCH("trainer %d: upstream queued %d phases, config has %d reward funcs", tt, n, n_funcs);
        for (int i = 1; i <= n_funcs && i < n; i++) {
            const int item = gc_reward_item_index(up_key[i]);
            if (item < 0) { fprintf(stderr, "line %d: unknown reward %s\n", line_no, up_key[i]); return 2; }
            if (up_before[i]) gc_item_add(&run, pr_item_keys[item], -1, 0, up_before[i]);
        }
        gc_internal *in = gc_int(&run);
        gc_rq_reset(&in->rq);
        gc_rphase start = gc_rp(RP_TRAINER_VICTORY, 0, 0, 0);
        gc_phase_start(&run, &start);
        gc_event ev;
        while (gc_run_next_event(&run, &ev)) {}   /* "trainer defeated" is not a reward's text */

        /* what is queued, in the order it runs; every reward is run, as upstream's producer ran them */
        double money = -1;
        const char *rewards[MAX_PHASES];
        char text[MAX_PHASES][256], full_text[MAX_PHASES][256];
        int n_rewards = 0, n_mine = 0;
        gc_rphase q;
        while (gc_rq_next(&in->rq, &q)) {
            n_mine++;
            if (q.type == RP_MONEY_REWARD && n_mine == 1) {
                money = q.d;
            } else if (q.type == RP_MODIFIER_REWARD && n_mine >= 2 && q.key && n_rewards < MAX_PHASES) {
                rewards[n_rewards] = q.key;
                gc_phase_start(&run, &q);
                text[n_rewards][0] = full_text[n_rewards][0] = 0;
                int msgs = 0;
                while (gc_run_next_event(&run, &ev)) {
                    if (ev.kind != GC_EV_MESSAGE || !ev.msg.key) continue;
                    if (msgs == 0) pr_msg_format(&ev.msg, &src, text[n_rewards], sizeof text[n_rewards]);
                    else if (msgs == 1) pr_msg_format(&ev.msg, &src, full_text[n_rewards], sizeof full_text[n_rewards]);
                    msgs++;
                }
                if (msgs < 1 || msgs > 2) MISMATCH("trainer %d: reward %s showed %d messages", tt, q.key, msgs);
                n_rewards++;
            } else {
                MISMATCH("trainer %d: unexpected phase %d at position %d", tt, q.type, n_mine);
            }
        }
        if (money != up_money) MISMATCH("trainer %d: money phase x%g, port x%g", tt, up_money, money);

        /* the reward funcs (positions 1..n_funcs), then the voucher's reward */
        const char *voucher = NULL;
        const char *up_voucher = n > 1 + n_funcs ? up_key[n - 1] : NULL;
        if (n_rewards != n - 1) MISMATCH("trainer %d: %d rewards, upstream %d", tt, n_rewards, n - 1);
        for (int i = 0; i < n_rewards; i++) {
            if (i < n_funcs) {
                if (i + 1 >= n || strcmp(rewards[i], up_key[i + 1])) MISMATCH("trainer %d: reward %d is %s, upstream %s", tt, i, rewards[i], i + 1 < n ? up_key[i + 1] : "none");
                else {
                    const int stack = gc_item_stack(&run, rewards[i]);
                    if (stack != up_after[i + 1]) MISMATCH("trainer %d: %s stack %d after (%d before), upstream %d", tt, rewards[i], stack, up_before[i + 1], up_after[i + 1]);
                    if (!up_text[i] || strcmp(text[i], up_text[i])) MISMATCH("trainer %d: %s text '%s', upstream '%s'", tt, rewards[i], text[i], up_text[i] ? up_text[i] : "none");
                    else texts_compared++;
                    if (!up_full[i] != !full_text[i][0] || (up_full[i] && strcmp(full_text[i], up_full[i])))
                        MISMATCH("trainer %d: %s stack-full text '%s', upstream '%s'", tt, rewards[i], full_text[i], up_full[i] ? up_full[i] : "none");
                    full_stacks += up_full[i] != NULL;
                    at_cap += up_before[i + 1] == up_after[i + 1] && up_before[i + 1] > 0;
                    from_zero += up_before[i + 1] == 0 && up_after[i + 1] == 1;
                    for (int k = 0; k < 3; k++) by_key[k] += !strcmp(rewards[i], func_keys[k]);
                }
            } else {
                voucher = rewards[i];
                if (gc_item_stack(&run, voucher)) MISMATCH("trainer %d: %s was added to the items", tt, voucher);
                if (!up_text[i] || strcmp(text[i], up_text[i])) MISMATCH("trainer %d: %s text '%s', upstream '%s'", tt, voucher, text[i], up_text[i] ? up_text[i] : "none");
                else texts_compared++;
            }
        }
        if (!up_voucher != !voucher || (voucher && strcmp(voucher, up_voucher)))
            MISMATCH("trainer %d: voucher reward port %s, upstream %s", tt, voucher ? voucher : "none", up_voucher ? up_voucher : "none");
        for (int k = 0; k < 5; k++)
            if (run.pokeballs[k] != balls_after[k])
                MISMATCH("trainer %d: Poke Ball type %d count %d (%d before), upstream %d", tt, k, run.pokeballs[k], balls_before[k], balls_after[k]);
        ball_cases += memcmp(balls_before, balls_after, sizeof balls_before) != 0;
        if (run.stubs[GC_STUB_TRAINER_REWARDS])
            MISMATCH("trainer %d: trainer rewards stub counted %u", tt, run.stubs[GC_STUB_TRAINER_REWARDS]);
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
    printf("first win unlocks: Plus %d, Premium %d; later wins: VOUCHER %d, VOUCHER_PLUS %d; no voucher %d (%d of them boss trainers)\n",
           first_plus, first_premium, later_voucher, later_voucher_plus, no_voucher, boss_no_voucher);
    printf("trainers with reward funcs %d: rewards EXP_SHARE %d, SUPER_EXP_CHARM %d, TERA_ORB %d; stack from zero %d, refused at the cap %d; %d texts compared\n",
           funcs, by_key[0], by_key[1], by_key[2], from_zero, at_cap, texts_compared);
    printf("stack full: %d itemStackFull messages compared, %d cases where the Poke Ball counts moved\n", full_stacks, ball_cases);
    printf("TOTAL mismatches: %d\n", bad);
    if (bad) return 1;
    if (!first_plus || !first_premium || !later_voucher || !later_voucher_plus || !no_voucher || !funcs || !at_cap || !from_zero ||
        !by_key[0] || !by_key[1] || !by_key[2] || !full_stacks || !ball_cases) {
        fprintf(stderr, "a kind of case went unexercised\n");
        return 3;
    }
    return 0;
}
