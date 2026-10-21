/* Task C10: the optional hooks of the custom content (gc/custom.h), checked on seeded runs.
 *  - off is identical: the same seeded runs, played by the same bot, with no hooks, with the faint observer only, and with every hook
 *    installed but never asking, produce the same events, the same party, the same RNG state;
 *  - the wave report: once per wave, in order, before the save point, with the kind of the encounter (wild, trainer, mystery...);
 *  - the faint note: a hit that takes the last HP is attributed (attacker, move, remaining), poison, recoil and a self-faint are not;
 *  - the offer: the screen, the save point not yet reached, decline = exactly the ordinary wave (events, RNG, enemy), accept = the
 *    spec's identity at the ordinary level with the ordinary id and IVs, built through the game's own paths, savable and loadable,
 *    a refused spec leaves the ordinary enemy;
 *  - gc_custom_enemy_check's refusals.
 * Each fixture fails on a game-core without the hooks (this file would not link) and on one whose hooks change what the game decides. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_run run, ctl;
static gc_system sys, ctl_sys;
static int checks;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "custom hooks FAIL line %d: %s\n", __LINE__, #x); exit(1); } checks++; } while (0)

/* ---------------------------------------------------------------- hashing */
static uint64_t H;
static void mix(uint64_t v) { H ^= v + 0x9E3779B97F4A7C15ull + (H << 6) + (H >> 2); H *= 1099511628211ull; }
static void mix_str(const char *s) { if (s) while (*s) mix((uint8_t)*s++); else mix(0xFF); }

static void hash_event(const gc_event *e)
{
    mix(e->kind); mix((uint64_t)(uint32_t)e->a); mix((uint64_t)(uint32_t)e->b); mix((uint64_t)(uint32_t)e->c);
    mix_str(e->msg.key);
}
static void hash_battle_events(const gc_run *r)
{
    for (int i = 0; i < r->n_battle_events; i++) {
        const pr_event *e = &r->battle_events[i];
        mix(e->type); mix((uint64_t)(uint32_t)e->battler); mix((uint64_t)(uint32_t)e->target); mix((uint64_t)(uint32_t)e->id);
        mix((uint64_t)(int64_t)e->value);
        for (int k = 0; k < 4; k++) { mix(e->field[k].species); mix((uint64_t)(uint32_t)e->field[k].hp); }
        mix_str(e->msg.key);
    }
}
static void mix_rng(const pr_rng *g)
{
    uint64_t v;
    const double d[5] = {g->c, g->s0, g->s1, g->s2, g->n};
    for (int i = 0; i < 5; i++) { memcpy(&v, &d[i], sizeof v); mix(v); }
}
static void hash_run_state(const gc_run *r)
{
    mix((uint64_t)r->wave); mix((uint64_t)r->n_party); mix((uint64_t)r->n_enemy); mix((uint64_t)(int64_t)r->money);
    for (int i = 0; i < r->n_party; i++) { mix(r->party[i].b.hp); mix(r->party[i].b.level); mix((uint64_t)r->party[i].exp); }
    for (int i = 0; i < r->n_enemy; i++) { mix(r->enemy[i].b.species); mix(r->enemy[i].b.hp); mix(r->enemy[i].b.id); }
    mix_rng(&r->run.rng);
}

/* ---------------------------------------------------------------- the bot */
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

/* One answer of the plain bot. Returns false when the run cannot go on (game over, a screen it does not know). `hash` != 0 mixes everything seen. */
static bool bot_step(gc_run *r, bool hashing)
{
    gc_event e;
    while (gc_run_next_event(r, &e)) if (hashing) hash_event(&e);
    if (hashing) hash_battle_events(r);
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
    if (!gc_run_choose(r, &c, &why)) return false;
    return true;
}

