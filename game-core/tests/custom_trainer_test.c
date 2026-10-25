/* A custom team in place of an ordinary trainer's party (gc/custom.h, the trainer() hook), checked on seeded runs.
 *  - off is identical: with the hook installed but answering no, a seeded run is the one with no hooks, bit for bit;
 *  - the hook is asked only on an ordinary random single trainer wave (never a double, a boss, the final wave, Classic's wave 20);
 *  - a team replaces the party at the ordinary levels, with its moves, ability slot, shiny tier, passive and held items, the wave-scaled items
 *    are not given to it, and the ordinary trainer's money and AI stay;
 *  - the run's random numbers are the same as with the ordinary party once the battle starts (the team draws nothing);
 *  - the trainer's name is plain text in every message of the battle, and it comes back after a save and a load once the app sets it;
 *  - a team game-core cannot build leaves the ordinary party; gc_custom_team_check's refusals.
 * Each fixture fails on a game-core without the hook (this file would not link). */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_run run, ctl, back;
static gc_system sys, ctl_sys, back_sys;
static int checks;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "custom trainer FAIL line %d: %s\n", __LINE__, #x); exit(1); } checks++; } while (0)

static uint64_t H;
static void mix(uint64_t v) { H ^= v + 0x9E3779B97F4A7C15ull + (H << 6) + (H >> 2); H *= 1099511628211ull; }
static void mix_str(const char *s) { if (s) while (*s) mix((uint8_t)*s++); else mix(0xFF); }
static void hash_event(const gc_event *e)
{
    mix(e->kind); mix((uint64_t)(uint32_t)e->a); mix((uint64_t)(uint32_t)e->b); mix((uint64_t)(uint32_t)e->c);
    mix_str(e->msg.key);
    for (int i = 0; i < e->msg.n_args; i++) { mix_str(e->msg.args[i].name); mix(e->msg.args[i].kind); mix_str(e->msg.args[i].s); }
}
static void mix_rng(const pr_rng *g)
{
    uint64_t v;
    const double d[5] = {g->c, g->s0, g->s1, g->s2, g->n};
    for (int i = 0; i < 5; i++) { memcpy(&v, &d[i], sizeof v); mix(v); }
}
static uint64_t rng_hash(const gc_run *r) { H = 1469598103934665603ull; mix_rng(&r->run.rng); return H; }

static void new_run_in(gc_run *r, gc_system *s, const char *seed, const gc_custom_hooks *hooks)
{
    memset(r, 0, sizeof *r);
    gc_system_init_defaults(s);
    gc_run_new(r, GAME_MODES_CLASSIC, seed, 7);
    if (hooks) gc_run_set_custom(r, hooks);
    gc_starter team[3];
    memset(team, 0, sizeof team);
    const int species[3] = {SPECIES_ID_BULBASAUR, SPECIES_ID_CHARMANDER, SPECIES_ID_SQUIRTLE};
    for (int i = 0; i < 3; i++) {
        team[i].species = (uint16_t)species[i];
        team[i].tera_type = -1;
        for (int k = 0; k < 6; k++) team[i].ivs[k] = 15;
    }
    const char *why;
    CHECK(gc_run_set_starters(r, team, 3, &why));
    r->sys = s;
    for (int i = 0; i < r->n_party; i++) { r->party[i].b.level = 100; gc_mon_recalc(r, &r->party[i]); r->party[i].b.hp = r->party[i].b.stats[0]; }
}

static void heal_party_in(gc_run *r)
{
    for (int i = 0; i < r->n_party; i++) r->party[i].b.hp = r->party[i].b.stats[0];
}

