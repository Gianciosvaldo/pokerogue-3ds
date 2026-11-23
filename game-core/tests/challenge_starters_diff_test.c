#define _POSIX_C_SOURCE 200809L
/* Challenge Mode's starter rules against upstream at 8555c08: replays tests/challenge-starters.producer.test.ts's trace (Fresh Start through the
 * real getRunValueLimit, getSpeciesStarterValue, checkStarterValidForChallenge, getStarterData and SelectStarterPhase.initBattle) through
 * game-core:
 *  - gc_starter_budget(GAME_MODES_CHALLENGE) is the cost limit;
 *  - gc_challenge_starter_cost is the starter's value (Fresh Start: the base cost);
 *  - gc_challenge_starter_valid is checkStarterValidForChallenge, with and without soft;
 *  - gc_challenge_starter_view is getStarterData's copy of the profile: caught attributes, natures, abilities, passive, value reduction, egg
 *    moves, IVs;
 *  - the party gc_starters_build_party makes from the same starters on the same Phaser RNG state matches upstream's after its
 *    STARTER_MODIFY: id, species, form, level, nature, ability index and ability, passive, gender, luck, tera type, HP, the six stats, the six
 *    IVs, the four moves, and the RNG state afterwards.
 * And what reward-core reads of the challenges, through game-core's reward context: the Eviolite's and the Mini Black Hole's pool weights
 * (Fresh Start), then a Mint and a vitamin on the first member through gc_port_apply_reward (the stats Flip Stat recalculates).
 * Usage: challenge_starters_diff_test <trace> */
#include "../src/internal.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_run run;
static gc_system sys;
static long checks, failures, cases, queries, members, fresh_cases, views_changed, soft_only;
static long chart_values, effectiveness_values, passive_values, enemy_passives;
static int inverse_v, flip_v, passives_v;   /* the case's Inverse Battle, Flip Stat and Passives values */
static char exp_e[6][400], exp_p[6][40];
static char exp_y[32][80];
static char exp_f[200];
static long fusion_stats, reward_weights, reward_applies;
static char exp_r[80], exp_n[200], exp_u[200];   /* the Eviolite's and Mini Black Hole's weights, the Mint's and the vitamin's results */
static int got_y;
static pr_battle bt;   /* the battle rules are read from the challenge fields of a battle: nothing else of it is looked at */

static void set_battle(int wave)
{
    memset(&bt, 0, sizeof bt);
    bt.game_mode = GAME_MODES_CHALLENGE;
    bt.wave_index = wave;
    bt.challenge_flags = run.run.challenge_flags;
    bt.challenge_flags_known = true;
    bt.challenge_passives = run.run.challenge_passives;
}

#define FAIL(...) do { if (failures < 40) { fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } failures++; } while (0)
#define CHECK(c, ...) do { checks++; if (!(c)) FAIL(__VA_ARGS__); } while (0)

static char *tok(char **save)
{
    char *t = strtok_r(NULL, " \n", save);
    if (!t) { fprintf(stderr, "short line\n"); exit(2); }
    return t;
}
static double num(char **save) { return strtod(tok(save), NULL); }
static long inum(char **save) { return strtol(tok(save), NULL, 10); }
static uint64_t hex(char **save) { return strtoull(tok(save), NULL, 16); }
static pr_rng read_rng(char **save)
{
    pr_rng r = {0};
    r.c = num(save);
    r.s0 = num(save);
    r.s1 = num(save);
    r.s2 = num(save);
    return r;
}
static bool same_rng(const pr_rng *a, const pr_rng *b) { return a->c == b->c && a->s0 == b->s0 && a->s1 == b->s1 && a->s2 == b->s2; }

/* a PLAYER pool entry's weight in the thresholds regenerateModifierPoolThresholds made (0 when it isn't in them) */
static long pool_weight(const pr_reward_thresholds *th, int tier, int item)
{
    for (int i = 0; i < th->n[tier]; i++)
        if (pr_player_pool_entries[pr_player_pool[tier].start + th->index[tier][i]].item == item)
            return th->total[tier][i] - (i ? th->total[tier][i - 1] : 0);
    return 0;
}

