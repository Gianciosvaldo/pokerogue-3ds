/* Replays reward-core's apply trace (reward-core/tests/apply.producer.test.ts, run by upstream's own
 * modifier classes at 8555c08, unchanged and not copied here) through game-core's reward application,
 * gc_port_apply_reward -> gc_reward_apply (Task 60). Each case is a random party with random HP, status, moves,
 * PP used and PP Ups, a Healing Charm now and then, and one item used on one member (and one move), with the
 * result upstream got: the party menu's filter (selectFilter / moveSelectFilter), then the modifier's apply
 * through BattleScene.addModifier.
 *
 * For the healing, revive, PP restore and PP Up items (the rest of the trace is evolution, form change,
 * Mega Bracelet, Dynamax Band and DNA Splicer cases, reward-core's own), the party is set up in a gc_run as
 * game-core holds it (the case's members at a rotated party_order, so a wrong party index shows), the
 * reward is taken once for free and once as a purchase, and compared with upstream:
 *   - taken or refused (a refusal: "partyUiHandler:anyEffect"). A free reward the party menu lets through is
 *     taken; one it refuses is not. Sacred Ash has no party menu: free, it is taken whatever it did;
 *     bought, taken only if it did something (SelectModifierPhase.applyModifier, cost -1 against a price);
 *   - every member's HP, status and each move's PP used, PP Ups and maximum PP (getMovePp()) after.
 * Hardcore cases are skipped (game-core models no challenge). Usage: reward_apply_diff_test <trace> */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const keys[] = {"POTION", "SUPER_POTION", "HYPER_POTION", "MAX_POTION", "FULL_RESTORE", "REVIVE",
                                   "MAX_REVIVE", "SACRED_ASH", "ETHER", "MAX_ETHER", "ELIXIR", "MAX_ELIXIR", "PP_UP", "PP_MAX"};
#define N_KEYS ((int)(sizeof keys / sizeof keys[0]))

static long num(char **p)
{
    char *start = *p;
    const long v = strtol(start, p, 10);
    if (*p == start) { fprintf(stderr, "malformed trace: missing number near '%.40s'\n", start); exit(2); }
    return v;
}
static void word(char **p, char *out, size_t cap)
{
    while (**p == ' ') (*p)++;
    size_t n = 0;
    while (**p && **p != ' ' && **p != '\n') { if (n + 1 < cap) out[n++] = **p; (*p)++; }
    out[n] = 0;
}

typedef struct {
    int hp, max_hp, status, n_moves;
    int moves[4], pp_used[4], pp_up[4], max_pp[4];
} member;

static gc_run run;
static gc_system sys;

static void setup(const member *m, int n, int rot, int stacks)
{
    memset(&run, 0, sizeof run);
    gc_system_init_defaults(&sys);
    gc_run_new(&run, GAME_MODES_CLASSIC, "reward-apply-diff", 7);
    gc_starter team[6];
    memset(team, 0, sizeof team);
    int k = 0;
    for (int i = 0; i < gc_starter_cost_count && k < n; i++) {
        if (gc_starter_costs[i].cost != 1) continue;
        team[k].species = gc_starter_costs[i].id;
        team[k].tera_type = -1;
        for (int j = 0; j < 6; j++) team[k].ivs[j] = 15;
        k++;
    }
    const char *why;
    if (k != n || !gc_run_set_starters(&run, team, n, &why)) { fputs("could not set up a party\n", stderr); exit(2); }
    run.sys = &sys;
    for (int i = 0; i < n; i++) run.party_order[i] = (int8_t)((i + rot) % n);
    for (int i = 0; i < n; i++) {
        pr_pokemon *b = &run.party[run.party_order[i]].b;
        b->stats[0] = m[i].max_hp;
        b->hp = m[i].hp;
        b->status = (uint8_t)m[i].status;
        memset(b->moves, 0, sizeof b->moves);
        for (int s = 0; s < 4; s++) { b->pp_used[s] = 0; b->pp_up[s] = 0; b->max_pp_override[s] = -1; }
        for (int s = 0; s < m[i].n_moves; s++) {
            gc_mon_set_move(&run.party[run.party_order[i]], s, m[i].moves[s]);
            b->pp_used[s] = (uint8_t)m[i].pp_used[s];
            b->pp_up[s] = (uint8_t)m[i].pp_up[s];
        }
        b->n_moves = (uint8_t)m[i].n_moves;
    }
    run.n_items = 0;
    if (stacks > 0) gc_item_add(&run, "HEALING_CHARM", -1, 0, stacks);
}