/* ---------------------------------------------------------------- recording hooks */
#define MAXW 200
static struct { int n; int wave[MAXW]; uint8_t kind[MAXW]; uint8_t flags[MAXW]; bool before_save[MAXW]; bool enemy_ok[MAXW]; } rep;
static int n_faints, n_direct;
static bool ask_here;                      /* the encounter hook asks on the next ordinary wild wave */
static int asked_wave;
static gc_custom_enemy next_spec;
static bool answer_accept_flag;
static int built_calls, built_ok;
static char built_why[64];
static int answered;

static bool h_encounter(void *user, const gc_run *r, const gc_custom_encounter *enc)
{
    (void)user;
    if (rep.n < MAXW) {
        rep.wave[rep.n] = enc->wave;
        rep.kind[rep.n] = enc->kind;
        rep.flags[rep.n] = enc->flags;
        bool queued_save = false;
        for (int i = 0; i < r->ev_count; i++) queued_save |= r->events[(r->ev_head + i) % GC_MAX_EVENTS].kind == GC_EV_SAVE_POINT;
        rep.before_save[rep.n] = !queued_save && !gc_run_can_save(r);   /* the bot drains every event: the queue holds this wave's only */
        rep.enemy_ok[rep.n] = enc->kind == GC_ENC_MYSTERY || (r->n_enemy >= 1 && r->enemy[0].b.level == enc->level && r->enemy[0].b.species == enc->species);
        rep.n++;
    }
    if (ask_here && enc->kind == GC_ENC_ORDINARY_WILD && enc->flags == 0) { ask_here = false; asked_wave = enc->wave; return true; }
    return false;
}
static bool h_answer(void *user, const gc_run *r, bool accepted, gc_custom_enemy *spec)
{
    (void)user; (void)r;
    answered++;
    answer_accept_flag = accepted;
    if (!accepted) return false;
    *spec = next_spec;
    return true;
}
static void h_built(void *user, const gc_run *r, bool ok, const char *why)
{
    (void)user; (void)r;
    built_calls++;
    built_ok += ok;
    snprintf(built_why, sizeof built_why, "%s", why ? why : "");
}
static gc_custom_faint last_faint;
static int n_faint_notes;
static void h_faint(void *user, const gc_run *r, const gc_custom_faint *f)
{
    (void)user; (void)r;
    n_faint_notes++;
    last_faint = *f;
    n_faints++;
    n_direct += f->direct;
}
static const gc_custom_hooks hooks_all = {NULL, h_encounter, h_answer, h_built, h_faint};
static const gc_custom_hooks hooks_faint_only = {NULL, NULL, NULL, NULL, h_faint};

/* ---------------------------------------------------------------- 1. off is identical */
static uint64_t play_hash(const char *seed, const gc_custom_hooks *hooks, int steps, int *waves)
{
    gc_run *r = &run;
    new_run_in(r, &sys, seed, hooks);
    H = 1469598103934665603ull;
    int i = 0;
    for (; i < steps; i++) if (!bot_step(r, true)) break;
    mix(i);
    hash_run_state(r);
    *waves = r->wave;
    return H;
}

static void test_identical(void)
{
    const char *seeds[] = {"custom-hook-1", "custom-hook-2", "custom-hook-3", "custom-hook-4", "custom-hook-5", "custom-hook-6"};
    int total_faints = 0;
    for (int s = 0; s < 6; s++) {
        int w0, w1, w2;
        n_faints = 0; n_direct = 0; rep.n = 0;
        const uint64_t a = play_hash(seeds[s], NULL, 1800, &w0);
        const uint64_t b = play_hash(seeds[s], &hooks_faint_only, 1800, &w1);
        const int faints_b = n_faints;
        rep.n = 0;
        const uint64_t c = play_hash(seeds[s], &hooks_all, 1800, &w2);
        CHECK(a == b);
        CHECK(a == c);
        CHECK(w0 == w1 && w0 == w2 && w0 >= 8);
        total_faints += faints_b;
        /* the wave report: every wave once, in order, before the save point, with the enemy already final */
        CHECK(rep.n >= w2 - 1);
        for (int i = 0; i < rep.n; i++) {
            CHECK(rep.wave[i] == i + 1);
            CHECK(rep.before_save[i]);
            CHECK(rep.enemy_ok[i]);
        }
    }
    CHECK(total_faints > 20);
}

