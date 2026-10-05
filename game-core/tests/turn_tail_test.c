/* Task 92: the rest of a turn after a victory, a trainer's switch-outs, the replacement prompts that come
 * with them and the "Will you switch?" question, checked by hand-built states on seeded runs. Each of these
 * paths used to be a stand-in (game-core dropped the turn's tail after a victory, sent a trainer's next
 * Pokemon itself, ignored the AI's switch-out and the engine's stop at CheckSwitchPhase), so every fixture
 * fails on a game-core without Task 92.
 *  - victory with a pending end-of-turn effect: the turn's burn damage lands after the enemy faints, the
 *    experience comes before it, and the next Pokemon of a trainer follows it;
 *  - the same victory over a battle's last enemy: the burn damage, then the wave's rewards;
 *  - the AI's switch-out (a COMMAND_POKEMON of the enemy) is executed in the turn it was picked;
 *  - a double faint (Explosion) at a trainer's: its next Pokemon, then the forced replacement prompt;
 *  - a wild battle's start runs InitEncounterPhase and the entry phases, with the "Will you switch?" prompt
 *    (run->ask_switch) answered either way or without it (the default, the SET battle style). */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_run run;
static gc_system sys;
static int checks;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "turn tail FAIL line %d: %s\n", __LINE__, #x); exit(1); } checks++; } while (0)

static int alive_member(void)
{
    for (int i = 0; i < run.n_party; i++) if (run.party[i].b.hp > 0) return i;
    return 0;
}

static void heal_party(void)
{
    for (int i = 0; i < run.n_party; i++) run.party[i].b.hp = run.party[i].b.stats[0];
}

static void new_run(const char *seed, bool ask)
{
    memset(&run, 0, sizeof run);
    gc_system_init_defaults(&sys);
    gc_run_new(&run, GAME_MODES_CLASSIC, seed, 7);
    run.ask_switch = ask;
    gc_starter team[3];
    memset(team, 0, sizeof team);
    const int species[3] = {SPECIES_ID_BULBASAUR, SPECIES_ID_CHARMANDER, SPECIES_ID_SQUIRTLE};
    for (int i = 0; i < 3; i++) {
        team[i].species = (uint16_t)species[i];
        team[i].tera_type = -1;
        for (int k = 0; k < 6; k++) team[i].ivs[k] = 15;
    }
    const char *why;
    CHECK(gc_run_set_starters(&run, team, 3, &why));
    run.sys = &sys;
}

static void drain(void)
{
    gc_event e;
    while (gc_run_next_event(&run, &e)) {}
}

/* answers whatever the run asks the way a player who only wants to get on would; returns on `stop_at`
 * (a COMMAND screen of a battle of this type, with at least `min_enemy` enemy Pokemon) or the first screen
 * that isn't one of the plain ones. -1 = gave up. The party is level 100 and healed at every command, so
 * the waves go quickly. */
static int play_to_trainer(const char *seed, int min_enemy, int max_wave)
{
    const char *why;
    new_run(seed, false);
    for (int i = 0; i < run.n_party; i++) {
        run.party[i].b.level = 100;
        gc_mon_recalc(&run, &run.party[i]);
    }
    heal_party();
    for (int step = 0; step < 4000; step++) {
        gc_event e;
        while (gc_run_next_event(&run, &e)) {}
        const gc_screen *s = gc_run_screen(&run);
        gc_choice c;
        memset(&c, 0, sizeof c);
        switch (s->kind) {
        case GC_SCREEN_COMMAND:
            if (run.battle_type == BATTLE_TYPE_TRAINER && run.n_enemy >= min_enemy) return run.wave;
            if (run.wave > max_wave) return -1;
            heal_party();
            c.kind = GC_CH_FIGHT;
            c.b = -1;
            for (int i = 0; i < 4; i++) if (s->move_ok[i]) { c.a = (int16_t)i; break; }
            break;
        case GC_SCREEN_SWITCH:
            c.kind = GC_CH_PICK;
            c.a = -1;
            for (int i = 0; i < run.n_party; i++) if (s->pick_ok[i]) { c.a = (int16_t)i; break; }
            break;
        case GC_SCREEN_LEARN_MOVE: c.kind = GC_CH_FORGET; c.a = -1; break;
        case GC_SCREEN_EVOLVE: c.kind = GC_CH_CANCEL; c.a = 1; break;
        case GC_SCREEN_REWARD: c.kind = GC_CH_SKIP; break;
        case GC_SCREEN_BIOME: c.kind = GC_CH_BIOME; break;
        case GC_SCREEN_MYSTERY: c.kind = GC_CH_MYSTERY; c.b = (int16_t)alive_member(); break;
        default: return -1;
        }
        if (!gc_run_choose(&run, &c, &why)) return -1;
    }
    return -1;
}

