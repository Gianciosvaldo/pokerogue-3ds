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
 * Hardcore cases are skipped (game-core models no challenge).
 *
 * Rare Candy and Rarer Candy (Task 96) are in a trace of their own, game-core/tests/candy.producer.test.ts (the apply
 * trace above has no levels, exp, friendship or starter data to compare): upstream's PokemonLevelIncrementModifier through
 * addModifier on random parties at, below and above the level cap, with Candy Jars, in Classic and Endless. Replayed free
 * and bought, compared on taken or refused, every member's level, exp and friendship, the LevelUpPhases queued (party
 * index, old level, new level, in order) and each member's starter friendship progress and candy count.
 * Usage: reward_apply_diff_test <trace> (an apply trace or a candy trace; the records say which) */
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

/* ---- Rare Candy / Rarer Candy: the candy trace's records ---- */
typedef struct { int species, level, friendship, growth; double exp; } cmember;
typedef struct { int root; long friendship, candy; } cstarter;

static void candy_setup(const cmember *m, const cstarter *st, int n, int rot, int stacks, int wave, int mode)
{
    memset(&run, 0, sizeof run);
    gc_system_init_defaults(&sys);
    gc_run_new(&run, mode, "candy-diff", 7);
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
    run.wave = wave;
    for (int i = 0; i < n; i++) run.party_order[i] = (int8_t)((i + rot) % n);
    for (int i = 0; i < n; i++) {
        gc_mon *g = &run.party[run.party_order[i]];
        gc_starter s;
        memset(&s, 0, sizeof s);
        s.species = (uint16_t)m[i].species;
        s.tera_type = -1;
        for (int j = 0; j < 6; j++) s.ivs[j] = 15;
        gc_mon_new_player(&run, g, &s, m[i].level);
        g->b.level = (uint8_t)m[i].level;
        g->b.friendship = (uint8_t)m[i].friendship;
        g->exp = m[i].exp;
    }
    for (int i = 0; i < n; i++) {
        sys.starters[st[i].root].friendship = (int)st[i].friendship;
        sys.starters[st[i].root].candy_count = (int)st[i].candy;
    }
    run.n_items = 0;
    if (stacks > 0) gc_item_add(&run, "CANDY_JAR", -1, 0, stacks);
}

static const char *const candy_keys[] = {"RARE_CANDY", "RARER_CANDY"};
typedef struct {
    long cases, replayed, comparisons, mismatches, shown;
    long taken[2], refused[2], cases_by_key[2];
    long at_or_over_cap, friendship_capped, friendship_near_cap, with_jar, level_ups, starter_gains, classic, endless;
} candy_stats;

static const char *const pokeball_keys[] = {"POKEBALL", "GREAT_BALL", "ULTRA_BALL", "ROGUE_BALL", "MASTER_BALL"};
typedef struct {
    long cases, replayed, comparisons, mismatches, shown;
    long taken[5], refused[5], cases_by_key[5];
} pokeball_stats;