/* One answer of the plain bot, the same as custom_hooks_test's. `fn` sees every event. */
static void (*on_event)(const gc_event *e);
static bool bot_step(gc_run *r, bool hashing)
{
    gc_event e;
    while (gc_run_next_event(r, &e)) { if (hashing) hash_event(&e); if (on_event) on_event(&e); }
    const gc_screen *s = gc_run_screen(r);
    gc_choice c;
    memset(&c, 0, sizeof c);
    const char *why;
    switch (s->kind) {
    case GC_SCREEN_COMMAND:
        heal_party_in(r);
        c.kind = GC_CH_FIGHT;
        c.b = -1;
        for (int i = 0; i < 4; i++) if (s->move_ok[i]) { c.a = (int16_t)i; break; }
        break;
    case GC_SCREEN_TARGET: c.kind = GC_CH_TARGET; c.a = -1; for (int k = 2; k < 4; k++) if (s->target_ok[k]) { c.a = k; break; } if (c.a < 0) for (int k = 0; k < 2; k++) if (s->target_ok[k]) { c.a = k; break; } break;
    case GC_SCREEN_SWITCH:
        c.kind = GC_CH_PICK;
        c.a = -1;
        for (int i = 0; i < r->n_party; i++) if (s->pick_ok[i]) { c.a = (int16_t)i; break; }
        break;
    case GC_SCREEN_LEARN_MOVE: c.kind = GC_CH_FORGET; c.a = -1; break;
    case GC_SCREEN_EVOLVE: c.kind = GC_CH_CANCEL; c.a = 1; break;
    case GC_SCREEN_REWARD: c.kind = GC_CH_SKIP; break;
    case GC_SCREEN_BIOME: c.kind = GC_CH_BIOME; break;
    case GC_SCREEN_MYSTERY: c.kind = GC_CH_MYSTERY; c.b = 0; break;
    case GC_SCREEN_CHECK_SWITCH: c.kind = GC_CH_CONFIRM; c.a = 0; break;
    case GC_SCREEN_CUSTOM_OFFER: c.kind = GC_CH_CONFIRM; c.a = 0; break;
    default: return false;
    }
    return gc_run_choose(r, &c, &why);
}

/* ---------------------------------------------------------------- the hook */
static int calls, accepted_calls;
static bool answer_yes;
static int bad_calls;
static int last_wave;
static gc_custom_team the_team;
static int ordinary_n, ordinary_level0, ordinary_level_last;
static uint16_t ordinary_species0;
static uint32_t ordinary_id0;

static bool h_trainer(void *user, const gc_run *r, const gc_custom_encounter *enc, gc_custom_team *team)
{
    (void)user;
    calls++;
    last_wave = enc->wave;
    if (r->battle_type != BATTLE_TYPE_TRAINER || r->double_battle || enc->kind != GC_ENC_TRAINER) bad_calls++;
    if (enc->wave == 20 || pr_run_is_wave_final(r->run.mode, enc->wave)) bad_calls++;
    for (int i = 0; i < r->n_enemy; i++) if (r->enemy[i].b.is_boss) bad_calls++;
    if (enc->wave != r->wave || enc->level != r->enemy[0].b.level || enc->species != r->enemy[0].b.species) bad_calls++;
    ordinary_n = r->n_enemy;
    ordinary_level0 = r->enemy[0].b.level;
    ordinary_level_last = r->enemy[r->n_enemy - 1].b.level;
    ordinary_species0 = r->enemy[0].b.species;
    ordinary_id0 = r->enemy[0].b.id;
    if (!answer_yes) return false;
    accepted_calls++;
    *team = the_team;
    return true;
}
static const gc_custom_hooks hooks_no = {NULL, NULL, NULL, NULL, NULL, h_trainer};

