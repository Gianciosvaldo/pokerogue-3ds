/* The player's Terastallize command in a run (src/gc_battle.c, src/gc_reward.c): the screen offers it (CommandUiHandler.canTera), the choice
 * FIGHT with c = 1 is Command.TERA (CommandPhase.handleFightCommand's TERA pre-turn command), and the turn plays TeraPhase.
 *  - no Tera Orb: the screen does not offer it and the choice is refused, nothing in the commands changes;
 *  - with the Orb: the screen offers it with the Pokemon's Tera type, the turn Terastallizes (the text, terastallized, the arena counter at 1, the
 *    Pokemon's types are its Tera type), and from the next turn on the screen no longer offers it and the choice is refused (MAX_TERAS_PER_ARENA);
 *  - a Mega, a Gigantamax and Ultra Necrozma (as the species or as the fusion half) cannot, a Terapagos offers Stellar;
 *  - doubles: after slot 0 chose to Terastallize slot 1 is not offered it, and cancelling back to slot 0 offers it again;
 *  - a battle saved after the Tera loads back Terastallized with the counter used, and the screen does not offer it;
 *  - a new arena (the biome switch) and the party heal give the Tera back.
 * Each check fails on game-core as it was (no command, no screen field). */
#include "../src/internal.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_run run, back;
static gc_system sys;
static unsigned char save[GC_SAVE_MAX];
static int checks, n_tera, n_refused, n_text;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr, "tera command FAIL line %d: %s\n", __LINE__, #x); exit(1); } } while (0)

static void drain_text(bool *tera_text)
{
    gc_event e;
    while (gc_run_next_event(&run, &e)) {
        if (e.kind == GC_EV_MESSAGE && e.msg.key && !strcmp(e.msg.key, "battle:pokemonTerastallized")) {
            if (tera_text) *tera_text = true;
            n_text++;
        }
        if (e.kind == GC_EV_BATTLE) {
            const pr_event *ev = gc_run_battle_event_data(&run) + e.a;
            for (int k = 0; k < e.b; k++)
                if (ev[k].type == PR_EV_TEXT && ev[k].msg.key && !strcmp(ev[k].msg.key, "battle:pokemonTerastallized")) {
                    if (tera_text) *tera_text = true;
                    n_text++;
                }
        }
    }
    CHECK(!run.ev_overflow);
}

static void start(int species, bool orb, int tera_type)
{
    memset(&run, 0, sizeof run);
    gc_system_init_defaults(&sys);
    gc_run_new(&run, GAME_MODES_CLASSIC, "tera-command", 4242);
    run.sys = &sys;
    run.n_items = 0;
    run.n_party = 2;
    for (int i = 0; i < 2; i++) {
        gc_starter s;
        memset(&s, 0, sizeof s);
        s.species = (uint16_t)(i == 0 ? species : SPECIES_ID_BULBASAUR);
        s.tera_type = (int8_t)(i == 0 ? tera_type : -1);
        for (int k = 0; k < 6; k++) s.ivs[k] = 31;
        s.moves[0] = MOVE_ID_SPLASH;
        gc_mon_new_player(&run, &run.party[i], &s, 50);
        run.party_order[i] = (int8_t)i;
    }
    if (orb) gc_item_add(&run, "TERA_ORB", -1, 0, 1);
    run.money = 1000;
    run.battle_type = BATTLE_TYPE_WILD;
    gc_items_to_battle(&run);
    gc_rq_reset(&gc_int(&run)->rq);
    run.wave = 5;
    run.in_battle = true;
    gc_rq_push(&run, gc_rp(RP_NEW_BATTLE, 0, 0, 0));
    gc_run_until_screen(&run);
    drain_text(NULL);
    /* nothing the enemy does should end the test early */
    pr_pokemon *foe = run.bt.field[2];
    if (foe) { foe->hp = foe->stats[0] = 100000; foe->moves[0] = MOVE_ID_SPLASH; foe->n_moves = 1; foe->pp_used[0] = 0; }
    run.bt.player_teras_used_known = true;
}

static gc_choice g_choice;
static const gc_choice *fight(int slot, bool tera)
{
    g_choice = (gc_choice){GC_CH_FIGHT, (int16_t)slot, -1, (int16_t)(tera ? 1 : 0)};
    return &g_choice;
}

