/* Card 94: replays tests/catch.producer.test.ts's trace (upstream at 8555c08, its real CommandPhase.handleBallCommand,
 * AttemptCapturePhase, Pokemon.addToParty and the party screen's release) through game-core's catching.
 *
 * RULE lines: the run is put in the state the line says (mode, challenge, wave, biome, battle type, boss shield, dex)
 * and gc_catch_ball_refusal(ball) must give upstream's answer: allowed, or the same locale key. A ball number past
 * the five the command takes ("silent") is refused by the command with no text.
 * CASE blocks: a wild battle with the party, wild Pokemon, held items, dex entries, ball counts and RNG states the
 * block gives, a ball thrown through gc_run_choose, a full party answered (release one, or No); then, as upstream
 * left it: whether the roll caught it, the farewell and "added as a starter" messages, the ball counts, every party
 * member in order (species, level, HP, status, ball, where it was met, IVs, nature, moves...), the enemy, the held
 * items of each member, the dex entries and candy of the species' chain, and both RNG states.
 * Usage: catch_diff_test <trace> */
#include "../src/internal.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_run run;
static gc_system sys;
static int failures, rules_checked, cases_checked, cases_skipped, fields_checked;

#define FAIL(...) do { if (failures < 40) { fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } failures++; } while (0)

/* ---- trace parsing: "TAG key=value key=value", values without spaces ---- */
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
static unsigned long long kv_ull(const char *line, const char *key)
{
    const char *p = kv(line, key);
    return p ? strtoull(p, NULL, 10) : 0;
}
static void kv_str(const char *line, const char *key, char *out, size_t cap)
{
    const char *p = kv(line, key);
    size_t n = 0;
    while (p && *p && *p != ' ' && *p != '\n' && n + 1 < cap) out[n++] = *p++;
    out[n] = 0;
}
/* a comma-separated list of numbers; returns how many were read */
static int kv_list(const char *line, const char *key, double *out, int max)
{
    const char *p = kv(line, key);
    int n = 0;
    while (p && n < max) {
        char *end;
        const double v = strtod(p, &end);
        if (end == p) break;
        out[n++] = v;
        p = *end == ',' ? end + 1 : end;
        if (*end != ',') break;
    }
    return n;
}

static char *xstrdup(const char *s)
{
    const size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (!p) exit(2);
    memcpy(p, s, n);
    return p;
}

static const char *intern(const char *s)
{
    static char *table[64];
    for (int i = 0; i < 64; i++) {
        if (table[i] && !strcmp(table[i], s)) return table[i];
        if (!table[i]) { table[i] = xstrdup(s); return table[i]; }
    }
    fputs("too many item keys\n", stderr);
    exit(2);
}

/* ---- the C side's event log ---- */
#define LOG_MAX 512
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
    for (int i = 0; i < n_log; i++) if (!strcmp(log_keys[i], key)) n++;
    return n;
}
static int farewells(void)
{
    static const char *const k[] = {"partyUiHandler:goodbye", "partyUiHandler:byebye", "partyUiHandler:farewell",
                                    "partyUiHandler:soLong", "partyUiHandler:thisIsWhereWePart", "partyUiHandler:illMissYou",
                                    "partyUiHandler:illNeverForgetYou", "partyUiHandler:untilWeMeetAgain",
                                    "partyUiHandler:sayonara", "partyUiHandler:smellYaLater"};
    int n = 0;
    for (unsigned i = 0; i < sizeof k / sizeof *k; i++) n += times(k[i]);
    return n;
}

/* ---- a run in the state a case gives ---- */
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

static void new_run(const char *seed, int n)
{
    memset(&run, 0, sizeof run);
    gc_system_init_defaults(&sys);
    gc_run_new(&run, GAME_MODES_CLASSIC, seed, 7);
    gc_starter team[6];
    cheap_team(team, n);
    const char *why;
    if (!gc_run_set_starters(&run, team, n, &why)) { fprintf(stderr, "no party: %s\n", why); exit(2); }
    run.sys = &sys;
    drain();
}

