/* The one save of a mystery encounter. Upstream saves at the start of an encounter (EncounterPhase's saveAll before the question is shown); this port saves at the
 * same moment, written when the encounter's first question is on the screen and nothing has been chosen (gc_run_can_save), and a load comes back to that
 * question: the screen is set up again from the RNG it was first set up from, so what follows is what would have followed without the save.
 *
 * For every upstream encounter the run can be given (the forced encounter of the developer hook, wave 3): a save is allowed at the question and refused as soon as
 * an answer was taken; the bytes load; the loaded run shows the same question (options, which are open, the texts' arguments) with the events of a new encounter
 * (WAVE, MYSTERY, SAVE_POINT; nobody is sent out); saving it again gives the same bytes; and the same answer given to both leads to the same place (the next screen, money,
 * RNG, party, enemy). The whole-run version of this (a save and a load at every encounter's question, the outcome unchanged) is `gcplay --auto N --save-test --force-mystery T`
 * in the Makefile's me-save-soak. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_run run, back;
static uint8_t buf[GC_SAVE_MAX], buf2[GC_SAVE_MAX];
static int checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr, "mystery save line %d (type %d, mode %d): %s\n", __LINE__, cur_type, cur_mode, #x); exit(1); } } while (0)
static int cur_type = -1;
static int cur_mode = GAME_MODES_CLASSIC;   /* the mode of the runs the test starts: Classic, then Challenge Mode (upstream: hasMysteryEncounters is CLASSIC and CHALLENGE) */

static const int starters[5] = {SPECIES_ID_CATERPIE, SPECIES_ID_WEEDLE, SPECIES_ID_PIDGEY, SPECIES_ID_RATTATA, SPECIES_ID_MAGNEMITE};

static void start(gc_run *r, const char *seed)
{
    const char *why = NULL;
    memset(r, 0, sizeof *r);
    gc_run_new(r, cur_mode, seed, 777);
    gc_starter s[5];
    memset(s, 0, sizeof s);
    for (int i = 0; i < 5; i++) {
        s[i].species = starters[i];
        s[i].tera_type = -1;
        s[i].moves[0] = MOVE_ID_TACKLE;
        memset(s[i].ivs, 15, sizeof s[i].ivs);
    }
    if (!gc_run_set_starters(r, s, 5, &why)) { fprintf(stderr, "starters: %s\n", why ? why : "?"); exit(1); }
    if (cur_mode == GAME_MODES_CHALLENGE) {   /* a Challenge Mode run with one of its challenges on: the save carries the values, the load puts them back */
        r->challenges.value[CHALLENGES_FRESH_START] = 1;
        gc_challenge_sync(r);
    }
    gc_event ev;
    while (gc_run_next_event(r, &ev)) {}
    gc_item_add(r, "LEFTOVERS", -1, r->party[0].b.id, 1);   /* an item for battle-core's list to carry */
    gc_items_to_battle(r);
}

/* the encounter's first question of wave 3, as a run reaches it after the second battle */
static bool reach_question(int type)
{
    start(&run, "me-save");
    gc_debug_force_mystery(type, 3);
    run.bt.battle_rng = (pr_rng){3.0, 0.25, 0.5, 0.75, 7.0};   /* what the last battle left in battle-core: stale at the question (nothing reads it before the next battle seeds it), but part of the state */
    run.bt.battle_rng_live = true;
    gc_rq_reset(&gc_int(&run)->rq);   /* the starters' first COMMAND screen is left behind */
    run.wave = 2;
    gc_new_battle(&run);
    gc_run_until_screen(&run);
    gc_debug_force_mystery(-1, 1);
    return run.battle_type == BATTLE_TYPE_MYSTERY_ENCOUNTER && run.screen.kind == GC_SCREEN_MYSTERY && run.screen.mystery_type == type;
}

/* the answer the host driver test's stand-in gives: the first option the run allows, the first party member its list accepts */
static gc_choice answer_of(const gc_run *r)
{
    const gc_screen *s = gc_run_screen(r);
    gc_choice c;
    memset(&c, 0, sizeof c);
    c.kind = GC_CH_MYSTERY;
    for (int i = 0; i < s->n_options && i < 8; i++)
        if (s->option_ok[i] && !(s->option_pick[i] && s->mystery_type == GC_ME_GLOBAL_TRADE_SYSTEM && i == 2)) { c.a = i; break; }
    if (s->option_pick[c.a]) {
        for (int i = 0; i < r->n_party; i++) if (gc_mystery_member_ok(r, c.a, i)) { c.b = i; break; }
    } else {
        for (int i = 0; i < r->n_party; i++) if (r->party[i].b.hp > 0) { c.b = i; break; }
    }
    return c;
}

