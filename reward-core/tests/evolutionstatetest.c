/* Additive full-state replay for the application stage, not EvolutionPhase execution.
 * Strict record order: every produced record and field is consumed and compared. */
#include "pr/reward.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#define MON_FIELDS 63
static FILE *input;
static char line[4096];
static int line_no;
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
    char *end; double v = strtod(*s, &end);
    if (end == *s || !isfinite(v)) die("number");
    *s = end; return v;
}
static void end(char *s) { while (isspace((unsigned char)*s)) s++; if (*s) die("extra fields"); }
static void vector(char *s, double *v, int n) { for (int i = 0; i < n; i++) v[i] = number(&s); end(s); }
static void read_mon(pr_reward_mon *m, const double *v)
{
    int k = 0;
#define R(f) m->f = v[k++]
    R(id); R(species); R(form); R(fusion_species); R(fusion_form); R(gender); R(fusion_gender);
    R(level); R(exp); R(hp); R(max_hp); R(status); R(nature); R(tera_type); R(friendship);
    R(ability); R(cur_ability); R(passive); R(pause_evolutions); R(allowed_in_challenge);
    R(ability_index); R(fusion_ability_index); R(shiny); R(fusion_shiny); R(variant); R(fusion_variant);
    R(luck); R(fusion_luck); R(passive_flag); R(n_moves);
    for (int j = 0; j < 4; j++) { R(moves[j].move); R(moves[j].pp_used); R(moves[j].pp_up); R(moves[j].max_pp); }
    R(n_base_moves); for (int j = 0; j < 4; j++) { R(base_moves[j]); }
    for (int j = 0; j < 6; j++) { R(ivs[j]); }
    for (int j = 0; j < 6; j++) { R(stats[j]); }
#undef R
    if (k != MON_FIELDS || m->n_moves > 4 || m->n_base_moves > 4) die("mon shape");
}
static void mon_vector(const pr_reward_mon *m, double *v)
{
    int k = 0;
#define W(f) v[k++] = m->f
    W(id); W(species); W(form); W(fusion_species); W(fusion_form); W(gender); W(fusion_gender);
    W(level); W(exp); W(hp); W(max_hp); W(status); W(nature); W(tera_type); W(friendship);
    W(ability); W(cur_ability); W(passive); W(pause_evolutions); W(allowed_in_challenge);
    W(ability_index); W(fusion_ability_index); W(shiny); W(fusion_shiny); W(variant); W(fusion_variant);
    W(luck); W(fusion_luck); W(passive_flag); W(n_moves);
    for (int j = 0; j < 4; j++) { W(moves[j].move); W(moves[j].pp_used); W(moves[j].pp_up); W(moves[j].max_pp); }
    W(n_base_moves); for (int j = 0; j < 4; j++) { W(base_moves[j]); }
    for (int j = 0; j < 6; j++) { W(ivs[j]); }
    for (int j = 0; j < 6; j++) { W(stats[j]); }
#undef W
    if (k != MON_FIELDS) die("mon fields");
}
static uint16_t item_id(const char *s)
{
    if (!strcmp(s, "LEFTOVERS")) return PR_ITEM_LEFTOVERS;
    if (!strcmp(s, "HEALING_CHARM")) return PR_ITEM_HEALING_CHARM;
    die("item class"); return 0;
}
static pr_owned_item read_item(char kind)
{
    char name[64]; int n = 0; char *s = record(kind);
    if (sscanf(s, " %63s%n", name, &n) != 1) die("item");
    s += n; pr_owned_item item = {0}; item.item = item_id(name);
    item.stack = number(&s); item.owner = number(&s); item.arg = number(&s);
    item.battle_count = number(&s); item.active = number(&s); end(s); return item;
}
static const char *form_key(int key)
{
    if (key < 0) return "-";
    return pr_reward_strings[key][0] ? pr_reward_strings[key] : "_";
}
static bool same_item(const pr_owned_item *a, const pr_owned_item *b)
{
    return a->item == b->item && a->arg == b->arg && a->stack == b->stack && a->owner == b->owner
        && a->battle_count == b->battle_count && a->active == b->active;
}
static void overflow_test(void)
{
    pr_reward_ctx ctx = {0}; ctx.n_party = 1;
    ctx.party[0].species = SPECIES_ID_VULPIX; ctx.party[0].level = 10;
    ctx.party[0].fusion_species = -1; ctx.party[0].allowed_in_challenge = true;
    for (int i = 0; i < PR_REWARD_MAX_EVOLUTION_EVENTS; i++)
        if (!pr_reward_apply(&ctx, PR_ITEM_EVOLUTION_ITEM, 5, 0, 0, NULL)) die("queue capacity application");
    pr_reward_evolution_event before[PR_REWARD_MAX_EVOLUTION_EVENTS];
    memcpy(before, ctx.evolution_events, sizeof before);
    if (pr_reward_apply(&ctx, PR_ITEM_EVOLUTION_ITEM, 5, 0, 0, NULL)
        || !ctx.evolution_events_overflow || ctx.n_evolution_events != PR_REWARD_MAX_EVOLUTION_EVENTS
        || memcmp(before, ctx.evolution_events, sizeof before)) die("queue overflow changed/accepted");
    puts("EVOSTATE host queue capacity/overflow: pass");
}
int main(int argc, char **argv)
{
    if (argc < 2 || argc > 3) { fprintf(stderr, "usage: %s trace [--mutate-species|--mutate-ability]\n", argv[0]); return 2; }
    bool mutate_species = argc == 3 && !strcmp(argv[2], "--mutate-species");
    bool mutate_ability = argc == 3 && !strcmp(argv[2], "--mutate-ability");
    if (argc == 3 && !mutate_species && !mutate_ability) return 2;
    input = fopen(argv[1], "r"); if (!input) { perror(argv[1]); return 2; }
    double header[2]; vector(record('V'), header, 2);
    if (header[0] != 1 || header[1] < 1 || header[1] != floor(header[1])) die("version/count");
    int cases = 0, failures = 0, accepted[4] = {0}, rejected[4] = {0}, fused = 0;
    while (cases < header[1]) {
        double c[4]; vector(record('C'), c, 4);
        if (c[0] != cases || c[1] < 1 || c[1] > 6 || c[2] < 0 || c[2] >= c[1]) die("case header");
        int target = c[2], stone = c[3]; int family = stone == 5 ? 0 : stone == 6 ? 1 : stone == 7 ? 2 : stone == 10 ? 3 : -1;
        if (family < 0) die("stone family");
        pr_run run = {0}; pr_reward_ctx ctx = {0}; ctx.run = &run; ctx.n_party = c[1];
        double r[4]; vector(record('R'), r, 4);
        run.rng.c = r[0]; run.rng.s0 = r[1]; run.rng.s1 = r[2]; run.rng.s2 = r[3];
        for (int j = 0; j < ctx.n_party; j++) { double v[MON_FIELDS]; vector(record('P'), v, MON_FIELDS); read_mon(&ctx.party[j], v); }
        double count; vector(record('I'), &count, 1); ctx.n_items = count;
        if (count < 0 || count > PR_REWARD_MAX_OWNED) die("item count");
        for (int j = 0; j < ctx.n_items; j++) ctx.items[j] = read_item('H');
        bool ok = true; const char *msg = NULL;
        bool result = pr_reward_apply(&ctx, PR_ITEM_EVOLUTION_ITEM, stone, target, 0, &msg);
        if (mutate_species) ctx.party[target].species ^= 1;
        if (mutate_ability) ctx.party[target].ability ^= 1;
        double a[2]; vector(record('A'), a, 2); int nq = a[1];
        if (nq < 0 || nq > PR_REWARD_MAX_EVOLUTION_EVENTS) die("queue count");
        if (result != (bool)a[0] || ctx.n_evolution_events != nq || ctx.evolution_events_overflow) ok = false;
        (a[0] ? accepted : rejected)[family]++;
        for (int j = 0; j < nq; j++) {
            char pre[128], evo[128]; unsigned id, primary, species, item; int last, level, fusion, n = 0;
            char *s = record('Q');
            if (sscanf(s, " %u %u %u %u %127s %127s %d %d %d%n", &id, &primary, &species, &item, pre, evo, &last, &level, &fusion, &n) != 9) die("queue fields");
            end(s + n); fused += fusion;
            const pr_reward_evolution_event *e = &ctx.evolution_events[j];
            if (j >= ctx.n_evolution_events || e->pokemon_id != id || e->primary_species != primary
                || e->target_species != species || e->item != item || e->last_level != last
                || e->evolution_level != level || e->fusion != (bool)fusion
                || strcmp(form_key(e->pre_form_key), pre) || strcmp(form_key(e->evo_form_key), evo)) ok = false;
        }
        for (int j = 0; j < (int)c[1]; j++) {
            double v[MON_FIELDS], got[MON_FIELDS]; vector(record('T'), v, MON_FIELDS); mon_vector(&ctx.party[j], got);
            for (int k = 0; k < MON_FIELDS; k++) if (v[k] != got[k]) ok = false;
        }
        vector(record('J'), &count, 1);
        if (count < 0 || count > PR_REWARD_MAX_OWNED) die("after item count");
        if (ctx.n_items != (int)count || ctx.n_party != (int)c[1]) ok = false;
        for (int j = 0; j < (int)count; j++) { pr_owned_item expected = read_item('K'); if (!same_item(&ctx.items[j], &expected)) ok = false; }
        vector(record('S'), r, 4);
        if (run.rng.c != r[0] || run.rng.s0 != r[1] || run.rng.s1 != r[2] || run.rng.s2 != r[3]) ok = false;
        end(record('E'));
        if (!ok && failures++ < 12) fprintf(stderr, "FAIL EVOSTATE case=%d result=%d/%d queue=%u/%d\n", cases, result, (int)a[0], ctx.n_evolution_events, nq);
        cases++;
    }
    double footer; vector(record('Z'), &footer, 1);
    if (footer != header[1] || fgetc(input) != EOF || ferror(input)) die("completion/extra records");
    fclose(input);
    printf("EVOSTATE cases=%d identical=%d mismatches=%d accepted=%d/%d/%d/%d rejected=%d/%d/%d/%d fusionRequests=%d skipped=0\n",
        cases, cases - failures, failures, accepted[0], accepted[1], accepted[2], accepted[3], rejected[0], rejected[1], rejected[2], rejected[3], fused);
    if (!mutate_species && !mutate_ability && !failures) overflow_test();
    return failures ? 1 : 0;
}