static int cmp_str(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

typedef struct {
    int i, wave, biome, ball, n, rel, turn, segs, segidx, b_live;
    char bseed[40], wseed[80];
    double g[4], b[4];
} case_hdr;

/* set the fields of a Pokemon from a PM/EN/RM line's values */
static void set_mon_from(gc_mon *m, const char *l)
{
    pr_pokemon *b = &m->b;
    double v[8];
    b->species = (uint16_t)kv_long(l, "sp", 1);
    b->form = (uint8_t)kv_long(l, "form", 0);
    b->level = (uint8_t)kv_long(l, "lv", 1);
    b->hp = (int32_t)kv_long(l, "hp", 1);
    b->status = (uint8_t)kv_long(l, "st", 0);
    if (kv_list(l, "stats", v, 6) == 6) for (int k = 0; k < 6; k++) b->stats[k] = (int32_t)v[k];
    m->pokeball = (uint8_t)kv_long(l, "ball", 0);
    m->met_wave = (int16_t)kv_long(l, "mw", -1);
    m->met_level = (uint8_t)kv_long(l, "mlv", 0);
    m->met_biome = (int8_t)kv_long(l, "mb", -1);
    m->met_species = (uint16_t)kv_long(l, "msp", 0);
    if (kv_list(l, "ivs", v, 6) == 6) for (int k = 0; k < 6; k++) b->ivs[k] = (uint8_t)v[k];
    b->nature = (uint8_t)kv_long(l, "nat", 0);
    m->ability_index = (uint8_t)kv_long(l, "ab", 0);
    b->gender = (int8_t)kv_long(l, "gen", 0);
    b->shiny = kv_long(l, "shiny", 0) != 0;
    m->variant = (uint8_t)kv_long(l, "var", 0);
    b->friendship = (uint8_t)kv_long(l, "fr", 0);
    memset(b->moves, 0, sizeof b->moves);
    memset(b->pp_used, 0, sizeof b->pp_used);
    memset(b->pp_up, 0, sizeof b->pp_up);
    const int nm = kv_list(l, "moves", v, 4);
    for (int k = 0; k < nm; k++) gc_mon_set_move(m, k, (int)v[k]);
    b->n_moves = (uint8_t)nm;
}

/* the mon's fields as the trace prints them, to compare with an RM line */
static void check_mon(const case_hdr *h, const char *l, const gc_mon *m, const char *what)
{
    const pr_pokemon *b = &m->b;
    double v[8];
#define EQ(name, have, want) do { fields_checked++; const long long hv = (long long)(have), wv = (long long)(want); \
        if (hv != wv) FAIL("case %d %s: %s C=%lld upstream=%lld", h->i, what, name, hv, wv); } while (0)
    EQ("species", b->species, kv_long(l, "sp", -1));
    EQ("form", b->form, kv_long(l, "form", -1));
    EQ("level", b->level, kv_long(l, "lv", -1));
    EQ("hp", b->hp, kv_long(l, "hp", -1));
    EQ("status", b->status, kv_long(l, "st", -1));
    if (kv_list(l, "stats", v, 6) == 6) for (int k = 0; k < 6; k++) EQ("stat", b->stats[k], (long long)v[k]);
    EQ("pokeball", m->pokeball, kv_long(l, "ball", -1));
    EQ("met_wave", m->met_wave, kv_long(l, "mw", -9));
    EQ("met_level", m->met_level, kv_long(l, "mlv", -1));
    EQ("met_biome", m->met_biome, kv_long(l, "mb", -9));
    EQ("met_species", m->met_species, kv_long(l, "msp", -1));
    if (kv_list(l, "ivs", v, 6) == 6) for (int k = 0; k < 6; k++) EQ("iv", b->ivs[k], (long long)v[k]);
    EQ("nature", b->nature, kv_long(l, "nat", -1));
    EQ("ability index", m->ability_index, kv_long(l, "ab", -1));
    EQ("gender", b->gender, kv_long(l, "gen", -9));
    EQ("shiny", b->shiny, kv_long(l, "shiny", -1));
    EQ("variant", m->variant, kv_long(l, "var", -1));
    EQ("friendship", b->friendship, kv_long(l, "fr", -1));
    const int nm = kv_list(l, "moves", v, 4);
    EQ("n moves", b->n_moves, nm);
    for (int k = 0; k < nm; k++) EQ("move", b->moves[k], (long long)v[k]);
#undef EQ
}

