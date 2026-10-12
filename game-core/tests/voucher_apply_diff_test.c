#define _POSIX_C_SOURCE 200809L
/* C half of game-core's voucher application replay (tools/voucherapplydiff.sh; voucher-apply.producer.test.ts is the
 * upstream half). Upstream's AddVoucherModifierType / AddVoucherModifier is taken two ways, on random starting counts of
 * the four voucher types:
 *  - from the reward screen (SelectModifierPhase.applyModifier -> addModifier): here gc_port_apply_reward. Compared: that
 *    it was taken, the four counts afterwards, that no message was shown, that nothing was added to the run's items
 *    (a voucher is system data) and that the reward-apply stand-in wasn't counted
 *  - given by ModifierRewardPhase.doReward: here gc_reward_gain. Compared: the four counts afterwards and the text, as
 *    text, formatted from upstream's locale files (GC_LOCALES): "1× Egg Voucher Plus"
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

static const char *locale_file(void *ctx, const char *file, int fallback, size_t *len)
{
    (void)ctx;
    static struct { char name[64]; char *text; size_t len; } cache[8];
    static int n_cache;
    const char *dir = getenv("GC_LOCALES");
    if (fallback) return NULL;
    if (!dir) { fputs("GC_LOCALES must name upstream's locales/en folder (tools/voucherapplydiff.sh sets it)\n", stderr); exit(2); }
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

static char why[1024];
#define MISMATCH(...) do { if (ok) snprintf(why, sizeof why, __VA_ARGS__); ok = false; } while (0)

static void setup(const uint32_t counts[4])
{
    memset(&run, 0, sizeof run);
    gc_system_init_defaults(&sys);
    gc_run_new(&run, GAME_MODES_CLASSIC, "voucher-diff", 7);
    run.sys = &sys;
    memcpy(sys.voucher_counts, counts, 4 * sizeof counts[0]);
    gc_event e;
    while (gc_run_next_event(&run, &e)) {}
}

int main(int argc, char **argv)
{
    FILE *fp;
    if (argc != 2 || !(fp = fopen(argv[1], "r"))) { fprintf(stderr, "usage: voucher_apply_diff_test <trace>\n"); return 2; }
    static const pr_text_source src = {locale_file, NULL, "en"};
    static const char *const keys[3] = {"VOUCHER", "VOUCHER_PLUS", "VOUCHER_PREMIUM"};
    char line[1024];
    int cases = 0, bad = 0, declared = -1, shown = 0, by_key[3] = {0, 0, 0}, texts = 0, big = 0, picked = 0;
    while (fgets(line, sizeof line, fp)) {
        line_no++;
        char *save = NULL, *t = strtok_r(line, " \n", &save);
        if (!t) continue;
        if (!strcmp(t, "#")) {
            tok(&save);   /* "cases" */
            declared = (int)num(&save);
            continue;
        }
        if (strcmp(t, "K")) { fprintf(stderr, "line %d: unknown record %s\n", line_no, t); return 2; }
        char key[48];
        snprintf(key, sizeof key, "%s", tok(&save));
        uint32_t init[4];
        for (int k = 0; k < 4; k++) init[k] = (uint32_t)num(&save);
        int item = -1;
        for (int k = 0; k < 3; k++) if (!strcmp(key, keys[k])) item = k;
        if (item < 0) { fprintf(stderr, "line %d: key %s\n", line_no, key); return 2; }

        /* R: taken from the reward screen */
        if (!fgets(line, sizeof line, fp) || line[0] != 'R') { fprintf(stderr, "line %d: R record expected\n", line_no + 1); return 2; }
        line_no++;
        char *rs = NULL;
        strtok_r(line, " \n", &rs);
        const int up_applied = (int)num(&rs);
        char filter[160];
        snprintf(filter, sizeof filter, "%s", tok(&rs));
        uint32_t up_pick[4];
        for (int k = 0; k < 4; k++) up_pick[k] = (uint32_t)num(&rs);
        const int up_pick_texts = (int)num(&rs), up_modifiers = (int)num(&rs);

        /* G: given by ModifierRewardPhase */
        if (!fgets(line, sizeof line, fp) || line[0] != 'G') { fprintf(stderr, "line %d: G record expected\n", line_no + 1); return 2; }
        line_no++;
        char *gs = NULL;
        strtok_r(line, " \n", &gs);
        uint32_t up_give[4];
        for (int k = 0; k < 4; k++) up_give[k] = (uint32_t)num(&gs);
        const int up_give_texts = (int)num(&gs);
        char up_text[256];
        if (*gs == ' ') gs++;
        snprintf(up_text, sizeof up_text, "%s", gs);
        up_text[strcspn(up_text, "\n")] = 0;
        for (char *c = up_text; *c; c++) if (*c == '|') *c = '\n';
        if (!fgets(line, sizeof line, fp) || line[0] != 'E') { fprintf(stderr, "line %d: E record expected\n", line_no + 1); return 2; }
        line_no++;

        cases++;
        by_key[item]++;
        bool ok = true;

        setup(init);
        gc_offer o;
        memset(&o, 0, sizeof o);
        o.key = key;
        o.arg = -1;
        o.target = gc_reward_target(key);
        const char *why_not = "";
        const bool taken = gc_port_apply_reward(&run, &o, -1, -1, &why_not);
        int msgs = 0;
        gc_event ev;
        while (gc_run_next_event(&run, &ev)) msgs += ev.kind == GC_EV_MESSAGE;
        if (taken != (strcmp(filter, "OK") == 0 && up_applied)) MISMATCH("%s: taken %d, upstream applied %d (%s)", key, taken, up_applied, filter);
        for (int k = 0; k < 4; k++)
            if (sys.voucher_counts[k] != up_pick[k]) MISMATCH("%s picked: count %d is %u, upstream %u (from %u)", key, k, sys.voucher_counts[k], up_pick[k], init[k]);
        if (msgs != up_pick_texts) MISMATCH("%s picked: %d messages, upstream %d", key, msgs, up_pick_texts);
        if (run.n_items != up_modifiers) MISMATCH("%s picked: %d run items, upstream holds %d modifiers", key, run.n_items, up_modifiers);
        if (run.stubs[GC_STUB_REWARD_APPLY]) MISMATCH("%s picked: the reward-apply stand-in counted %u", key, run.stubs[GC_STUB_REWARD_APPLY]);

        setup(init);
        gc_reward_gain(&run, key);
        char text[256];
        text[0] = 0;
        msgs = 0;
        while (gc_run_next_event(&run, &ev)) {
            if (ev.kind != GC_EV_MESSAGE || !ev.msg.key) continue;
            if (msgs++ == 0) pr_msg_format(&ev.msg, &src, text, sizeof text);
        }
        for (int k = 0; k < 4; k++)
            if (sys.voucher_counts[k] != up_give[k]) MISMATCH("%s given: count %d is %u, upstream %u (from %u)", key, k, sys.voucher_counts[k], up_give[k], init[k]);
        if (msgs != up_give_texts) MISMATCH("%s given: %d messages, upstream %d", key, msgs, up_give_texts);
        else if (strcmp(text, up_text)) MISMATCH("%s given: text '%s', upstream '%s'", key, text, up_text);
        else texts++;
        if (run.n_items != 0) MISMATCH("%s given: %d run items", key, run.n_items);

        big += init[item] >= 99;
        picked += taken;
        if (!ok) {
            bad++;
            if (shown++ < 10) printf("FAIL line %d: %s\n", line_no, why);
        }
    }
    fclose(fp);
    if (declared != cases || cases < 1) { fprintf(stderr, "cut or malformed trace: %d cases read, %d declared\n", cases, declared); return 2; }
    printf("cases %d: mismatching %d\n", cases, bad);
    printf("VOUCHER %d, VOUCHER_PLUS %d, VOUCHER_PREMIUM %d; picked %d; %d texts compared; %d starting counts of 99 or more\n",
           by_key[0], by_key[1], by_key[2], picked, texts, big);
    printf("TOTAL mismatches: %d\n", bad);
    if (bad) return 1;
    if (!by_key[0] || !by_key[1] || !by_key[2] || !picked || !texts || !big) {
        fprintf(stderr, "a kind of case went unexercised\n");
        return 3;
    }
    return 0;
}