static void same_question(const gc_run *a, const gc_run *b)
{
    const gc_screen *x = gc_run_screen(a), *y = gc_run_screen(b);
    CHECK(x->kind == y->kind && x->mystery_type == y->mystery_type && x->mystery_tier == y->mystery_tier && x->n_options == y->n_options);
    CHECK(x->me_stage == y->me_stage && x->n_me_args == y->n_me_args);
    for (int i = 0; i < 8; i++) CHECK(x->option_ok[i] == y->option_ok[i] && x->option_pick[i] == y->option_pick[i]);
    for (int i = 0; i < 3; i++) CHECK(x->mc_trainer[i] == y->mc_trainer[i] && x->mc_variant[i] == y->mc_variant[i]);
    for (int i = 0; i < x->n_me_args; i++) {
        CHECK(!strcmp(x->me_args[i].name ? x->me_args[i].name : "", y->me_args[i].name ? y->me_args[i].name : ""));
        CHECK(x->me_args[i].v == y->me_args[i].v);
        CHECK(!strcmp(x->me_args[i].s ? x->me_args[i].s : "", y->me_args[i].s ? y->me_args[i].s : ""));
    }
    CHECK(a->wave == b->wave && a->biome == b->biome && a->money == b->money && a->n_party == b->n_party && a->n_items == b->n_items);
    /* battle-core's view of the party and its items is what a fresh question has: a fight, a learn dialog or an item effect after the answer reads them */
    CHECK(a->bt.n_party[0] == b->bt.n_party[0] && a->bt.n_party[0] == (uint8_t)a->n_party && a->bt.n_mods[0] == b->bt.n_mods[0] && a->bt.n_mods[0] > 0 && a->bt.n_mods[1] == b->bt.n_mods[1]);
    for (int i = 0; i < a->n_party; i++) CHECK(a->bt.party[0][i] == &a->party[a->party_order[i]].b && b->bt.party[0][i] == &b->party[b->party_order[i]].b);
    CHECK(a->mystery_type == b->mystery_type && a->mystery_tier == b->mystery_tier && a->n_mystery_history == b->n_mystery_history);
    /* the not-ported counters: a setup that uses a stand-in (Fight or Flight's boss) counts it once in the saved run and, the load's setup being the same, once in the loaded one */
    CHECK(!memcmp(a->unported, b->unported, sizeof a->unported) && !memcmp(a->stubs, b->stubs, sizeof a->stubs) && !memcmp(a->run_core_unported, b->run_core_unported, sizeof a->run_core_unported));
    CHECK(a->run.mode == b->run.mode && a->run.mode == cur_mode && !memcmp(&a->challenges, &b->challenges, sizeof a->challenges)
          && a->run.challenge_flags == b->run.challenge_flags && a->run.has_challenges == b->run.has_challenges && a->run.has_challenges == (cur_mode == GAME_MODES_CHALLENGE));
    CHECK(!memcmp(&a->run.rng, &b->run.rng, sizeof a->run.rng));   /* the screen's setup draws nothing from the run's RNG: the loaded one is where the saved one was */
    CHECK(!memcmp(&a->bt.battle_rng, &b->bt.battle_rng, sizeof a->bt.battle_rng) && a->bt.battle_rng_live == b->bt.battle_rng_live && a->bt.battle_rng.c == 3.0);   /* nor from the battle's, and what the last battle left there is kept */
    CHECK(!memcmp(&a->bt.rng, &b->bt.rng, sizeof a->bt.rng) && a->bt.rng_live == b->bt.rng_live && a->bt.rng_live);   /* and battle-core's mirror of the run's RNG, which the stress diagnostic folds */
    for (int i = 0; i < a->n_party; i++) {
        CHECK(a->party_order[i] == b->party_order[i]);
        CHECK(a->party[i].b.species == b->party[i].b.species && a->party[i].b.level == b->party[i].b.level && a->party[i].b.hp == b->party[i].b.hp);
    }
}

