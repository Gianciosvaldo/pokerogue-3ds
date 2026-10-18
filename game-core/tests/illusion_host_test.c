/* PR_DEV_ILLUSION, a departure from the source pin that lorenzo asked for (the pin leaves Illusion unimplemented;
 * this follows its commented-out draft). A real run: a player Zoroark enters the next wave disguised as the last
 * other party member, the field shows the disguise, a damaging hit ends it with "illusion wore off". Then the
 * disguise rules on their own: a trainer's party, no other member, the Ogerpon/Terapagos Tera exception, a fusion,
 * the flag cleared, a wild one from game-core's randomSpecies callback without touching the run's RNG, and which
 * hits end it. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_run run;
static int checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr, "Illusion host line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
static void settle(gc_run *r) { gc_event ev; while (gc_run_next_event(r, &ev)) {} }
static int texts(const char *key)
{
    int n = 0;
    for (int i = 0; i < run.n_battle_events; i++) {
        const pr_event *e = &run.battle_events[i];
        if (e->type == PR_EV_TEXT && e->msg.key && !strcmp(e->msg.key, key)) n++;
    }
    return n;
}
static int events(int type)
{
    int n = 0;
    for (int i = 0; i < run.n_battle_events; i++) n += run.battle_events[i].type == type;
    return n;
}
static void choose(uint8_t kind, int a)
{
    const char *why = NULL;
    gc_choice c = {kind, (int16_t)a, -1, 0};
    bool ok = gc_run_choose(&run, &c, &why);
    if (!ok) fprintf(stderr, "choice %d/%d refused on screen %d: %s\n", kind, a, run.screen.kind, why ? why : "?");
    CHECK(ok);
    settle(&run);
}
static void set_only_move(pr_pokemon *p, int move) { p->moves[0] = (uint16_t)move; p->n_moves = 1; p->pp_used[0] = 0; }

static pr_pokemon mon(uint16_t species, int ability, bool player)
{
    pr_pokemon p;
    memset(&p, 0, sizeof p);
    p.species = species; p.hp = 10; p.stats[STAT_HP] = 10; p.is_player = player; p.has_trainer = !player;
    p.base_ability = p.ability = (uint16_t)ability; p.ability_override = -1; p.custom_ability = -1;
    return p;
}

int main(void)
{
    const char *why = NULL;
    gc_run_new(&run, GAME_MODES_CLASSIC, "illusion", 777);
    gc_starter s[3] = {{.species = SPECIES_ID_BULBASAUR, .tera_type = -1, .moves = {MOVE_ID_TACKLE}},
                       {.species = SPECIES_ID_SQUIRTLE, .tera_type = -1, .moves = {MOVE_ID_SPLASH}},
                       {.species = SPECIES_ID_CHARMANDER, .tera_type = -1, .moves = {MOVE_ID_SPLASH}}};
    for (int i = 0; i < 3; i++) memset(s[i].ivs, 15, sizeof s[i].ivs);
    CHECK(gc_run_set_starters(&run, s, 3, &why)); settle(&run);
    CHECK(run.bt.deviations & PR_DEV_ILLUSION);                       /* game-core's real runs have it on */

    /* the lead becomes a Zoroark mid-battle: no disguise yet (it takes one when it enters) */
    gc_mon *m = &run.party[run.party_order[0]];
    gc_starter z = {.species = SPECIES_ID_ZOROARK, .tera_type = -1, .moves = {MOVE_ID_TACKLE}};
    memset(z.ivs, 15, sizeof z.ivs);
    gc_mon_new_player(&run, m, &z, 50);
    m->b.on_field = true; run.bt.field[0] = &m->b; run.bt.party[0][0] = &m->b;
    pr_pokemon *p = &m->b;
    CHECK(p->ability == ABILITY_ID_ILLUSION && !p->illusion.active);
    const pr_pokemon *last = &run.party[run.party_order[run.n_party - 1]].b;
    CHECK(last != p && last->species == SPECIES_ID_CHARMANDER);

    /* win wave 1; the next wave's start disguises it as the last other member */
    pr_pokemon *enemy = run.bt.field[2];
    enemy->hp = 1;
    choose(GC_CH_FIGHT, 0);
    for (int step = 0; step < 20 && run.screen.kind != GC_SCREEN_COMMAND; step++) {
        gc_choice pick = {0};
        if (run.screen.kind == GC_SCREEN_REWARD) pick.kind = GC_CH_SKIP;
        else if (run.screen.kind == GC_SCREEN_LEARN_MOVE) { pick.kind = GC_CH_FORGET; pick.a = -1; }
        else if (run.screen.kind == GC_SCREEN_EVOLVE) { pick.kind = GC_CH_CANCEL; pick.a = 1; }
        else if (run.screen.kind == GC_SCREEN_BIOME) { pick.kind = GC_CH_BIOME; pick.a = 0; }
        else { fprintf(stderr, "unexpected screen %d\n", run.screen.kind); return 1; }
        CHECK(gc_run_choose(&run, &pick, &why)); settle(&run);
    }
    CHECK(run.wave == 2 && run.screen.kind == GC_SCREEN_COMMAND && run.bt.field[0] == p);
    CHECK(p->illusion.active && p->illusion.species == SPECIES_ID_CHARMANDER && p->species == SPECIES_ID_ZOROARK);
    CHECK(p->ability == ABILITY_ID_ILLUSION);                           /* only the look changes */

    /* the foe's Tackle ends it: the field shows the real Zoroark again and the text says so */
    enemy = run.bt.field[2];
    set_only_move(enemy, MOVE_ID_TACKLE);
    enemy->custom_ability = ABILITY_ID_BALL_FETCH; enemy->ability_override = -1; pr_refresh_ability(enemy);
    set_only_move(p, MOVE_ID_SPLASH);
    p->hp = p->stats[STAT_HP];
    run.n_battle_events = 0;
    choose(GC_CH_FIGHT, 0);
    CHECK(!p->illusion.active);
    CHECK(texts("abilityTriggers:illusionBreak") == 1 && events(PR_EV_ILLUSION_BREAK) == 1);
    for (int i = 0; i < run.n_battle_events; i++) {
        const pr_event *e = &run.battle_events[i];
        if (e->type == PR_EV_ILLUSION_BREAK) CHECK(e->battler == 0 && e->field[0].species == SPECIES_ID_ZOROARK);
    }

    /* the rules on their own, on a battle of the test's */
    static pr_battle bt;
    memset(&bt, 0, sizeof bt);
    bt.deviations = PR_DEV_ILLUSION;
    pr_pokemon zo = mon(SPECIES_ID_ZOROARK, ABILITY_ID_ILLUSION, false), a = mon(SPECIES_ID_PIDGEY, ABILITY_ID_KEEN_EYE, false),
               b = mon(SPECIES_ID_RATTATA, ABILITY_ID_GUTS, false);
    pr_refresh_ability(&zo); pr_refresh_ability(&a); pr_refresh_ability(&b);
    bt.party[1][0] = &zo; bt.party[1][1] = &a; bt.party[1][2] = &b; bt.n_party[1] = 3;
    b.hp = 0;                                                           /* a fainted member is not allowed in battle */
    pr_illusion_on_summon(&bt, &zo);
    CHECK(zo.illusion.active && zo.illusion.species == SPECIES_ID_PIDGEY);
    pr_illusion_on_summon(&bt, &zo);                                    /* already disguised: kept */
    CHECK(zo.illusion.species == SPECIES_ID_PIDGEY);
    CHECK(!pr_illusion_on_hit(&bt, &zo, HIT_RESULT_NO_EFFECT) && zo.illusion.active);
    CHECK(!pr_illusion_on_hit(&bt, &zo, HIT_RESULT_STATUS) && zo.illusion.active);
    CHECK(pr_illusion_on_hit(&bt, &zo, HIT_RESULT_NOT_VERY_EFFECTIVE) && !zo.illusion.active);
    a.hp = 0;                                                           /* no other member: none */
    pr_illusion_on_summon(&bt, &zo); CHECK(!zo.illusion.active);
    a.hp = 10; a.species = SPECIES_ID_TERAPAGOS; a.terastallized = true; /* the Tera exception */
    pr_illusion_on_summon(&bt, &zo); CHECK(!zo.illusion.active);
    a.terastallized = false; zo.terastallized = true;
    pr_illusion_on_summon(&bt, &zo); CHECK(!zo.illusion.active);
    zo.terastallized = false;
    pr_illusion_on_summon(&bt, &zo); CHECK(zo.illusion.active && zo.illusion.species == SPECIES_ID_TERAPAGOS);
    pr_break_illusion(&zo);
    zo.fusion_species = SPECIES_ID_PIDGEY;                              /* NoFusionAbilityAbAttr */
    pr_illusion_on_summon(&bt, &zo); CHECK(!zo.illusion.active);
    zo.fusion_species = 0;
    bt.deviations = 0;                                                  /* the pin: Illusion does nothing */
    pr_illusion_on_summon(&bt, &zo); CHECK(!zo.illusion.active);
    bt.deviations = PR_DEV_ILLUSION;
    zo.ability = zo.base_ability = ABILITY_ID_KEEN_EYE; pr_refresh_ability(&zo);   /* without the ability */
    pr_illusion_on_summon(&bt, &zo); CHECK(!zo.illusion.active);

    /* a wild one: game-core's callback, a species other than its own, and the run's RNG untouched */
    pr_pokemon wild = mon(SPECIES_ID_ZOROARK, ABILITY_ID_ILLUSION, false);
    wild.has_trainer = false; wild.level = 30; pr_refresh_ability(&wild);
    CHECK(run.bt.illusion_species != NULL);
    const pr_run before = run.run;
    pr_illusion_on_summon(&run.bt, &wild);
    CHECK(wild.illusion.active && wild.illusion.species != SPECIES_ID_ZOROARK && wild.illusion.species > 0);
    CHECK(!memcmp(&before.rng, &run.run.rng, sizeof before.rng));
    const uint16_t first = wild.illusion.species;
    pr_break_illusion(&wild); pr_illusion_on_summon(&run.bt, &wild);
    CHECK(wild.illusion.species == first);                              /* same wave, same roll */

    printf("Illusion deviation: %d checks passed\n", checks);
    return 0;
}
