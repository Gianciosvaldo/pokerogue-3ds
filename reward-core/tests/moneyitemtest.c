/* Independent strict replay of the additive real-class money-item oracle. */
#include "pr/reward.h"
#include "pr_data.h"
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
    if (!strcmp(name, "NUGGET")) return PR_ITEM_NUGGET;
    if (!strcmp(name, "BIG_NUGGET")) return PR_ITEM_BIG_NUGGET;
    if (!strcmp(name, "RELIC_GOLD")) return PR_ITEM_RELIC_GOLD;
    if (!strcmp(name, "AMULET_COIN")) return PR_ITEM_AMULET_COIN;
    if (!strcmp(name, "HEALING_CHARM")) return PR_ITEM_HEALING_CHARM;
    if (!strcmp(name, "LEFTOVERS")) return PR_ITEM_LEFTOVERS;
    die("item name"); return 0;
}
#define MAX_SAFE_INTEGER 9007199254740991.0
static int family(int item) { return item == PR_ITEM_NUGGET ? 0 : item == PR_ITEM_BIG_NUGGET ? 1 : item == PR_ITEM_RELIC_GOLD ? 2 : -1; }
static pr_owned_item read_item(void)
{
    char *s = record('H'); pr_owned_item it = {0}; it.item = item_id(&s);
    if (family(it.item) >= 0) die("consumable in inventory");
    it.stack = integer(&s, 1, it.item == PR_ITEM_AMULET_COIN ? 5 : 1); it.owner = integer(&s, -1, 5);
    it.arg = integer(&s, 0, 0); it.battle_count = integer(&s, 0, 0); it.active = integer(&s, 0, 0); end(s);
    if (it.item != PR_ITEM_LEFTOVERS && it.owner != -1) die("global owner");
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
        if (it.owner >= c->n_party) die("holder");
    }
    if (fill) c->n_items = n;
    return bad;
}
static pr_reward_ctx fixture(void)
{
    pr_reward_ctx c = {0}; c.wave = 10; c.n_party = 1;
    c.party[0].species = SPECIES_ID_BULBASAUR; c.party[0].fusion_species = -1;
    return c;
}
static void refusal(pr_reward_ctx c, int item, double money)
{
    pr_reward_ctx before = c; double old = money;
    pr_reward_money_item_result result = {.amount = 123, .credited = 456}, oldResult = result;
    if (pr_reward_apply_money_item(&c, item, &money, &result)
        || memcmp(&before, &c, sizeof c) || memcmp(&money, &old, sizeof old)
        || memcmp(&result, &oldResult, sizeof result)) die("refusal mutation");
    guards++;
}
static void selftest(void)
{
    pr_reward_ctx c = fixture(); refusal(c, -1, 10); refusal(c, PR_ITEM_LEFTOVERS, 10);
    refusal(c, PR_ITEM_COUNT_, 10); refusal(c, PR_ITEM_NUGGET, NAN); refusal(c, PR_ITEM_NUGGET, INFINITY);
    refusal(c, PR_ITEM_NUGGET, -1); refusal(c, PR_ITEM_NUGGET, MAX_SAFE_INTEGER + 1);
#define GUARD(field, value) do { c = fixture(); c.field = value; refusal(c, PR_ITEM_NUGGET, 10); } while (0)
    GUARD(wave, 0); GUARD(wave, -1); GUARD(wave, 10001); GUARD(n_party, 0); GUARD(n_party, 7);
    GUARD(n_items, -1); GUARD(n_items, PR_REWARD_MAX_OWNED + 1);
    GUARD(party[0].species, 0); GUARD(party[0].species, PR_SPECIES_COUNT);
    GUARD(party[0].fusion_species, 0); GUARD(party[0].fusion_species, -2); GUARD(party[0].fusion_species, PR_SPECIES_COUNT);
    GUARD(party[0].species, SPECIES_ID_GIMMIGHOUL); GUARD(party[0].fusion_species, SPECIES_ID_GIMMIGHOUL);
#undef GUARD
    c = fixture(); c.n_party = 2; c.party[1] = c.party[0]; c.party[1].fusion_species = SPECIES_ID_GIMMIGHOUL; refusal(c, PR_ITEM_BIG_NUGGET, 10);
    c = fixture(); c.n_party = 2; c.party[1] = c.party[0]; c.party[1].species = SPECIES_ID_GIMMIGHOUL; refusal(c, PR_ITEM_RELIC_GOLD, 10);
#define ITEM_GUARD(field, value) do { c = fixture(); c.n_items = 1; c.items[0] = (pr_owned_item){.item = PR_ITEM_AMULET_COIN, .owner = -1, .stack = 1}; c.items[0].field = value; refusal(c, PR_ITEM_NUGGET, 10); } while (0)
    ITEM_GUARD(item, PR_ITEM_COUNT_); ITEM_GUARD(owner, 0); ITEM_GUARD(owner, -2); ITEM_GUARD(arg, 1);
    ITEM_GUARD(battle_count, 1); ITEM_GUARD(stack, 0); ITEM_GUARD(stack, -1); ITEM_GUARD(stack, 6);
#undef ITEM_GUARD
    c = fixture(); double money = 10; pr_reward_money_item_result r = {.amount = 123, .credited = 456}, old = r;
    if (pr_reward_apply_money_item(NULL, PR_ITEM_NUGGET, &money, &r)
        || pr_reward_apply_money_item(&c, PR_ITEM_NUGGET, NULL, &r) || money != 10 || memcmp(&old, &r, sizeof r)) die("null guards");
    guards += 2;
    pr_reward_ctx before = c; money = MAX_SAFE_INTEGER;
    if (!pr_reward_apply_money_item(&c, PR_ITEM_NUGGET, &money, &r) || money != MAX_SAFE_INTEGER || r.credited != 0
        || r.amount != 270 || memcmp(&c, &before, sizeof c)) die("cap success");
    money = 0.25;
    if (!pr_reward_apply_money_item(&c, PR_ITEM_NUGGET, &money, NULL) || money != 270.25) die("optional output/fraction");
    c.n_items = PR_REWARD_MAX_OWNED;
    for (int j = 0; j < c.n_items; j++) c.items[j] = (pr_owned_item){.item = PR_ITEM_AMULET_COIN, .owner = -1, .stack = 5};
    c.wave = 10000; money = MAX_SAFE_INTEGER - 1; before = c;
    if (!pr_reward_apply_money_item(&c, PR_ITEM_RELIC_GOLD, &money, &r) || money != MAX_SAFE_INTEGER
        || r.credited != 1 || !isfinite(r.amount) || memcmp(&c, &before, sizeof c)) die("full inventory/large amount");
    printf("MONEYITEM guards=%d no-mutation refusals passed; cap/fraction/optional-result/full-inventory checks passed\n", guards);
}
int main(int argc, char **argv)
{
    selftest(); if (argc == 2 && !strcmp(argv[1], "--selftest")) return 0;
    if (argc != 2 || !(input = fopen(argv[1], "r"))) die("usage: moneyitemtest trace | --selftest");
    char *s = record('M'); integer(&s, 1, 1); int n = integer(&s, 120, 100000); end(s);
    int fails = 0, stats[12] = {0}; stats[0] = n;
    for (int i = 0; i < n; i++) {
        pr_run run = {0}; pr_reward_ctx c = {0}; c.run = &run; memset(c.party, 0x42, sizeof c.party);
        s = record('C'); integer(&s, i, i); int item = item_id(&s), which = family(item);
        c.wave = integer(&s, 1, 10000); double money = number(&s), beforeMoney = money; end(s);
        if (which < 0 || money < 0 || money > MAX_SAFE_INTEGER) die("case shape");
        s = record('P'); c.n_party = integer(&s, 1, 6); end(s);
        for (int j = 0; j < c.n_party; j++) {
            s = record('B'); double id = number(&s); if (floor(id) != id || id < 0 || id > UINT32_MAX) die("Pokemon ID");
            c.party[j].id = id; c.party[j].species = integer(&s, 1, PR_SPECIES_COUNT - 1);
            c.party[j].fusion_species = integer(&s, -1, PR_SPECIES_COUNT - 1); end(s);
            stats[9] += c.party[j].fusion_species >= 0;
            stats[10] += c.party[j].species == SPECIES_ID_GHOLDENGO || c.party[j].fusion_species == SPECIES_ID_GHOLDENGO;
        }
        inventory(&c, true); int reverse[PR_REWARD_MAX_OWNED], count = 0;
        for (int j = c.n_items - 1; j >= 0; j--) if (c.items[j].item == PR_ITEM_AMULET_COIN) reverse[count++] = c.items[j].stack;
        stats[7] += count >= 2; stats[8] += money != floor(money);
        s = record('R'); double rng[4]; for (int j = 0; j < 4; j++) rng[j] = number(&s); end(s);
        run.rng = (pr_rng){.c = rng[0], .s0 = rng[1], .s1 = rng[2], .s2 = rng[3]}; pr_run beforeRun = run;
        pr_reward_ctx before = c; pr_reward_money_item_result r = {0};
        bool result = pr_reward_apply_money_item(&c, item, &money, &r);
        if (!result) die("unsupported generated case");
        s = record('A'); int expected = integer(&s, 1, 1); double amount = number(&s), balance = number(&s), credited = number(&s);
        integer(&s, 1, 1); integer(&s, 1, 1); integer(&s, 1, 1); end(s);
        stats[3 + which]++; stats[6] += balance == MAX_SAFE_INTEGER;
        double multiplier = which == 0 ? 1 : which == 1 ? 2.5 : 10;
        stats[11] += pr_run_get_money_reward(c.wave, multiplier, reverse, count, false) != amount;
        int bad = expected != result || r.amount != amount || money != balance || r.credited != credited
            || credited != balance - beforeMoney || memcmp(&c, &before, sizeof c);
        bad |= inventory(&c, false);
        s = record('E'); integer(&s, i, i); double actualRng[4] = {run.rng.c, run.rng.s0, run.rng.s1, run.rng.s2};
        for (int j = 0; j < 4; j++) if (number(&s) != actualRng[j]) bad = 1;
        end(s); if (memcmp(&run, &beforeRun, sizeof run)) bad = 1;
        if (bad) { if (fails < 8) fprintf(stderr, "FAIL MONEYITEM CASE %d\n", i); fails++; }
    }
    s = record('Z'); for (int j = 0; j < 12; j++) if (integer(&s, 0, 10000000) != stats[j]) die("footer counts"); end(s);
    if (fgets(line, sizeof line, input)) die("trailing record");
    fclose(input);
    printf("MONEYITEM cases=%d identical=%d failed=%d skipped=0 thrown=0 Nugget/Big/Relic=%d/%d/%d capped=%d duplicateCoins=%d fractional=%d fusedMons=%d GholdengoMons=%d orderSensitive=%d\n",
        n, n-fails, fails, stats[3],stats[4],stats[5],stats[6],stats[7],stats[8],stats[9],stats[10],stats[11]);
    return fails ? 1 : 0;
}