/* CRC-32 as the save's header keeps it, to mend a deliberately damaged body */
static uint32_t crc32_of(const uint8_t *p, size_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        c ^= p[i];
        for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (uint32_t)-(int32_t)(c & 1));
    }
    return ~c;
}
static uint32_t fnv(const char *s)
{
    uint32_t h = 2166136261u;
    while (*s) { h ^= (uint8_t)*s++; h *= 16777619u; }
    return h;
}

/* the one-byte marker of a save made at a question: its section header is the name's hash and the length 1 */
static long marker_at(const uint8_t *p, size_t len)
{
    const uint32_t h = fnv("me_question"), one = 1;
    for (size_t i = 20; i + 9 <= len; i++) if (!memcmp(p + i, &h, 4) && !memcmp(p + i + 4, &one, 4)) return (long)(i + 8);
    return -1;
}
static void mend(uint8_t *p, size_t len)
{
    const uint32_t body = (uint32_t)(len - 20), crc = crc32_of(p + 20, len - 20);
    memcpy(p + 12, &body, 4);
    memcpy(p + 16, &crc, 4);
}

static void one_type(int type, int *taken_ok)
{
    cur_type = type;
    if (!reach_question(type)) return;   /* the encounter's own rules (a party, money, items) kept it from this run */
    gc_event ev;
    while (gc_run_next_event(&run, &ev)) {}

    CHECK(gc_run_can_save(&run));
    const size_t len = gc_run_save(&run, buf, sizeof buf);
    CHECK(len > 20 && len < GC_SAVE_MAX);

    const char *why = NULL;
    memset(&back, 0xA5, sizeof back);
    CHECK(gc_run_load(&back, buf, len, &why));
    CHECK(back.load_dropped == 0);
    same_question(&run, &back);

    /* the events of the encounter's start, and nobody sent out */
    bool wave = false, mystery = false, save_point = false;
    int n_ev = 0;
    while (gc_run_next_event(&back, &ev)) {
        n_ev++;
        if (ev.kind == GC_EV_WAVE) { CHECK(ev.a == back.wave && ev.c == BATTLE_TYPE_MYSTERY_ENCOUNTER && !mystery); wave = true; }
        else if (ev.kind == GC_EV_MYSTERY) { CHECK(wave && ev.a == type && ev.b == back.mystery_tier && ev.c == back.wave); mystery = true; }
        else if (ev.kind == GC_EV_SAVE_POINT) { CHECK(mystery); save_point = true; }
        else CHECK(ev.kind != GC_EV_SUMMON && ev.kind != GC_EV_TRAINER);
    }
    CHECK(wave && mystery && save_point);

    /* it can be saved again, to the same bytes: the load kept what the save needs */
    CHECK(gc_run_can_save(&back));
    const size_t len2 = gc_run_save(&back, buf2, sizeof buf2);
    CHECK(len2 == len && !memcmp(buf, buf2, len));

    /* the same answer, the same consequence */
    const gc_choice c = answer_of(&run);
    const gc_choice c2 = answer_of(&back);
    CHECK(c.a == c2.a && c.b == c2.b);
    const char *w1 = NULL, *w2 = NULL;
    const bool ok1 = gc_run_choose(&run, &c, &w1), ok2 = gc_run_choose(&back, &c2, &w2);
    CHECK(ok1 == ok2);
    while (gc_run_next_event(&run, &ev)) {}
    while (gc_run_next_event(&back, &ev)) {}
    if (ok1) {
        (*taken_ok)++;
        CHECK(run.screen.kind == back.screen.kind && run.screen.me_stage == back.screen.me_stage);
        CHECK(run.money == back.money && run.n_party == back.n_party && run.n_enemy == back.n_enemy && run.n_items == back.n_items);
        CHECK(!memcmp(&run.run.rng, &back.run.rng, sizeof run.run.rng));
        for (int i = 0; i < run.n_party; i++) CHECK(run.party[i].b.species == back.party[i].b.species && run.party[i].b.level == back.party[i].b.level);
        for (int i = 0; i < run.n_enemy; i++) CHECK(run.enemy[i].b.species == back.enemy[i].b.species && run.enemy[i].b.level == back.enemy[i].b.level);
        /* an answer ended the encounter's save point, in the run and in the loaded one (an answer that leaves the encounter goes on to the next wave,
         * whose own first COMMAND screen is a save point: that is another wave's) */
        CHECK(run.wave == back.wave);
        if (run.wave == 3) {
            CHECK(!gc_run_can_save(&run) && !gc_run_can_save(&back));
            CHECK(gc_run_save(&run, buf2, sizeof buf2) == 0 && gc_run_save(&back, buf2, sizeof buf2) == 0);
        } else {
            CHECK(run.screen.kind == GC_SCREEN_COMMAND && gc_run_can_save(&run) == gc_run_can_save(&back));
        }
    } else {
        /* a refused answer leaves the question, and its save, as they were */
        CHECK(gc_run_can_save(&run) && gc_run_can_save(&back));
    }
}