int main(int argc, char **argv)
{
    FILE *f = argc == 2 ? fopen(argv[1], "r") : NULL;
    if (!f) { fputs("usage: reward_apply_diff_test <apply trace>\n", stderr); return 2; }
    static char line[16384];
    long cases = 0, replayed = 0, skipped_hardcore = 0, skipped_other = 0, mismatches = 0, comparisons = 0, shown = 0;
    long taken[N_KEYS] = {0}, refused[N_KEYS] = {0}, cases_by_key[N_KEYS] = {0};
    long free_no_effect = 0, hp_changed = 0, pp_changed = 0, pp_up_changed = 0, status_cured = 0, charm_cases = 0;
    long bought_taken = 0, bought_refused = 0, free_taken = 0, free_refused = 0;
    while (fgets(line, sizeof line, f)) {
        if (line[0] == '#') continue;
        if (line[0] == 'X') { fprintf(stderr, "upstream threw: %s", line); return 2; }
        if (line[0] != 'A') continue;
        cases++;
        char *p = line + 1, key[64];
        word(&p, key, sizeof key);
        num(&p);
        const int target = (int)num(&p), slot = (int)num(&p);
        const long challenges = num(&p);
        const int n = (int)num(&p);
        if (n < 1 || n > 6) { fputs("malformed trace: party size\n", stderr); return 2; }
        member before[6], after[6];
        memset(before, 0, sizeof before);
        for (int i = 0; i < n; i++) {
            if (!fgets(line, sizeof line, f) || line[0] != 'P') { fputs("malformed trace: P record\n", stderr); return 2; }
            char *q = line + 1;
            for (int k = 0; k < 8; k++) num(&q);                 /* id species form fusion species, form, gender, fusion gender, level */
            before[i].hp = (int)num(&q);
            before[i].max_hp = (int)num(&q);
            before[i].status = (int)num(&q);
            for (int k = 0; k < 14; k++) num(&q);                /* nature .. persistent treasure */
            before[i].n_moves = (int)num(&q);
            if (before[i].n_moves < 1 || before[i].n_moves > 4) { fputs("malformed trace: moves\n", stderr); return 2; }
            for (int s = 0; s < before[i].n_moves; s++) {
                before[i].moves[s] = (int)num(&q);
                before[i].pp_used[s] = (int)num(&q);
                before[i].pp_up[s] = (int)num(&q);
                before[i].max_pp[s] = (int)num(&q);
            }
        }
        if (!fgets(line, sizeof line, f) || line[0] != 'I') { fputs("malformed trace: I record\n", stderr); return 2; }
        int stacks = 0;
        {
            char *q = line + 1;
            const int n_items = (int)num(&q);
            for (int j = 0; j < n_items; j++) {
                char ikey[64];
                word(&q, ikey, sizeof ikey);
                const int stack = (int)num(&q);
                num(&q); num(&q);
                if (!strcmp(ikey, "HEALING_CHARM")) stacks = stack;
            }
        }
        if (!fgets(line, sizeof line, f) || line[0] != 'D') { fputs("malformed trace: D record\n", stderr); return 2; }
        if (!fgets(line, sizeof line, f) || line[0] != 'R') { fputs("malformed trace: R record\n", stderr); return 2; }
        char *r = line + 1;
        const int applied = (int)num(&r);
        char filter[160];
        word(&r, filter, sizeof filter);
        const bool filter_ok = !strcmp(filter, "OK");
        int item = -1;
        for (int k = 0; k < N_KEYS; k++) if (!strcmp(key, keys[k])) item = k;
        if (item < 0) {                                           /* evolution, form change, Mega, Dynamax, DNA: not Task 60's */
            if (!fgets(line, sizeof line, f) || line[0] != 'E') { fputs("malformed trace: E record\n", stderr); return 2; }
            skipped_other++;
            continue;
        }
        for (int i = 0; i < n; i++) {
            after[i].hp = (int)num(&r);
            after[i].status = (int)num(&r);
            after[i].n_moves = (int)num(&r);
            for (int s = 0; s < after[i].n_moves; s++) {
                after[i].pp_used[s] = (int)num(&r);
                after[i].pp_up[s] = (int)num(&r);
                after[i].max_pp[s] = (int)num(&r);
            }
        }
        if (!fgets(line, sizeof line, f) || line[0] != 'E') { fputs("malformed trace: E record\n", stderr); return 2; }

        if (challenges & (1L << CHALLENGES_HARDCORE)) { skipped_hardcore++; continue; }
        cases_by_key[item]++;
        charm_cases += stacks > 0;
        const bool sacred_ash = !strcmp(key, "SACRED_ASH");

        for (int purchase = 0; purchase < 2; purchase++) {
            const int rot = (int)((cases + purchase) % n);
            setup(before, n, rot, stacks);
            gc_offer o;
            memset(&o, 0, sizeof o);
            o.key = key;
            o.arg = -1;
            o.target = gc_reward_target(key);
            o.cost = purchase ? 400 : 0;
            const char *why = "";
            const int party = run.party_order[target];
            const bool ok = gc_port_apply_reward(&run, &o, sacred_ash ? -1 : party, sacred_ash ? -1 : slot, &why);
            const bool expect_ok = sacred_ash ? (!purchase || applied) : (filter_ok && (!purchase || applied));
            comparisons++;
            bool bad = false;
            if (ok != expect_ok) { bad = true; if (shown++ < 12) fprintf(stderr, "case %ld %s (%s): taken %d, upstream %d (filter %s, applied %d)\n", cases, key, purchase ? "bought" : "free", ok, expect_ok, filter, applied); }
            if (!ok && !strcmp(why, "partyUiHandler:anyEffect") == 0 && !bad) { bad = true; if (shown++ < 12) fprintf(stderr, "case %ld %s: refusal text '%s'\n", cases, key, why); }
            for (int i = 0; i < n && !bad; i++) {
                const pr_pokemon *b = &run.party[run.party_order[i]].b;
                if (b->hp != after[i].hp || b->status != after[i].status || b->n_moves != after[i].n_moves) {
                    bad = true;
                    if (shown++ < 12) fprintf(stderr, "case %ld %s (%s): member %d hp %d status %d moves %d, upstream %d %d %d\n", cases, key, purchase ? "bought" : "free", i, b->hp, b->status, b->n_moves, after[i].hp, after[i].status, after[i].n_moves);
                }
                for (int s = 0; s < after[i].n_moves && !bad; s++)
                    if (b->pp_used[s] != after[i].pp_used[s] || b->pp_up[s] != after[i].pp_up[s] || gc_move_max_pp(b, s) != after[i].max_pp[s]) {
                        bad = true;
                        if (shown++ < 12) fprintf(stderr, "case %ld %s (%s): member %d move %d (id %d) pp_used %d pp_up %d max %d, upstream %d %d %d\n", cases, key, purchase ? "bought" : "free", i, s, b->moves[s], b->pp_used[s], b->pp_up[s], gc_move_max_pp(b, s), after[i].pp_used[s], after[i].pp_up[s], after[i].max_pp[s]);
                    }
            }
            if (bad) { mismatches++; continue; }
            (ok ? taken : refused)[item]++;
            if (purchase) { bought_taken += ok; bought_refused += !ok; } else { free_taken += ok; free_refused += !ok; }
            if (!purchase) {
                bool changed = false;
                for (int i = 0; i < n; i++) {
                    hp_changed += before[i].hp != after[i].hp;
                    status_cured += before[i].status != after[i].status;
                    for (int s = 0; s < before[i].n_moves; s++) {
                        pp_changed += before[i].pp_used[s] != after[i].pp_used[s];
                        pp_up_changed += before[i].pp_up[s] != after[i].pp_up[s];
                    }
                    changed |= before[i].hp != after[i].hp || before[i].status != after[i].status;
                    for (int s = 0; s < before[i].n_moves; s++) changed |= before[i].pp_used[s] != after[i].pp_used[s] || before[i].pp_up[s] != after[i].pp_up[s];
                }
                free_no_effect += ok && !changed;
            }
        }
        replayed++;
    }
    fclose(f);
    printf("%ld cases in the trace: %ld replayed twice (free and bought), %ld skipped (%ld Hardcore, %ld other items), %ld comparisons, %ld mismatches\n",
           cases, replayed, skipped_hardcore + skipped_other, skipped_hardcore, skipped_other, comparisons, mismatches);
    printf("free rewards: %ld taken, %ld refused; bought: %ld taken, %ld refused; %ld cases with a Healing Charm\n", free_taken, free_refused, bought_taken, bought_refused, charm_cases);
    printf("what changed (free rewards): %ld HP values, %ld statuses, %ld PP used, %ld PP Ups; %ld taken with no effect (Sacred Ash)\n", hp_changed, status_cured, pp_changed, pp_up_changed, free_no_effect);
    printf("%-14s %7s %7s %7s\n", "item", "cases", "taken", "refused");
    for (int k = 0; k < N_KEYS; k++) printf("%-14s %7ld %7ld %7ld\n", keys[k], cases_by_key[k], taken[k], refused[k]);
    return mismatches ? 1 : 0;
}
