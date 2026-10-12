/* Fleeing against upstream at 8555c08: replays tests/flee.producer.test.ts's trace (the real AttemptRunPhase.start())
 * through game-core's run: a wild battle in the case's game mode and wave, the lead's and the enemy's raw speed, stat
 * stages, paralysis, abilities, boss shield, the escape attempts so far, the battle seed and turn; then the RUN
 * command through gc_run_choose.
 *
 * Compared with what upstream did: whether the escape succeeded (a probe case has its roll set just under or at the
 * chance, so a chance off by one flips it), the flee message, and then
 *  - on a failure: the attempts counter and the run still on a command screen (the battle RNG state after the roll is
 *    not compared: the turn's end resets it, here as upstream, and upstream's phase is run without the turn's end);
 *  - on a success: the next wave began, and the biome changed (a SelectBiomePhase was pushed) exactly when upstream
 *    pushed one.
 * Usage: flee_diff_test <trace> */
#include "../src/internal.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_run run;
static gc_system sys;
static int failures, cases_checked, cases_skipped, fields_checked, n_success, n_failure, n_biome;

#define FAIL(...) do { if (failures < 40) { fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } failures++; } while (0)

static const char *kv(const char *line, const char *key)
{
    char pat[48];
    snprintf(pat, sizeof pat, " %s=", key);
    const char *p = strstr(line, pat);
    return p ? p + strlen(pat) : NULL;
}
static long kv_long(const char *line, const char *key, long def)
{
    const char *p = kv(line, key);
    if (!p) return def;
    char *end;
    const long v = strtol(p, &end, 10);
    return end == p ? def : v;
}
static void kv_str(const char *line, const char *key, char *out, size_t cap)
{
    const char *p = kv(line, key);
    size_t n = 0;
    while (p && *p && *p != ' ' && *p != '\n' && n + 1 < cap) out[n++] = *p++;
    out[n] = 0;
}
#define LOG_MAX 256
static const char *log_keys[LOG_MAX];
static int n_log;
static void drain(void)
{
    gc_event e;
    while (gc_run_next_event(&run, &e)) {
        if (e.kind == GC_EV_MESSAGE && n_log < LOG_MAX) log_keys[n_log++] = e.msg.key;
        if (e.kind == GC_EV_BATTLE)
            for (int i = 0; i < e.b; i++) {
                const pr_event *be = &run.battle_events[e.a + i];
                if (be->type == PR_EV_TEXT && n_log < LOG_MAX) log_keys[n_log++] = be->msg.key;
            }
    }
}
static int times(const char *key)
{
    int n = 0;
    for (int i = 0; i < n_log; i++) if (log_keys[i] && !strcmp(log_keys[i], key)) n++;
    return n;
}

static void cheap_team(gc_starter *team, int n)
{
    memset(team, 0, n * sizeof *team);
    int k = 0;
    for (int i = 0; i < gc_starter_cost_count && k < n; i++) {
        if (gc_starter_costs[i].cost != 1) continue;
        team[k].species = gc_starter_costs[i].id;
        team[k].tera_type = -1;
        for (int j = 0; j < 6; j++) team[k].ivs[j] = 15;
        k++;
    }
}

static void new_run(int mode, const char *seed)
{
    memset(&run, 0, sizeof run);
    gc_system_init_defaults(&sys);
    gc_run_new(&run, mode, seed, 7);
    gc_starter team[1];
    cheap_team(team, 1);
    const char *why;
    if (!gc_run_set_starters(&run, team, 1, &why)) { fprintf(stderr, "no party: %s\n", why); exit(2); }
    run.sys = &sys;
    drain();
}