static void replay_party(int idx, int fresh, int n, pr_rng before, pr_rng after, const gc_starter *team, char exp[][400])
{
    char seed[48];
    snprintf(seed, sizeof seed, "challenge-diff-%d", idx);
    memset(&run, 0, sizeof run);
    gc_system_init_defaults(&sys);
    gc_run_new(&run, GAME_MODES_CHALLENGE, seed, 7);
    run.sys = &sys;
    gc_challenge_set set;
    memset(&set, 0, sizeof set);
    set.value[CHALLENGES_FRESH_START] = (uint8_t)fresh;
    set.value[CHALLENGES_INVERSE_BATTLE] = (uint8_t)inverse_v;
    set.value[CHALLENGES_FLIP_STAT] = (uint8_t)flip_v;
    set.value[CHALLENGES_PASSIVES] = (uint8_t)passives_v;
    const char *why = NULL;
    CHECK(gc_run_set_challenges(&run, &set, &why), "case %d: gc_run_set_challenges refused Fresh Start %d, Inverse %d, Flip %d, Passives %d: %s", idx, fresh, inverse_v, flip_v, passives_v, why ? why : "");
    run.run.rng = before;
    gc_starters_build_party(&run, team, n);
    CHECK(run.n_party == n, "case %d: %d members, expected %d", idx, run.n_party, n);
    CHECK(same_rng(&run.run.rng, &after), "case %d (Fresh Start %d): the RNG after the party is c=%.17g s=(%.17g %.17g %.17g), upstream c=%.17g s=(%.17g %.17g %.17g)",
          idx, fresh, run.run.rng.c, run.run.rng.s0, run.run.rng.s1, run.run.rng.s2, after.c, after.s0, after.s1, after.s2);
    for (int k = 0; k < n && k < run.n_party; k++) {
        const gc_mon *m = &run.party[run.party_order[k]];
        const pr_pokemon *p = &m->b;
        char line[400];
        snprintf(line, sizeof line, "%s", exp[k]);
        char *save = NULL;
        strtok_r(line, " \n", &save);   /* M */
        const long id = inum(&save), species = inum(&save), form = inum(&save), level = inum(&save), nature = inum(&save), ability_index = inum(&save);
        const long ability = inum(&save), passive = inum(&save), gender = inum(&save), luck = inum(&save), tera = inum(&save), hp = inum(&save);
        members++;
        CHECK((uint32_t)id == p->id, "case %d member %d: id %u, upstream %ld", idx, k, p->id, id);
        CHECK(species == p->species, "case %d member %d: species %d, upstream %ld", idx, k, p->species, species);
        CHECK(form == p->form, "case %d (Fresh Start %d) member %d (%ld): form %d, upstream %ld", idx, fresh, k, species, p->form, form);
        CHECK(level == p->level, "case %d member %d: level %d, upstream %ld", idx, k, p->level, level);
        CHECK(nature == p->nature, "case %d member %d: nature %d, upstream %ld", idx, k, p->nature, nature);
        CHECK(ability_index == m->ability_index, "case %d (Fresh Start %d) member %d (%ld): ability index %d, upstream %ld", idx, fresh, k, species, m->ability_index, ability_index);
        CHECK(ability == p->base_ability, "case %d (Fresh Start %d) member %d (%ld): ability %d, upstream %ld", idx, fresh, k, species, p->base_ability, ability);
        CHECK(passive == (p->passive_flag ? 1 : 0), "case %d (Fresh Start %d) member %d (%ld): passive %d, upstream %ld", idx, fresh, k, species, p->passive_flag, passive);
        CHECK(gender == p->gender, "case %d member %d: gender %d, upstream %ld", idx, k, p->gender, gender);
        CHECK(luck == m->luck, "case %d (Fresh Start %d) member %d (%ld): luck %d, upstream %ld", idx, fresh, k, species, m->luck, luck);
        CHECK(tera == p->tera_type, "case %d (Fresh Start %d) member %d (%ld form %d): tera type %d, upstream %ld", idx, fresh, k, species, p->form, p->tera_type, tera);
        CHECK(hp == p->hp, "case %d (Fresh Start %d) member %d (%ld): hp %d, upstream %ld", idx, fresh, k, species, p->hp, hp);
        for (int s = 0; s < 6; s++) {
            const long want = inum(&save);
            CHECK(want == p->stats[s], "case %d (Fresh Start %d) member %d (%ld): stat %d is %d, upstream %ld", idx, fresh, k, species, s, p->stats[s], want);
        }
        for (int s = 0; s < 6; s++) {
            const long want = inum(&save);
            CHECK(want == p->ivs[s], "case %d (Fresh Start %d) member %d (%ld): iv %d is %d, upstream %ld", idx, fresh, k, species, s, p->ivs[s], want);
        }
        for (int s = 0; s < 4; s++) {
            const long want = inum(&save);
            const long got = s < p->n_moves ? p->moves[s] : 0;
            CHECK(want == got, "case %d (Fresh Start %d) member %d (%ld): move %d is %ld, upstream %ld", idx, fresh, k, species, s, got, want);
        }
        /* the battle rules: Inverse Battle in the type effectiveness against the member, Passives in hasPassive() (a player's Pokemon, no battle yet) */
        set_battle(0);
        char *es = NULL;
        strtok_r(exp_e[k], " \n", &es);   /* E */
        inum(&es);   /* k */
        for (int t = 0; t < 18; t++) {
            const double want = num(&es), got = pr_get_attack_type_effectiveness(&bt, p, t, NULL, -1);
            CHECK(want == got, "case %d (Inverse %d) member %d (%ld): effectiveness of type %d is %g, upstream %g", idx, inverse_v, k, species, t, got, want);
            effectiveness_values++;
        }
        char *ps = NULL;
        strtok_r(exp_p[k], " \n", &ps);   /* P */
        inum(&ps);
        const long want_passive = inum(&ps);
        CHECK(want_passive == (pr_has_passive(&bt, p) ? 1 : 0), "case %d (Passives %d) member %d (%ld): hasPassive %d, upstream %ld", idx, passives_v, k, species, pr_has_passive(&bt, p), want_passive);
        passive_values++;
    }
    /* Flip Stat on a fusion: member 0 fused with member 1's species and form, its stats recalculated */
    if (exp_f[0] && run.n_party >= 2) {
        char line[200];
        snprintf(line, sizeof line, "%s", exp_f);
        char *fs = NULL;
        strtok_r(line, " \n", &fs);   /* F */
        pr_pokemon fused = run.party[run.party_order[0]].b;
        fused.fusion_species = (uint16_t)inum(&fs);
        fused.fusion_form = (uint8_t)inum(&fs);
        set_battle(0);
        pr_calculate_stats(&bt, &fused);
        for (int s = 0; s < 6; s++) {
            const long want = inum(&fs);
            CHECK(want == fused.stats[s], "case %d (Flip %d): member 0 fused with %d form %d: stat %d is %d, upstream %ld", idx, flip_v, fused.fusion_species, fused.fusion_form, s,
                  fused.stats[s], want);
        }
        fusion_stats++;
    }
    /* an enemy's hasPassive(): the first member's Pokemon with the enemy's fields set, on each wave */
    if (n > 0 && run.n_party > 0) {
        pr_pokemon enemy = run.party[run.party_order[0]].b;
        for (int y = 0; y < got_y; y++) {
            char line[80];
            snprintf(line, sizeof line, "%s", exp_y[y]);
            char *ys = NULL;
            strtok_r(line, " \n", &ys);   /* Y */
            const long trainer = inum(&ys), boss = inum(&ys), flag = inum(&ys), wave = inum(&ys), want = inum(&ys);
            enemy.is_player = false;
            enemy.has_trainer = trainer != 0;
            enemy.is_boss = boss != 0;
            enemy.passive_flag = flag != 0;
            set_battle((int)wave);
            const bool got = pr_has_passive(&bt, &enemy);
            CHECK((long)got == want, "case %d (Passives %d): an enemy (trainer %ld boss %ld passive %ld) on wave %ld has passive %d, upstream %ld", idx, passives_v, trainer, boss, flag, wave, got, want);
            enemy_passives++;
        }
    }
    /* the rewards: the weights with both unlocked, then a Mint and a vitamin on the first member (its storage index) */
    if (exp_r[0] && run.n_party > 0) {
        sys.unlocks.eviolite = sys.unlocks.mini_black_hole = true;
        pr_reward_thresholds th;
        pr_reward_regenerate_thresholds(gc_reward_ctx(&run), 0, &th);
        char line[200];
        snprintf(line, sizeof line, "%s", exp_r);
        char *rs = NULL;
        strtok_r(line, " \n", &rs);   /* R */
        const long want_ev = inum(&rs), want_mbh = inum(&rs);
        const long got_ev = pool_weight(&th, 2, PR_ITEM_EVIOLITE), got_mbh = pool_weight(&th, 4, PR_ITEM_MINI_BLACK_HOLE);
        CHECK(got_ev == want_ev && got_mbh == want_mbh, "case %d (Fresh Start %d): Eviolite weight %ld, Mini Black Hole weight %ld, upstream %ld %ld", idx, fresh,
              got_ev, got_mbh, want_ev, want_mbh);
        reward_weights++;
        const int storage = run.party_order[0];
        const char *kinds[2] = {"MINT", "BASE_STAT_BOOSTER"};
        const char *exps[2] = {exp_n, exp_u};
        for (int r = 0; r < 2; r++) {
            snprintf(line, sizeof line, "%s", exps[r]);
            char *ns = NULL;
            strtok_r(line, " \n", &ns);   /* N / U */
            const long arg = inum(&ns), want_ok = inum(&ns), want_hp = inum(&ns);
            gc_offer o = {.key = kinds[r], .arg = (int16_t)arg, .tier = -1, .target = GC_TARGET_POKEMON};
            const char *why = NULL;
            const bool ok = gc_port_apply_reward(&run, &o, storage, -1, &why);
            const pr_pokemon *p = &run.party[storage].b;
            CHECK((long)ok == want_ok, "case %d (Flip %d): %s %ld applied %d, upstream %ld (%s)", idx, flip_v, kinds[r], arg, ok, want_ok, why ? why : "");
            CHECK(p->hp == want_hp, "case %d (Flip %d): after the %s %ld hp %d, upstream %ld", idx, flip_v, kinds[r], arg, p->hp, want_hp);
            for (int st = 0; st < 6; st++) {
                const long want = inum(&ns);
                CHECK(want == p->stats[st], "case %d (Flip %d): after the %s %ld stat %d is %d, upstream %ld", idx, flip_v, kinds[r], arg, st, p->stats[st], want);
            }
            reward_applies++;
        }
    }
    cases++;
}