static int count_events(int type, int battler)
{
    int n = 0;
    for (int i = 0; i < run.n_battle_events; i++)
        if (run.battle_events[i].type == type && (battler < 0 || run.battle_events[i].battler == battler)) n++;
    return n;
}

static gc_event evs[256];
static int n_ev;
static void collect(void)
{
    n_ev = 0;
    while (n_ev < 256 && gc_run_next_event(&run, &evs[n_ev])) n_ev++;
}

static int gc_ev_index(int kind, int from)
{
    for (int i = from; i < n_ev; i++) if (evs[i].kind == kind) return i;
    return -1;
}

/* the first of battle-core's events of the last choose with this type (and battler, -1 = any) from `from` on */
static int bev(int type, int battler, int from)
{
    for (int i = from; i < run.n_battle_events; i++)
        if (run.battle_events[i].type == type && (battler < 0 || run.battle_events[i].battler == battler)) return i;
    return -1;
}

static int bev_text(const char *key, int from)
{
    for (int i = from; i < run.n_battle_events; i++) {
        const pr_event *e = &run.battle_events[i];
        if (e->type == PR_EV_TEXT && e->msg.key && !strcmp(e->msg.key, key)) return i;
    }
    return -1;
}

/* battle-core's phase trace of its last call: did it run this phase? */
static bool traced(const char *name)
{
    for (int i = 0; i < run.eng.n_trace; i++) if (!strcmp(run.eng.trace[i], name)) return true;
    return false;
}

/* the player's lead hits at a level just under the level cap (so it earns experience) */
static void weaken_lead(void)
{
    const int level = (int)pr_run_get_max_exp_level(&run.run, run.wave, false) - 1;
    gc_mon *me = &run.party[run.party_order[0]];
    me->b.level = (uint8_t)level;
    gc_mon_recalc(&run, me);
    me->b.hp = me->b.stats[0];
    me->exp = gc_level_total_exp(level, gc_species(me->b.species)->growth_rate);
}

static void set_moves(gc_mon *m, int move)
{
    gc_mon_set_move(m, 0, move);
    m->b.n_moves = 1;
    m->b.pp_used[0] = 0;
}

static void fight(void)
{
    const char *why = "";
    gc_choice c = {GC_CH_FIGHT, 0, -1, 0};
    CHECK(gc_run_choose(&run, &c, &why));
    collect();
}

/* TurnEndPhase's burn damage: toDmgValue(maxHp / 16) */
static int burn_damage(const gc_mon *m)
{
    const int d = m->b.stats[0] / 16;
    return d > 1 ? d : 1;
}

/* A trainer's Pokemon faints with the turn's burn damage still to come. battle-core stops at the
 * VictoryPhase; game-core gives out the experience, then lets the engine carry on (pr_continue): the burn
 * damage lands, the trainer's next Pokemon comes out, and the turn after is the player's again. */