/* a team of three: Gyro's first members' shape (a shiny with a tier, a passive for all, held items) */
static void make_team(void)
{
    memset(&the_team, 0, sizeof the_team);
    the_team.name = "Gyro Zeppeli";
    the_team.passive = ABILITY_ID_SPEED_BOOST;
    the_team.n = 3;
    gc_custom_team_member *a = &the_team.member[0], *b = &the_team.member[1], *c = &the_team.member[2];
    a->species = SPECIES_ID_PIKACHU;
    a->moves[0] = MOVE_ID_THUNDERBOLT; a->moves[1] = MOVE_ID_QUICK_ATTACK;
    a->shiny = true; a->variant = 1;
    a->n_items = 2;
    a->items[0] = (gc_custom_team_item){"LEFTOVERS", -1, 1};
    a->items[1] = (gc_custom_team_item){"LEFTOVERS", -1, 1};   /* equal items stack */
    b->species = SPECIES_ID_ONIX;
    b->ability_slot = 2;
    b->moves[0] = MOVE_ID_EARTHQUAKE; b->moves[2] = MOVE_ID_ROCK_SLIDE;
    c->species = SPECIES_ID_SNORLAX;
    c->moves[0] = MOVE_ID_BODY_SLAM;
    c->n_items = 1;
    c->items[0] = (gc_custom_team_item){"ATTACK_TYPE_BOOSTER", 0, 1};
}

static uint64_t play_hash(const char *seed, const gc_custom_hooks *hooks, int steps, int *waves)
{
    new_run_in(&run, &sys, seed, hooks);
    H = 1469598103934665603ull;
    on_event = NULL;
    int i = 0;
    for (; i < steps; i++) if (!bot_step(&run, true)) break;
    mix(i);
    mix((uint64_t)run.wave); mix((uint64_t)(int64_t)run.money); mix_rng(&run.run.rng);
    for (int k = 0; k < run.n_enemy; k++) { mix(run.enemy[k].b.species); mix(run.enemy[k].b.id); }
    *waves = run.wave;
    return H;
}

static void test_identical(void)
{
    const char *seeds[] = {"trainer-1", "trainer-2", "trainer-3", "trainer-4", "trainer-5", "trainer-6"};
    int asked = 0;
    for (int s = 0; s < 6; s++) {
        int w0, w1;
        calls = 0; answer_yes = false;
        const uint64_t a = play_hash(seeds[s], NULL, 1800, &w0);
        const uint64_t b = play_hash(seeds[s], &hooks_no, 1800, &w1);
        CHECK(a == b);
        CHECK(w0 == w1 && w0 >= 8);
        asked += calls;
    }
    CHECK(asked > 5);
    CHECK(bad_calls == 0);
}

/* the team becomes the party */
static const gc_item *item_of(const gc_run *r, uint32_t id, const char *key, int arg)
{
    for (int i = 0; i < r->n_enemy_items; i++)
        if (r->enemy_items[i].pokemon_id == id && !strcmp(r->enemy_items[i].key, key) && r->enemy_items[i].arg == arg) return &r->enemy_items[i];
    return NULL;
}

static bool saw_appeared_with_name, saw_send_out_with_name, saw_trainer_event;
static void watch_events(const gc_event *e)
{
    if (e->kind == GC_EV_TRAINER) saw_trainer_event = true;
    if (e->kind != GC_EV_MESSAGE || !e->msg.key) return;
    if (!strcmp(e->msg.key, "battle:trainerAppeared") && e->msg.n_args == 1 && e->msg.args[0].kind == PR_ARG_STR && !strcmp(e->msg.args[0].s, "Gyro Zeppeli")) saw_appeared_with_name = true;
    if (!strcmp(e->msg.key, "battle:trainerSendOut") && e->msg.n_args >= 1 && e->msg.args[0].kind == PR_ARG_STR && !strcmp(e->msg.args[0].s, "Gyro Zeppeli")) saw_send_out_with_name = true;
}