static void replay_case(char **block, int n_block)
{
    const char *head = block[0];
    const int idx = (int)kv_long(head, "i", -1);
    if (getenv("FLEE_DIFF_ONLY") && atoi(getenv("FLEE_DIFF_ONLY")) != idx) return;
    const int wave = (int)kv_long(head, "wave", 1), mode = (int)kv_long(head, "mode", 0);
    const int att = (int)kv_long(head, "att", 0), turn = (int)kv_long(head, "turn", 1);
    char bseed[40];
    kv_str(head, "seed", bseed, sizeof bseed);

    char seed[48];
    for (int attempt = 0;; attempt++) {
        snprintf(seed, sizeof seed, "flee-diff-%d-%d", idx, attempt);
        new_run(mode, seed);
        if (run.battle_type == BATTLE_TYPE_WILD && run.n_enemy == 1 && gc_run_screen(&run)->kind == GC_SCREEN_COMMAND) break;
        if (attempt > 60) { fprintf(stderr, "case %d: no single wild battle in mode %d to set up\n", idx, mode); exit(2); }
    }

    pr_pokemon *p = run.bt.field[0], *en = run.bt.field[2];
    if (!p || !en) { fprintf(stderr, "case %d: no pokemon on the field\n", idx); exit(2); }
    p->stats[STAT_SPD] = (int32_t)kv_long(head, "spdp", 1);
    en->stats[STAT_SPD] = (int32_t)kv_long(head, "spde", 1);
    p->stat_stages[STAT_SPD - 1] = (int8_t)kv_long(head, "stp", 0);
    en->stat_stages[STAT_SPD - 1] = (int8_t)kv_long(head, "ste", 0);
    p->status = kv_long(head, "parp", 0) ? STATUS_EFFECT_PARALYSIS : 0;
    en->status = kv_long(head, "pare", 0) ? STATUS_EFFECT_PARALYSIS : 0;
    p->ability = p->base_ability = kv_long(head, "abp", 0) ? ABILITY_ID_RUN_AWAY : ABILITY_ID_INSOMNIA;
    en->ability = en->base_ability = kv_long(head, "abe", 0) ? ABILITY_ID_RUN_AWAY : ABILITY_ID_INSOMNIA;
    /* the enemy's moveset is Splash alone, as upstream's: its command then draws what upstream's draws */
    memset(en->moves, 0, sizeof en->moves);
    memset(en->pp_used, 0, sizeof en->pp_used);
    en->moves[0] = MOVE_ID_SPLASH;
    en->n_moves = 1;
    const int segs = (int)kv_long(head, "segs", 0);
    en->boss_segments = (uint8_t)segs;
    en->is_boss = segs > 0;
    run.enemy[0].b.is_boss = en->is_boss;
    run.enemy[0].b.boss_segments = en->boss_segments;
    const int old_biome = run.biome;
    run.wave = wave;
    run.bt.wave_index = wave;
    run.bt.turn = turn;
    run.bt.escape_attempts = att;
    snprintf(run.battle_seed, sizeof run.battle_seed, "%.16s", bseed);
    pr_seed_from_ascii(&run.bt.battle_seed, bseed);
    run.bt.battle_rng_live = false;

    n_log = 0;
    const char *why = "";
    gc_choice c = {GC_CH_RUN, 0, 0, 0};
    if (!gc_run_choose(&run, &c, &why)) { FAIL("case %d: the run command was refused: %s", idx, why); return; }
    drain();

    const char *res = NULL;
    for (int li = 1; li < n_block; li++) if (!strncmp(block[li], "RES ", 4)) res = block[li];
    if (!res) { fputs("no RES line\n", stderr); exit(2); }
    const int up_ok = (int)kv_long(res, "ok", -1);
    const int c_ok = times("battle:runAwaySuccess") > 0;
    fields_checked += 3;
    if (c_ok != up_ok)
        FAIL("case %d (wave %d mode %d att %d spd %ld/%ld segs %d abp %ld probe=%s): escaped C=%d upstream=%d (upstream chance %ld, roll %ld)",
             idx, wave, mode, att, kv_long(head, "spdp", 0), kv_long(head, "spde", 0), segs, kv_long(head, "abp", 0),
             strstr(head, "probe=under") ? "under" : strstr(head, "probe=at") ? "at" : "free", c_ok, up_ok, kv_long(res, "base", -1),
             kv_long(res, "roll", -1));
    if (times("battle:runAwayCannotEscape") != !up_ok) FAIL("case %d: cannot-escape messages C=%d upstream=%d", idx, times("battle:runAwayCannotEscape"), !up_ok);
    if (times("battle:runAwaySuccess") + times("battle:runAwayCannotEscape") != 1) FAIL("case %d: %d flee messages", idx, times("battle:runAwaySuccess") + times("battle:runAwayCannotEscape"));

    if (up_ok) {
        n_success++;
        const char *pushed = kv(res, "pushed");
        const int up_biome = pushed && strstr(pushed, "SelectBiomePhase") && strchr(pushed, ' ') > strstr(pushed, "SelectBiomePhase");
        const int c_biome = run.biome != old_biome || gc_run_screen(&run)->kind == GC_SCREEN_BIOME;
        fields_checked += 2;
        if (up_biome) n_biome++;
        if (c_ok) {
            if (run.wave != wave + 1) FAIL("case %d: wave after the flee C=%d upstream=%d", idx, run.wave, wave + 1);
            if (c_biome != up_biome)
                FAIL("case %d (wave %d mode %d): the biome changed in C=%d, SelectBiomePhase pushed upstream=%d", idx, wave, mode, c_biome, up_biome);
        }
    } else {
        n_failure++;
        const long up_att = kv_long(res, "att2", -1);
        fields_checked += 2;
        if (run.bt.escape_attempts != up_att) FAIL("case %d: escape attempts C=%d upstream=%ld", idx, run.bt.escape_attempts, up_att);
        if (gc_run_screen(&run)->kind != GC_SCREEN_COMMAND) FAIL("case %d: after a failed flee the screen is %d, not a command screen", idx, gc_run_screen(&run)->kind);
    }
    if (getenv("FLEE_DIFF_VERBOSE")) {
        fprintf(stderr, "case %d: C %s, wave %d, said:", idx, c_ok ? "escaped" : "failed", run.wave);
        for (int k = 0; k < n_log; k++) fprintf(stderr, " %s", log_keys[k] ? log_keys[k] : "(null)");
        fputc('\n', stderr);
    }
    cases_checked++;
}

int main(int argc, char **argv)
{
    if (argc < 2) { fputs("usage: flee_diff_test <trace>\n", stderr); return 2; }
    FILE *f = fopen(argv[1], "r");
    if (!f) { fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }
    static char buf[4096];
    char *block[8];
    int n_block = 0;
    bool in_case = false;
    while (fgets(buf, sizeof buf, f)) {
        if (!strncmp(buf, "SKIP ", 5)) { cases_skipped++; continue; }
        if (!strncmp(buf, "CASE ", 5)) { in_case = true; n_block = 0; }
        if (!in_case) continue;
        if (buf[0] == 'E' && (buf[1] == '\n' || buf[1] == 0)) {
            replay_case(block, n_block);
            for (int k = 0; k < n_block; k++) free(block[k]);
            n_block = 0;
            in_case = false;
            continue;
        }
        if (n_block < 8) { block[n_block] = malloc(strlen(buf) + 1); memcpy(block[n_block], buf, strlen(buf) + 1); n_block++; }
    }
    fclose(f);
    printf("flee_diff_test: %d cases replayed (%d skipped upstream: speed ties), %d escaped, %d failed, %d with a biome select, %d fields compared. Mismatches: %d\n",
           cases_checked, cases_skipped, n_success, n_failure, n_biome, fields_checked, failures);
    return failures ? 1 : 0;
}