static void item_lines(char **out, int *n, const char *tag)
{
    for (int i = 0; i < run.n_items; i++) {
        int idx = -1;
        for (int k = 0; k < run.n_party; k++)
            if (run.party[run.party_order[k]].b.id == run.items[i].pokemon_id) idx = k;
        char buf[160];
        snprintf(buf, sizeof buf, "%s idx=%d key=%s arg=%d stack=%d", tag, idx, run.items[i].key, run.items[i].arg, run.items[i].stack);
        out[(*n)++] = xstrdup(buf);
    }
}

static void replay_case(char **block, int n_block)
{
    case_hdr h;
    memset(&h, 0, sizeof h);
    const char *head = block[0];
    h.i = (int)kv_long(head, "i", -1);
    /* CATCH_DIFF_ONLY=<case> replays that case alone, CATCH_DIFF_VERBOSE=1 prints what C ended on and said */
    if (getenv("CATCH_DIFF_ONLY") && atoi(getenv("CATCH_DIFF_ONLY")) != h.i) return;
    h.wave = (int)kv_long(head, "wave", 1);
    h.biome = (int)kv_long(head, "biome", 0);
    h.ball = (int)kv_long(head, "ball", 0);
    h.n = (int)kv_long(head, "n", 1);
    h.rel = (int)kv_long(head, "rel", -2);
    h.turn = (int)kv_long(head, "turn", 1);
    h.segs = (int)kv_long(head, "segs", 0);
    h.segidx = (int)kv_long(head, "segidx", 0);
    kv_str(head, "bseed", h.bseed, sizeof h.bseed);
    kv_str(head, "wseed", h.wseed, sizeof h.wseed);
    kv_list(head, "g", h.g, 4);
    h.b_live = kv(head, "b") && strncmp(kv(head, "b"), "none", 4) != 0;
    if (h.b_live) kv_list(head, "b", h.b, 4);

    /* a wave 1 battle that is a single wild one, for the party size and a seed that gives it */
    char seed[48];
    for (int attempt = 0;; attempt++) {
        snprintf(seed, sizeof seed, "catch-diff-%d-%d", h.i, attempt);
        new_run(seed, h.n);
        if (run.battle_type == BATTLE_TYPE_WILD && run.n_enemy == 1) break;
        if (attempt > 40) { fputs("no single wild battle to set up\n", stderr); exit(2); }
    }

    int n_pm = 0;
    run.n_items = run.n_enemy_items = 0;
    for (int li = 1; li < n_block; li++) {
        const char *l = block[li];
        if (!strncmp(l, "PM ", 3)) {
            const int idx = (int)kv_long(l, "idx", -1);
            if (idx < 0 || idx >= run.n_party) { fputs("bad PM index\n", stderr); exit(2); }
            set_mon_from(&run.party[idx], l);
            n_pm++;
        } else if (!strncmp(l, "EN ", 3)) {
            gc_mon *e = &run.enemy[0];
            set_mon_from(e, l);
        } else if (!strncmp(l, "ENX ", 4)) {
            gc_mon *e = &run.enemy[0];
            e->b.id = (uint32_t)kv_ull(l, "id");
            e->b.status_sleep_turns = (int16_t)kv_long(l, "sleep", -1);
        }
    }
    if (n_pm != run.n_party) { fputs("party size mismatch in the trace\n", stderr); exit(2); }
    gc_mon *e = &run.enemy[0];
    e->b.is_boss = h.segs > 0;
    e->b.boss_segments = (uint8_t)h.segs;
    e->b.boss_segment_index = (uint8_t)h.segidx;
    for (int li = 1; li < n_block; li++) {
        const char *l = block[li];
        if (!strncmp(l, "PI ", 3)) {
            const int idx = (int)kv_long(l, "idx", -1);
            char key[48];
            kv_str(l, "key", key, sizeof key);
            gc_item_add(&run, intern(key), (int16_t)kv_long(l, "arg", -1), run.party[idx].b.id, (int)kv_long(l, "stack", 1));
        } else if (!strncmp(l, "EI ", 3)) {
            char key[48];
            kv_str(l, "key", key, sizeof key);
            gc_item *it = &run.enemy_items[run.n_enemy_items++];
            it->key = intern(key);
            it->arg = (int16_t)kv_long(l, "arg", -1);
            it->stack = (int)kv_long(l, "stack", 1);
            it->pokemon_id = e->b.id;
        } else if (!strncmp(l, "BALLS ", 6)) {
            const char *p = l + 6;
            for (int b = 0; b < 5; b++) { run.pokeballs[b] = (int)strtol(p, (char **)&p, 10); if (*p == ',') p++; }
        } else if (!strncmp(l, "DP ", 3)) {
            const int id = (int)kv_long(l, "id", 0);
            double iv[6];
            gc_dex_entry *d = &sys.dex[id];
            d->caught_attr = kv_ull(l, "caught");
            d->nature_attr = (uint32_t)kv_ull(l, "nat");
            d->caught_count = (uint32_t)kv_ull(l, "cnt");
            if (kv_list(l, "ivs", iv, 6) == 6) for (int k = 0; k < 6; k++) d->ivs[k] = (uint8_t)iv[k];
            const long candy = kv_long(l, "candy", -1), abil = kv_long(l, "abil", -1);
            if (candy >= 0) sys.starters[id].candy_count = (int32_t)candy;
            if (abil >= 0) sys.starters[id].ability_attr = (uint32_t)abil;
        }
    }
    gc_items_to_battle(&run);
    run.wave = h.wave;
    run.biome = h.biome;
    run.bt.wave_index = h.wave;
    run.bt.biome = (int16_t)h.biome;
    run.bt.turn = h.turn;
    run.run.rng.c = h.g[0]; run.run.rng.s0 = h.g[1]; run.run.rng.s1 = h.g[2]; run.run.rng.s2 = h.g[3];
    run.bt.rng = run.run.rng;
    run.bt.rng_live = true;
    snprintf(run.battle_seed, sizeof run.battle_seed, "%.16s", h.bseed);
    pr_seed_from_ascii(&run.bt.battle_seed, h.bseed);
    pr_seed_from_ascii(&run.run.wave_seed, h.wseed);
    run.bt.wave_seed = run.run.wave_seed;
    run.bt.battle_rng_live = h.b_live;
    if (h.b_live) { run.bt.battle_rng.c = h.b[0]; run.bt.battle_rng.s0 = h.b[1]; run.bt.battle_rng.s1 = h.b[2]; run.bt.battle_rng.s2 = h.b[3]; }

    /* the throw, and the answers */
    n_log = 0;
    const char *why = "";
    gc_choice c = {GC_CH_BALL, (int16_t)h.ball, 0, 0};
    if (!gc_run_choose(&run, &c, &why)) { FAIL("case %d: the throw was refused: %s", h.i, why); return; }
    int kind = -1;
    for (int step = 0; step < 200; step++) {
        drain();
        const gc_screen *s = gc_run_screen(&run);
        gc_choice a;
        memset(&a, 0, sizeof a);
        kind = s->kind;
        if (kind == GC_SCREEN_SWITCH) { a.kind = GC_CH_PICK; a.a = (int16_t)(h.rel >= 0 ? h.rel : -1); }
        else if (kind == GC_SCREEN_LEARN_MOVE) { a.kind = GC_CH_FORGET; a.a = -1; }
        else if (kind == GC_SCREEN_EVOLVE) { a.kind = GC_CH_CANCEL; a.a = 1; }
        else if (kind == GC_SCREEN_CHECK_SWITCH) { a.kind = GC_CH_CONFIRM; }
        else break;
        if (kind == GC_SCREEN_SWITCH && h.rel == -2) { FAIL("case %d: a party prompt with room in the party", h.i); return; }
        if (!gc_run_choose(&run, &a, &why)) { FAIL("case %d: answer refused: %s", h.i, why); return; }
    }

    if (getenv("CATCH_DIFF_VERBOSE")) {
        fprintf(stderr, "case %d: C ends on screen %d (wave %d, %d phases pending); said:", h.i, kind, run.wave, 0);
        for (int k = 0; k < n_log; k++) fprintf(stderr, " %s", log_keys[k]);
        fputc('\n', stderr);
    }

    /* ---- compare with what upstream left ---- */
    int idx_rm = 0;
    char *want_items[64], *have_items[64];
    int n_want = 0, n_have = 0;
    for (int li = 1; li < n_block; li++) {
        const char *l = block[li];
        if (!strncmp(l, "RES ", 4)) {
            const int caught = times("battle:pokemonCaught") > 0;
            fields_checked += 6;
            if (caught != kv_long(l, "caught", -1)) FAIL("case %d: caught C=%d upstream=%ld", h.i, caught, kv_long(l, "caught", -1));
            if (run.n_party != kv_long(l, "n", -1)) FAIL("case %d: party size C=%d upstream=%ld", h.i, run.n_party, kv_long(l, "n", -1));
            if (farewells() != kv_long(l, "farewell", -1)) FAIL("case %d: farewells C=%d upstream=%ld", h.i, farewells(), kv_long(l, "farewell", -1));
            if (times("battle:addedAsAStarter") != kv_long(l, "started", -1))
                FAIL("case %d: added-as-a-starter messages C=%d upstream=%ld", h.i, times("battle:addedAsAStarter"), kv_long(l, "started", -1));
            if (times("battle:partyFull") != kv_long(l, "partyFull", -1))
                FAIL("case %d: party-full messages C=%d upstream=%ld", h.i, times("battle:partyFull"), kv_long(l, "partyFull", -1));
            if (run.bt.last_used_pokeball != kv_long(l, "last", -9))
                FAIL("case %d: lastUsedPokeball C=%d upstream=%ld", h.i, run.bt.last_used_pokeball, kv_long(l, "last", -9));
        } else if (!strncmp(l, "RBALLS ", 7)) {
            const char *p = l + 7;
            for (int b = 0; b < 5; b++) {
                const long v = strtol(p, (char **)&p, 10);
                if (*p == ',') p++;
                fields_checked++;
                if (run.pokeballs[b] != v) FAIL("case %d: balls[%d] C=%d upstream=%ld", h.i, b, run.pokeballs[b], v);
            }
        } else if (!strncmp(l, "RM ", 3)) {
            const int idx = (int)kv_long(l, "idx", -1);
            if (idx >= run.n_party) { FAIL("case %d: upstream has a party member %d, C has %d", h.i, idx, run.n_party); continue; }
            char what[24];
            snprintf(what, sizeof what, "party[%d]", idx);
            check_mon(&h, l, &run.party[run.party_order[idx]], what);
            idx_rm++;
        } else if (!strncmp(l, "REN ", 4)) {
            fields_checked += 2;
            if (run.enemy[0].b.hp != kv_long(l, "hp", -1)) FAIL("case %d: enemy hp C=%d upstream=%ld", h.i, run.enemy[0].b.hp, kv_long(l, "hp", -1));
            if (run.enemy[0].b.status != kv_long(l, "st", -1)) FAIL("case %d: enemy status C=%d upstream=%ld", h.i, run.enemy[0].b.status, kv_long(l, "st", -1));
        } else if (!strncmp(l, "RI ", 3)) {
            want_items[n_want++] = xstrdup(l);
        } else if (!strncmp(l, "DA ", 3)) {
            const int id = (int)kv_long(l, "id", 0);
            const gc_dex_entry *d = &sys.dex[id];
            double iv[6];
            fields_checked += 4;
            if (d->caught_attr != kv_ull(l, "caught")) FAIL("case %d: dex %d caught C=%llu upstream=%llu", h.i, id, (unsigned long long)d->caught_attr, kv_ull(l, "caught"));
            if (d->nature_attr != kv_ull(l, "nat")) FAIL("case %d: dex %d natures C=%u upstream=%llu", h.i, id, d->nature_attr, kv_ull(l, "nat"));
            if (d->caught_count != kv_ull(l, "cnt")) FAIL("case %d: dex %d count C=%u upstream=%llu", h.i, id, d->caught_count, kv_ull(l, "cnt"));
            if (kv_list(l, "ivs", iv, 6) == 6)
                for (int k = 0; k < 6; k++) if (d->ivs[k] != (uint8_t)iv[k]) { FAIL("case %d: dex %d iv %d C=%u upstream=%d", h.i, id, k, d->ivs[k], (int)iv[k]); break; }
            const long candy = kv_long(l, "candy", -1), abil = kv_long(l, "abil", -1);
            if (candy >= 0) {
                fields_checked += 2;
                if (sys.starters[id].candy_count != candy) FAIL("case %d: dex %d candy C=%d upstream=%ld", h.i, id, sys.starters[id].candy_count, candy);
                if (sys.starters[id].ability_attr != (uint32_t)abil) FAIL("case %d: dex %d ability attr C=%u upstream=%ld", h.i, id, sys.starters[id].ability_attr, abil);
            }
        } else if (!strncmp(l, "RRNG ", 5)) {
            double g[4], b[4];
            kv_list(l, "g", g, 4);
            fields_checked += 2;
            /* the battle's copy of the global RNG: game-core rolls the reward options on run.run.rng after it */
            const pr_rng *r = &run.bt.rng;
            if (r->c != g[0] || r->s0 != g[1] || r->s1 != g[2] || r->s2 != g[3])
                FAIL("case %d: global RNG C=%.17g,%.17g,%.17g,%.17g upstream=%.17g,%.17g,%.17g,%.17g", h.i, r->c, r->s0, r->s1, r->s2, g[0], g[1], g[2], g[3]);
            const char *bp = kv(l, "b");
            if (bp && strncmp(bp, "none", 4)) {
                kv_list(l, "b", b, 4);
                const pr_rng *q = &run.bt.battle_rng;
                if (!run.bt.battle_rng_live || q->c != b[0] || q->s0 != b[1] || q->s1 != b[2] || q->s2 != b[3])
                    FAIL("case %d: battle RNG C=%d:%.17g,%.17g,%.17g,%.17g upstream=%.17g,%.17g,%.17g,%.17g", h.i, run.bt.battle_rng_live, q->c, q->s0, q->s1, q->s2, b[0], b[1], b[2], b[3]);
            } else if (run.bt.battle_rng_live) {
                FAIL("case %d: C used the battle RNG, upstream did not", h.i);
            }
        }
    }
    if (idx_rm != run.n_party) FAIL("case %d: party of %d, upstream's lines give %d", h.i, run.n_party, idx_rm);
    item_lines(have_items, &n_have, "RI");
    /* the trace's lines are "RI idx=.. key=.. arg=.. stack=..", ours the same without the line's own newline */
    for (int k = 0; k < n_want; k++) { char *nl = strchr(want_items[k], '\n'); if (nl) *nl = 0; }
    qsort(want_items, n_want, sizeof *want_items, cmp_str);
    qsort(have_items, n_have, sizeof *have_items, cmp_str);
    fields_checked += n_want + n_have;
    if (n_want != n_have) FAIL("case %d: %d held items upstream, %d in C", h.i, n_want, n_have);
    for (int k = 0; k < n_want && k < n_have; k++)
        if (strcmp(want_items[k], have_items[k])) { FAIL("case %d: held item C='%s' upstream='%s'", h.i, have_items[k], want_items[k]); break; }
    for (int k = 0; k < n_want; k++) free(want_items[k]);
    for (int k = 0; k < n_have; k++) free(have_items[k]);
    (void)kind;
    cases_checked++;
}