static bool victory_tail_trainer(const char *seed)
{
    if (play_to_trainer(seed, 2, 30) < 0) return false;
    drain();
    weaken_lead();
    const int lead = run.party_order[0];
    gc_mon *me = &run.party[lead];
    const int old = gc_enemy_active_index(&run);
    gc_mon *foe = &run.enemy[old];
    foe->b.hp = 1;
    set_moves(me, MOVE_ID_QUICK_ATTACK);   /* first, so the enemy's own move can't change the numbers */
    me->b.status = STATUS_EFFECT_BURN;
    const int hp0 = me->b.hp, exp0 = me->exp, turn0 = run.bt.turn;
    fight();
    if (run.screen.kind != GC_SCREEN_COMMAND) return false;         /* a level-up's evolution screen */
    if (bev_text("battle:trainerComeBack", 0) >= 0) return false;   /* the AI switched: not this fixture */
    const int faint = bev(PR_EV_FAINT, BATTLER_INDEX_ENEMY, 0);
    if (faint < 0 || bev_text("battle:useMove", bev_text("battle:useMove", 0) + 1) >= 0) return false;   /* no faint, or the enemy moved first */

    const int t = gc_ev_index(GC_EV_TURN, 0);
    const int b1 = gc_ev_index(GC_EV_BATTLE, t + 1);
    const int x = gc_ev_index(GC_EV_EXP, b1 + 1);
    const int b2 = gc_ev_index(GC_EV_BATTLE, x + 1);
    CHECK(t >= 0 && b1 > t && x > b1 && b2 > x);          /* the turn, the faint, the experience, the rest of the turn */
    CHECK(evs[x].a == lead && evs[x].b > 0 && me->exp > exp0);
    const int cut = evs[b1].a + evs[b1].b;
    CHECK(evs[b2].a == cut);

    const int burn = bev_text("statusEffect:burn.activation", 0);
    const int go = bev_text("battle:trainerGo", 0);
    const int in = bev(PR_EV_SWITCH_IN, BATTLER_INDEX_ENEMY, 0);
    CHECK(faint < cut);                                   /* the faint is before the experience... */
    CHECK(burn >= cut);                                   /* ...the burn damage after it */
    CHECK(go > burn && in > go);                          /* and the next Pokemon after the burn damage */
    CHECK(hp0 - me->b.hp == burn_damage(me));
    CHECK(count_events(PR_EV_FAINT, BATTLER_INDEX_ENEMY) == 1);

    const int now = gc_enemy_active_index(&run);
    CHECK(now >= 0 && now != old && run.enemy[now].b.hp > 0 && run.enemy[old].b.hp <= 0);
    CHECK(run.bt.field[2] == &run.enemy[now].b);
    CHECK(run.bt.turn == turn0 + 1);
    CHECK(run.in_battle && run.screen.kind == GC_SCREEN_COMMAND);
    return true;
}

/* The same victory over a battle's last enemy: the burn damage first, then BattleEndPhase's chain
 * (the wave's rewards). */
static void victory_tail_last_enemy(void)
{
    new_run("tail-last-enemy", false);
    drain();
    gc_mon *me = &run.party[run.party_order[0]];
    gc_mon *foe = &run.enemy[0];
    CHECK(run.battle_type == BATTLE_TYPE_WILD && run.n_enemy == 1);
    me->b.hp = me->b.stats[0];
    foe->b.hp = 1;
    set_moves(foe, MOVE_ID_SPLASH);
    set_moves(me, MOVE_ID_TACKLE);
    me->b.status = STATUS_EFFECT_BURN;
    const int hp0 = me->b.hp;
    fight();
    const int faint = bev(PR_EV_FAINT, BATTLER_INDEX_ENEMY, 0);
    const int burn = bev_text("statusEffect:burn.activation", 0);
    CHECK(faint >= 0 && burn > faint);
    CHECK(hp0 - me->b.hp == burn_damage(me));
    CHECK(run.screen.kind == GC_SCREEN_REWARD);
    CHECK(!run.in_battle);
}