static void test_team_replaces_party(void)
{
    make_team();
    answer_yes = true;
    calls = accepted_calls = 0;
    on_event = NULL;
    /* the first eligible wave, a team there; the control run with no hook reaches the same wave */
    new_run_in(&run, &sys, "trainer-team-1", &(gc_custom_hooks){NULL, NULL, NULL, NULL, NULL, h_trainer});
    saw_appeared_with_name = saw_send_out_with_name = saw_trainer_event = false;
    on_event = watch_events;
    int steps = 0;
    while (!accepted_calls && steps++ < 4000) CHECK(bot_step(&run, false));
    CHECK(accepted_calls == 1);
    const int wave = last_wave;
    CHECK(bad_calls == 0);
    /* the hook ran inside the step that created the wave: look at the run as that step left it */
    CHECK(run.battle_type == BATTLE_TYPE_TRAINER && run.wave == wave);
    CHECK(run.n_enemy == 3);
    CHECK(run.enemy[0].b.species == SPECIES_ID_PIKACHU && run.enemy[1].b.species == SPECIES_ID_ONIX && run.enemy[2].b.species == SPECIES_ID_SNORLAX);
    CHECK(ordinary_n >= 1);
    /* levels: the ordinary party's */
    const int top = ordinary_level_last;
    CHECK(run.enemy[2].b.level == (ordinary_n >= 3 ? top : top));
    CHECK(run.enemy[0].b.level <= run.enemy[2].b.level);
    for (int i = 0; i < 3; i++) {
        CHECK(run.enemy[i].b.has_trainer && run.enemy[i].b.trainer_slot == TRAINER_SLOT_TRAINER);
        CHECK(!run.enemy[i].b.is_boss);
        CHECK(run.enemy[i].b.hp > 0);
    }
    CHECK(run.enemy[0].b.n_moves == 2 && run.enemy[0].b.moves[0] == MOVE_ID_THUNDERBOLT && run.enemy[0].b.moves[1] == MOVE_ID_QUICK_ATTACK);
    CHECK(run.enemy[1].b.n_moves == 2 && run.enemy[1].b.moves[0] == MOVE_ID_EARTHQUAKE && run.enemy[1].b.moves[1] == MOVE_ID_ROCK_SLIDE);
    CHECK(run.enemy[0].b.shiny && run.enemy[0].variant == 1 && !run.enemy[1].b.shiny);
    CHECK(run.enemy[1].ability_index == 2 && run.enemy[0].luck == 2 && run.enemy[1].luck == 0);
    for (int i = 0; i < 3; i++) {
        CHECK(run.enemy[i].b.custom_passive == ABILITY_ID_SPEED_BOOST);
        CHECK(run.enemy[i].b.passive == ABILITY_ID_SPEED_BOOST);
    }
    for (int i = 0; i < 3; i++) for (int j = i + 1; j < 3; j++) CHECK(run.enemy[i].b.id != run.enemy[j].b.id);
    CHECK(run.enemy[0].b.id == ordinary_id0);
    /* items: the team's, stacked; nobody else holds the wave's generated ones */
    const gc_item *lo = item_of(&run, run.enemy[0].b.id, "LEFTOVERS", -1);
    CHECK(lo && lo->stack == 2);
    CHECK(item_of(&run, run.enemy[2].b.id, "ATTACK_TYPE_BOOSTER", 0));
    for (int i = 0; i < run.n_enemy_items; i++) {
        const gc_item *it = &run.enemy_items[i];
        if (!it->pokemon_id) continue;
        CHECK(it->pokemon_id == run.enemy[0].b.id || it->pokemon_id == run.enemy[2].b.id);
    }
    /* the name, plain, in every message of the battle */
    for (int k = 0; k < GC_TNAME_COUNT_; k++) CHECK(run.trainer_names[k] && !strcmp(run.trainer_names[k], "Gyro Zeppeli"));
    CHECK(run.trainer_type >= 0);   /* the ordinary trainer type stays: money and AI keep working */
    /* finish the step's events: the intro names him */
    gc_event e;
    while (gc_run_next_event(&run, &e)) watch_events(&e);
    CHECK(saw_trainer_event && saw_appeared_with_name);
    CHECK(run.screen.kind == GC_SCREEN_COMMAND || run.screen.kind == GC_SCREEN_CHECK_SWITCH);
    for (int i = 0; i < 40 && !saw_send_out_with_name; i++) { if (!bot_step(&run, false)) break; }
    CHECK(saw_send_out_with_name);

    /* the same seed with the hook answering no: the same wave, and the run's random numbers are the same at the save point */
    answer_yes = false;
    calls = accepted_calls = 0;
    new_run_in(&ctl, &ctl_sys, "trainer-team-1", &(gc_custom_hooks){NULL, NULL, NULL, NULL, NULL, h_trainer});
    on_event = NULL;
    for (int i = 0; i < 4000 && ctl.wave < wave; i++) CHECK(bot_step(&ctl, false));
    CHECK(ctl.wave == wave && ctl.battle_type == BATTLE_TYPE_TRAINER);
    answer_yes = true;
    calls = accepted_calls = 0;
    new_run_in(&run, &sys, "trainer-team-1", &(gc_custom_hooks){NULL, NULL, NULL, NULL, NULL, h_trainer});
    for (int i = 0; i < 4000 && run.wave < wave; i++) CHECK(bot_step(&run, false));
    CHECK(run.wave == wave && accepted_calls == 1);
    CHECK(rng_hash(&run) == rng_hash(&ctl));
    CHECK(run.money == ctl.money);
    CHECK(run.trainer_type == ctl.trainer_type && run.trainer_variant == ctl.trainer_variant);
}