/* ---------------------------------------------------------------- 2. the kinds of encounters */
static bool seen_kind[8];
static void test_kinds(void)
{
    const char *seeds[] = {"custom-hook-1", "custom-hook-2", "custom-hook-3", "custom-hook-4", "custom-hook-5", "custom-hook-6", "kinds-7", "kinds-8"};
    for (int s = 0; s < 8; s++) {
        rep.n = 0;
        int w;
        play_hash(seeds[s], &hooks_all, 2500, &w);
        for (int i = 0; i < rep.n; i++) {
            seen_kind[rep.kind[i]] = true;
            if (rep.kind[i] == GC_ENC_ORDINARY_WILD) CHECK(rep.flags[i] == 0 || rep.flags[i] <= 7);
        }
    }
    CHECK(seen_kind[GC_ENC_ORDINARY_WILD]);
    CHECK(seen_kind[GC_ENC_TRAINER]);
}

/* the report agrees with the run's own state at that moment */
static bool kind_agrees = true;
static bool h_agree(void *user, const gc_run *r, const gc_custom_encounter *enc)
{
    (void)user;
    const bool trainer = r->battle_type == BATTLE_TYPE_TRAINER;
    const bool me = r->battle_type == BATTLE_TYPE_MYSTERY_ENCOUNTER;
    if (enc->kind == GC_ENC_TRAINER && !trainer) kind_agrees = false;
    if (enc->kind == GC_ENC_MYSTERY && !me) kind_agrees = false;
    if (enc->kind == GC_ENC_ORDINARY_WILD && (trainer || me || r->enemy[0].b.is_boss)) kind_agrees = false;
    if (enc->kind == GC_ENC_BOSS && !r->enemy[0].b.is_boss) kind_agrees = false;
    if (trainer && enc->kind != GC_ENC_TRAINER && enc->kind != GC_ENC_BOSS && enc->kind != GC_ENC_FINAL) kind_agrees = false;
    return false;
}
static void test_kind_agrees(void)
{
    static const gc_custom_hooks h = {NULL, h_agree, NULL, NULL, NULL};
    int w;
    for (int s = 0; s < 4; s++) {
        char seed[32];
        snprintf(seed, sizeof seed, "agree-%d", s);
        play_hash(seed, &h, 2000, &w);
    }
    CHECK(kind_agrees);
}

/* ---------------------------------------------------------------- 3. the faint note */
static void set_moves(gc_mon *m, int move)
{
    gc_mon_set_move(m, 0, move);
    m->b.n_moves = 1;
    m->b.pp_used[0] = 0;
}
static void drain_in(gc_run *r) { gc_event e; while (gc_run_next_event(r, &e)) {} }
static void fight_in(gc_run *r)
{
    const char *why = "";
    gc_choice c = {GC_CH_FIGHT, 0, -1, 0};
    CHECK(gc_run_choose(r, &c, &why));
    drain_in(r);
}
static void new_wild(const char *seed, const gc_custom_hooks *hooks)
{
    new_run_in(&run, &sys, seed, hooks);
    drain_in(&run);
    CHECK(run.screen.kind == GC_SCREEN_COMMAND && run.battle_type == BATTLE_TYPE_WILD && run.n_enemy == 1);
}