static void single(void)
{
    /* with the Orb */
    start(SPECIES_ID_PIKACHU, true, POKEMON_TYPE_WATER);
    CHECK(run.screen.kind == GC_SCREEN_COMMAND);
    pr_pokemon *p = run.bt.field[0];
    CHECK(p && !p->terastallized && run.bt.player_teras_used == 0);
    CHECK(run.screen.can_tera && run.screen.tera_type == POKEMON_TYPE_WATER);
    const char *why = NULL;
    const int turn = run.bt.turn;
    CHECK(gc_run_choose(&run, &(gc_choice){GC_CH_FIGHT, 0, -1, 1}, &why));
    bool text = false;
    drain_text(&text);
    CHECK(text);
    CHECK(run.screen.kind == GC_SCREEN_COMMAND && run.bt.turn == turn + 1);
    CHECK(p->terastallized && run.bt.player_teras_used == 1);
    int8_t ty[PR_MAX_TYPES];
    CHECK(pr_get_types(p, ty, 0) == 1 && ty[0] == POKEMON_TYPE_WATER);   /* Pikachu is now only a Water type */
    n_tera++;
    /* the limit is one per arena: the next screen does not offer it and the choice is refused */
    CHECK(!run.screen.can_tera && run.screen.tera_type == POKEMON_TYPE_UNKNOWN);
    why = NULL;
    CHECK(!gc_run_choose(&run, &(gc_choice){GC_CH_FIGHT, 0, -1, 1}, &why) && why && *why);
    n_refused++;
    CHECK(run.screen.kind == GC_SCREEN_COMMAND && run.bt.turn == turn + 1);   /* still the same command screen, nothing was taken */
    CHECK(run.bt.pre_commands[0].command != COMMAND_TERA);
    /* a plain FIGHT still works, and so does saving here */
    CHECK(gc_run_choose(&run, fight(0, false), &why));
    drain_text(NULL);
    CHECK(p->terastallized && run.bt.player_teras_used == 1);

    /* win the battle (the foe has 1 HP), take nothing from the rewards, and arrive at the next wave: the arena's Tera is still used (the
     * counter lives on through the waves of one arena), so the screen does not offer it there either, and a save at that wave's start loads
     * back with the counter */
    const int wave = run.wave;
    for (int step = 0; step < 400 && run.wave == wave; step++) {
        drain_text(NULL);
        const gc_screen *sc = gc_run_screen(&run);
        gc_choice c = {0};
        pr_pokemon *foe = run.bt.field[2];
        switch (sc->kind) {
        case GC_SCREEN_COMMAND:
            if (foe) { foe->hp = 1; }
            run.bt.field[0]->moves[0] = MOVE_ID_TACKLE; run.bt.field[0]->n_moves = 1; run.bt.field[0]->pp_used[0] = 0;
            c = *fight(0, false);
            break;
        case GC_SCREEN_TARGET: c = (gc_choice){GC_CH_TARGET, 2, 0, 0}; break;
        case GC_SCREEN_SWITCH: c = (gc_choice){GC_CH_PICK, -1, 0, 0}; break;
        case GC_SCREEN_LEARN_MOVE: c = (gc_choice){GC_CH_FORGET, -1, 0, 0}; break;
        case GC_SCREEN_EVOLVE: c.kind = GC_CH_CANCEL; break;
        case GC_SCREEN_REWARD: c.kind = GC_CH_SKIP; break;
        case GC_SCREEN_CHECK_SWITCH: c = (gc_choice){GC_CH_CONFIRM, 0, 0, 0}; break;
        default: CHECK(false);
        }
        CHECK(gc_run_choose(&run, &c, &why));
    }
    drain_text(NULL);
    CHECK(run.wave == wave + 1 && run.screen.kind == GC_SCREEN_COMMAND);
    fprintf(stderr, "next wave: terastallized=%d counter=%g can_tera=%d\n", run.bt.field[0] ? run.bt.field[0]->terastallized : -1,
            run.bt.player_teras_used, run.screen.can_tera);
    CHECK(run.bt.player_teras_used == 1 && !run.screen.can_tera);
    CHECK(gc_run_can_save(&run));
    const size_t len = gc_run_save(&run, save, sizeof save);
    CHECK(len > 0);
    memset(&back, 0, sizeof back);
    CHECK(gc_run_load(&back, save, len, NULL));
    CHECK(back.screen.kind == GC_SCREEN_COMMAND && back.bt.player_teras_used == 1 && !back.screen.can_tera);
    CHECK(!gc_run_choose(&back, fight(0, true), &why));

    /* a new arena or the party heal gives the Tera back */
    run.bt.player_teras_used = 0;
    gc_battle_command_screen(&run);
    CHECK(run.screen.can_tera);
}