/* the eligibility rule on synthetic states */
static void test_wave_rule(void)
{
    new_run_in(&run, &sys, "trainer-rule", NULL);
    run.battle_type = BATTLE_TYPE_TRAINER;
    run.double_battle = false;
    run.n_enemy = 2;
    run.enemy[0].b.is_boss = run.enemy[1].b.is_boss = false;
    CHECK(gc_custom_trainer_wave(&run, 13));
    CHECK(!gc_custom_trainer_wave(&run, 20));   /* Classic's wave 20 */
    CHECK(!gc_custom_trainer_wave(&run, 200));  /* the final wave */
    CHECK(!gc_custom_trainer_wave(&run, 8));    /* a fixed rival battle */
    CHECK(!gc_custom_trainer_wave(&run, 182));  /* a fixed Elite Four battle */
    run.double_battle = true;
    CHECK(!gc_custom_trainer_wave(&run, 13));
    run.double_battle = false;
    run.enemy[1].b.is_boss = true;
    CHECK(!gc_custom_trainer_wave(&run, 13));
    run.enemy[1].b.is_boss = false;
    run.n_enemy = 0;
    CHECK(!gc_custom_trainer_wave(&run, 13));
    run.n_enemy = 2;
    run.battle_type = BATTLE_TYPE_WILD;
    CHECK(!gc_custom_trainer_wave(&run, 13));
}

/* enemy_items for a team's battle: the generated items of its members go, the tokens stay */
static void test_items_replaced(void)
{
    make_team();
    answer_yes = true;
    calls = accepted_calls = 0;
    new_run_in(&run, &sys, "trainer-team-1", &(gc_custom_hooks){NULL, NULL, NULL, NULL, NULL, h_trainer});
    on_event = NULL;
    for (int i = 0; i < 4000 && !accepted_calls; i++) CHECK(bot_step(&run, false));
    CHECK(accepted_calls == 1);
    gc_item *fake = &run.enemy_items[run.n_enemy_items++];
    memset(fake, 0, sizeof *fake);
    fake->key = pr_item_names[0];
    fake->arg = -1;
    fake->stack = 3;
    fake->pokemon_id = run.enemy[1].b.id;            /* a generated item of the Onix, who holds nothing in the team */
    gc_item *token = &run.enemy_items[run.n_enemy_items++];
    memset(token, 0, sizeof *token);
    token->key = pr_item_names[1];
    token->arg = -1;
    token->stack = 1;
    token->pokemon_id = 0;                            /* a token: no holder */
    gc_custom_team_items(&run);
    for (int i = 0; i < run.n_enemy_items; i++) CHECK(run.enemy_items[i].pokemon_id != run.enemy[1].b.id);
    bool kept = false;
    for (int i = 0; i < run.n_enemy_items; i++) kept |= run.enemy_items[i].pokemon_id == 0 && run.enemy_items[i].key == pr_item_names[1];
    CHECK(kept);
    CHECK(item_of(&run, run.enemy[0].b.id, "LEFTOVERS", -1)->stack == 2);   /* the team's items are not doubled by a second call */
}