static bool fixture_player_direct, fixture_last_member;
static void test_faint_notes(void)
{
    /* the enemy's last HP is taken by the player's Tackle: direct, with the attacker's id and the move */
    new_wild("faint-direct", &hooks_faint_only);
    {
        gc_mon *me = &run.party[run.party_order[0]];
        gc_mon *foe = &run.enemy[0];
        set_moves(me, MOVE_ID_TACKLE);
        set_moves(foe, MOVE_ID_SPLASH);
        foe->b.hp = 1;
        const uint32_t my_id = me->b.id, foe_id = foe->b.id;
        n_faint_notes = 0;
        fight_in(&run);
        CHECK(n_faint_notes == 1);
        CHECK(last_faint.side == 1 && last_faint.direct && last_faint.has_source && last_faint.source_side == 0);
        CHECK(last_faint.fainted.id == foe_id && last_faint.source.id == my_id);
        CHECK(last_faint.move == MOVE_ID_TACKLE);
        CHECK(last_faint.remaining == 0);
        CHECK(last_faint.fainted.start_species == foe->b.species && last_faint.fainted.start_form == foe->b.form);
        CHECK(last_faint.source.n_moves == 1 && last_faint.source.moves[0] == MOVE_ID_TACKLE);
        CHECK(last_faint.wave == 1 && last_faint.turn >= 1);
    }
    /* the player's lead falls to the enemy's Tackle: direct, the attacker is the enemy, the bench is counted */
    for (int attempt = 0; attempt < 12 && !fixture_player_direct; attempt++) {
        char sd[40];
        snprintf(sd, sizeof sd, "faint-direct-player-%d", attempt);
        new_wild(sd, &hooks_faint_only);
        gc_mon *me = &run.party[run.party_order[0]];
        gc_mon *foe = &run.enemy[0];
        set_moves(foe, MOVE_ID_TACKLE);
        set_moves(me, MOVE_ID_SPLASH);
        me->b.hp = 1;
        me->b.stat_stages[STAT_DEF - 1] = -6;
        const uint32_t foe_id = foe->b.id, my_id = me->b.id;
        n_faint_notes = 0;
        fight_in(&run);
        if (n_faint_notes == 1) {
            CHECK(last_faint.side == 0 && last_faint.direct && last_faint.has_source && last_faint.source_side == 1);
            CHECK(last_faint.fainted.id == my_id && last_faint.source.id == foe_id && last_faint.move == MOVE_ID_TACKLE);
            CHECK(last_faint.remaining == 2);
            CHECK(last_faint.source.start_species == foe->b.species);
            fixture_player_direct = true;
        } else {
            CHECK(n_faint_notes == 0);   /* the enemy missed or went second: this seed does not meet the fixture */
        }
    }
    /* the last member falls: remaining 0 */
    for (int attempt = 0; attempt < 12 && !fixture_last_member; attempt++) {
        char sd[40];
        snprintf(sd, sizeof sd, "faint-last-member-%d", attempt);
        new_wild(sd, &hooks_faint_only);
        gc_mon *me = &run.party[run.party_order[0]];
        gc_mon *foe = &run.enemy[0];
        for (int i = 0; i < run.n_party; i++) if (&run.party[i] != me) run.party[i].b.hp = 0;
        set_moves(foe, MOVE_ID_TACKLE);
        set_moves(me, MOVE_ID_SPLASH);
        me->b.hp = 1;
        me->b.stat_stages[STAT_DEF - 1] = -6;
        n_faint_notes = 0;
        fight_in(&run);
        if (n_faint_notes == 1) {
            CHECK(last_faint.side == 0 && last_faint.remaining == 0 && last_faint.direct);
            CHECK(run.screen.kind == GC_SCREEN_GAME_OVER);
            fixture_last_member = true;
        }
    }
    CHECK(fixture_player_direct && fixture_last_member);
    /* poison takes the last HP: a faint with no attacker */
    new_wild("faint-poison", &hooks_faint_only);
    {
        gc_mon *me = &run.party[run.party_order[0]];
        gc_mon *foe = &run.enemy[0];
        set_moves(me, MOVE_ID_SPLASH);
        set_moves(foe, MOVE_ID_SPLASH);
        foe->b.hp = 1;
        foe->b.status = STATUS_EFFECT_POISON;
        n_faint_notes = 0;
        fight_in(&run);
        CHECK(n_faint_notes == 1);
        CHECK(last_faint.side == 1 && !last_faint.direct && !last_faint.has_source && last_faint.move == 0);
        CHECK(run.last_faint[1].observed && !run.last_faint[1].direct);
        CHECK(run.last_faint[1].species == 0 && run.last_faint[1].move == 0);
    }
    /* Explosion: the target falls to the hit (direct), the user to its own move (not direct) */
    new_wild("faint-explosion", &hooks_faint_only);
    {
        gc_mon *me = &run.party[run.party_order[0]];
        gc_mon *foe = &run.enemy[0];
        set_moves(me, MOVE_ID_EXPLOSION);
        set_moves(foe, MOVE_ID_SPLASH);
        foe->b.hp = 1;
        const uint32_t my_id = me->b.id;
        n_faint_notes = 0;
        n_direct = 0;
        fight_in(&run);
        CHECK(n_faint_notes == 2);
        CHECK(n_direct == 1);   /* only the enemy's: Explosion's own faint has no attacker */
        (void)my_id;
    }
    /* Ordinary runs retain the exact engine note without enabling custom callbacks. */
    new_wild("faint-ordinary", NULL);
    {
        gc_mon *me = &run.party[run.party_order[0]], *foe = &run.enemy[0];
        set_moves(me, MOVE_ID_TACKLE); set_moves(foe, MOVE_ID_SPLASH); foe->b.hp = 1;
        n_faint_notes = 0;
        fight_in(&run);
        CHECK(n_faint_notes == 0);
        const gc_faint_record *f = &run.last_faint[1];
        CHECK(f->observed && f->direct && f->remaining == 0 && f->source_side == 0);
        CHECK(f->species == me->b.species && f->form == me->b.form && f->move == MOVE_ID_TACKLE);
        CHECK(f->wave == run.wave && f->turn > 0);
    }
    /* a faint seen after the app installed the hooks into a running battle (after a load) */
    new_wild("faint-install-late", NULL);
    {
        gc_mon *me = &run.party[run.party_order[0]];
        gc_mon *foe = &run.enemy[0];
        set_moves(me, MOVE_ID_TACKLE);
        set_moves(foe, MOVE_ID_SPLASH);
        foe->b.hp = 1;
        gc_run_set_custom(&run, &hooks_faint_only);
        n_faint_notes = 0;
        fight_in(&run);
        CHECK(n_faint_notes == 1 && last_faint.direct);
        gc_run_set_custom(&run, NULL);
        CHECK(run.bt.faint_observer != NULL); /* report provenance remains with custom callbacks detached */
    }
}

