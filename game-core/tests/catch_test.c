/* Card 94: catching in the run. Host fixtures on seeded runs for what game-core does around battle-core's
 * capture roll (the roll, its RNG draws and "Gotcha!" are battle-core's own, and its own differential covers them):
 *  - the ball rules: CommandPhase.checkCanUseBall and handleBallCommand's boss rule, per ball type, mode and
 *    biome, and what a refused throw leaves alone;
 *  - each ball type as the one a Pokemon is caught with, the ball spent, lastUsedPokeball left to a failed throw,
 *    the new member as upstream's addToParty makes it (hp, status, moves, stats and id from the enemy, the
 *    met-fields of the catch), the enemy's removePokemon;
 *  - a full party: the prompt, No, Release on any place (the last place's quirk, the lead), the released member's
 *    held items;
 *  - the enemy's held items going to the new member, the dex, candy and IVs of a catch (the dex differential,
 *    tools/difftest.sh, compares those with upstream), a caught boss, the Daily Run's candy rule;
 *  - save and load with the caught Pokemon in the party. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_run run;
static gc_system sys;
static int checks;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "catch FAIL line %d: %s\n", __LINE__, #x); exit(1); } checks++; } while (0)

/* every message of the events drained since clear_log(): game-core's own and battle-core's text */
#define LOG_MAX 256
static const char *log_keys[LOG_MAX];
static int n_log;
static void clear_log(void) { n_log = 0; }
static void note(const char *k) { if (k && n_log < LOG_MAX) log_keys[n_log++] = k; }
static void drain(void)
{
    gc_event e;
    while (gc_run_next_event(&run, &e)) {
        if (e.kind == GC_EV_MESSAGE) note(e.msg.key);
        if (e.kind == GC_EV_BATTLE)
            for (int i = 0; i < e.b; i++) {
                const pr_event *be = &run.battle_events[e.a + i];
                if (be->type == PR_EV_TEXT) note(be->msg.key);
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

static void new_run(const char *seed, bool with_sys)
{
    memset(&run, 0, sizeof run);
    gc_system_init_defaults(&sys);
    gc_run_new(&run, GAME_MODES_CLASSIC, seed, 7);
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
    if (with_sys) run.sys = &sys;
    drain();
    CHECK(run.screen.kind == GC_SCREEN_COMMAND);
    CHECK(run.battle_type == BATTLE_TYPE_WILD && run.n_enemy == 1);
    clear_log();
}

/* the foe as a catch that cannot fail: a Caterpie (catch rate 255) with 1 HP and asleep (x2.5, so the modified
 * rate is past 255 for every ball and the shakes pass without a roll) */
static void sure_catch(gc_mon *foe)
{
    foe->b.species = SPECIES_ID_CATERPIE;
    foe->b.form = 0;
    foe->b.hp = 1;
    foe->b.status = STATUS_EFFECT_SLEEP;
    foe->b.status_sleep_turns = 4;
}

/* answers what the run asks after a catch (a move to learn, an evolution) until it stops at something these
 * fixtures read: the catch prompt (SWITCH), the reward screen, a command screen or the end. Returns the kind. */
static int settle(void)
{
    const char *why;
    for (int step = 0; step < 200; step++) {
        drain();
        const gc_screen *s = gc_run_screen(&run);
        gc_choice c;
        memset(&c, 0, sizeof c);
        switch (s->kind) {
        case GC_SCREEN_LEARN_MOVE: c.kind = GC_CH_FORGET; c.a = -1; break;
        case GC_SCREEN_EVOLVE: c.kind = GC_CH_CANCEL; c.a = 1; break;
        case GC_SCREEN_CHECK_SWITCH: c.kind = GC_CH_CONFIRM; break;
        default: return s->kind;
        }
        CHECK(gc_run_choose(&run, &c, &why));
    }
    CHECK(!"settle: no end");
    return -1;
}

static bool throw_ball(int ball, const char **why)
{
    gc_choice c = {GC_CH_BALL, (int16_t)ball, 0, 0};
    const bool ok = gc_run_choose(&run, &c, why);
    return ok;
}

static void pick(int k)
{
    const char *why = "";
    gc_choice c = {GC_CH_PICK, (int16_t)k, 0, 0};
    CHECK(gc_run_choose(&run, &c, &why));
}

/* bring the party to `n` members (copies of the lead with their own ids and, for each, a Leftovers) */
static void fill_party(int n)
{
    while (run.n_party < n) {
        const int k = run.n_party++;
        run.party[k] = run.party[0];
        run.party[k].b.id = run.party[0].b.id + 1000u * (uint32_t)k;
        run.party[k].b.on_field = false;
        run.party_order[k] = (int8_t)k;
        run.bt.n_party[0] = (uint8_t)run.n_party;
        run.bt.party[0][k] = &run.party[k].b;
    }
    for (int k = 0; k < run.n_party; k++) {
        run.party[k].b.species = (uint16_t)(SPECIES_ID_BULBASAUR + k);   /* told apart */
        gc_item_add(&run, "LEFTOVERS", -1, run.party[k].b.id, 1);
    }
    gc_items_to_battle(&run);
}

static int items_of(uint32_t id)
{
    int n = 0;
    for (int i = 0; i < run.n_items; i++) if (run.items[i].pokemon_id == id) n += run.items[i].stack;
    return n;
}

/* ------------------------------------------------------------------ the ball rules */
static void rules(void)
{
    new_run("catch-rules", true);
    gc_mon *foe = &run.enemy[0];
    const int balls = POKEBALL_TYPE_MASTER_BALL;

    /* a plain wild Pokemon: all five balls, and not the Luxury Ball (the command takes five types) */
    for (int b = 0; b <= balls; b++) CHECK(!gc_catch_ball_refusal(&run, b));

    /* a trainer's Pokemon is not to be caught */
    run.battle_type = BATTLE_TYPE_TRAINER;
    for (int b = 0; b <= balls; b++) CHECK(!strcmp(gc_catch_ball_refusal(&run, b), "battle:noPokeballTrainer"));
    const char *why = "";
    const int before = run.pokeballs[POKEBALL_TYPE_POKEBALL];
    run.pokeballs[POKEBALL_TYPE_MASTER_BALL] = 2;
    CHECK(!throw_ball(POKEBALL_TYPE_POKEBALL, &why) && !strcmp(why, "battle:noPokeballTrainer"));
    CHECK(run.pokeballs[POKEBALL_TYPE_POKEBALL] == before);   /* a refused throw spends nothing */
    CHECK(run.screen.kind == GC_SCREEN_COMMAND);
    run.battle_type = BATTLE_TYPE_WILD;

    /* a mystery encounter's battle: only the encounters built with catchAllowed */
    run.me_battle = true;
    run.me_catch_allowed = false;
    CHECK(!strcmp(gc_catch_ball_refusal(&run, 0), "battle:noPokeballMysteryEncounter"));
    run.me_catch_allowed = true;
    CHECK(!gc_catch_ball_refusal(&run, 0));
    run.me_battle = run.me_catch_allowed = false;

    /* a ball that isn't there, one that can't be thrown, a type that doesn't exist */
    run.pokeballs[POKEBALL_TYPE_LUXURY_BALL] = 3;
    run.pokeballs[POKEBALL_TYPE_ULTRA_BALL] = 0;
    CHECK(!throw_ball(POKEBALL_TYPE_ULTRA_BALL, &why));
    CHECK(!throw_ball(POKEBALL_TYPE_LUXURY_BALL, &why) && run.pokeballs[POKEBALL_TYPE_LUXURY_BALL] == 3);
    CHECK(!throw_ball(9, &why));
    CHECK(!throw_ball(-1, &why));

    /* a boss that still has a shield: only the Master Ball (its last segment is open to every ball) */
    foe->b.is_boss = true;
    foe->b.boss_segments = 3;
    foe->b.boss_segment_index = 2;
    for (int b = 0; b < balls; b++) CHECK(!strcmp(gc_catch_ball_refusal(&run, b), "battle:noPokeballStrong"));
    CHECK(!gc_catch_ball_refusal(&run, POKEBALL_TYPE_MASTER_BALL));
    foe->b.boss_segment_index = 1;
    CHECK(!strcmp(gc_catch_ball_refusal(&run, POKEBALL_TYPE_ULTRA_BALL), "battle:noPokeballStrong"));
    foe->b.boss_segment_index = 0;
    for (int b = 0; b <= balls; b++) CHECK(!gc_catch_ball_refusal(&run, b));
    /* ... unless its ability is Wonder Guard (hasAbility(WONDER_GUARD, false, true)) */
    foe->b.boss_segment_index = 2;
    foe->b.base_ability = ABILITY_ID_WONDER_GUARD;
    CHECK(!gc_catch_ball_refusal(&run, POKEBALL_TYPE_POKEBALL));
    foe->b.base_ability = ABILITY_ID_NONE;
    foe->b.passive = ABILITY_ID_WONDER_GUARD;
    CHECK(!gc_catch_ball_refusal(&run, POKEBALL_TYPE_POKEBALL));
    foe->b.passive = ABILITY_ID_NONE;

    /* the classic final boss: weaken it first (a shield left: "weaken it first"), the Master Ball not even then
     * in a challenge run */
    run.wave = 200;
    run.biome = BIOME_ID_END;
    foe->b.species = SPECIES_ID_ETERNATUS;
    for (int i = 0; i < gc_starter_cost_count; i++) sys.dex[gc_starter_costs[i].id].caught_attr = 1;
    foe->b.boss_segment_index = 3;
    for (int b = 0; b < balls; b++) CHECK(!strcmp(gc_catch_ball_refusal(&run, b), "battle:noPokeballForceFinalBossCatchable"));
    CHECK(!gc_catch_ball_refusal(&run, POKEBALL_TYPE_MASTER_BALL));
    run.run.has_challenges = true;
    CHECK(!strcmp(gc_catch_ball_refusal(&run, POKEBALL_TYPE_MASTER_BALL), "battle:noPokeballForceFinalBossCatchable"));
    run.run.has_challenges = false;
    foe->b.boss_segment_index = 0;
    for (int b = 0; b <= balls; b++) CHECK(!gc_catch_ball_refusal(&run, b));
    /* ... and not at all while a starter but one is missing from the dex */
    sys.dex[gc_starter_costs[0].id].caught_attr = 0;
    sys.dex[gc_starter_costs[1].id].caught_attr = 0;
    for (int b = 0; b <= balls; b++) CHECK(!strcmp(gc_catch_ball_refusal(&run, b), "battle:noPokeballForceFinalBoss"));
    sys.dex[gc_starter_costs[1].id].caught_attr = 1;
    for (int b = 0; b <= balls; b++) CHECK(!gc_catch_ball_refusal(&run, b));   /* one missing is allowed */
    /* the paradox waves before it: an uncaught species on the field can't be ball-thrown at */
    run.wave = 195;
    foe->b.species = SPECIES_ID_GREAT_TUSK;
    foe->b.is_boss = false;
    sys.dex[SPECIES_ID_GREAT_TUSK].caught_attr = 0;
    CHECK(!strcmp(gc_catch_ball_refusal(&run, 0), "battle:noPokeballForce"));
    sys.dex[SPECIES_ID_GREAT_TUSK].caught_attr = 1;
    CHECK(!gc_catch_ball_refusal(&run, 0));
    /* endless: every wild Pokemon of the END biome but the minor boss's (waves that are multiples of 250) */
    run.run.mode = GAME_MODES_ENDLESS;
    run.wave = 5195;
    CHECK(!strcmp(gc_catch_ball_refusal(&run, 0), "battle:noPokeballForce"));
    run.wave = 5250;
    CHECK(!strcmp(gc_catch_ball_refusal(&run, 0), "battle:noPokeballForceFinalBoss"));
    /* daily: the final boss, an event seed's catchable one aside (not ported) */
    run.run.mode = GAME_MODES_DAILY;
    run.wave = 50;
    CHECK(!strcmp(gc_catch_ball_refusal(&run, 0), "battle:noPokeballForceFinalBoss"));
    /* no dex at all (run.sys NULL) counts as an empty one */
    run.run.mode = GAME_MODES_CLASSIC;
    run.wave = 195;
    run.sys = NULL;
    CHECK(!strcmp(gc_catch_ball_refusal(&run, 0), "battle:noPokeballForce"));
    run.wave = 200;
    CHECK(!strcmp(gc_catch_ball_refusal(&run, 0), "battle:noPokeballForceFinalBoss"));
    /* outside the END biome none of that applies */
    run.biome = BIOME_ID_TOWN;
    CHECK(!gc_catch_ball_refusal(&run, 0));
}

/* ------------------------------------------------------------------ each ball, a failed throw */
static void ball_types(void)
{
    for (int ball = POKEBALL_TYPE_POKEBALL; ball <= POKEBALL_TYPE_MASTER_BALL; ball++) {
        char seed[32];
        snprintf(seed, sizeof seed, "catch-ball-%d", ball);
        new_run(seed, true);
        gc_mon *foe = &run.enemy[0];
        sure_catch(foe);
        const gc_mon saved = *foe;
        const int n = run.n_party, wave = run.wave;
        run.pokeballs[ball] = 3;
        const char *why = "";
        CHECK(throw_ball(ball, &why));
        CHECK(settle() == GC_SCREEN_REWARD);
        CHECK(run.pokeballs[ball] == 2);
        CHECK(times("battle:pokemonCaught") == 1);
        CHECK(times("battle:partyFull") == 0);
        CHECK(run.n_party == n + 1);
        CHECK(run.party_order[n] == n);
        const gc_mon *m = &run.party[n];
        CHECK(m->pokeball == ball);
        CHECK(m->met_wave == wave && m->met_level == saved.b.level && m->met_biome == run.biome
              && m->met_species == SPECIES_ID_CATERPIE);
        /* addPlayerPokemon(..., dataSource): the enemy's own id, hp, status, stats, moves and IVs, no new roll */
        CHECK(m->b.id == saved.b.id && m->b.species == saved.b.species && m->b.level == saved.b.level);
        CHECK(m->b.hp == 1 && m->b.status == STATUS_EFFECT_SLEEP);
        CHECK(!memcmp(m->b.stats, saved.b.stats, sizeof m->b.stats));
        CHECK(!memcmp(m->b.ivs, saved.b.ivs, sizeof m->b.ivs) && m->b.nature == saved.b.nature);
        CHECK(!memcmp(m->b.moves, saved.b.moves, sizeof m->b.moves) && m->b.n_moves == saved.b.n_moves);
        CHECK(m->b.friendship == saved.b.friendship && m->b.gender == saved.b.gender && m->exp == saved.exp);
        CHECK(m->b.is_player && !m->b.on_field && !m->b.is_boss);
        CHECK(m->ability_index == saved.ability_index);
        /* removePokemon(): the enemy is out of it */
        CHECK(run.enemy[0].b.hp == 0 && run.enemy[0].b.status == STATUS_EFFECT_FAINT);
    }

    /* a throw that fails: the ball is spent and the battle remembers it (lastUsedPokeball, for Ball Fetch);
     * the turn goes on to the next command; a success after it doesn't move what the failure set */
    new_run("catch-fail", true);
    gc_mon *foe = &run.enemy[0];
    foe->b.species = SPECIES_ID_MEWTWO;   /* catch rate 3 */
    foe->b.form = 0;
    foe->b.hp = foe->b.stats[0];
    run.pokeballs[POKEBALL_TYPE_POKEBALL] = 3;
    run.pokeballs[POKEBALL_TYPE_MASTER_BALL] = 3;
    const int n = run.n_party;
    const char *why = "";
    CHECK(run.bt.last_used_pokeball == -1);
    CHECK(throw_ball(POKEBALL_TYPE_POKEBALL, &why));
    CHECK(settle() == GC_SCREEN_COMMAND);
    CHECK(run.pokeballs[POKEBALL_TYPE_POKEBALL] == 2);
    CHECK(run.n_party == n && run.enemy[0].b.hp > 0);
    CHECK(run.bt.last_used_pokeball == POKEBALL_TYPE_POKEBALL);
    CHECK(throw_ball(POKEBALL_TYPE_MASTER_BALL, &why));
    CHECK(settle() == GC_SCREEN_REWARD);
    CHECK(run.n_party == n + 1 && run.party[n].pokeball == POKEBALL_TYPE_MASTER_BALL);
    CHECK(run.bt.last_used_pokeball == POKEBALL_TYPE_POKEBALL);
}

/* ------------------------------------------------------------------ a full party */
typedef struct { int ids[6]; int order[6]; } party_look;
static party_look look(void)
{
    party_look p;
    for (int k = 0; k < 6; k++) { p.ids[k] = (int)run.party[k].b.id; p.order[k] = run.party_order[k]; }
    return p;
}

static gc_mon full_party_catch(const char *seed)
{
    new_run(seed, true);
    fill_party(6);
    gc_mon *foe = &run.enemy[0];
    sure_catch(foe);
    foe->b.hp = 7;   /* it joins with what it has left */
    const gc_mon saved = *foe;
    run.pokeballs[POKEBALL_TYPE_MASTER_BALL] = 3;
    const char *why = "";
    clear_log();
    CHECK(throw_ball(POKEBALL_TYPE_MASTER_BALL, &why));
    CHECK(settle() == GC_SCREEN_SWITCH);
    return saved;
}

static void full_party(void)
{
    /* the prompt: every member can be released, No is allowed; it comes before the experience */
    gc_mon saved = full_party_catch("catch-full-0");
    const gc_screen *s = gc_run_screen(&run);
    CHECK(s->kind == GC_SCREEN_SWITCH && s->party == 0 && s->can_skip);
    for (int i = 0; i < 6; i++) CHECK(s->pick_ok[i]);
    CHECK(times("battle:pokemonCaught") == 1 && times("battle:partyFull") == 1 && times("menu:swapOrRelease") == 1);
    CHECK(times("battle:expGain") == 0);
    CHECK(run.n_party == 6);
    /* an answer that isn't one */
    const char *why = "";
    gc_choice bad = {GC_CH_FIGHT, 0, 0, 0};
    CHECK(!gc_run_choose(&run, &bad, &why));
    gc_choice out_of_range = {GC_CH_PICK, 6, 0, 0};
    CHECK(!gc_run_choose(&run, &out_of_range, &why));
    CHECK(run.screen.kind == GC_SCREEN_SWITCH);

    /* No: nobody joins, the Pokemon goes; the wave goes on with its experience */
    party_look before = look();
    pick(-1);
    CHECK(settle() == GC_SCREEN_REWARD);
    CHECK(run.n_party == 6);
    party_look after = look();
    CHECK(!memcmp(&before, &after, sizeof before));
    CHECK(farewells() == 0 && times("battle:expGain") >= 1);
    CHECK(run.enemy[0].b.hp == 0 && run.enemy[0].b.status == STATUS_EFFECT_FAINT);
    CHECK(items_of(saved.b.id) == 0);

    /* Release the third member: the new Pokemon is in its place, with the hp it had; its Leftovers go, the others'
     * stay, a goodbye is said once */
    saved = full_party_catch("catch-full-1");
    before = look();
    const uint32_t gone = run.party[2].b.id;
    CHECK(items_of(gone) == 1);
    clear_log();
    pick(2);
    CHECK(settle() == GC_SCREEN_REWARD);
    CHECK(farewells() == 1);
    CHECK(run.n_party == 6);
    CHECK(run.party[2].b.id == saved.b.id && run.party[2].pokeball == POKEBALL_TYPE_MASTER_BALL);
    CHECK(run.party[2].b.hp == 7 && run.party[2].b.status == STATUS_EFFECT_SLEEP && run.party[2].b.is_player);
    CHECK(run.party[2].met_wave == 1 && run.party[2].b.species == SPECIES_ID_CATERPIE);
    after = look();
    for (int k = 0; k < 6; k++) CHECK(k == 2 || before.ids[k] == after.ids[k]);
    CHECK(!memcmp(before.order, after.order, sizeof before.order));
    CHECK(items_of(gone) == 0);
    for (int k = 0; k < 6; k++) CHECK(k == 2 || items_of(run.party[k].b.id) == 1);
    CHECK(run.bt.party[0][2] == &run.party[2].b);

    /* Release the last place: the party screen's cursor ends one before the new end, so the new Pokemon goes in
     * front of the member that was there */
    saved = full_party_catch("catch-full-2");
    before = look();
    pick(5);
    CHECK(settle() == GC_SCREEN_REWARD);
    after = look();
    CHECK((uint32_t)after.ids[5] == saved.b.id);                 /* storage: in the released place ... */
    CHECK(run.party_order[0] == 0 && run.party_order[1] == 1 && run.party_order[2] == 2 && run.party_order[3] == 3);
    CHECK(run.party_order[4] == 5 && run.party_order[5] == 4);   /* ... but fifth in the party, the old fifth sixth */
    for (int k = 0; k < 6; k++) CHECK(run.bt.party[0][k] == &run.party[run.party_order[k]].b);
    CHECK(run.party[run.party_order[4]].b.id == saved.b.id);
    CHECK(run.party[run.party_order[5]].b.id == (uint32_t)before.ids[4]);

    /* Release the lead, which is on the field: the new Pokemon takes the place and leads the next wave */
    saved = full_party_catch("catch-full-3");
    const int lead = run.party_order[0];
    CHECK(run.party[lead].b.on_field);
    pick(lead);
    CHECK(settle() == GC_SCREEN_REWARD);
    CHECK(run.party[run.party_order[0]].b.id == saved.b.id && !run.party[run.party_order[0]].b.on_field);
    CHECK(run.bt.field[0] == NULL);
    gc_choice skip = {GC_CH_SKIP, 0, 0, 0};
    CHECK(gc_run_choose(&run, &skip, &why));
    CHECK(settle() == GC_SCREEN_COMMAND);
    CHECK(run.wave == 2 && run.bt.field[0] == &run.party[run.party_order[0]].b);
    CHECK(run.bt.field[0]->id == saved.b.id);
}

/* ------------------------------------------------------------------ the enemy's items */
static void held_items(void)
{
    new_run("catch-items", true);
    gc_mon *foe = &run.enemy[0];
    sure_catch(foe);
    const uint32_t id = foe->b.id;
    run.n_enemy_items = 0;
    gc_item *a = &run.enemy_items[run.n_enemy_items++];
    a->key = "BERRY"; a->arg = 3; a->stack = 2; a->pokemon_id = id;
    gc_item *b = &run.enemy_items[run.n_enemy_items++];
    b->key = "LEFTOVERS"; b->arg = -1; b->stack = 1; b->pokemon_id = id;
    gc_item *t = &run.enemy_items[run.n_enemy_items++];
    t->key = "ENEMY_DAMAGE_BOOSTER"; t->arg = -1; t->stack = 1; t->pokemon_id = 0;
    gc_items_to_battle(&run);
    run.pokeballs[POKEBALL_TYPE_ROGUE_BALL] = 1;
    const int mine = run.n_items;
    const char *why = "";
    CHECK(throw_ball(POKEBALL_TYPE_ROGUE_BALL, &why));
    CHECK(settle() == GC_SCREEN_REWARD);
    CHECK(run.n_items == mine + 2);
    int berry = 0, leftovers = 0;
    for (int i = 0; i < run.n_items; i++) {
        if (run.items[i].pokemon_id != id) continue;
        if (!strcmp(run.items[i].key, "BERRY") && run.items[i].arg == 3 && run.items[i].stack == 2) berry++;
        if (!strcmp(run.items[i].key, "LEFTOVERS") && run.items[i].stack == 1) leftovers++;
    }
    CHECK(berry == 1 && leftovers == 1);
    CHECK(run.n_enemy_items == 0);   /* the battle is over: the token went with it, the held items before that */
}

/* ------------------------------------------------------------------ the dex, candy and IVs */
static int candy(int species) { return sys.starters[species].candy_count; }

static void dex_and_candy(void)
{
    /* a starter species: its dex entry, its candy, "added as a starter" */
    new_run("catch-dex-0", true);
    gc_mon *foe = &run.enemy[0];
    sure_catch(foe);
    const uint8_t ivs[6] = {31, 12, 0, 9, 31, 7};
    memcpy(foe->b.ivs, ivs, 6);
    foe->b.nature = 3;
    run.pokeballs[POKEBALL_TYPE_ULTRA_BALL] = 5;
    const char *why = "";
    CHECK(sys.dex[SPECIES_ID_CATERPIE].caught_attr == 0 && candy(SPECIES_ID_CATERPIE) == 0);
    clear_log();
    CHECK(throw_ball(POKEBALL_TYPE_ULTRA_BALL, &why));
    CHECK(settle() == GC_SCREEN_REWARD);
    const gc_dex_entry *d = &sys.dex[SPECIES_ID_CATERPIE];
    CHECK(d->caught_attr != 0 && d->caught_count == 1 && (d->nature_attr & (1u << 4)));
    CHECK(candy(SPECIES_ID_CATERPIE) == 1);
    CHECK(!memcmp(d->ivs, ivs, 6));
    CHECK(times("battle:addedAsAStarter") == 1);
    /* a second one of the species: counted, candy again, no message */
    new_run("catch-dex-1", true);
    sys.dex[SPECIES_ID_CATERPIE].caught_attr = 1;
    sys.dex[SPECIES_ID_CATERPIE].caught_count = 4;
    sys.starters[SPECIES_ID_CATERPIE].candy_count = 10;
    sys.starters[SPECIES_ID_CATERPIE].egg_moves = 5;   /* a catch unlocks no egg move and takes none */
    sure_catch(&run.enemy[0]);
    run.pokeballs[POKEBALL_TYPE_POKEBALL] = 5;
    CHECK(throw_ball(POKEBALL_TYPE_POKEBALL, &why));
    CHECK(settle() == GC_SCREEN_REWARD);
    CHECK(sys.dex[SPECIES_ID_CATERPIE].caught_count == 5 && candy(SPECIES_ID_CATERPIE) == 11);
    CHECK(sys.starters[SPECIES_ID_CATERPIE].egg_moves == 5);
    CHECK(times("battle:addedAsAStarter") == 0);

    /* an evolved species whose chain has a starter at its foot: the IVs go to the starter and down (not to the
     * species caught), the count to every species in the chain, the candy to the root only, the message for the
     * starter that was new */
    new_run("catch-dex-2", true);
    foe = &run.enemy[0];
    sure_catch(foe);
    foe->b.species = SPECIES_ID_BUTTERFREE;
    memcpy(foe->b.ivs, ivs, 6);
    run.pokeballs[POKEBALL_TYPE_GREAT_BALL] = 5;
    clear_log();
    CHECK(throw_ball(POKEBALL_TYPE_GREAT_BALL, &why));
    CHECK(settle() == GC_SCREEN_REWARD);
    CHECK(sys.dex[SPECIES_ID_BUTTERFREE].caught_count == 1 && sys.dex[SPECIES_ID_METAPOD].caught_count == 1
          && sys.dex[SPECIES_ID_CATERPIE].caught_count == 1);
    CHECK(candy(SPECIES_ID_CATERPIE) == 1 && candy(SPECIES_ID_BUTTERFREE) == 0 && candy(SPECIES_ID_METAPOD) == 0);
    CHECK(!memcmp(sys.dex[SPECIES_ID_CATERPIE].ivs, ivs, 6));
    for (int i = 0; i < 6; i++) CHECK(sys.dex[SPECIES_ID_BUTTERFREE].ivs[i] == 0 && sys.dex[SPECIES_ID_METAPOD].ivs[i] == 0);
    CHECK(times("battle:addedAsAStarter") == 1);

    /* a shiny boss: candy x5 per variant tier x2 for the boss; Master Ball past its shield */
    new_run("catch-dex-3", true);
    foe = &run.enemy[0];
    sure_catch(foe);
    foe->b.shiny = true;
    foe->variant = 1;
    foe->b.is_boss = true;
    foe->b.boss_segments = 3;
    foe->b.boss_segment_index = 2;
    run.pokeballs[POKEBALL_TYPE_ULTRA_BALL] = 5;
    run.pokeballs[POKEBALL_TYPE_MASTER_BALL] = 5;
    CHECK(!throw_ball(POKEBALL_TYPE_ULTRA_BALL, &why) && !strcmp(why, "battle:noPokeballStrong"));
    CHECK(run.pokeballs[POKEBALL_TYPE_ULTRA_BALL] == 5 && run.screen.kind == GC_SCREEN_COMMAND);
    CHECK(throw_ball(POKEBALL_TYPE_MASTER_BALL, &why));
    CHECK(settle() == GC_SCREEN_REWARD);
    CHECK(candy(SPECIES_ID_CATERPIE) == 5 * 2 * 2);
    const gc_mon *m = &run.party[run.n_party - 1];
    CHECK(m->b.is_player && !m->b.is_boss && m->b.boss_segments == 0 && m->b.boss_segment_index == 0);

    /* the Daily Run gives candy for a species with no prevolution only when the catch unlocks something new */
    new_run("catch-dex-4", true);
    run.run.mode = GAME_MODES_DAILY;
    sure_catch(&run.enemy[0]);
    run.pokeballs[POKEBALL_TYPE_POKEBALL] = 5;
    CHECK(throw_ball(POKEBALL_TYPE_POKEBALL, &why));
    CHECK(settle() == GC_SCREEN_REWARD);
    CHECK(candy(SPECIES_ID_CATERPIE) == 1);   /* new */
    new_run("catch-dex-5", true);
    run.run.mode = GAME_MODES_DAILY;
    sure_catch(&run.enemy[0]);
    {
        const gc_mon probe = run.enemy[0];
        sys.dex[SPECIES_ID_CATERPIE].caught_attr = gc_mon_dex_attr(&probe) & gc_species_full_unlocks_data(SPECIES_ID_CATERPIE);
    }
    run.pokeballs[POKEBALL_TYPE_POKEBALL] = 5;
    CHECK(throw_ball(POKEBALL_TYPE_POKEBALL, &why));
    CHECK(settle() == GC_SCREEN_REWARD);
    CHECK(candy(SPECIES_ID_CATERPIE) == 0 && sys.dex[SPECIES_ID_CATERPIE].caught_count == 1);

    /* no system data (a run without a profile): the catch still joins the party */
    new_run("catch-dex-6", false);
    CHECK(run.sys == NULL);
    sure_catch(&run.enemy[0]);
    run.pokeballs[POKEBALL_TYPE_POKEBALL] = 5;
    const int n = run.n_party;
    CHECK(throw_ball(POKEBALL_TYPE_POKEBALL, &why));
    CHECK(settle() == GC_SCREEN_REWARD);
    CHECK(run.n_party == n + 1);

    /* setPokemonSpeciesCaught's showMessage: a new starter that is itself evolved (Pikachu, upstream's TODO) returns
     * before its prevolution unless the message is shown; a catch shows it, the other callers (an egg) don't */
    {
        gc_system quiet, shown;
        gc_system_init_defaults(&quiet);
        gc_system_init_defaults(&shown);
        gc_mon pika;
        memset(&pika, 0, sizeof pika);
        pika.b.species = SPECIES_ID_PIKACHU;
        pika.b.nature = 3;
        quiet.dex[SPECIES_ID_PIKACHU].caught_attr = shown.dex[SPECIES_ID_PIKACHU].caught_attr = 0;
        quiet.dex[SPECIES_ID_PICHU].caught_attr = shown.dex[SPECIES_ID_PICHU].caught_attr = 0;
        gc_caught_report rq, rs;
        CHECK(gc_system_set_pokemon_caught_ex(&quiet, &pika, true, true, false, false, &rq));
        CHECK(gc_system_set_pokemon_caught_ex(&shown, &pika, true, false, true, false, &rs));
        CHECK(rq.n_started == 0 && quiet.dex[SPECIES_ID_PIKACHU].caught_attr != 0 && quiet.dex[SPECIES_ID_PICHU].caught_attr == 0);
        CHECK(rs.n_started == 2 && rs.started[0] == SPECIES_ID_PIKACHU && rs.started[1] == SPECIES_ID_PICHU);
        CHECK(shown.dex[SPECIES_ID_PICHU].caught_attr != 0);
    }
}

/* ------------------------------------------------------------------ save and load */
static void save_load(void)
{
    new_run("catch-save", true);
    sure_catch(&run.enemy[0]);
    const uint32_t id = run.enemy[0].b.id;
    run.pokeballs[POKEBALL_TYPE_ROGUE_BALL] = 2;
    const char *why = "";
    const int n = run.n_party, wave = run.wave;
    CHECK(throw_ball(POKEBALL_TYPE_ROGUE_BALL, &why));
    CHECK(settle() == GC_SCREEN_REWARD);
    gc_choice skip = {GC_CH_SKIP, 0, 0, 0};
    CHECK(gc_run_choose(&run, &skip, &why));
    CHECK(settle() == GC_SCREEN_COMMAND);
    CHECK(run.wave == wave + 1 && gc_run_can_save(&run));
    static uint8_t buf[GC_SAVE_MAX];
    const size_t len = gc_run_save(&run, buf, sizeof buf);
    CHECK(len > 0);
    static gc_run other;
    CHECK(gc_run_load(&other, buf, len, &why));
    CHECK(!other.load_dropped);
    CHECK(other.n_party == n + 1);
    for (int k = 0; k <= n; k++) {
        const gc_mon *a = &run.party[k], *b = &other.party[k];
        CHECK(a->b.id == b->b.id && a->pokeball == b->pokeball && a->met_wave == b->met_wave);
        CHECK(a->met_level == b->met_level && a->met_biome == b->met_biome && a->met_species == b->met_species);
        CHECK(a->b.hp == b->b.hp && a->b.status == b->b.status);
    }
    CHECK(other.party[n].b.id == id && other.party[n].pokeball == POKEBALL_TYPE_ROGUE_BALL);
    CHECK(other.party[n].met_wave == wave && other.party[n].met_species == SPECIES_ID_CATERPIE);
    CHECK(other.pokeballs[POKEBALL_TYPE_ROGUE_BALL] == 1);
    /* the loaded run goes on: a ball at the next wave's wild Pokemon, or whatever it is, doesn't stop it */
    CHECK(gc_run_screen(&other)->kind == GC_SCREEN_COMMAND);
}

/* no argument: every fixture; or one of rules, balls, full, items, dex, save */
int main(int argc, char **argv)
{
    const char *only = argc > 1 ? argv[1] : NULL;
    if (!only || !strcmp(only, "rules")) rules();
    if (!only || !strcmp(only, "balls")) ball_types();
    if (!only || !strcmp(only, "full")) full_party();
    if (!only || !strcmp(only, "items")) held_items();
    if (!only || !strcmp(only, "dex")) dex_and_candy();
    if (!only || !strcmp(only, "save")) save_load();
    printf("catch: %d checks, 0 failures\n", checks);
    return 0;
}
