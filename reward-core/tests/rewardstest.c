#define _POSIX_C_SOURCE 200809L
/* C half of reward-core's reward option test: replays the cases written by rewards.producer.test.ts. Each case
 * is a run state, a party, the player's items and up to three reward screens in a row (a first roll, a reroll,
 * a copy), and each screen's options (item, generated argument, tier, luck upgrade, English name) and the RNG
 * state after it are compared. A case whose options depend on something the port reports as unported is
 * counted apart ("unported"), never as a pass. */
#include "pr/reward.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *tok(char **save)
{
    char *t = strtok_r(NULL, " \n", save);
    if (!t) { fprintf(stderr, "short line\n"); exit(2); }
    return t;
}
static double num(char **save) { return strtod(tok(save), NULL); }
static void read_state(char **save, pr_rng *r)
{
    memset(r, 0, sizeof *r);
    r->c = num(save); r->s0 = num(save); r->s1 = num(save); r->s2 = num(save);
}
static bool same_state(const pr_rng *a, const pr_rng *b) { return a->c == b->c && a->s0 == b->s0 && a->s1 == b->s1 && a->s2 == b->s2; }
static int item_index(const char *key)
{
    for (int i = 0; i < PR_ITEM_COUNT_; i++)
        if (!strcmp(pr_item_keys[i], key)) return i;
    return -1;
}
/* the English name with spaces as '_', as the producer writes it */
static void name_of(pr_item_type t, char *out, size_t n)
{
    const int id = pr_item_type_name(t);
    const char *s = id >= 0 && pr_reward_strings[id] ? pr_reward_strings[id] : "?";
    size_t k = 0;
    for (; s[k] && k + 1 < n; k++) out[k] = s[k] == ' ' ? '_' : s[k];
    out[k] = 0;
}

int pr_reward_debug_case;
int pr_reward_weight(pr_reward_ctx *ctx, int item, int tier, int reroll_count);   /* src/pools.c */
static int caught_list[16], n_caught;
static bool caught(void *user, int species)
{
    (void)user;
    for (int i = 0; i < n_caught; i++)
        if (caught_list[i] == species) return true;
    return false;
}