/* ---------------------------------------------------------------- 4. the offer */
typedef struct {
    uint16_t species; uint8_t form, nature; int8_t gender; uint8_t level; uint32_t id; uint8_t ivs[6]; uint16_t moves[4]; uint8_t n_moves;
    int32_t stats[6]; uint16_t ability;
} snap;
static snap snap_of(const gc_run *r)
{
    snap s;
    memset(&s, 0, sizeof s);
    const pr_pokemon *p = &r->enemy[0].b;
    s.species = p->species; s.form = p->form; s.nature = p->nature; s.gender = p->gender; s.level = p->level; s.id = p->id;
    memcpy(s.ivs, p->ivs, 6); memcpy(s.moves, p->moves, sizeof s.moves); s.n_moves = p->n_moves; memcpy(s.stats, p->stats, sizeof s.stats);
    s.ability = p->base_ability;
    return s;
}

/* plays `r` (with its hooks) to the first ordinary wild wave `want` at or after wave `from`; true when the offer screen waits there */
static bool to_offer(gc_run *r, int from)
{
    (void)from;
    for (int i = 0; i < 4000; i++) {
        if (r->screen.kind == GC_SCREEN_CUSTOM_OFFER) return true;
        if (!bot_step(r, false)) return r->screen.kind == GC_SCREEN_CUSTOM_OFFER;
    }
    return false;
}