static void damaged(void)
{
    cur_type = GC_ME_SLUMBERING_SNORLAX;
    CHECK(reach_question(GC_ME_SLUMBERING_SNORLAX));
    gc_event ev;
    while (gc_run_next_event(&run, &ev)) {}
    const size_t len = gc_run_save(&run, buf, sizeof buf);
    CHECK(len > 20);
    const char *why = NULL;
    const long at = marker_at(buf, len);
    CHECK(at > 0 && buf[at] == 1);

    /* the marker says something else: refused, and the run is left empty */
    memcpy(buf2, buf, len);
    buf2[at] = 2;
    mend(buf2, len);
    memset(&back, 0xA5, sizeof back);
    CHECK(!gc_run_load(&back, buf2, len, &why));

    /* without the marker the same bytes are an ordinary battle save of a run with no enemy: refused */
    const uint32_t h = fnv("me_question");
    memcpy(buf2, buf, len);
    uint32_t other = h ^ 1u;   /* the section's name becomes one the loader does not know */
    memcpy(buf2 + at - 8, &other, 4);
    mend(buf2, len);
    memset(&back, 0xA5, sizeof back);
    CHECK(!gc_run_load(&back, buf2, len, &why));

    /* a save made at a battle's first command that carries the marker (a damaged file, or a build that wrote it where it should not): not a question, refused */
    start(&run, "me-save");
    gc_run_until_screen(&run);
    CHECK(run.screen.kind == GC_SCREEN_COMMAND && gc_run_can_save(&run));
    const size_t blen = gc_run_save(&run, buf, sizeof buf);
    CHECK(blen > 20 && blen + 9 <= sizeof buf && marker_at(buf, blen) < 0);
    memset(&back, 0xA5, sizeof back);
    CHECK(gc_run_load(&back, buf, blen, &why));   /* the battle save itself loads */
    memcpy(buf2, buf, blen);
    memcpy(buf2 + blen, &h, 4);
    const uint32_t one = 1;
    memcpy(buf2 + blen + 4, &one, 4);
    buf2[blen + 8] = 1;
    mend(buf2, blen + 9);
    memset(&back, 0xA5, sizeof back);
    CHECK(!gc_run_load(&back, buf2, blen + 9, &why));
}

int main(void)
{
    int exercised = 0, answered = 0;
    /* Classic, then Challenge Mode (a run with Fresh Start on): both have encounters upstream, so both save at the question */
    const int modes[2] = {GAME_MODES_CLASSIC, GAME_MODES_CHALLENGE};
    for (int m = 0; m < 2; m++) {
        cur_mode = modes[m];
        int ex = 0, ans = 0;
        for (int type = 0; type < GC_ME_UPSTREAM_COUNT; type++) {
            const int before = checks;
            one_type(type, &ans);
            if (checks != before) ex++;
        }
        /* the encounters this fixed party and money allow are most of them: a test that skipped them all would prove nothing */
        CHECK(ex >= 24);
        CHECK(ans >= 22);
        if (m == 0) { exercised = ex; answered = ans; }
    }
    cur_mode = GAME_MODES_CLASSIC;
    damaged();
    gc_debug_force_mystery(-1, 1);
    printf("mystery save: %d checks passed, %d of %d encounters exercised, %d answered (Classic and Challenge Mode)\n", checks, exercised, GC_ME_UPSTREAM_COUNT, answered);
    return 0;
}