/* the name belongs to its battle: the next trainer battle with no team is an ordinary trainer's */
static void test_name_does_not_leak(void)
{
    make_team();
    answer_yes = true;
    calls = accepted_calls = 0;
    new_run_in(&run, &sys, "trainer-team-1", &(gc_custom_hooks){NULL, NULL, NULL, NULL, NULL, h_trainer});
    on_event = NULL;
    for (int i = 0; i < 4000 && !accepted_calls; i++) CHECK(bot_step(&run, false));
    CHECK(accepted_calls == 1);
    const int wave = run.wave;
    answer_yes = false;
    bool found = false;
    for (int i = 0; i < 12000 && !found; i++) {
        if (!bot_step(&run, false)) break;
        if (run.wave > wave && run.battle_type == BATTLE_TYPE_TRAINER) found = true;
    }
    CHECK(found);
    CHECK(run.trainer_names[0] && strcmp(run.trainer_names[0], "Gyro Zeppeli") != 0);
    CHECK(run.enemy[0].b.custom_passive == -1 && !run.enemy[0].b.passive_flag);
}

static uint64_t items_hash(const gc_run *r)
{
    H = 1469598103934665603ull;
    mix((uint64_t)r->n_enemy_items);
    for (int i = 0; i < r->n_enemy_items; i++) { mix_str(r->enemy_items[i].key); mix((uint64_t)(int64_t)r->enemy_items[i].arg); mix((uint64_t)r->enemy_items[i].stack); mix(r->enemy_items[i].pokemon_id ? 1 : 0); }
    return H;
}

/* the team's items belong to its battle too: the next trainer's items are the ones it would have had with no team before */
static void test_items_do_not_leak(void)
{
    make_team();
    uint64_t with_team = 0, without = 0;
    int wave_found[2] = {0, 0};
    for (int pass = 0; pass < 2; pass++) {
        answer_yes = pass == 0;
        calls = accepted_calls = 0;
        new_run_in(&run, &sys, "trainer-team-1", &(gc_custom_hooks){NULL, NULL, NULL, NULL, NULL, h_trainer});
        on_event = NULL;
        int first = 0;
        for (int i = 0; i < 4000 && !first; i++) { CHECK(bot_step(&run, false)); if (calls) first = run.wave; }
        answer_yes = false;
        bool found = false;
        for (int i = 0; i < 12000 && !found; i++) {
            if (!bot_step(&run, false)) break;
            if (run.wave > first && run.battle_type == BATTLE_TYPE_TRAINER) found = true;
        }
        CHECK(found);
        wave_found[pass] = run.wave;
        if (pass) without = items_hash(&run); else with_team = items_hash(&run);
    }
    CHECK(wave_found[0] == wave_found[1]);
    CHECK(with_team == without);
}