static void no_orb(void)
{
    start(SPECIES_ID_PIKACHU, false, POKEMON_TYPE_WATER);
    CHECK(run.screen.kind == GC_SCREEN_COMMAND && !run.screen.can_tera);
    const struct pr_turn_command before_turn = run.bt.commands[0], before_pre = run.bt.pre_commands[0];
    const char *why = NULL;
    CHECK(!gc_run_choose(&run, fight(0, true), &why) && why && *why);
    n_refused++;
    CHECK(!memcmp(&before_turn, &run.bt.commands[0], sizeof before_turn) && !memcmp(&before_pre, &run.bt.pre_commands[0], sizeof before_pre));
    CHECK(run.screen.kind == GC_SCREEN_COMMAND && !run.bt.field[0]->terastallized && run.bt.player_teras_used == 0);
    CHECK(gc_run_choose(&run, fight(0, false), &why));   /* the plain command is untouched */
    drain_text(NULL);
    CHECK(!run.bt.field[0]->terastallized);
}

static void forms(void)
{
    start(SPECIES_ID_PIKACHU, true, -1);
    pr_pokemon *p = run.bt.field[0];
    CHECK(gc_can_species_tera(&run, p) && gc_can_terastallize(&run, p));
    run.bt.player_teras_used = 1;
    CHECK(gc_can_species_tera(&run, p) && !gc_can_terastallize(&run, p));   /* the arena's Tera is used */
    run.bt.player_teras_used = 0;
    /* Mega and Gigantamax cannot */
    start(SPECIES_ID_CHARIZARD, true, -1);
    p = run.bt.field[0];
    const pr_species *sp = gc_species(SPECIES_ID_CHARIZARD);
    int mega = -1, max = -1, base = p->form;
    for (int f = 0; sp && f < sp->nforms; f++) {
        const char *k = gc_form(SPECIES_ID_CHARIZARD, f)->form_key;
        if (!strcmp(k, "mega-x") && mega < 0) mega = f;
        if (!strcmp(k, "gigantamax") && max < 0) max = f;
    }
    CHECK(mega >= 0 && max >= 0);
    p->form = (uint8_t)mega;
    CHECK(!gc_can_species_tera(&run, p));
    p->form = (uint8_t)max;
    CHECK(!gc_can_species_tera(&run, p));
    p->form = (uint8_t)base;
    CHECK(gc_can_species_tera(&run, p));
    /* Ultra Necrozma, as the species and as the fusion half */
    int ultra = -1;
    sp = gc_species(SPECIES_ID_NECROZMA);
    for (int f = 0; sp && f < sp->nforms; f++)
        if (!strcmp(gc_form(SPECIES_ID_NECROZMA, f)->form_key, "ultra")) ultra = f;
    CHECK(ultra >= 0);
    p->species = SPECIES_ID_NECROZMA;
    p->form = (uint8_t)ultra;
    CHECK(!gc_can_species_tera(&run, p));
    p->form = 0;
    CHECK(gc_can_species_tera(&run, p));
    p->species = SPECIES_ID_CHARIZARD;
    p->form = (uint8_t)base;
    p->fusion_species = SPECIES_ID_NECROZMA;
    p->fusion_form = (uint8_t)ultra;
    CHECK(!gc_can_species_tera(&run, p));
    p->fusion_form = 0;
    CHECK(gc_can_species_tera(&run, p));
    /* no Orb: nothing can */
    start(SPECIES_ID_CHARIZARD, false, -1);
    CHECK(!gc_can_species_tera(&run, run.bt.field[0]));
    /* Terapagos offers Stellar */
    start(SPECIES_ID_TERAPAGOS, true, POKEMON_TYPE_FIRE);
    CHECK(run.screen.can_tera && run.screen.tera_type == POKEMON_TYPE_STELLAR);
}