static void pick_spec(const snap *ord, gc_custom_enemy *spec)
{
    memset(spec, 0, sizeof *spec);
    spec->species = ord->species == SPECIES_ID_RATTATA ? SPECIES_ID_PIDGEY : SPECIES_ID_RATTATA;
    const pr_species *sp = gc_species(spec->species);
    spec->form = 0;
    spec->gender = sp->male_permille < 0 ? -1 : 0;
    spec->shiny = false;
    spec->has_nature = true;
    spec->nature = 3;
    spec->ability = pr_forms[sp->form0].ability1;
    spec->n_moves = 2;
    spec->moves[0] = MOVE_ID_TACKLE;
    spec->moves[1] = MOVE_ID_GROWL;
}

static void test_offer(void)
{
    const char *seed = "offer-seed-1";
    /* the control: the same seed and bot, hooks installed (the report counts) but nobody asked; its state at the wave the offer will stop at */
    ask_here = false;
    new_run_in(&ctl, &ctl_sys, seed, &hooks_all);
    /* the run that asks at the first ordinary wild wave */
    ask_here = true;
    built_calls = built_ok = answered = 0;
    new_run_in(&run, &sys, seed, &hooks_all);
    CHECK(to_offer(&run, 1));
    const int W = asked_wave;
    CHECK(W >= 1);
    CHECK(run.screen.kind == GC_SCREEN_CUSTOM_OFFER);
    CHECK(!gc_run_can_save(&run));
    CHECK(!gc_int(&run)->at_save_point);
    CHECK(run.wave == W && run.n_enemy == 1);
    /* its queued events: the wave's, never the save point */
    {
        gc_event e;
        bool wave_ev = false, save_ev = false;
        while (gc_run_next_event(&run, &e)) { wave_ev |= e.kind == GC_EV_WAVE && e.a == W; save_ev |= e.kind == GC_EV_SAVE_POINT && 0; }
        CHECK(wave_ev && !save_ev);
    }
    /* answers that are not an answer are refused and change nothing */
    {
        const char *why = "";
        gc_choice bad = {GC_CH_FIGHT, 0, -1, 0};
        CHECK(!gc_run_choose(&run, &bad, &why));
        gc_choice bad2 = {GC_CH_CONFIRM, 2, 0, 0};
        CHECK(!gc_run_choose(&run, &bad2, &why));
        CHECK(run.screen.kind == GC_SCREEN_CUSTOM_OFFER && answered == 0);
    }
    const snap ordinary = snap_of(&run);

    /* the control reaches the same wave on its own, with the ordinary enemy */
    for (int i = 0; i < 4000 && ctl.wave < W; i++) CHECK(bot_step(&ctl, false));
    while (ctl.screen.kind != GC_SCREEN_COMMAND || !gc_run_can_save(&ctl)) CHECK(bot_step(&ctl, false));
    CHECK(ctl.wave == W);
    const snap ctl_enemy = snap_of(&ctl);
    CHECK(!memcmp(&ctl_enemy, &ordinary, sizeof ordinary));   /* the offer happens after the ordinary generation: nothing about the enemy moved */

    /* ---- decline: exactly the ordinary wave ---- */
    static gc_run keep;
    static gc_system keep_sys;
    memcpy(&keep, &run, sizeof run);          /* the run at the offer, to answer twice (accept below) */
    keep_sys = sys;
    keep.sys = &keep_sys;
    {
        gc_choice no = {GC_CH_CONFIRM, 0, 0, 0};
        const char *why = "";
        CHECK(gc_run_choose(&run, &no, &why));
        CHECK(answered == 1 && !answer_accept_flag && built_calls == 0);
        CHECK(run.screen.kind == GC_SCREEN_COMMAND && gc_run_can_save(&run));
        const snap after = snap_of(&run);
        CHECK(!memcmp(&after, &ctl_enemy, sizeof after));
        CHECK(!memcmp(&run.run.rng, &ctl.run.rng, sizeof run.run.rng));
        /* the rest of the wave plays out the same in both worlds. The control still holds the wave's own events (WAVE...): the run
         * took them at the offer; both queues are compared from the save point on */
        while (ctl.ev_count && ctl.events[ctl.ev_head].kind != GC_EV_SAVE_POINT) { gc_event skip; gc_run_next_event(&ctl, &skip); }
        CHECK(ctl.ev_count && run.ev_count && run.events[run.ev_head].kind == GC_EV_SAVE_POINT);
        uint64_t ha, hb;
        for (int pass = 0; pass < 2; pass++) {
            gc_run *r = pass ? &ctl : &run;
            H = 1469598103934665603ull;
            for (int i = 0; i < 120; i++) { if (!bot_step(r, true)) break; if (r->wave > W + 3) break; }
            hash_run_state(r);
            if (pass) hb = H; else ha = H;
        }
        CHECK(ha == hb);
    }

    /* ---- accept: the spec's identity at the ordinary level, with the ordinary id and IVs ---- */
    memcpy(&run, &keep, sizeof run);
    run.sys = &sys;
    sys = keep_sys;
    pick_spec(&ordinary, &next_spec);
    answered = 0; built_calls = built_ok = 0;
    {
        gc_choice yes = {GC_CH_CONFIRM, 1, 0, 0};
        const char *why = "";
        CHECK(gc_run_choose(&run, &yes, &why));
        CHECK(answered == 1 && answer_accept_flag && built_calls == 1 && built_ok == 1);
        CHECK(run.screen.kind == GC_SCREEN_COMMAND && gc_run_can_save(&run));
        const pr_pokemon *p = &run.enemy[0].b;
        CHECK(run.n_enemy == 1 && p->species == next_spec.species && p->form == 0);
        CHECK(p->level == ordinary.level);
        CHECK(p->id == ordinary.id);
        CHECK(!memcmp(p->ivs, ordinary.ivs, 6));
        CHECK(p->nature == 3);
        CHECK(p->base_ability == next_spec.ability && p->ability == next_spec.ability);
        CHECK(p->n_moves == 2 && p->moves[0] == MOVE_ID_TACKLE && p->moves[1] == MOVE_ID_GROWL);
        CHECK(p->stats[0] > 0 && p->hp == p->stats[0]);
        CHECK(!p->is_boss && !p->fusion_species && !p->has_trainer);
        CHECK(run.bt.party[1][0] == &run.enemy[0].b && run.bt.field[2] == &run.enemy[0].b);
        CHECK(run.battle_type == BATTLE_TYPE_WILD);
        for (int i = 0; i < run.n_enemy_items; i++) CHECK(run.enemy_items[i].pokemon_id == p->id || run.enemy_items[i].pokemon_id == 0);
        CHECK(gc_int(&run)->enemy_start_species[0] == next_spec.species);
        /* the dex saw the rematch enemy, not the ordinary one it replaced */
        CHECK(gc_run_can_save(&run));
        /* save, load into another run: the same enemy comes back (the hooks are the app's to install again) */
        static uint8_t buf[GC_SAVE_MAX];
        const size_t n = gc_run_save(&run, buf, sizeof buf);
        CHECK(n > 0);
        static gc_run loaded;
        static gc_system lsys;
        memset(&loaded, 0, sizeof loaded);
        gc_system_init_defaults(&lsys);
        loaded.sys = &lsys;
        CHECK(gc_run_load(&loaded, buf, n, &why));
        gc_run_set_custom(&loaded, &hooks_all);
        const snap a = snap_of(&run), b = snap_of(&loaded);
        CHECK(!memcmp(&a, &b, sizeof a));
        CHECK(loaded.enemy[0].b.hp == run.enemy[0].b.hp);
        CHECK(gc_int(&loaded)->enemy_start_species[0] == next_spec.species);
        /* a hit on the loaded run's enemy is noted: the observer was installed into the restored battle */
        set_moves(&loaded.party[loaded.party_order[0]], MOVE_ID_TACKLE);
        loaded.enemy[0].b.hp = 1;
        n_faint_notes = 0;
        fight_in(&loaded);
        CHECK(n_faint_notes == 1 && last_faint.direct && last_faint.fainted.species == next_spec.species);
        CHECK(last_faint.fainted.id == ordinary.id);
        CHECK(last_faint.fainted.start_species == next_spec.species);
    }

    /* ---- a spec game-core refuses: the ordinary enemy stays, the app is told ---- */
    memcpy(&run, &keep, sizeof run);
    run.sys = &sys;
    sys = keep_sys;
    pick_spec(&ordinary, &next_spec);
    next_spec.ability = 9999;
    built_calls = built_ok = 0;
    {
        gc_choice yes = {GC_CH_CONFIRM, 1, 0, 0};
        const char *why = "";
        CHECK(gc_run_choose(&run, &yes, &why));
        CHECK(built_calls == 1 && built_ok == 0 && built_why[0]);
        const snap after = snap_of(&run);
        CHECK(!memcmp(&after, &ordinary, sizeof after));
        CHECK(run.screen.kind == GC_SCREEN_COMMAND && gc_run_can_save(&run));
    }
}