static bool pokeball_case(FILE *f, char *line, size_t cap_line, pokeball_stats *ps)
{
    char key[64];
    char *p = line + 1;
    word(&p, key, sizeof key);
    int init_counts[5];
    for (int k = 0; k < 5; k++) init_counts[k] = (int)num(&p);

    if (!fgets(line, (int)cap_line, f) || line[0] != 'R') { fputs("malformed trace: pokeball R record\n", stderr); return false; }
    char *r = line + 1;
    const int applied = (int)num(&r);
    char filter[160];
    word(&r, filter, sizeof filter);
    const bool filter_ok = !strcmp(filter, "OK");
    int final_counts[5];
    for (int k = 0; k < 5; k++) final_counts[k] = (int)num(&r);

    if (!fgets(line, (int)cap_line, f) || line[0] != 'E') { fputs("malformed trace: pokeball E record\n", stderr); return false; }

    int item = -1;
    for (int k = 0; k < 5; k++) if (!strcmp(key, pokeball_keys[k])) item = k;
    if (item < 0) { fprintf(stderr, "malformed trace: pokeball key %s\n", key); return false; }

    ps->cases++;
    ps->cases_by_key[item]++;

    for (int purchase = 0; purchase < 2; purchase++) {
        memset(&run, 0, sizeof run);
        gc_system_init_defaults(&sys);
        gc_run_new(&run, GAME_MODES_CLASSIC, "pokeball-diff", 7);
        run.sys = &sys;
        for (int k = 0; k < 5; k++) run.pokeballs[k] = (uint8_t)init_counts[k];

        gc_offer o;
        memset(&o, 0, sizeof o);
        o.key = key;
        o.arg = -1;
        o.target = gc_reward_target(key);
        o.cost = purchase ? 400 : 0;
        const char *why = "";
        const bool ok = gc_port_apply_reward(&run, &o, -1, -1, &why);
        const bool expect_ok = filter_ok && applied;

        ps->comparisons++;
        bool bad = false;
        if (ok != expect_ok) {
            bad = true;
            if (ps->shown++ < 12)
                fprintf(stderr, "pokeball case %ld %s (%s): taken %d, upstream %d\n",
                        ps->cases, key, purchase ? "bought" : "free", ok, expect_ok);
        }
        for (int k = 0; k < 5 && !bad; k++) {
            if (run.pokeballs[k] != final_counts[k]) {
                bad = true;
                if (ps->shown++ < 12)
                    fprintf(stderr, "pokeball case %ld %s (%s): ball %d count %d, upstream %d (initial %d)\n",
                            ps->cases, key, purchase ? "bought" : "free", k, run.pokeballs[k], final_counts[k], init_counts[k]);
            }
        }

        if (ok && !bad) {
            gc_event e;
            if (!gc_run_next_event(&run, &e) || e.kind != GC_EV_MESSAGE) {
                bad = true;
                if (ps->shown++ < 12)
                    fprintf(stderr, "pokeball case %ld %s (%s): missing GC_EV_MESSAGE event\n",
                            ps->cases, key, purchase ? "bought" : "free");
            } else {
                if (strcmp(e.msg.key, "battle:rewardGain") != 0) {
                    bad = true;
                    if (ps->shown++ < 12)
                        fprintf(stderr, "pokeball case %ld %s (%s): msg key '%s', expected 'battle:rewardGain'\n",
                                ps->cases, key, purchase ? "bought" : "free", e.msg.key);
                }
                char want_mod_key[128];
                snprintf(want_mod_key, sizeof want_mod_key, "modifierType:ModifierType.%s.name", key);
                if (e.msg.n_args < 1 || e.msg.args[0].kind != PR_ARG_KEY ||
                    strcmp(e.msg.args[0].name, "modifierName") != 0 ||
                    strcmp(e.msg.args[0].s, want_mod_key) != 0) {
                    bad = true;
                    if (ps->shown++ < 12)
                        fprintf(stderr, "pokeball case %ld %s (%s): msg arg0 '%s'='%s', expected '%s'\n",
                                ps->cases, key, purchase ? "bought" : "free", e.msg.args[0].name, e.msg.args[0].s, want_mod_key);
                }
            }
        }

        if (bad) { ps->mismatches++; continue; }
        (ok ? ps->taken : ps->refused)[item]++;
    }
    ps->replayed++;
    return true;
}