/* The AI's switch-out: the trainer's COMMAND_POKEMON runs in the turn it was picked, before the player's move. */
static bool trainer_switch_out(const char *seed)
{
    if (play_to_trainer(seed, 2, 30) < 0) return false;
    drain();
    const int lead = run.party_order[0];
    gc_mon *me = &run.party[lead];
    const int old = gc_enemy_active_index(&run);
    set_moves(&run.enemy[old], MOVE_ID_SPLASH);
    set_moves(me, MOVE_ID_SPLASH);
    const int turn0 = run.bt.turn;
    fight();
    const int come = bev_text("battle:trainerComeBack", 0);
    if (come < 0) return false;                           /* the AI kept its Pokemon in */

    const int out = bev(PR_EV_SWITCH_OUT, BATTLER_INDEX_ENEMY, come);
    const int go = bev_text("battle:trainerGo", come);
    const int in = bev(PR_EV_SWITCH_IN, BATTLER_INDEX_ENEMY, come);
    const int use = bev_text("battle:useMove", 0);
    CHECK(out > come && go > out && in > go && use > in); /* comes back, goes out, then the player's move */
    const int now = gc_enemy_active_index(&run);
    CHECK(now >= 0 && now != old && run.enemy[now].b.hp > 0);
    CHECK(run.bt.field[2] == &run.enemy[now].b);
    CHECK(run.enemy[now].b.on_field && !run.enemy[old].b.on_field);
    CHECK(gc_int(&run)->ai.enemy_switch_counter >= 1);
    CHECK(run.bt.turn == turn0 + 1);
    CHECK(run.screen.kind == GC_SCREEN_COMMAND);
    return true;
}

/* A double faint at a trainer's (Explosion): its next Pokemon comes out first, then the prompt for the
 * player's replacement, which can't be skipped and doesn't offer the fainted lead. */
static bool double_faint(const char *seed)
{
    const char *why = "";
    if (play_to_trainer(seed, 2, 30) < 0) return false;
    drain();
    const int lead = run.party_order[0];
    gc_mon *me = &run.party[lead];
    set_moves(me, MOVE_ID_EXPLOSION);
    const int turn0 = run.bt.turn;
    fight();
    if (bev(PR_EV_FAINT, BATTLER_INDEX_ENEMY, 0) < 0) return false;  /* the Explosion didn't reach it */
    CHECK(run.screen.kind == GC_SCREEN_SWITCH);
    CHECK(!run.screen.can_skip);
    CHECK(!run.screen.pick_ok[lead]);
    int bench = -1;
    for (int i = 0; i < run.n_party; i++) if (run.screen.pick_ok[i]) bench = i;
    CHECK(bench >= 0);

    const int f_foe = bev(PR_EV_FAINT, BATTLER_INDEX_ENEMY, 0);
    const int f_me = bev(PR_EV_FAINT, 0, 0);
    const int go = bev_text("battle:trainerGo", f_me);   /* the AI may have sent one out before the move */
    const int in = bev(PR_EV_SWITCH_IN, BATTLER_INDEX_ENEMY, f_me);
    CHECK(f_foe >= 0 && f_me > f_foe && go > f_me && in > go);
    const int now = gc_enemy_active_index(&run);
    int fainted = 0;
    for (int i = 0; i < run.n_enemy; i++) fainted += run.enemy[i].b.hp <= 0;
    CHECK(fainted == 1);
    CHECK(now >= 0 && run.enemy[now].b.hp > 0 && run.bt.field[2] == &run.enemy[now].b);

    gc_choice none = {GC_CH_PICK, -1, 0, 0};
    CHECK(!gc_run_choose(&run, &none, &why));
    gc_choice dead = {GC_CH_PICK, (int16_t)lead, 0, 0};
    CHECK(!gc_run_choose(&run, &dead, &why));
    CHECK(run.screen.kind == GC_SCREEN_SWITCH);
    gc_choice pick = {GC_CH_PICK, (int16_t)bench, 0, 0};
    CHECK(gc_run_choose(&run, &pick, &why));
    drain();
    CHECK(run.screen.kind == GC_SCREEN_COMMAND);
    CHECK(run.bt.field[0] == &run.party[bench].b && run.party[bench].b.on_field);
    CHECK(run.bt.turn == turn0 + 1);
    return true;
}

/* The default (the SET battle style): no question, and the start of a wild battle is carried through,
 * the enemy's and the player's entry phases included. */
static void wild_start_without_question(void)
{
    new_run("wild-start", false);
    CHECK(run.bt.battle_style_set);
    CHECK(run.screen.kind == GC_SCREEN_COMMAND);
    CHECK(traced("InitEncounterPhase") && traced("PostSummonPhase:2") && traced("PostSummonPhase:0"));
    CHECK(bev_text("battle:switchQuestion", 0) < 0);
}