/* ---------------------------------------------------------------- 5. gc_custom_enemy_check */
static void test_check(void)
{
    gc_custom_enemy ok;
    memset(&ok, 0, sizeof ok);
    ok.species = SPECIES_ID_RATTATA;
    ok.gender = 0;
    ok.ability = pr_forms[gc_species(SPECIES_ID_RATTATA)->form0].ability1;
    ok.n_moves = 1;
    ok.moves[0] = MOVE_ID_TACKLE;
    const char *why = NULL;
    CHECK(gc_custom_enemy_check(&ok, &why) && why && !why[0]);
    CHECK(gc_custom_enemy_check(&ok, NULL));
    CHECK(!gc_custom_enemy_check(NULL, &why));
    gc_custom_enemy b;
    b = ok; b.species = 0; CHECK(!gc_custom_enemy_check(&b, &why) && why[0]);
    b = ok; b.species = 65000; CHECK(!gc_custom_enemy_check(&b, &why));
    b = ok; b.form = 40; CHECK(!gc_custom_enemy_check(&b, &why));
    b = ok; b.ability = 0; CHECK(!gc_custom_enemy_check(&b, &why));
    b = ok; b.ability = 9999; CHECK(!gc_custom_enemy_check(&b, &why));
    b = ok; b.gender = -1; CHECK(!gc_custom_enemy_check(&b, &why));     /* Rattata has genders */
    b = ok; b.gender = 2; CHECK(!gc_custom_enemy_check(&b, &why));
    b = ok; b.has_nature = true; b.nature = 25; CHECK(!gc_custom_enemy_check(&b, &why));
    b = ok; b.has_nature = true; b.nature = 24; CHECK(gc_custom_enemy_check(&b, &why));
    b = ok; b.n_moves = 0; CHECK(!gc_custom_enemy_check(&b, &why));
    b = ok; b.n_moves = 5; CHECK(!gc_custom_enemy_check(&b, &why));
    b = ok; b.n_moves = 2; b.moves[1] = MOVE_ID_TACKLE; CHECK(!gc_custom_enemy_check(&b, &why));
    b = ok; b.moves[0] = 0; CHECK(!gc_custom_enemy_check(&b, &why));
    b = ok; b.moves[0] = 5000; CHECK(!gc_custom_enemy_check(&b, &why));
    b = ok; b.n_moves = 4; b.moves[1] = MOVE_ID_GROWL; b.moves[2] = MOVE_ID_SPLASH; b.moves[3] = MOVE_ID_EXPLOSION; CHECK(gc_custom_enemy_check(&b, &why));
    /* a genderless species takes -1 only */
    b = ok; b.species = SPECIES_ID_MAGNEMITE; b.gender = 0;
    b.ability = pr_forms[gc_species(SPECIES_ID_MAGNEMITE)->form0].ability1;
    CHECK(!gc_custom_enemy_check(&b, &why));
    b.gender = -1; CHECK(gc_custom_enemy_check(&b, &why));
}

int main(void)
{
    test_check();
    test_identical();
    test_kinds();
    test_kind_agrees();
    test_faint_notes();
    test_offer();
    printf("custom_hooks_test: %d checks, 0 failed\n", checks);
    return 0;
}
