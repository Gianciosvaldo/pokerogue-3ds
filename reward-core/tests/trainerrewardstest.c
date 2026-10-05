#define _POSIX_C_SOURCE 200809L
/* C half of reward-core's trainer reward test (card 97): replays trainer-rewards.producer.test.ts's cases.
 *  F  pr_reward_victory_rewards and pr_reward_fixed_battle_settings (victory path), or the biome path alone: every
 *     queued item in order, every field of the reward settings (an upstream setting the port has no field for is a
 *     mismatch), and the RNG state after.
 *  V  pr_reward_trainer_victory_rewards: every queued phase in order (the money multiplier, each item; any other
 *     phase is a mismatch), the voucher unlock afterwards, the four voucher count changes and the RNG state.
 *  G  pr_reward_trainer_items: whether upstream threw, then every item, its species stat booster key, its holder
 *     and its stack, and the RNG state.
 * Prints each family's total of mismatching cases and what the cases exercised. Exit 1 on any mismatch, 2 on a
 * malformed or cut file (the producer's last line counts the cases), 3 when a family went unexercised. */
#include "pr/reward.h"
#include "pr_enums.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int line_no;
static char *tok(char **save)
{
    char *t = strtok_r(NULL, " \n", save);
    if (!t) { fprintf(stderr, "line %d: short line\n", line_no); exit(2); }
    return t;
}
static int inum(char **save)
{
    char *t = tok(save), *end;
    long v = strtol(t, &end, 10);
    if (*end) { fprintf(stderr, "line %d: not a number: %s\n", line_no, t); exit(2); }
    return (int)v;
}
static double dnum(char **save)
{
    char *t = tok(save), *end;
    double v = strtod(t, &end);
    if (*end) { fprintf(stderr, "line %d: not a number: %s\n", line_no, t); exit(2); }
    return v;
}
static void read_state(char **save, pr_rng *r)
{
    memset(r, 0, sizeof *r);
    r->c = dnum(save); r->s0 = dnum(save); r->s1 = dnum(save); r->s2 = dnum(save);
}
static bool same_state(const pr_rng *a, const pr_rng *b) { return a->c == b->c && a->s0 == b->s0 && a->s1 == b->s1 && a->s2 == b->s2; }
/* the separator between two queued entries */
static void sep(char **save, int i, int n)
{
    if (i + 1 < n && strcmp(tok(save), ";")) { fprintf(stderr, "line %d: expected ;\n", line_no); exit(2); }
}

static const char *const ssb_keys[] = {"LIGHT_BALL", "THICK_CLUB", "METAL_POWDER", "QUICK_POWDER", "DEEP_SEA_SCALE",
                                       "DEEP_SEA_TOOTH"};

static char why[256];
#define MISMATCH(...) do { if (ok) snprintf(why, sizeof why, __VA_ARGS__); ok = false; } while (0)

/* upstream's customModifierRewardSettings ("none", or a count and key=value tokens) into a pr_reward_custom with
 * upstream's defaults; an unknown key is reported */
static bool read_settings(char **save, bool *has, pr_reward_custom *cs, char *bad, size_t bad_n)
{
    memset(cs, 0, sizeof *cs);
    cs->reroll_multiplier = NAN;
    cs->allow_luck_upgrades = true;
    bad[0] = 0;
    char *t = tok(save);
    if (!strcmp(t, "none")) { *has = false; return true; }
    *has = true;
    const int n = atoi(t);
    for (int i = 0; i < n; i++) {
        char *kv = tok(save), *eq = strchr(kv, '=');
        if (!eq) { fprintf(stderr, "line %d: bad setting %s\n", line_no, kv); exit(2); }
        *eq = 0;
        const char *v = eq + 1;
        if (!strcmp(kv, "guaranteedModifierTiers")) {
            if (*v++ != '[') { fprintf(stderr, "line %d: bad tiers\n", line_no); exit(2); }
            while (*v && *v != ']') {
                char *end;
                cs->tiers[cs->n_tiers++] = (int8_t)strtol(v, &end, 10);
                v = *end == ',' ? end + 1 : end;
                if (cs->n_tiers == PR_REWARD_MAX_OPTIONS) break;
            }
        } else if (!strcmp(kv, "allowLuckUpgrades") && (!strcmp(v, "true") || !strcmp(v, "false"))) {
            cs->allow_luck_upgrades = !strcmp(v, "true");
        } else if (!strcmp(kv, "fillRemaining") && (!strcmp(v, "true") || !strcmp(v, "false"))) {
            cs->fill_remaining = !strcmp(v, "true");
        } else if (!bad[0]) {
            snprintf(bad, bad_n, "%s=%s", kv, v);
        }
    }
    return !bad[0];
}
static bool same_settings(const pr_reward_custom *a, const pr_reward_custom *b)
{
    if (a->n_tiers != b->n_tiers || a->n_options != b->n_options || a->n_funcs != b->n_funcs) return false;
    for (int i = 0; i < a->n_tiers; i++)
        if (a->tiers[i] != b->tiers[i]) return false;
    return a->fill_remaining == b->fill_remaining && a->allow_luck_upgrades == b->allow_luck_upgrades
        && (a->reroll_multiplier == b->reroll_multiplier || (isnan(a->reroll_multiplier) && isnan(b->reroll_multiplier)));
}