int main(int argc, char **argv)
{
    FILE *f = fopen(argc > 1 ? argv[1] : "challenge-starters.txt", "r");
    if (!f) { perror("trace"); return 2; }
    char *line = NULL;
    size_t cap = 0;
    long n_cases = 0;
    static gc_starter team[6];
    static char exp[6][400];
    gc_challenge_set set;
    memset(&set, 0, sizeof set);
    gc_system_init_defaults(&sys);
    int idx = -1, fresh = 0, n = 0, got_s = 0, got_m = 0;
    pr_rng before = {0}, after = {0};
    bool open_case = false, saw_end = false;
    int q_species = 0, q_form = 0;
    double q_cost = 0;
    int q_hard = 0, q_soft = 0;
    while (getline(&line, &cap, f) > 0) {
        char *save = NULL;
        char *kind = strtok_r(line, " \n", &save);
        if (!kind) continue;
        if (!strcmp(kind, "V")) {
            inum(&save);
            inum(&save);
            n_cases = inum(&save);
        } else if (!strcmp(kind, "C")) {
            if (open_case) replay_party(idx, fresh, n, before, after, team, exp);
            open_case = false;
            got_y = 0;
            exp_f[0] = 0;
            exp_r[0] = exp_n[0] = exp_u[0] = 0;
            idx = (int)inum(&save);
            fresh = (int)inum(&save);
            const long budget = inum(&save);
            inverse_v = (int)inum(&save);
            flip_v = (int)inum(&save);
            passives_v = (int)inum(&save);
            memset(&set, 0, sizeof set);
            set.value[CHALLENGES_FRESH_START] = (uint8_t)fresh;
            CHECK(gc_starter_budget(GAME_MODES_CHALLENGE) == budget, "case %d: the cost limit is %d, upstream getRunValueLimit() %ld", idx, gc_starter_budget(GAME_MODES_CHALLENGE), budget);
            if (fresh) fresh_cases++;
        } else if (!strcmp(kind, "D")) {
            const int sp = (int)inum(&save);
            gc_dex_entry *d = &sys.dex[sp];
            gc_starter_entry *s = &sys.starters[sp];
            d->caught_attr = hex(&save);
            d->nature_attr = (uint32_t)inum(&save);
            s->ability_attr = (uint32_t)inum(&save);
            s->passive_attr = (uint32_t)inum(&save);
            s->value_reduction = (uint32_t)inum(&save);
            s->egg_moves = (uint8_t)inum(&save);
            for (int i = 0; i < 6; i++) d->ivs[i] = (uint8_t)inum(&save);
        } else if (!strcmp(kind, "Q")) {
            q_species = (int)inum(&save);
            q_form = (int)inum(&save);
            inum(&save);
            inum(&save);
            inum(&save);   /* shiny, variant, female: no ported challenge looks at them */
            q_cost = num(&save);
            q_hard = (int)inum(&save);
            q_soft = (int)inum(&save);
            queries++;
            const double cost = gc_challenge_starter_cost(&sys, &set, q_species);
            CHECK(cost == q_cost, "case %d (Fresh Start %d): the cost of %d is %.4f, upstream %.4f", idx, fresh, q_species, cost, q_cost);
            const int hard = gc_challenge_starter_valid(&set, q_species, q_form, false);
            const int soft = gc_challenge_starter_valid(&set, q_species, q_form, true);
            CHECK(hard == q_hard, "case %d (Fresh Start %d): %d form %d valid is %d, upstream %d", idx, fresh, q_species, q_form, hard, q_hard);
            CHECK(soft == q_soft, "case %d (Fresh Start %d): %d form %d soft-valid is %d, upstream %d", idx, fresh, q_species, q_form, soft, q_soft);
            if (q_soft && !q_hard) soft_only++;
        } else if (!strcmp(kind, "W")) {
            const int sp = (int)inum(&save);
            gc_starter_view v;
            gc_challenge_starter_view(&sys, &set, sp, &v);
            const uint64_t caught = hex(&save);
            const long nature = inum(&save), ability = inum(&save), passive = inum(&save), reduction = inum(&save), egg = inum(&save);
            CHECK(caught == v.caught_attr, "case %d (Fresh Start %d): %d caught attributes %llx, upstream %llx", idx, fresh, sp, (unsigned long long)v.caught_attr, (unsigned long long)caught);
            CHECK(nature == (long)v.nature_attr, "case %d (Fresh Start %d): %d natures %u, upstream %ld", idx, fresh, sp, v.nature_attr, nature);
            CHECK(ability == (long)v.ability_attr, "case %d (Fresh Start %d): %d abilities %u, upstream %ld", idx, fresh, sp, v.ability_attr, ability);
            CHECK(passive == (long)v.passive_attr, "case %d (Fresh Start %d): %d passive %u, upstream %ld", idx, fresh, sp, v.passive_attr, passive);
            CHECK(reduction == (long)v.value_reduction, "case %d (Fresh Start %d): %d value reduction %u, upstream %ld", idx, fresh, sp, v.value_reduction, reduction);
            CHECK(egg == v.egg_moves, "case %d (Fresh Start %d): %d egg moves %d, upstream %ld", idx, fresh, sp, v.egg_moves, egg);
            for (int i = 0; i < 6; i++) {
                const long want = inum(&save);
                CHECK(want == v.ivs[i], "case %d (Fresh Start %d): %d IV %d is %d, upstream %ld", idx, fresh, sp, i, v.ivs[i], want);
            }
            if (fresh && (v.value_reduction != sys.starters[sp].value_reduction || v.egg_moves != sys.starters[sp].egg_moves)) views_changed++;
        } else if (!strcmp(kind, "B")) {
            n = (int)inum(&save);
            before = read_rng(&save);
            tok(&save);   /* | */
            after = read_rng(&save);
            memset(team, 0, sizeof team);
            got_s = got_m = 0;
            open_case = true;
        } else if (!strcmp(kind, "S")) {
            gc_starter *s = &team[got_s++];
            s->species = (uint16_t)inum(&save);
            s->form = (uint8_t)inum(&save);
            s->shiny = inum(&save) != 0;
            s->variant = (uint8_t)inum(&save);
            s->female = inum(&save) != 0;
            s->ability_index = (uint8_t)inum(&save);
            s->nature = (uint8_t)inum(&save);
            s->passive = inum(&save) != 0;
            s->tera_type = (int8_t)inum(&save);
            for (int i = 0; i < 6; i++) s->ivs[i] = (uint8_t)inum(&save);
            for (int i = 0; i < 4; i++) s->moves[i] = (uint16_t)inum(&save);
        } else if (!strcmp(kind, "M")) {
            snprintf(exp[got_m], sizeof exp[got_m], "M %s", save);
            got_m++;
        } else if (!strcmp(kind, "E")) {
            snprintf(exp_e[(int)strtol(save, NULL, 10)], sizeof exp_e[0], "E %s", save);
        } else if (!strcmp(kind, "P")) {
            snprintf(exp_p[(int)strtol(save, NULL, 10)], sizeof exp_p[0], "P %s", save);
        } else if (!strcmp(kind, "F")) {
            snprintf(exp_f, sizeof exp_f, "F %s", save);
        } else if (!strcmp(kind, "R")) {
            snprintf(exp_r, sizeof exp_r, "R %s", save);
        } else if (!strcmp(kind, "N")) {
            snprintf(exp_n, sizeof exp_n, "N %s", save);
        } else if (!strcmp(kind, "U")) {
            snprintf(exp_u, sizeof exp_u, "U %s", save);
        } else if (!strcmp(kind, "Y")) {
            snprintf(exp_y[got_y++], sizeof exp_y[0], "Y %s", save);
        } else if (!strcmp(kind, "T")) {
            /* getTypeDamageMultiplier over the whole chart under the Inverse Battle value: the one function every read of the chart goes through */
            const int inv = (int)inum(&save);
            memset(&bt, 0, sizeof bt);
            bt.challenge_flags = inv ? 1u << CHALLENGES_INVERSE_BATTLE : 0;
            for (int a = -1; a <= 18; a++)
                for (int d = -1; d <= 18; d++) {
                    const double want = num(&save), got = pr_type_chart_multiplier(&bt, a, d);
                    CHECK(want == got, "Inverse %d: the chart value of %d against %d is %g, upstream %g", inv, a, d, got, want);
                    chart_values++;
                }
        } else if (!strcmp(kind, "Z")) {
            saw_end = true;
        }
    }
    if (open_case) replay_party(idx, fresh, n, before, after, team, exp);
    free(line);
    fclose(f);
    if (!saw_end || cases != n_cases) {
        fprintf(stderr, "incomplete trace: %ld of %ld cases, end marker %d\n", cases, n_cases, saw_end);
        return 2;
    }
    printf("CHALLENGE-STARTERS cases=%ld fresh=%ld queries=%ld members=%ld softOnly=%ld viewsChanged=%ld chart=%ld effectiveness=%ld passives=%ld enemyPassives=%ld fusionStats=%ld rewardWeights=%ld rewardApplies=%ld checks=%ld\n",
           cases, fresh_cases, queries, members, soft_only, views_changed, chart_values, effectiveness_values, passive_values, enemy_passives, fusion_stats, reward_weights, reward_applies, checks);
    printf("TOTAL mismatches=%ld\n", failures);
    return failures ? 1 : 0;
}