/* ---- the ball rules ---- */
static int starter_ids[1200], n_starter_ids;
static int cmp_int(const void *a, const void *b) { return *(const int *)a - *(const int *)b; }

static void replay_rule(const char *l)
{
    static bool ready;
    if (!ready) {
        new_run("catch-diff-rules", 1);
        for (int i = 0; i < gc_starter_cost_count; i++) starter_ids[n_starter_ids++] = gc_starter_costs[i].id;
        qsort(starter_ids, n_starter_ids, sizeof *starter_ids, cmp_int);
        ready = true;
    }
    pr_run *rr = &run.run;
    rr->mode = (uint8_t)kv_long(l, "mode", 0);
    rr->has_challenges = false;
    rr->challenge_flags = 0;
    rr->challenge_fresh_start = 0;
    if (kv_long(l, "chal", 0)) {
        rr->has_challenges = true;
        rr->challenge_flags = PR_CHALLENGE_FRESH_START;
        rr->challenge_fresh_start = 1;
    }
    run.wave = (int)kv_long(l, "wave", 1);
    run.biome = (int)kv_long(l, "biome", 0);
    const int btype = (int)kv_long(l, "btype", 0);
    run.battle_type = btype == 1 ? BATTLE_TYPE_TRAINER : BATTLE_TYPE_WILD;
    run.me_battle = btype == 3;
    run.me_catch_allowed = kv_long(l, "catch", 0) != 0;
    const int species = (int)kv_long(l, "species", 1);
    pr_pokemon *foe = run.bt.field[BATTLER_INDEX_ENEMY];
    foe->species = (uint16_t)species;
    foe->hp = (int32_t)kv_long(l, "hp", 1);
    const int segs = (int)kv_long(l, "segs", 0);
    foe->is_boss = segs > 0;
    foe->boss_segments = (uint8_t)segs;
    foe->boss_segment_index = (uint8_t)kv_long(l, "segidx", 0);
    foe->base_ability = kv_long(l, "wg", 0) ? ABILITY_ID_WONDER_GUARD : ABILITY_ID_NONE;
    foe->passive = ABILITY_ID_NONE;
    const int missing = (int)kv_long(l, "missing", 0);
    for (int k = 0; k < n_starter_ids; k++) sys.dex[starter_ids[k]].caught_attr = k < missing ? 0 : 157;
    sys.dex[species].caught_attr = kv_long(l, "caught", 0) ? 157 : 0;
    int caught_starters = 0;
    for (int k = 0; k < n_starter_ids; k++) if (sys.dex[starter_ids[k]].caught_attr) caught_starters++;
    if (caught_starters != kv_long(l, "starters", -1))
        FAIL("rule %d: starters in the dex C=%d upstream=%ld (%s)", rules_checked, caught_starters, kv_long(l, "starters", -1), l);

    const int ball = (int)kv_long(l, "ball", 0);
    const char *want = strstr(l, " -> ");
    if (!want) { fputs("malformed RULE line\n", stderr); exit(2); }
    want += 4;
    char key[64];
    size_t n = 0;
    while (*want && *want != '\n' && n + 1 < sizeof key) key[n++] = *want++;
    key[n] = 0;
    const char *have = gc_catch_ball_refusal(&run, ball);
    bool same;
    if (!strcmp(key, "-")) same = have == NULL && ball < 5;
    else if (!strcmp(key, "silent")) same = have == NULL && ball >= 5;
    else same = have && !strcmp(have, key);
    rules_checked++;
    fields_checked++;
    if (!same) FAIL("rule %d: C=%s upstream=%s (%s)", rules_checked, have ? have : (ball >= 5 ? "silent" : "-"), key, l);
}

int main(int argc, char **argv)
{
    if (argc < 2) { fputs("usage: catch_diff_test <trace>\n", stderr); return 2; }
    FILE *f = fopen(argv[1], "r");
    if (!f) { fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }
    static char buf[4096];
    char *block[512];
    int n_block = 0;
    bool in_case = false;
    while (fgets(buf, sizeof buf, f)) {
        if (!strncmp(buf, "RULE ", 5)) { replay_rule(buf); continue; }
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
        if (n_block < 512) block[n_block++] = xstrdup(buf);
    }
    fclose(f);
    printf("catch_diff_test: %d ball-rule states and %d catch cases replayed (%d skipped upstream), %d fields compared. Mismatches: %d\n",
           rules_checked, cases_checked, cases_skipped, fields_checked, failures);
    return failures ? 1 : 0;
}