/* levels, ids and IVs of a team over an ordinary party of four at 10, 20, 30, 40 */
static void test_levels(void)
{
    answer_yes = false;
    calls = 0;
    new_run_in(&run, &sys, "trainer-team-1", &(gc_custom_hooks){NULL, NULL, NULL, NULL, NULL, h_trainer});
    on_event = NULL;
    for (int i = 0; i < 4000 && !calls; i++) CHECK(bot_step(&run, false));
    CHECK(calls == 1 && run.battle_type == BATTLE_TYPE_TRAINER);
    static gc_run snap;
    for (int size = 1; size <= 6; size++) {
        memcpy(&snap, &run, sizeof snap);
        snap.n_enemy = 4;
        for (int i = 0; i < 4; i++) {
            snap.enemy[i] = run.enemy[0];
            snap.enemy[i].b.level = 10 * (i + 1);
            snap.enemy[i].b.id = 1000u + (uint32_t)i;
            snap.enemy[i].b.nature = (uint8_t)i;
            for (int k = 0; k < 6; k++) snap.enemy[i].b.ivs[k] = (uint8_t)(i * 5 + k);
        }
        make_team();
        the_team.n = (uint8_t)size;
        for (int e = 3; e < size; e++) the_team.member[e] = the_team.member[2];
        CHECK(gc_custom_build_trainer(&snap, &the_team));
        CHECK(snap.n_enemy == size);
        for (int e = 0; e < size; e++) {
            const int want = size < 4 ? 10 * (4 - size + e + 1) : 10 * (e < 4 ? e + 1 : 4);
            CHECK(snap.enemy[e].b.level == want);
            CHECK(snap.enemy[e].b.nature == (e < 4 ? e : 3));
            CHECK(snap.enemy[e].b.ivs[2] == (e < 4 ? e * 5 + 2 : 3 * 5 + 2));
            if (e < 4) CHECK(snap.enemy[e].b.id == 1000u + (uint32_t)e);
            for (int f = 0; f < e; f++) CHECK(snap.enemy[e].b.id != snap.enemy[f].b.id);
        }
    }
}

static void test_save_load(void)
{
    make_team();
    answer_yes = true;
    calls = accepted_calls = 0;
    new_run_in(&run, &sys, "trainer-team-2", &(gc_custom_hooks){NULL, NULL, NULL, NULL, NULL, h_trainer});
    on_event = NULL;
    for (int i = 0; i < 4000 && !accepted_calls; i++) CHECK(bot_step(&run, false));
    CHECK(accepted_calls == 1);
    gc_event e;
    while (gc_run_next_event(&run, &e)) {}
    CHECK(gc_run_can_save(&run));
    static uint8_t buf[GC_SAVE_MAX];
    const size_t n = gc_run_save(&run, buf, sizeof buf);
    CHECK(n > 0);
    gc_system_init_defaults(&back_sys);
    memset(&back, 0, sizeof back);
    const char *why = "";
    gc_run_new(&back, GAME_MODES_CLASSIC, "other", 1);
    back.sys = &back_sys;
    CHECK(gc_run_load(&back, buf, n, &why));
    CHECK(back.n_enemy == 3 && back.battle_type == BATTLE_TYPE_TRAINER);
    for (int i = 0; i < 3; i++) {
        CHECK(back.enemy[i].b.species == run.enemy[i].b.species && back.enemy[i].b.id == run.enemy[i].b.id);
        CHECK(back.enemy[i].b.custom_passive == ABILITY_ID_SPEED_BOOST && back.enemy[i].b.passive == ABILITY_ID_SPEED_BOOST);
        CHECK(back.enemy[i].b.n_moves == run.enemy[i].b.n_moves);
    }
    CHECK(back.n_enemy_items == run.n_enemy_items);
    const gc_item *lo = item_of(&back, back.enemy[0].b.id, "LEFTOVERS", -1);
    CHECK(lo && lo->stack == 2);
    /* the name is the app's to set again: an ordinary trainer's text until then, the team's after */
    CHECK(strcmp(back.trainer_names[0] ? back.trainer_names[0] : "", "Gyro Zeppeli") != 0);
    gc_custom_set_trainer_name(&back, "Gyro Zeppeli");
    for (int k = 0; k < GC_TNAME_COUNT_; k++) CHECK(back.trainer_names[k] && !strcmp(back.trainer_names[k], "Gyro Zeppeli"));
    gc_custom_set_trainer_name(&back, NULL);
    CHECK(strcmp(back.trainer_names[0] ? back.trainer_names[0] : "", "Gyro Zeppeli") != 0);
}

