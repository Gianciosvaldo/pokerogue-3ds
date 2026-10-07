/* Additive bounded Mint application replay; compares every produced field.
 * Strict record order: every produced record and field is consumed and compared. */
#include "pr/reward.h"
#include "pr/battle.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#define MON_FIELDS 65
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
    for (int i = 0; i < MON_FIELDS; i++) {
        double max = i == 0 ? UINT32_MAX : INT32_MAX;
        if (v[i] != floor(v[i]) || v[i] < -1 || v[i] > max) die("mon integer range");
    }
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
    R(base_nature); R(custom_nature); m->has_explicit_nature = true;
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
    W(base_nature); W(custom_nature);
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
    double v[5]; for (int j = 0; j < 5; j++) v[j] = number(&s); end(s);
    if (v[0] != 1 || v[1] < -1 || v[1] > 5 || v[1] != floor(v[1])
        || v[2] != 0 || v[3] != 0 || v[4] != 1) die("inventory shape");
    item.stack = v[0]; item.owner = v[1]; item.arg = v[2];
    item.battle_count = v[3]; item.active = v[4]; return item;
}
static bool same_item(const pr_owned_item *a, const pr_owned_item *b)
{
    return a->item == b->item && a->arg == b->arg && a->stack == b->stack && a->owner == b->owner
        && a->battle_count == b->battle_count && a->active == b->active;
}
static int refusal_guards;
static pr_reward_ctx fixture(void)
{
    pr_reward_ctx c = {0}; c.n_party = 1;
    pr_reward_mon *m = &c.party[0];
    m->id = 42; m->species = SPECIES_ID_BULBASAUR; m->fusion_species = -1;
    m->level = 20; m->nature = m->base_nature = NATURE_HARDY;
    m->custom_nature = -1; m->has_explicit_nature = true;
    pr_battle bt = {0}; pr_pokemon p = {0}; p.species = m->species; p.level = m->level;
    pr_calculate_stats(&bt, &p); memcpy(m->stats, p.stats, sizeof m->stats);
    m->max_hp = p.stats[STAT_HP]; m->hp = m->max_hp / 2;
    return c;
}
static void refused(pr_reward_ctx *c, int nature, int target)
{
    pr_reward_ctx before = *c;
    if (pr_reward_apply_mint(c, nature, target) || memcmp(&before, c, sizeof *c)) die("refusal mutated context");
    refusal_guards++;
}
static void selftest(void)
{
    refusal_guards = 0;
    pr_reward_ctx c = fixture();
    if (pr_reward_apply_mint(NULL, 1, 0)) die("null");
    refused(&c, -1, 0); refused(&c, 25, 0); refused(&c, 1, -1); refused(&c, 1, 1);
    refused(&c, NATURE_HARDY, 0);
    c.party[0].has_explicit_nature = false; refused(&c, 1, 0);
    c = fixture(); c.party[0].custom_nature = 25; refused(&c, 1, 0);
    c = fixture(); c.party[0].base_nature = 25; refused(&c, 1, 0);
    c = fixture(); c.party[0].nature = 1; refused(&c, 2, 0);
    c = fixture(); c.party[0].ivs[0] = 32; refused(&c, 1, 0);
    c = fixture(); c.party[0].level = 256; refused(&c, 1, 0);
    c = fixture(); c.party[0].level = 65535; refused(&c, 1, 0);
    c = fixture(); c.party[0].species = -1; refused(&c, 1, 0);
    c = fixture(); c.party[0].form = -1; refused(&c, 1, 0);
    c = fixture(); c.party[0].form = 100; refused(&c, 1, 0);
    c = fixture(); c.party[0].fusion_species = 30000; refused(&c, 1, 0);
    c = fixture(); c.party[0].fusion_species = -2; refused(&c, 1, 0);
    c = fixture(); c.n_party = 7; refused(&c, 1, 0);
    c = fixture(); c.n_items = PR_REWARD_MAX_OWNED + 1; refused(&c, 1, 0);
    c = fixture(); c.n_items = 1; c.items[0].item = PR_ITEM_COUNT_; refused(&c, 1, 0);
    c = fixture(); c.challenges = 1u << CHALLENGES_FLIP_STAT; refused(&c, 1, 0);
    c = fixture(); pr_run run = {0}; run.mode = GAME_MODES_SPLICED_ENDLESS; c.run = &run; refused(&c, 1, 0);
    const int unsupported[] = {PR_ITEM_BASE_STAT_BOOSTER, PR_ITEM_SOUL_DEW,
        PR_ITEM_MYSTERY_ENCOUNTER_SHUCKLE_JUICE, PR_ITEM_MYSTERY_ENCOUNTER_OLD_GATEAU,
        PR_ITEM_MYSTERY_ENCOUNTER_MACHO_BRACE};
    for (unsigned j = 0; j < sizeof unsupported / sizeof *unsupported; j++) {
        c = fixture(); c.n_items = 1; c.items[0].item = unsupported[j]; c.items[0].owner = 0; c.items[0].stack = 1;
        refused(&c, 1, 0);
        c.items[0].owner = -1; refused(&c, 1, 0); /* upstream pokemonId=-1 wildcard */
    }
    c = fixture(); int old_hp = c.party[0].hp;
    for (int j = 0; j < PR_REWARD_MAX_NATURE_EVENTS; j++) {
        if (!pr_reward_apply_mint(&c, 1 + (j % 2), 0)) die("unlock request capacity");
        if (c.party[0].base_nature != NATURE_HARDY || c.party[0].hp != old_hp
            || c.nature_unlock_events[j].species != SPECIES_ID_BULBASAUR) die("base/HP/unlock request");
    }
    refused(&c, 3, 0);
    c = fixture(); c.party[0].hp = 0;
    if (!pr_reward_apply_mint(&c, 1, 0) || c.party[0].hp) die("fainted mint");
    printf("MINT host guards: pass (%d refusals preserve whole context; bounds, metadata, unsupported stats, capacity, base nature, HP)\n", refusal_guards);
}
int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "--selftest")) { selftest(); return 0; }
    bool mutate_base = argc == 3 && !strcmp(argv[2], "--mutate-base");
    bool mutate_unlock = argc == 3 && !strcmp(argv[2], "--mutate-unlock");
    bool mutate_stats = argc == 3 && !strcmp(argv[2], "--mutate-stats");
    if (argc < 2 || argc > 3 || (argc == 3 && !mutate_base && !mutate_unlock && !mutate_stats)) {
        fprintf(stderr, "usage: %s trace [--mutate-base|--mutate-unlock|--mutate-stats] | --selftest\n", argv[0]); return 2;
    }
    input = fopen(argv[1], "r"); if (!input) { perror(argv[1]); return 2; }
    double header[2]; vector(record('V'), header, 2);
    if (header[0] != 1 || header[1] < 100 || header[1] > 1000000 || header[1] != floor(header[1])) die("version/count");
    int cases = 0, failures = 0, accepted[25] = {0}, rejected[25] = {0}, fused = 0, fainted = 0, custom = 0, forms = 0, wonder = 0;
    while (cases < header[1]) {
        double c[4]; vector(record('C'), c, 4);
        if (c[0] != cases || c[1] < 1 || c[1] > 6 || c[1] != floor(c[1]) || c[2] < 0 || c[2] >= c[1]
            || c[2] != floor(c[2]) || c[3] < 0 || c[3] >= 25 || c[3] != floor(c[3])) die("case header");
        int target = c[2], nature = c[3];
        pr_run run = {0}; pr_reward_ctx ctx = {0}; ctx.run = &run; ctx.n_party = c[1];
        double r[4]; vector(record('R'), r, 4);
        run.rng.c = r[0]; run.rng.s0 = r[1]; run.rng.s1 = r[2]; run.rng.s2 = r[3];
        for (int j = 0; j < ctx.n_party; j++) {
            double v[MON_FIELDS], got[MON_FIELDS]; vector(record('P'), v, MON_FIELDS); read_mon(&ctx.party[j], v);
            mon_vector(&ctx.party[j], got);
            for (int k = 0; k < MON_FIELDS; k++) if (v[k] != got[k]) die("mon narrowing");
        }
        double count; vector(record('I'), &count, 1);
        if (count < 0 || count > PR_REWARD_MAX_OWNED || count != floor(count)) die("item count");
        ctx.n_items = count;
        for (int j = 0; j < ctx.n_items; j++) ctx.items[j] = read_item('H');
        const pr_reward_mon *m = &ctx.party[target];
        fused += m->fusion_species >= 0; fainted += m->hp == 0; custom += m->custom_nature >= 0;
        forms += m->form > 0 || m->fusion_form > 0;
        wonder += m->ability == ABILITY_ID_WONDER_GUARD || m->passive == ABILITY_ID_WONDER_GUARD;
        bool ok = true; const char *msg = NULL;
        bool result = pr_reward_apply(&ctx, PR_ITEM_MINT, nature, target, 0, &msg);
        if (mutate_base) ctx.party[target].base_nature = (ctx.party[target].base_nature + 1) % 25;
        if (mutate_unlock && ctx.n_nature_unlock_events) ctx.nature_unlock_events[0].species ^= 1;
        if (mutate_stats) ctx.party[target].stats[STAT_ATK]++;
        double a[2]; vector(record('A'), a, 2); int nq = a[1];
        if ((a[0] != 0 && a[0] != 1) || nq < 0 || nq > 1 || a[1] != nq || nq != a[0]) die("result/request shape");
        if (result != (bool)a[0] || ctx.n_nature_unlock_events != nq || (result ? msg != NULL : msg == NULL)) ok = false;
        (a[0] ? accepted : rejected)[nature]++;
        for (int j = 0; j < nq; j++) {
            double q[2]; vector(record('Q'), q, 2);
            if (ctx.nature_unlock_events[j].species != q[0] || ctx.nature_unlock_events[j].nature != q[1]) ok = false;
        }
        for (int j = 0; j < (int)c[1]; j++) {
            double v[MON_FIELDS], got[MON_FIELDS]; vector(record('T'), v, MON_FIELDS); mon_vector(&ctx.party[j], got);
            for (int k = 0; k < MON_FIELDS; k++) if (v[k] != got[k]) {
                if (failures < 8) fprintf(stderr, "field mismatch case=%d mon=%d field=%d got=%.17g expected=%.17g\n", cases, j, k, got[k], v[k]);
                ok = false;
            }
        }
        vector(record('J'), &count, 1);
        if (count < 0 || count > PR_REWARD_MAX_OWNED || count != floor(count)) die("after item count");
        if (ctx.n_items != (int)count || ctx.n_party != (int)c[1]) ok = false;
        for (int j = 0; j < (int)count; j++) { pr_owned_item expected = read_item('K'); if (!same_item(&ctx.items[j], &expected)) ok = false; }
        vector(record('S'), r, 4);
        if (run.rng.c != r[0] || run.rng.s0 != r[1] || run.rng.s1 != r[2] || run.rng.s2 != r[3]) ok = false;
        end(record('E'));
        if (!ok && failures++ < 8) fprintf(stderr, "FAIL MINT case=%d result=%d/%d requests=%u/%d\n", cases, result, (int)a[0], ctx.n_nature_unlock_events, nq);
        cases++;
    }
    double footer; vector(record('Z'), &footer, 1);
    if (footer != header[1] || fgetc(input) != EOF || ferror(input)) die("completion/extra records");
    fclose(input);
    printf("MINT cases=%d identical=%d mismatches=%d accepted=", cases, cases - failures, failures);
    for (int n = 0; n < 25; n++) printf("%s%d", n ? "/" : "", accepted[n]);
    printf(" rejected="); for (int n = 0; n < 25; n++) printf("%s%d", n ? "/" : "", rejected[n]);
    printf(" fusion=%d fainted=%d custom=%d forms=%d wonder=%d skipped=0\n", fused, fainted, custom, forms, wonder);
    if (failures) return 1;
    for (int n = 0; n < 25; n++) if (!accepted[n] || !rejected[n]) return 3;
    if (!fused || !fainted || !custom || !forms || !wonder) return 3;
    selftest(); return 0;
}
