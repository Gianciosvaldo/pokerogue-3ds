/* Independent strict replay of the additive real-class Lure oracle. */
#include "pr/reward.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *input;
static int line_no, guards;
static char line[4096];
static void die(const char *s) { fprintf(stderr, "invalid trace line %d: %s\n", line_no, s); exit(2); }
static char *record(char kind)
{
    if (!fgets(line, sizeof line, input)) die("unexpected EOF");
    line_no++;
    if (!strchr(line, '\n') || line[0] != kind || (line[1] != ' ' && line[1] != '\n')) die("record order/length");
    return line + 1;
}
static double number(char **s)
{
    char *end; double n = strtod(*s, &end);
    if (end == *s || !isfinite(n)) die("number");
    *s = end; return n;
}
static int integer(char **s, int lo, int hi)
{
    double n = number(s); if (floor(n) != n || n < lo || n > hi) die("integer range"); return n;
}
static void end(char *s) { while (isspace((unsigned char)*s)) s++; if (*s) die("extra fields"); }
static int item_id(char **s)
{
    char name[64]; int used;
    if (sscanf(*s, " %63s%n", name, &used) != 1) die("item name");
    *s += used;
    if (!strcmp(name, "LURE")) return PR_ITEM_LURE;
    if (!strcmp(name, "SUPER_LURE")) return PR_ITEM_SUPER_LURE;
    if (!strcmp(name, "MAX_LURE")) return PR_ITEM_MAX_LURE;
    if (!strcmp(name, "HEALING_CHARM")) return PR_ITEM_HEALING_CHARM;
    if (!strcmp(name, "LEFTOVERS")) return PR_ITEM_LEFTOVERS;
    die("item name"); return 0;
}
static int family(int item) { return item == PR_ITEM_LURE ? 0 : item == PR_ITEM_SUPER_LURE ? 1 : item == PR_ITEM_MAX_LURE ? 2 : -1; }
static pr_owned_item read_item(void)
{
    char *s = record('H'); pr_owned_item it = {0}; it.item = item_id(&s);
    it.stack = integer(&s, 1, 2); it.owner = integer(&s, -1, 0); it.arg = integer(&s, 0, 0);
    it.battle_count = integer(&s, 0, 30); it.active = integer(&s, 0, 0); end(s);
    if ((family(it.item) >= 0 && (it.owner != -1 || !it.battle_count))
        || (family(it.item) < 0 && (it.stack != 1 || it.battle_count != 0))) die("item shape");
    return it;
}
static bool same(const pr_owned_item *a, const pr_owned_item *b)
{
    return a->item == b->item && a->stack == b->stack && a->owner == b->owner && a->arg == b->arg
        && a->battle_count == b->battle_count && a->active == b->active;
}
static int inventory(pr_reward_ctx *c, bool fill)
{
    char *s = record('I'); int n = integer(&s, 0, PR_REWARD_MAX_OWNED); end(s);
    int bad = !fill && n != c->n_items;
    for (int j = 0; j < n; j++) { pr_owned_item it = read_item();
        if (fill) c->items[j] = it;
        else if (j >= c->n_items || !same(&it, &c->items[j])) bad = 1;
    }
    if (fill) c->n_items = n;
    return bad;
}
static int compare_inventory(pr_reward_ctx *c, pr_reward_ctx *expected)
{
    inventory(expected, true);
    int bad = c->n_items != expected->n_items;
    for (int j = 0; j < expected->n_items; j++)
        if (j >= c->n_items || !same(&c->items[j], &expected->items[j])) bad = 1;
    return bad;
}
static void opaque_unchanged(const pr_reward_ctx *before, const pr_reward_ctx *after)
{
    pr_reward_ctx a = *before, b = *after;
    memset(a.items, 0, sizeof a.items); memset(b.items, 0, sizeof b.items); a.n_items = b.n_items = 0;
    if (memcmp(&a, &b, sizeof a)) die("non-inventory context mutated");
}
static void refusal(pr_reward_ctx c)
{
    pr_reward_ctx before = c; int expired = 123; double chance = 456;
    if (pr_reward_add_lure(&c, PR_ITEM_LURE) != PR_REWARD_LURE_UNSUPPORTED
        || pr_reward_lapse_lures(&c, &expired) || pr_reward_lure_chance(&c, 8, &chance)
        || expired != 123 || chance != 456 || memcmp(&before, &c, sizeof c)) die("guard mutation");
    guards++;
}
static void selftest(void)
{
    pr_reward_ctx c = {0};
    c.n_items = -1; refusal(c); c.n_items = PR_REWARD_MAX_OWNED + 1; refusal(c);
    c.n_items = 1; c.items[0] = (pr_owned_item){.item = PR_ITEM_LURE, .owner = -1, .stack = 1, .battle_count = 9};
    pr_reward_ctx base = c;
#define GUARD(field, value) do { c = base; c.items[0].field = value; refusal(c); } while (0)
    GUARD(item, PR_ITEM_COUNT_); GUARD(owner, 0); GUARD(owner, -2); GUARD(arg, 1);
    GUARD(stack, 0); GUARD(stack, -1); GUARD(stack, 3);
    GUARD(battle_count, 0); GUARD(battle_count, -1); GUARD(battle_count, 11);
    c = base; c.items[0].item = PR_ITEM_SUPER_LURE; c.items[0].battle_count = 16; refusal(c);
    c = base; c.items[0].item = PR_ITEM_MAX_LURE; c.items[0].battle_count = 31; refusal(c);
#undef GUARD
    c = base; pr_reward_ctx before = c; double chance = 456;
    if (pr_reward_add_lure(&c, PR_ITEM_LEFTOVERS) != PR_REWARD_LURE_UNSUPPORTED
        || pr_reward_add_lure(&c, -1) != PR_REWARD_LURE_UNSUPPORTED
        || pr_reward_lure_chance(&c, NAN, &chance) || pr_reward_lure_chance(&c, INFINITY, &chance)
        || pr_reward_lure_chance(&c, -1, &chance) || pr_reward_lure_chance(&c, 8, NULL)
        || chance != 456 || memcmp(&c, &before, sizeof c)) die("argument guards");
    guards += 6;
    if (pr_reward_add_lure(NULL, PR_ITEM_LURE) != PR_REWARD_LURE_UNSUPPORTED
        || pr_reward_lapse_lures(NULL, NULL) || pr_reward_lure_chance(NULL, 8, &chance)) die("null guards");
    guards += 3;
    c.n_items = PR_REWARD_MAX_OWNED;
    for (int j = 0; j < c.n_items; j++) c.items[j] = (pr_owned_item){.item = PR_ITEM_HEALING_CHARM, .owner = -1, .stack = 1};
    before = c;
    if (pr_reward_add_lure(&c, PR_ITEM_LURE) != PR_REWARD_LURE_UNSUPPORTED || memcmp(&before, &c, sizeof c)) die("capacity");
    guards++;
    c.items[0] = base.items[0];
    if (pr_reward_add_lure(&c, PR_ITEM_LURE) != PR_REWARD_LURE_REFRESHED || c.items[0].battle_count != 10
        || c.items[0].stack != 1 || c.n_items != PR_REWARD_MAX_OWNED) die("capacity refresh");
    before = c;
    if (pr_reward_add_lure(&c, PR_ITEM_LURE) != PR_REWARD_LURE_FULL || memcmp(&before, &c, sizeof c)) die("full refusal");
    guards += 2;
    c = base; c.items[0].stack = 2; c.items[0].battle_count = 1;
    c.items[1] = (pr_owned_item){.item = PR_ITEM_LEFTOVERS, .stack = 99, .owner = -1, .battle_count = 77, .active = true}; c.n_items = 2;
    if (!pr_reward_lure_chance(&c, 8, &chance) || chance != 2) die("stack-independent chance");
    int expired = -1;
    if (!pr_reward_lapse_lures(&c, &expired) || expired != 1 || c.n_items != 1 || c.items[0].battle_count != 77) die("opaque removal");
    printf("LURE guards=%d whole-context/refusal and capacity checks passed; opaque inventory/stack/expiry checks passed\n", guards);
}
int main(int argc, char **argv)
{
    selftest(); if (argc == 2 && !strcmp(argv[1], "--selftest")) return 0;
    if (argc != 2 || !(input = fopen(argv[1], "r"))) die("usage: luretest trace | --selftest");
    char *s = record('L'); integer(&s, 1, 1); int n = integer(&s, 120, 100000); end(s);
    int fails = 0, stats[18] = {0}; stats[0] = n;
    for (int i = 0; i < n; i++) {
        pr_run run = {0}; pr_reward_ctx c = {0}; c.run = &run; c.wave = 123; c.n_party = 1; memset(c.party, 0x42, sizeof c.party);
        s = record('C'); integer(&s, i, i); int item = item_id(&s), which = family(item);
        int ticks = integer(&s, 1, 35); double base = number(&s); end(s); if (which < 0 || base < 0) die("case shape");
        s = record('R'); double rng[4]; for (int j = 0; j < 4; j++) rng[j] = number(&s); end(s);
        run.rng = (pr_rng){.c = rng[0], .s0 = rng[1], .s1 = rng[2], .s2 = rng[3]};
        pr_run runBefore = run;
        inventory(&c, true); pr_reward_ctx oracle = c; int duplicate = 0;
        for (int j = 0; j < c.n_items; j++) if (family(c.items[j].item) >= 0) {
            stats[16] += c.items[j].stack == 2;
            for (int k = 0; k < j; k++) if (c.items[k].item == c.items[j].item) duplicate = 1;
        }
        stats[15] += duplicate;
        pr_reward_ctx before = c; int status = pr_reward_add_lure(&c, item); opaque_unchanged(&before, &c);
        if (status < PR_REWARD_LURE_FULL || status > PR_REWARD_LURE_REFRESHED) die("unsupported generated case");
        s = record('A'); int expected = integer(&s, 1, 3), sound = integer(&s, 0, 1); double expectedChance = number(&s); end(s);
        stats[(expected == PR_REWARD_LURE_ADDED ? 3 : expected == PR_REWARD_LURE_REFRESHED ? 6 : 9) + which]++;
        double chance = -1; if (!pr_reward_lure_chance(&c, base, &chance)) die("chance unsupported");
        int bad = status != expected || sound != (status == PR_REWARD_LURE_REFRESHED) || chance != expectedChance;
        bad |= compare_inventory(&c, &oracle);
        for (int tick = 0; tick < ticks; tick++) {
            before = c; for (int j = 0; j < oracle.n_items; j++) if (family(oracle.items[j].item) >= 0 && oracle.items[j].battle_count == 1) stats[12 + family(oracle.items[j].item)]++;
            int removed = -1; if (!pr_reward_lapse_lures(&c, &removed)) die("lapse unsupported"); opaque_unchanged(&before, &c);
            s = record('T'); integer(&s, tick, tick); int expectedRemoved = integer(&s, 0, PR_REWARD_MAX_OWNED);
            expectedChance = number(&s); end(s);
            if (!pr_reward_lure_chance(&c, base, &chance)) die("chance unsupported");
            bad |= removed != expectedRemoved || chance != expectedChance; bad |= compare_inventory(&c, &oracle); stats[17]++;
        }
        s = record('E'); integer(&s, i, i);
        double actualRng[4] = {run.rng.c, run.rng.s0, run.rng.s1, run.rng.s2};
        for (int j = 0; j < 4; j++) if (number(&s) != actualRng[j]) bad = 1;
        end(s);
        if (memcmp(&run, &runBefore, sizeof run)) bad = 1;
        if (bad) { if (fails < 8) fprintf(stderr, "FAIL LURE CASE %d\n", i); fails++; }
    }
    s = record('Z'); for (int j = 0; j < 18; j++) if (integer(&s, 0, 10000000) != stats[j]) die("footer counts"); end(s);
    if (fgets(line, sizeof line, input)) die("trailing record");
    fclose(input);
    printf("LURE cases=%d identical=%d failed=%d skipped=0 thrown=0 added=%d/%d/%d refreshed=%d/%d/%d full=%d/%d/%d expired=%d/%d/%d duplicate=%d stackTwo=%d ticks=%d\n",
        n, n-fails, fails, stats[3],stats[4],stats[5],stats[6],stats[7],stats[8],stats[9],stats[10],stats[11],stats[12],stats[13],stats[14],stats[15],stats[16],stats[17]);
    return fails ? 1 : 0;
}