/* one case, the C record `line` already read; returns false on a malformed trace */
static bool candy_case(FILE *f, char *line, size_t cap_line, candy_stats *cs)
{
    char key[64];
    char *p = line + 1;
    word(&p, key, sizeof key);
    const int target = (int)num(&p), wave = (int)num(&p), mode = (int)num(&p), stacks = (int)num(&p), n = (int)num(&p);
    if (n < 1 || n > 6) { fputs("malformed trace: candy party size\n", stderr); return false; }
    cmember before[6];
    cstarter st_before[6], st_after[6];
    memset(before, 0, sizeof before);
    for (int i = 0; i < n; i++) {
        if (!fgets(line, (int)cap_line, f) || line[0] != 'P') { fputs("malformed trace: candy P record\n", stderr); return false; }
        char *q = line + 1;
        before[i].species = (int)num(&q);
        before[i].level = (int)num(&q);
        before[i].exp = (double)num(&q);
        before[i].friendship = (int)num(&q);
        before[i].growth = (int)num(&q);
    }
    for (int i = 0; i < n; i++) {
        if (!fgets(line, (int)cap_line, f) || line[0] != 'S') { fputs("malformed trace: candy S record\n", stderr); return false; }
        char *q = line + 1;
        st_before[i].root = (int)num(&q);
        st_before[i].friendship = num(&q);
        st_before[i].candy = num(&q);
    }
    if (!fgets(line, (int)cap_line, f) || line[0] != 'R') { fputs("malformed trace: candy R record\n", stderr); return false; }
    char *r = line + 1;
    const int applied = (int)num(&r);
    char filter[160];
    word(&r, filter, sizeof filter);
    const bool filter_ok = !strcmp(filter, "OK");
    const int level_cap = (int)num(&r);
    cmember after[6];
    memset(after, 0, sizeof after);
    for (int i = 0; i < n; i++) {
        after[i].level = (int)num(&r);
        after[i].exp = (double)num(&r);
        after[i].friendship = (int)num(&r);
        const double expected_exp = (double)num(&r);
        if (after[i].level != before[i].level && after[i].exp != expected_exp) { fprintf(stderr, "case %ld %s: upstream exp %.0f is not getLevelTotalExp %.0f\n", cs->cases, key, after[i].exp, expected_exp); return false; }
    }
    if (!fgets(line, (int)cap_line, f) || line[0] != 'T') { fputs("malformed trace: candy T record\n", stderr); return false; }
    r = line + 1;
    for (int i = 0; i < n; i++) { st_after[i].root = st_before[i].root; st_after[i].friendship = num(&r); st_after[i].candy = num(&r); }
    if (!fgets(line, (int)cap_line, f) || line[0] != 'L') { fputs("malformed trace: candy L record\n", stderr); return false; }
    r = line + 1;
    const int n_up = (int)num(&r);
    if (n_up > 6) { fputs("malformed trace: candy level-up count\n", stderr); return false; }
    int up[6][3];
    for (int i = 0; i < n_up; i++) { up[i][0] = (int)num(&r); up[i][1] = (int)num(&r); up[i][2] = (int)num(&r); }
    if (!fgets(line, (int)cap_line, f) || line[0] != 'E') { fputs("malformed trace: candy E record\n", stderr); return false; }

    int item = -1;
    for (int k = 0; k < 2; k++) if (!strcmp(key, candy_keys[k])) item = k;
    if (item < 0) { fprintf(stderr, "malformed trace: candy key %s\n", key); return false; }
    cs->cases++;
    cs->cases_by_key[item]++;
    cs->with_jar += stacks > 0;
    cs->classic += mode == GAME_MODES_CLASSIC;
    cs->endless += mode == GAME_MODES_ENDLESS;
    for (int i = 0; i < n; i++) {
        cs->at_or_over_cap += before[i].level >= level_cap;
        cs->friendship_capped += before[i].friendship >= 200;
        cs->friendship_near_cap += before[i].friendship >= 195 && before[i].friendship < 200;
    }
    if (mode != GAME_MODES_CLASSIC && mode != GAME_MODES_ENDLESS) { fputs("malformed trace: candy mode\n", stderr); return false; }

    for (int purchase = 0; purchase < 2; purchase++) {
        const int rot = (int)((cs->cases + purchase) % n);
        candy_setup(before, st_before, n, rot, stacks, wave, mode);
        gc_offer o;
        memset(&o, 0, sizeof o);
        o.key = key;
        o.arg = -1;
        o.target = gc_reward_target(key);
        o.cost = purchase ? 400 : 0;
        const char *why = "";
        const bool rare = item == 0;
        const bool ok = gc_port_apply_reward(&run, &o, rare ? run.party_order[target] : -1, -1, &why);
        const bool expect_ok = filter_ok && applied;
        cs->comparisons++;
        bool bad = false;
        if (ok != expect_ok) { bad = true; if (cs->shown++ < 12) fprintf(stderr, "case %ld %s (%s): taken %d, upstream %d (filter %s, applied %d)\n", cs->cases, key, purchase ? "bought" : "free", ok, expect_ok, filter, applied); }
        for (int i = 0; i < n && !bad; i++) {
            const gc_mon *g = &run.party[run.party_order[i]];
            const int want_level = expect_ok ? after[i].level : before[i].level;
            if (g->b.level != want_level || g->exp != after[i].exp || g->b.friendship != after[i].friendship) {
                bad = true;
                if (cs->shown++ < 12) fprintf(stderr, "case %ld %s (%s) wave %d mode %d jars %d: member %d (species %d) level %d exp %.0f friendship %d, upstream %d %.0f %d (before %d %.0f %d, cap %d)\n",
                                              cs->cases, key, purchase ? "bought" : "free", wave, mode, stacks, i, before[i].species, g->b.level, g->exp, g->b.friendship,
                                              want_level, after[i].exp, after[i].friendship, before[i].level, before[i].exp, before[i].friendship, level_cap);
            }
        }
        for (int i = 0; i < n && !bad; i++) {
            const gc_starter_entry *e = &sys.starters[st_after[i].root];
            if (e->friendship != st_after[i].friendship || e->candy_count != st_after[i].candy) {
                bad = true;
                if (cs->shown++ < 12) fprintf(stderr, "case %ld %s (%s) mode %d: member %d starter %d friendship %d candy %d, upstream %ld %ld (before %ld %ld, member friendship %d)\n",
                                              cs->cases, key, purchase ? "bought" : "free", mode, i, st_after[i].root, e->friendship, e->candy_count, st_after[i].friendship, st_after[i].candy,
                                              st_before[i].friendship, st_before[i].candy, before[i].friendship);
            }
        }
        /* the LevelUpPhases the candy queued, in the order upstream unshifted them */
        gc_internal *in = gc_int(&run);
        gc_rphase qp;
        int n_got = 0, got[6][3];
        while (gc_rq_next(&in->rq, &qp)) {
            if (qp.type != RP_LEVEL_UP) continue;
            int idx = -1;
            for (int i = 0; i < n; i++) if (run.party_order[i] == qp.a) idx = i;
            if (n_got < 6) { got[n_got][0] = idx; got[n_got][1] = qp.b; got[n_got][2] = qp.c; }
            n_got++;
        }
        const int want_up = expect_ok ? n_up : 0;
        if (!bad && n_got != want_up) { bad = true; if (cs->shown++ < 12) fprintf(stderr, "case %ld %s (%s): %d LevelUpPhases queued, upstream %d\n", cs->cases, key, purchase ? "bought" : "free", n_got, want_up); }
        for (int i = 0; i < want_up && !bad; i++)
            if (got[i][0] != up[i][0] || got[i][1] != up[i][1] || got[i][2] != up[i][2]) {
                bad = true;
                if (cs->shown++ < 12) fprintf(stderr, "case %ld %s (%s): LevelUpPhase %d is member %d %d -> %d, upstream %d %d -> %d\n", cs->cases, key, purchase ? "bought" : "free", i, got[i][0], got[i][1], got[i][2], up[i][0], up[i][1], up[i][2]);
            }
        if (bad) { cs->mismatches++; continue; }
        (ok ? cs->taken : cs->refused)[item]++;
        if (!purchase) {
            cs->level_ups += n_got;
            for (int i = 0; i < n; i++) cs->starter_gains += st_after[i].friendship != st_before[i].friendship || st_after[i].candy != st_before[i].candy;
        }
    }
    cs->replayed++;
    return true;
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
    candy_stats cs;
    memset(&cs, 0, sizeof cs);
    pokeball_stats ps;
    memset(&ps, 0, sizeof ps);
    while (fgets(line, sizeof line, f)) {
        if (line[0] == '#') continue;
        if (line[0] == 'X') { fprintf(stderr, "upstream threw: %s", line); return 2; }
        if (line[0] == 'C') {
            if (!candy_case(f, line, sizeof line, &cs)) return 2;
            continue;
        }
        if (line[0] == 'K') {
            if (!pokeball_case(f, line, sizeof line, &ps)) return 2;
            continue;
        }
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
    if (cs.cases) {
        printf("%ld candy cases in the trace: %ld replayed twice (free and bought), %ld comparisons, %ld mismatches\n", cs.cases, cs.replayed, cs.comparisons, cs.mismatches);
        printf("free and bought: Rare Candy %ld taken %ld refused, Rarer Candy %ld taken %ld refused; %ld Classic, %ld Endless; %ld with Candy Jars\n",
               cs.taken[0], cs.refused[0], cs.taken[1], cs.refused[1], cs.classic, cs.endless, cs.with_jar);
        printf("members: %ld at or above the wave's level cap, %ld with friendship 195-199, %ld with friendship 200 or more; %ld LevelUpPhases queued, %ld cases moved a starter's friendship or candy (free)\n",
               cs.at_or_over_cap, cs.friendship_near_cap, cs.friendship_capped, cs.level_ups, cs.starter_gains);
    }
    if (ps.cases) {
        printf("%ld pokeball cases in the trace: %ld replayed twice (free and bought), %ld comparisons, %ld mismatches\n", ps.cases, ps.replayed, ps.comparisons, ps.mismatches);
        printf("free and bought: POKEBALL %ld taken, GREAT_BALL %ld taken, ULTRA_BALL %ld taken, ROGUE_BALL %ld taken, MASTER_BALL %ld taken\n",
               ps.taken[0], ps.taken[1], ps.taken[2], ps.taken[3], ps.taken[4]);
    }
    if (!cases && !cs.cases && !ps.cases) return 1;
    if (!cases) return (mismatches || cs.mismatches || ps.mismatches) ? 1 : 0;
    printf("%ld cases in the trace: %ld replayed twice (free and bought), %ld skipped (%ld Hardcore, %ld other items), %ld comparisons, %ld mismatches\n",
           cases, replayed, skipped_hardcore + skipped_other, skipped_hardcore, skipped_other, comparisons, mismatches);
    printf("free rewards: %ld taken, %ld refused; bought: %ld taken, %ld refused; %ld cases with a Healing Charm\n", free_taken, free_refused, bought_taken, bought_refused, charm_cases);
    printf("what changed (free rewards): %ld HP values, %ld statuses, %ld PP used, %ld PP Ups; %ld taken with no effect (Sacred Ash)\n", hp_changed, status_cured, pp_changed, pp_up_changed, free_no_effect);
    printf("%-14s %7s %7s %7s\n", "item", "cases", "taken", "refused");
    for (int k = 0; k < N_KEYS; k++) printf("%-14s %7ld %7ld %7ld\n", keys[k], cases_by_key[k], taken[k], refused[k]);
    return (mismatches || cs.mismatches || ps.mismatches) ? 1 : 0;
}