/* a team game-core cannot build leaves the ordinary party and the ordinary name */
static void test_refused(void)
{
    make_team();
    the_team.member[1].moves[0] = 0;
    the_team.member[1].moves[2] = 0;   /* no move */
    answer_yes = true;
    calls = accepted_calls = 0;
    new_run_in(&run, &sys, "trainer-team-1", &(gc_custom_hooks){NULL, NULL, NULL, NULL, NULL, h_trainer});
    on_event = NULL;
    for (int i = 0; i < 4000 && !accepted_calls; i++) CHECK(bot_step(&run, false));
    CHECK(accepted_calls == 1 && run.battle_type == BATTLE_TYPE_TRAINER);
    CHECK(run.n_enemy == ordinary_n && run.enemy[0].b.species == ordinary_species0);
    CHECK(strcmp(run.trainer_names[0] ? run.trainer_names[0] : "", "Gyro Zeppeli") != 0);
}

static void test_check(void)
{
    const char *why;
    make_team();
    CHECK(gc_custom_team_check(&the_team, &why));
    CHECK(!gc_custom_team_check(NULL, &why));
    gc_custom_team t = the_team;
    t.n = 0; CHECK(!gc_custom_team_check(&t, &why));
    t = the_team; t.n = 7; CHECK(!gc_custom_team_check(&t, &why));
    t = the_team; t.name = ""; CHECK(!gc_custom_team_check(&t, &why));
    t = the_team; t.name = NULL; CHECK(!gc_custom_team_check(&t, &why));
    t = the_team; t.name = "012345678901234567890123456789012345678901234567"; CHECK(!gc_custom_team_check(&t, &why));   /* 48 characters */
    t = the_team; t.name = "01234567890123456789012345678901234567890123456"; CHECK(gc_custom_team_check(&t, &why));     /* 47 characters */
    t = the_team; t.passive = PR_ABILITY_COUNT; CHECK(!gc_custom_team_check(&t, &why));
    t = the_team; t.passive = -2; CHECK(!gc_custom_team_check(&t, &why));
    t = the_team; t.member[0].species = 0; CHECK(!gc_custom_team_check(&t, &why));
    t = the_team; t.member[0].form = 40; CHECK(!gc_custom_team_check(&t, &why));
    t = the_team; t.member[0].ability_slot = 3; CHECK(!gc_custom_team_check(&t, &why));
    t = the_team; t.member[0].variant = 3; CHECK(!gc_custom_team_check(&t, &why));
    t = the_team; t.member[1].variant = 1; CHECK(!gc_custom_team_check(&t, &why));   /* a tier without shiny */
    t = the_team; t.member[0].moves[0] = t.member[0].moves[1]; CHECK(!gc_custom_team_check(&t, &why));   /* repeated */
    t = the_team; t.member[0].moves[3] = PR_MOVE_COUNT; CHECK(!gc_custom_team_check(&t, &why));
    t = the_team; t.member[0].n_items = 6; CHECK(!gc_custom_team_check(&t, &why));
    t = the_team; t.member[0].items[0].key = "NOT_AN_ITEM"; CHECK(!gc_custom_team_check(&t, &why));
    t = the_team; t.member[0].items[0].key = NULL; CHECK(!gc_custom_team_check(&t, &why));
    t = the_team; t.member[0].items[0].stack = 0; CHECK(!gc_custom_team_check(&t, &why));
    t = the_team; t.member[0].items[0].stack = 100; CHECK(!gc_custom_team_check(&t, &why));
    t = the_team; t.passive = -1; CHECK(gc_custom_team_check(&t, &why));
}

int main(void)
{
    test_check();
    test_identical();
    test_team_replaces_party();
    test_wave_rule();
    test_items_replaced();
    test_name_does_not_leak();
    test_items_do_not_leak();
    test_levels();
    test_save_load();
    test_refused();
    printf("custom_trainer_test: %d checks, 0 failed\n", checks);
    return 0;
}