/* run->ask_switch: CheckSwitchPhase's "Will you switch?" is a screen of its own, answered yes or no. */
static void wild_start_with_question(void)
{
    const char *why = "";
    gc_choice fight_c = {GC_CH_FIGHT, 0, -1, 0};
    gc_choice maybe = {GC_CH_CONFIRM, 2, 0, 0};
    gc_choice no = {GC_CH_CONFIRM, 0, 0, 0};
    gc_choice yes = {GC_CH_CONFIRM, 1, 0, 0};

    new_run("wild-ask-no", true);
    CHECK(!run.bt.battle_style_set);
    CHECK(run.screen.kind == GC_SCREEN_CHECK_SWITCH && run.eng.awaiting_confirm);
    CHECK(bev_text("battle:switchQuestion", 0) >= 0);
    CHECK(!traced("InitEncounterPhase"));                 /* the engine waits for the answer */
    CHECK(!gc_run_choose(&run, &fight_c, &why));          /* only yes or no */
    CHECK(!gc_run_choose(&run, &maybe, &why));
    CHECK(run.screen.kind == GC_SCREEN_CHECK_SWITCH);
    const int lead = run.party_order[0];
    CHECK(gc_run_choose(&run, &no, &why));
    drain();
    CHECK(run.screen.kind == GC_SCREEN_COMMAND);
    CHECK(traced("InitEncounterPhase") && traced("PostSummonPhase:2") && traced("PostSummonPhase:0"));
    CHECK(run.bt.field[0] == &run.party[lead].b);

    new_run("wild-ask-yes", true);
    CHECK(run.screen.kind == GC_SCREEN_CHECK_SWITCH);
    const int lead2 = run.party_order[0];
    CHECK(gc_run_choose(&run, &yes, &why));
    drain();
    CHECK(run.screen.kind == GC_SCREEN_SWITCH && run.screen.can_skip);   /* a pick that can be cancelled */
    CHECK(!run.screen.pick_ok[lead2]);
    const int bench = run.party_order[1];
    CHECK(run.screen.pick_ok[bench]);
    gc_choice pick = {GC_CH_PICK, (int16_t)bench, 0, 0};
    CHECK(gc_run_choose(&run, &pick, &why));
    drain();
    CHECK(run.screen.kind == GC_SCREEN_COMMAND);
    CHECK(run.bt.field[0] == &run.party[bench].b && !run.party[lead2].b.on_field);
    CHECK(run.party_order[0] == bench);
    CHECK(traced("InitEncounterPhase") && traced("PostSummonPhase:2"));

    new_run("wild-ask-cancel", true);
    const int lead3 = run.party_order[0];
    CHECK(gc_run_choose(&run, &yes, &why));
    drain();
    CHECK(run.screen.kind == GC_SCREEN_SWITCH);
    gc_choice cancel = {GC_CH_PICK, -1, 0, 0};
    CHECK(gc_run_choose(&run, &cancel, &why));
    drain();
    CHECK(run.screen.kind == GC_SCREEN_COMMAND);
    CHECK(run.bt.field[0] == &run.party[lead3].b);
    CHECK(traced("InitEncounterPhase"));
}

/* seeds until `want` of them give the case the fixture needs (the AI's own choices decide which do) */
#define SCAN(fn, prefix, tries, want) do { \
    int got = 0; \
    for (int i = 0; i < (tries) && got < (want); i++) { \
        char seed[40]; \
        snprintf(seed, sizeof seed, prefix "%d", i); \
        if (fn(seed)) got++; \
    } \
    printf("  %-24s %d seeds\n", #fn, got); \
    CHECK(got >= (want)); \
} while (0)

int main(void)
{
    SCAN(victory_tail_trainer, "tail-trainer-", 16, 3);
    victory_tail_last_enemy();
    SCAN(trainer_switch_out, "tail-switch-", 40, 2);
    SCAN(double_faint, "tail-faint-", 16, 3);
    wild_start_without_question();
    wild_start_with_question();
    printf("turn tail: %d checks, 0 failures\n", checks);
    return 0;
}