static void doubles(void)
{
    int played = 0;
    for (int seed = 1; seed <= 40 && !played; seed++) {
        char key[64];
        snprintf(key, sizeof key, "tera-doubles-%d", seed);
        memset(&run, 0, sizeof run);
        gc_run_new(&run, GAME_MODES_ENDLESS, key, seed);
        run.wave = 1;
        gc_item_add(&run, "TERA_ORB", -1, 0, 1);
        gc_starter team[3] = {{.species = 1, .tera_type = -1}, {.species = 4, .tera_type = -1}, {.species = 7, .tera_type = -1}};
        for (int k = 0; k < 3; k++) memset(team[k].ivs, 15, 6);
        const char *why = NULL;
        if (!gc_run_set_starters(&run, team, 3, &why)) { fprintf(stderr, "set_starters: %s\n", why ? why : "?"); CHECK(false); }
        drain_text(NULL);
        if (!run.double_battle || run.battle_type != BATTLE_TYPE_WILD || !run.bt.field[1] || !run.bt.field[0]) continue;
        played = 1;
        for (int k = 0; k < 4; k++) {
            pr_pokemon *m = run.bt.field[k];
            m->hp = m->stats[STAT_HP] = 100000;
            m->moves[0] = MOVE_ID_SPLASH;
            m->n_moves = 1;
            m->pp_used[0] = 0;
            m->max_pp_override[0] = -1;
        }
        CHECK(run.screen.kind == GC_SCREEN_COMMAND && run.screen.slot == 0 && run.screen.can_tera);
        CHECK(gc_run_choose(&run, fight(0, true), &why));
        drain_text(NULL);
        CHECK(run.screen.kind == GC_SCREEN_COMMAND && run.screen.slot == 1);
        CHECK(run.bt.pre_commands[0].command == COMMAND_TERA && !run.bt.pre_commands[0].skip);
        CHECK(!run.screen.can_tera);                                            /* canTera: the planned Tera counts against the limit */
        CHECK(!gc_run_choose(&run, fight(0, true), &why));
        /* back out of slot 1 to slot 0 (CommandPhase.cancel): slot 0 is offered the Tera again, and the plan is not kept */
        CHECK(run.screen.can_undo);
        CHECK(gc_run_choose(&run, &(gc_choice){GC_CH_CANCEL, 0, 0, 0}, &why));
        CHECK(run.screen.kind == GC_SCREEN_COMMAND && run.screen.slot == 0 && run.screen.can_tera);
        CHECK(run.bt.pre_commands[0].command != COMMAND_TERA);
        /* slot 0 plays a plain fight, slot 1 may now Terastallize */
        CHECK(gc_run_choose(&run, fight(0, false), &why));
        drain_text(NULL);
        CHECK(run.screen.kind == GC_SCREEN_COMMAND && run.screen.slot == 1 && run.screen.can_tera);
        CHECK(gc_run_choose(&run, fight(0, true), &why));
        bool text = false;
        drain_text(&text);
        CHECK(text && run.bt.player_teras_used == 1);
        CHECK(!run.bt.field[0]->terastallized && run.bt.field[1]->terastallized);
        n_tera++;
    }
    CHECK(played);
}

/* where a Classic run gets the Orb: Rival 4's win (wave 95) gives TERA_ORB (setModifierRewardFuncs(() => modifierTypes.TERA_ORB)); the Orb is not in
 * the Classic reward pool (weight 0 in Classic), and no Tera Shard is offered without it */
static void rival_four_gives_the_orb(void)
{
    const gc_trainer_config *c = gc_trainer_config_of(TRAINER_TYPE_RIVAL_4);
    CHECK(c && c->n_reward_funcs == 1);
    pr_reward_trainer_victory tv;
    CHECK(pr_reward_trainer_victory_rewards(TRAINER_TYPE_RIVAL_4, true, false, &tv) == 0);
    CHECK(tv.n_items >= 1 && tv.items[0] == PR_ITEM_TERA_ORB);
    CHECK(CLASSIC_FIXED_BOSS_WAVES_RIVAL_4 == 95);
    /* the Orb as the reward is taken into the items as its own modifier (no stacking: TerastallizeAccessModifier has no match()) */
    start(SPECIES_ID_PIKACHU, false, -1);
    CHECK(gc_item_stack(&run, "TERA_ORB") == 0);
    CHECK(gc_port_give_reward(&run, "TERA_ORB") == -1 && gc_item_stack(&run, "TERA_ORB") == 1);
    CHECK(gc_port_give_reward(&run, "TERA_ORB") == -1 && gc_item_stack(&run, "TERA_ORB") == 2);
    gc_battle_command_screen(&run);
    CHECK(run.screen.can_tera);
}

int main(void)
{
    rival_four_gives_the_orb();
    single();
    no_orb();
    forms();
    doubles();
    printf("Player Tera command: %d checks, %d Terastallizations, %d refusals, %d texts\n", checks, n_tera, n_refused, n_text);
    return 0;
}