int main(int argc, char **argv)
{
    FILE *f = fopen(argc > 1 ? argv[1] : "rewards-cases.txt", "r");
    if (!f) { perror("cases"); return 2; }
    static char line[1 << 16];
    int line_no = 0, cases = 0, pass = 0, fail = 0, unported = 0, threw = 0, screens = 0, shown = 0;
    static pr_run run;
    static pr_reward_ctx ctx;
    static pr_reward_thresholds th;
    static pr_reward_option out[PR_REWARD_MAX_OPTIONS], prev[PR_REWARD_MAX_OPTIONS];
    int n_prev = 0, member = 0;
    bool ok = true;
    char why[256] = "";
    int flag_hist[16] = {0};
    int opt_hist[5] = {0}, upgraded = 0, custom_screens = 0, reroll_screens = 0, copy_screens = 0, locked_screens = 0;
    while (fgets(line, sizeof line, f)) {
        line_no++;
        char *save;
        char *t = strtok_r(line, " \n", &save);
        if (!t || t[0] == '#') continue;
        if (!strcmp(t, "X")) { threw++; continue; }
        if (!strcmp(t, "R")) {
            memset(&run, 0, sizeof run);
            memset(&ctx, 0, sizeof ctx);
            memset(&th, 0, sizeof th);
            cases++;
            ok = true;
            why[0] = 0;
            n_prev = 0;
            member = 0;
            run.mode = (int)num(&save);
            pr_seed_from_ascii(&run.seed, tok(&save));
            ctx.run = &run;
            ctx.wave = (int)num(&save);
            ctx.challenges = (uint32_t)num(&save);
            run.has_challenges = ctx.challenges != 0;
            ctx.luck = (int)num(&save);
            ctx.biome = (int)num(&save);
            ctx.time_of_day = (int)num(&save);
            ctx.weather = (int)num(&save);
            ctx.unlocked_eviolite = num(&save) != 0;
            ctx.unlocked_mini_black_hole = num(&save) != 0;
            ctx.fusions_boosted = num(&save) != 0;
            n_caught = (int)num(&save);
            for (int i = 0; i < n_caught; i++) caught_list[i] = (int)num(&save);
            ctx.caught = caught;
            for (int i = 0; i < 5; i++) ctx.pokeball_counts[i] = (int)num(&save);
            ctx.n_party = (int)num(&save);
            continue;
        }
        if (!strcmp(t, "P")) {
            pr_reward_mon *m = &ctx.party[member++];
            memset(m, 0, sizeof *m);
            m->id = (uint32_t)num(&save);
            m->species = (int16_t)num(&save);
            m->form = (int16_t)num(&save);
            m->fusion_species = (int16_t)num(&save);
            m->fusion_form = (int16_t)num(&save);
            m->gender = (int8_t)num(&save);
            m->fusion_gender = (int8_t)num(&save);
            m->level = (uint8_t)num(&save);
            m->hp = (int32_t)num(&save);
            m->max_hp = (int32_t)num(&save);
            m->status = (uint8_t)num(&save);
            m->nature = (uint8_t)num(&save);
            m->tera_type = (int8_t)num(&save);
            m->friendship = (uint8_t)num(&save);
            m->ability = (uint16_t)num(&save);
            m->cur_ability = (uint16_t)num(&save);
            m->passive = (uint16_t)num(&save);
            m->can_apply_ability = num(&save) != 0;
            m->can_apply_passive = num(&save) != 0;
            m->pause_evolutions = num(&save) != 0;
            m->allowed_in_challenge = num(&save) != 0;
            m->can_toxic = num(&save) != 0;
            m->can_burn = num(&save) != 0;
            m->has_learnable_level_moves = num(&save) != 0;
            m->persistent_treasure = (int)num(&save);
            m->n_moves = (uint8_t)num(&save);
            for (int i = 0; i < m->n_moves; i++) {
                m->moves[i].move = (uint16_t)num(&save);
                m->moves[i].pp_used = (uint8_t)num(&save);
                m->moves[i].max_pp = (uint8_t)num(&save);
            }
            m->n_base_moves = (uint8_t)num(&save);
            for (int i = 0; i < m->n_base_moves; i++) m->base_moves[i] = (uint16_t)num(&save);
            const int nl = (int)num(&save);
            for (int i = 0; i < nl; i++) {
                const int mv = (int)num(&save);
                if (i < PR_REWARD_MAX_LEVEL_MOVES) m->level_moves[m->n_level_moves++] = (uint16_t)mv;
            }
            m->n_used_tms = (uint8_t)num(&save);
            for (int i = 0; i < m->n_used_tms; i++) m->used_tms[i] = (uint16_t)num(&save);
            const int na = (int)num(&save);
            for (int i = 0; i < na; i++) {
                const int ty = (int)num(&save);
                if (i < PR_REWARD_MAX_ATTACK_TYPES) m->attack_types[m->n_attack_types++] = (int8_t)ty;
            }
            continue;
        }
        if (!strcmp(t, "I")) {
            ctx.n_items = (int)num(&save);
            for (int i = 0; i < ctx.n_items; i++) {
                pr_owned_item *o = &ctx.items[i];
                const char *key = tok(&save);
                const int item = item_index(key);
                if (item < 0) { snprintf(why, sizeof why, "unknown item %s", key); ok = false; }
                o->item = (uint16_t)(item < 0 ? 0 : item);
                o->arg = (int16_t)num(&save);
                o->stack = (int32_t)num(&save);
                o->owner = (int8_t)num(&save);
                o->battle_count = (int16_t)num(&save);
                o->active = num(&save) != 0;
            }
            continue;
        }
        if (!strcmp(t, "S")) {
            static int8_t tiers[PR_REWARD_MAX_OPTIONS];
            static pr_reward_custom custom;
            pr_reward_select sel = {0};
            sel.reroll_count = (int)num(&save);
            sel.is_copy = num(&save) != 0;
            const bool lock = num(&save) != 0;
            const int nt = (int)num(&save);
            for (int i = 0; i < nt; i++) tiers[i] = (int8_t)num(&save);
            if (lock) { sel.locked_tiers = tiers; sel.n_locked_tiers = nt; }
            const bool has_custom = num(&save) != 0;
            memset(&custom, 0, sizeof custom);
            custom.n_tiers = (int)num(&save);
            for (int i = 0; i < custom.n_tiers; i++) custom.tiers[i] = (int8_t)num(&save);
            custom.n_funcs = (int)num(&save);
            for (int i = 0; i < custom.n_funcs; i++) {
                const char *key = tok(&save);
                const int item = item_index(key);
                custom.funcs[i] = (uint16_t)(item < 0 ? PR_ITEM_COUNT_ : item);
            }
            custom.fill_remaining = num(&save) != 0;
            custom.allow_luck_upgrades = num(&save) != 0;
            custom.reroll_multiplier = NAN;
            if (num(&save) != 0) {   /* copy(): the old options */
                custom.n_options = n_prev;
                memcpy(custom.options, prev, sizeof prev);
            }
            if (has_custom) sel.custom = &custom;
            read_state(&save, &run.rng);
            screens++;
            custom_screens += has_custom;
            reroll_screens += sel.reroll_count > 0;
            copy_screens += sel.is_copy;
            locked_screens += lock;
            if (getenv("DEBUG_CASE") && atoi(getenv("DEBUG_CASE")) == cases)
                for (int ti = 0; ti < 6; ti++)
                    for (int k = 0; k < pr_player_pool[ti].n; k++) {
                        const pr_pool_entry_p *e = &pr_player_pool_entries[pr_player_pool[ti].start + k];
                        fprintf(stderr, "W %d %d %s %d\n", ti, k, pr_item_keys[e->item],
                                e->weight >= 0 ? e->weight : pr_reward_weight(&ctx, e->item, ti, sel.reroll_count));
                    }
            pr_reward_debug_case = getenv("DEBUG_CASE") && atoi(getenv("DEBUG_CASE")) == cases;
            /* the screen */
            const int n = pr_reward_select_start(&ctx, &sel, &th, out);
            /* the upstream result */
            if (!fgets(line, sizeof line, f)) break;
            line_no++;
            t = strtok_r(line, " \n", &save);
            if (!t || strcmp(t, "O")) { fprintf(stderr, "line %d: no O line\n", line_no); return 2; }
            const int count = (int)num(&save);
            const int un = (int)num(&save);
            const int want_count = pr_reward_get_modifier_count(&ctx, sel.custom);
            if (ok && want_count != count) { snprintf(why, sizeof why, "count %d, upstream %d", want_count, count); ok = false; }
            if (ok && n != un) { snprintf(why, sizeof why, "screen %d: %d options, upstream %d", screens, n, un); ok = false; }
            for (int i = 0; i < un; i++) {
                const char *key = tok(&save);
                const int arg = (int)num(&save), tier = (int)num(&save), up = (int)num(&save);
                const char *name = tok(&save);
                if (!ok || i >= n) continue;
                char mine[128];
                name_of(out[i].type, mine, sizeof mine);
                if (item_index(key) != out[i].type.item || arg != out[i].type.arg || tier != out[i].tier ||
                    up != out[i].upgrade_count || strcmp(name, mine)) {
                    snprintf(why, sizeof why, "option %d: %s %d tier %d up %d (%s), upstream %s %d tier %d up %d (%s)", i,
                             pr_item_keys[out[i].type.item], out[i].type.arg, out[i].tier, out[i].upgrade_count, mine, key,
                             arg, tier, up, name);
                    ok = false;
                }
                if (tier >= 0 && tier < 5) opt_hist[tier]++;
                upgraded += up > 0;
            }
            pr_rng after;
            read_state(&save, &after);
            if (ok && !same_state(&run.rng, &after)) { snprintf(why, sizeof why, "screen %d: RNG state differs", screens); ok = false; }
            n_prev = n > 0 ? n : 0;
            memcpy(prev, out, sizeof out);
            continue;
        }
        if (!strcmp(t, "E")) {
            if (run.unported) {
                unported++;
                for (int b = 0; b < 16; b++) flag_hist[b] += (run.unported >> b) & 1;
            } else if (ok) {
                pass++;
            } else {
                fail++;
                if (shown++ < 12) fprintf(stderr, "case %d (line %d, mode %d wave %d): %s\n", cases, line_no, run.mode, ctx.wave, why);
            }
            continue;
        }
    }
    printf("cases %d: pass %d, fail %d, unported %d, upstream threw %d\n", cases, pass, fail, unported, threw);
    printf("screens %d (rerolls %d, locked %d, copies %d, custom %d); options by tier %d/%d/%d/%d/%d, luck-upgraded %d\n",
           screens, reroll_screens, locked_screens, copy_screens, custom_screens, opt_hist[0], opt_hist[1], opt_hist[2],
           opt_hist[3], opt_hist[4], upgraded);
    if (unported)
        printf("unported by reason: challenge hook %d, undefined upstream %d, table size %d, item func %d\n",
               flag_hist[8], flag_hist[2], flag_hist[9], flag_hist[10]);
    return fail ? 1 : 0;
}