int main(int argc, char **argv)
{
    FILE *f = fopen(argc > 1 ? argv[1] : "trainer-rewards.txt", "r");
    if (!f) { perror("cases"); return 2; }
    static char line[16384];
    int cases = 0, footer = -1, shown = 0;
    int nf = 0, nv = 0, ng = 0, bad_f = 0, bad_v = 0, bad_g = 0;
    /* what the cases exercised */
    int f_biome = 0, f_settings_victory = 0, f_settings_biome = 0, f_single_gen = 0, f_sg_settings = 0, f_draws = 0;
    int f_wave_settings[8] = {0};
    static const int settings_waves[8] = {25, 55, 95, 115, 145, 165, 195, 0};
    int v_funcs[3] = {0}, v_voucher = 0, v_voucher_up = 0, v_unlock = 0, v_none = 0;
    static const int func_trainers[3] = {TRAINER_TYPE_RIVAL, TRAINER_TYPE_RIVAL_2, TRAINER_TYPE_RIVAL_4};
    static const int gen_trainers[7] = {TRAINER_TYPE_RED, TRAINER_TYPE_STEVEN, TRAINER_TYPE_WALLACE, TRAINER_TYPE_KIERAN,
                                        TRAINER_TYPE_GUZMA_2, TRAINER_TYPE_RIVAL_5, TRAINER_TYPE_RIVAL_6};
    int g_cases[7] = {0}, g_given[7] = {0}, g_two = 0, g_throws = 0, g_other = 0;

    while (fgets(line, sizeof line, f)) {
        line_no++;
        char *save;
        char *t = strtok_r(line, " \n", &save);
        if (!t) continue;
        if (!strcmp(t, "#")) {
            if (!strcmp(tok(&save), "cases")) footer = inum(&save);
            continue;
        }
        bool ok = true;
        char family = t[0];
        cases++;
        if (!strcmp(t, "F")) {
            nf++;
            pr_run run;
            memset(&run, 0, sizeof run);
            run.mode = inum(&save);
            run.challenge_generation = (uint8_t)inum(&save);
            const int wave = inum(&save);
            run.offset_gym = inum(&save) != 0;
            const bool biome = inum(&save) != 0;
            pr_rng before, after;
            read_state(&save, &before);
            read_state(&save, &after);
            run.rng = before;
            /* the port's queue */
            uint16_t items[8];
            int n_items = 0;
            bool select = true, has_cs = false;
            pr_reward_custom cs;
            if (!biome) n_items = pr_reward_victory_rewards(&run, wave, items, &select);
            if (select) has_cs = pr_reward_fixed_battle_settings(&run, wave, biome, &cs);
            const int mine = n_items + (select ? 1 : 0);
            const int n = inum(&save);
            if (n != mine) MISMATCH("%d queued, upstream %d", mine, n);
            for (int i = 0; i < n; i++) {
                const char *what = tok(&save);
                if (!strcmp(what, "R")) {
                    const char *key = tok(&save);
                    if (i >= n_items || strcmp(pr_item_keys[items[i]], key))
                        MISMATCH("entry %d: upstream item %s", i, key);
                } else if (!strcmp(what, "S")) {
                    bool up_has;
                    pr_reward_custom up;
                    char bad[64];
                    if (!read_settings(&save, &up_has, &up, bad, sizeof bad)) MISMATCH("a setting the port has no field for: %s", bad);
                    if (i != n_items || !select) MISMATCH("entry %d: upstream reward screen", i);
                    else if (up_has != has_cs) MISMATCH("settings: port %d, upstream %d", has_cs, up_has);
                    else if (up_has && !same_settings(&cs, &up)) MISMATCH("settings differ");
                    if (up_has) {
                        if (biome) f_settings_biome++;
                        else f_settings_victory++;
                        if (run.challenge_generation) f_sg_settings++;
                        for (int w = 0; settings_waves[w]; w++)
                            if (settings_waves[w] == wave) f_wave_settings[w]++;
                    }
                } else {
                    fprintf(stderr, "line %d: unknown entry %s\n", line_no, what);
                    exit(2);
                }
                sep(&save, i, n);
            }
            if (!same_state(&run.rng, &after)) MISMATCH("RNG state after differs");
            f_biome += biome;
            f_single_gen += run.challenge_generation != 0;
            f_draws += !same_state(&before, &after);
            if (!ok) bad_f++;
        } else if (!strcmp(t, "V")) {
            nv++;
            const int tt = inum(&save);
            const bool unlocked = inum(&save) != 0, upgrade = inum(&save) != 0;
            pr_rng before, after;
            read_state(&save, &before);
            read_state(&save, &after);
            const bool unlocked_after = inum(&save) != 0;
            int delta[4];
            for (int k = 0; k < 4; k++) delta[k] = inum(&save);
            pr_reward_trainer_victory o;
            if (pr_reward_trainer_victory_rewards(tt, unlocked, upgrade, &o) < 0) {
                MISMATCH("trainer %d: no config in the port", tt);
                memset(&o, 0, sizeof o);
            }
            const int n = inum(&save);
            if (n != 1 + o.n_items) MISMATCH("trainer %d: %d phases, upstream %d", tt, 1 + o.n_items, n);
            for (int i = 0; i < n; i++) {
                const char *what = tok(&save);
                if (!strcmp(what, "M")) {
                    const double m = dnum(&save);
                    if (i != 0 || m != o.money_multiplier) MISMATCH("trainer %d: money phase %d x%g, port x%g", tt, i, m, o.money_multiplier);
                } else if (!strcmp(what, "R")) {
                    const char *key = tok(&save);
                    if (i < 1 || i > o.n_items || strcmp(pr_item_keys[o.items[i - 1]], key))
                        MISMATCH("trainer %d: phase %d upstream item %s", tt, i, key);
                } else {
                    MISMATCH("trainer %d: upstream phase %s", tt, what);
                }
                sep(&save, i, n);
            }
            if (unlocked_after != (unlocked || o.unlock_voucher)) MISMATCH("trainer %d: voucher unlocked after %d", tt, unlocked_after);
            for (int k = 0; k < 4; k++) {
                const int mine = o.unlock_voucher && o.voucher_type == k ? 1 : 0;
                if (delta[k] != mine) MISMATCH("trainer %d: voucher count %d +%d, port +%d", tt, k, delta[k], mine);
            }
            if (!same_state(&before, &after)) MISMATCH("trainer %d: upstream rolled", tt);
            for (int k = 0; k < 3; k++)
                if (tt == func_trainers[k] && o.n_items) v_funcs[k]++;
            for (int k = 0; k < o.n_items; k++) {
                if (o.items[k] == PR_ITEM_VOUCHER || o.items[k] == PR_ITEM_VOUCHER_PLUS || o.items[k] == PR_ITEM_VOUCHER_PREMIUM) {
                    if (upgrade) v_voucher_up++;
                    else v_voucher++;
                }
            }
            v_unlock += o.unlock_voucher;
            v_none += o.voucher_type < 0;
            if (!ok) bad_v++;
        } else if (!strcmp(t, "G")) {
            ng++;
            const int tt = inum(&save), n_party = inum(&save);
            if (n_party < 0 || n_party > 6) { fprintf(stderr, "line %d: party of %d\n", line_no, n_party); return 2; }
            pr_reward_enemy_mon party[6];
            for (int i = 0; i < n_party; i++) {
                party[i].species = (uint16_t)inum(&save);
                party[i].ability = (uint16_t)inum(&save);
                party[i].passive = (uint16_t)inum(&save);
            }
            pr_rng before, after;
            read_state(&save, &before);
            read_state(&save, &after);
            pr_owned_item out[2];
            const int mine = pr_reward_trainer_items(tt, party, n_party, out);
            const char *r = tok(&save);
            if (!strcmp(r, "T")) {
                tok(&save);
                g_throws++;
                if (mine != -1) MISMATCH("trainer %d (party %d): upstream threw, port %d items", tt, n_party, mine);
            } else {
                const int n = atoi(r);
                if (n != mine) MISMATCH("trainer %d (party %d): %d items, upstream %d", tt, n_party, mine, n);
                for (int i = 0; i < n; i++) {
                    const char *key = tok(&save), *arg = tok(&save);
                    const int owner = inum(&save), stack = inum(&save);
                    if (i >= mine || mine < 0) continue;
                    const char *my_arg = out[i].item == PR_ITEM_RARE_SPECIES_STAT_BOOSTER && out[i].arg >= 0 && out[i].arg < 6
                        ? ssb_keys[out[i].arg] : "-";
                    if (strcmp(pr_item_keys[out[i].item], key) || strcmp(my_arg, arg) || out[i].owner != owner
                        || out[i].stack != stack)
                        MISMATCH("trainer %d: item %d upstream %s %s on %d x%d, port %s %s on %d x%d", tt, i, key, arg,
                                 owner, stack, pr_item_keys[out[i].item], my_arg, out[i].owner, out[i].stack);
                }
                g_two += n == 2;
            }
            if (!same_state(&before, &after)) MISMATCH("trainer %d: upstream rolled", tt);
            bool known = false;
            for (int k = 0; k < 7; k++)
                if (tt == gen_trainers[k]) { known = true; g_cases[k]++; g_given[k] += mine > 0; }
            g_other += !known;
            if (!ok) bad_g++;
        } else {
            fprintf(stderr, "line %d: unknown case %s\n", line_no, t);
            return 2;
        }
        if (!ok && shown++ < 10) fprintf(stderr, "line %d (%c): %s\n", line_no, family, why);
    }
    fclose(f);
    if (footer != cases) { fprintf(stderr, "%d cases, the file's last line says %d: cut or malformed\n", cases, footer); return 2; }

    const int bad = bad_f + bad_v + bad_g;
    printf("cases %d: mismatching %d (fixed battles %d of %d, trainer victories %d of %d, trainer items %d of %d)\n",
           cases, bad, bad_f, nf, bad_v, nv, bad_g, ng);
    printf("fixed battles: %d biome path, %d with settings on the victory path and %d on the biome path; "
           "%d Single Generation (%d with its settings, %d drew)\n", f_biome, f_settings_victory, f_settings_biome,
           f_single_gen, f_sg_settings, f_draws);
    printf("  settings by wave:");
    for (int w = 0; settings_waves[w]; w++) printf(" %d:%d", settings_waves[w], f_wave_settings[w]);
    printf("\ntrainer victories: reward funcs RIVAL %d, RIVAL_2 %d, RIVAL_4 %d; voucher rewards %d plain, %d upgraded; "
           "%d unlocked now; %d trainers without a voucher\n", v_funcs[0], v_funcs[1], v_funcs[2], v_voucher,
           v_voucher_up, v_unlock, v_none);
    printf("trainer items (cases/with items): RED %d/%d STEVEN %d/%d WALLACE %d/%d KIERAN %d/%d GUZMA_2 %d/%d "
           "RIVAL_5 %d/%d RIVAL_6 %d/%d; %d with two items, %d where upstream threw, %d other trainers\n",
           g_cases[0], g_given[0], g_cases[1], g_given[1], g_cases[2], g_given[2], g_cases[3], g_given[3], g_cases[4],
           g_given[4], g_cases[5], g_given[5], g_cases[6], g_given[6], g_two, g_throws, g_other);
    printf("TOTAL mismatches: %d\n", bad);
    if (bad) return 1;
    bool covered = f_settings_victory && f_settings_biome && f_sg_settings && f_draws && v_voucher && v_voucher_up
        && v_unlock && v_none && g_two && g_throws && g_other;
    for (int w = 0; settings_waves[w]; w++) covered = covered && f_wave_settings[w];
    for (int k = 0; k < 3; k++) covered = covered && v_funcs[k];
    for (int k = 0; k < 7; k++) covered = covered && g_given[k];
    if (!covered) { fprintf(stderr, "a family went unexercised (a 0 above)\n"); return 3; }
    return 0;
}
