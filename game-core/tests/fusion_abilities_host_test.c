/* PR_DEV_FUSION_ABILITIES, a departure from the source pin that lorenzo asked for (2026-11-02): the abilities the pin switches off on a fusion
 * (NoFusionAbilityAbAttr) work, their form change lands on whichever of the two species it belongs to, the stats follow the form and the
 * types follow it through the half they come from. A real run each time: the lead is a fusion, the foe plays the move that triggers it. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_run run;
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr, "fusion abilities line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
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

/* wave 1 of a fresh run with the lead made of `first` and `second`; the foe has no ability and only Splash */
static pr_pokemon *start(uint16_t first, uint16_t second, int fusion_ability_index, int fusion_form, unsigned deviations)
{
    const char *why = NULL;
    gc_run_new(&run, GAME_MODES_CLASSIC, "fusion-abilities", 4242);
    gc_starter s[3] = {{.species = SPECIES_ID_BULBASAUR, .tera_type = -1, .moves = {MOVE_ID_TACKLE}},
                       {.species = SPECIES_ID_SQUIRTLE, .tera_type = -1, .moves = {MOVE_ID_SPLASH}},
                       {.species = SPECIES_ID_CHARMANDER, .tera_type = -1, .moves = {MOVE_ID_SPLASH}}};
    for (int i = 0; i < 3; i++) memset(s[i].ivs, 15, sizeof s[i].ivs);
    CHECK(gc_run_set_starters(&run, s, 3, &why)); settle(&run);
    CHECK(run.bt.deviations & PR_DEV_FUSION_ABILITIES);               /* game-core's real runs have it on */
    run.bt.deviations = deviations;

    gc_mon *m = &run.party[run.party_order[0]];
    gc_starter st = {.species = first, .tera_type = -1, .moves = {MOVE_ID_SPLASH}};
    memset(st.ivs, 15, sizeof st.ivs);
    gc_mon_new_player(&run, m, &st, 50);
    pr_pokemon *p = &m->b;
    p->fusion_species = second; p->fusion_form = (uint8_t)fusion_form; p->fusion_ability_index = (int8_t)fusion_ability_index;
    p->fusion_custom_ability = -1; p->fusion_gender_known = true; p->ability_lookup_known = true;
    pr_refresh_ability(p);
    pr_calculate_stats(&run.bt, p);
    p->hp = p->stats[STAT_HP];
    p->on_field = true; run.bt.field[0] = p; run.bt.party[0][0] = p;

    pr_pokemon *enemy = run.bt.field[2];
    set_only_move(enemy, MOVE_ID_SPLASH);
    enemy->stats[STAT_HP] = 100000; enemy->hp = enemy->stats[STAT_HP];   /* the foe survives what the lead throws at it */
    enemy->custom_ability = ABILITY_ID_BALL_FETCH; enemy->ability_override = -1; pr_refresh_ability(enemy);
    return p;
}

static bool has_types(const pr_pokemon *p, int a, int b)
{
    int8_t types[PR_MAX_TYPES];
    const int n = pr_get_types(p, types, 0);
    return n == 2 && types[0] == a && types[1] == b;
}

/* Forecast and Flower Gift: the weather changes the form of the species that has the ability, a fusion's second included */
static void weather_forms(void)
{
    /* Squirtle fused with Castform: Forecast is the fusion's ability; Sunny Day takes Castform's half to its sunny form, Squirtle's stays */
    pr_pokemon *p = start(SPECIES_ID_SQUIRTLE, SPECIES_ID_CASTFORM, 0, 0, PR_DEV_FUSION_ABILITIES);
    CHECK(p->ability == ABILITY_ID_FORECAST);
    CHECK(has_types(p, POKEMON_TYPE_WATER, POKEMON_TYPE_NORMAL));
    const int32_t hp_before = p->stats[STAT_HP];
    set_only_move(p, MOVE_ID_SUNNY_DAY);
    run.n_battle_events = 0;
    choose(GC_CH_FIGHT, 0);
    CHECK(run.bt.weather == WEATHER_TYPE_SUNNY);
    CHECK(p->fusion_form == 1 && p->form == 0);                       /* Castform's half changed, Squirtle's did not */
    CHECK(has_types(p, POKEMON_TYPE_WATER, POKEMON_TYPE_FIRE));         /* the second type is the form's: Fire for Normal */
    CHECK(p->stats[STAT_HP] >= hp_before && p->species == SPECIES_ID_SQUIRTLE && p->fusion_species == SPECIES_ID_CASTFORM);
    CHECK(texts("battlePokemonForm:formChange") == 1);

    /* rain, and snow, after it: the other weather forms of the same half */
    set_only_move(p, MOVE_ID_RAIN_DANCE);
    choose(GC_CH_FIGHT, 0);
    CHECK(run.bt.weather == WEATHER_TYPE_RAIN && p->fusion_form == 2 && p->form == 0);
    {
        int8_t types[PR_MAX_TYPES];                                    /* Water and Water: one type, the second is not repeated */
        CHECK(pr_get_types(p, types, 0) == 1 && types[0] == POKEMON_TYPE_WATER);
    }

    /* a switch to another member takes the form back (the Active trigger), the fusion's half only */
    choose(GC_CH_POKEMON, 1);
    CHECK(p->fusion_form == 0 && p->form == 0 && has_types(p, POKEMON_TYPE_WATER, POKEMON_TYPE_NORMAL));

    /* Flower Gift on Cherrim as the second species: sunshine form in the sun, nothing in the types (Grass either way) */
    p = start(SPECIES_ID_SQUIRTLE, SPECIES_ID_CHERRIM, 0, 0, PR_DEV_FUSION_ABILITIES);
    CHECK(p->ability == ABILITY_ID_FLOWER_GIFT);
    set_only_move(p, MOVE_ID_SUNNY_DAY);
    choose(GC_CH_FIGHT, 0);
    CHECK(run.bt.weather == WEATHER_TYPE_SUNNY && p->fusion_form == 1 && p->form == 0);
    CHECK(has_types(p, POKEMON_TYPE_WATER, POKEMON_TYPE_GRASS));

    /* Castform as the FIRST species: the fused Pokemon's ability is the second species' (Torrent), so Forecast is not in play and the
     * weather changes nothing, as in the pin */
    p = start(SPECIES_ID_CASTFORM, SPECIES_ID_SQUIRTLE, 0, 0, PR_DEV_FUSION_ABILITIES);
    CHECK(p->ability != ABILITY_ID_FORECAST);
    set_only_move(p, MOVE_ID_SUNNY_DAY);
    choose(GC_CH_FIGHT, 0);
    CHECK(run.bt.weather == WEATHER_TYPE_SUNNY && p->form == 0 && p->fusion_form == 0);

    /* the pin: the flag cleared, Forecast does nothing on a fusion */
    p = start(SPECIES_ID_SQUIRTLE, SPECIES_ID_CASTFORM, 0, 0, 0);
    CHECK(p->ability == ABILITY_ID_FORECAST);
    set_only_move(p, MOVE_ID_SUNNY_DAY);
    choose(GC_CH_FIGHT, 0);
    CHECK(run.bt.weather == WEATHER_TYPE_SUNNY && p->fusion_form == 0 && p->form == 0);
}

/* the ability slot (0, 1, 2 hidden) that holds `ability` in `form` of `species` */
static int slot(uint16_t species, int form, int ability)
{
    pr_pokemon tmp;
    memset(&tmp, 0, sizeof tmp);
    tmp.species = species;
    tmp.form = (uint8_t)form;
    const pr_form *f = pr_pokemon_base_form(&tmp);
    return f->ability1 == ability ? 0 : f->ability2 == ability ? 1 : f->ability_hidden == ability ? 2 : -1;
}
static void end_turn(void) { run.n_battle_events = 0; choose(GC_CH_FIGHT, 0); }
static void set_hp_percent(pr_pokemon *p, int percent) { p->hp = p->stats[STAT_HP] * percent / 100; }

/* Zen Mode, Power Construct, Shields Down, Schooling: the form follows the HP at the end of a turn, and the fusion's second species changes
 * (the first stays), with the stats averaged again from the new form */
static void hp_forms(void)
{
    /* Zen Mode (Darmanitan, 50% or less) */
    pr_pokemon *p = start(SPECIES_ID_SQUIRTLE, SPECIES_ID_DARMANITAN, slot(SPECIES_ID_DARMANITAN, 0, ABILITY_ID_ZEN_MODE), 0, PR_DEV_FUSION_ABILITIES);
    CHECK(p->ability == ABILITY_ID_ZEN_MODE);
    CHECK(has_types(p, POKEMON_TYPE_WATER, POKEMON_TYPE_FIRE));
    int32_t atk = p->stats[STAT_ATK], spatk = p->stats[STAT_SPATK];
    set_hp_percent(p, 40);
    end_turn();
    CHECK(p->fusion_form == 1 && p->form == 0);
    CHECK(has_types(p, POKEMON_TYPE_WATER, POKEMON_TYPE_PSYCHIC));       /* Zen Darmanitan is Fire/Psychic: the second type comes from the fusion's form */
    CHECK(p->stats[STAT_ATK] < atk && p->stats[STAT_SPATK] > spatk);     /* the stats follow the form */
    CHECK(p->hp > 0 && p->hp <= p->stats[STAT_HP]);
    set_hp_percent(p, 90);
    end_turn();
    CHECK(p->fusion_form == 0 && p->stats[STAT_ATK] == atk && p->stats[STAT_SPATK] == spatk);   /* and back */
    p = start(SPECIES_ID_SQUIRTLE, SPECIES_ID_DARMANITAN, slot(SPECIES_ID_DARMANITAN, 0, ABILITY_ID_ZEN_MODE), 0, 0);
    set_hp_percent(p, 40);
    end_turn();
    CHECK(p->fusion_form == 0);                                           /* the pin */

    /* Power Construct (Zygarde, 50% or less: from the power construct form, 2, to the complete one, 4) */
    p = start(SPECIES_ID_SQUIRTLE, SPECIES_ID_ZYGARDE, slot(SPECIES_ID_ZYGARDE, 2, ABILITY_ID_POWER_CONSTRUCT), 2, PR_DEV_FUSION_ABILITIES);
    CHECK(p->ability == ABILITY_ID_POWER_CONSTRUCT);
    const int32_t max_hp = p->stats[STAT_HP];
    set_hp_percent(p, 40);
    end_turn();
    CHECK(p->fusion_form == 4 && p->form == 0);
    CHECK(p->stats[STAT_HP] > max_hp);                                    /* the complete form has twice the HP */

    /* Shields Down (Minior, 50% or less: from the meteor form, 0, to the core form, 7) */
    p = start(SPECIES_ID_SQUIRTLE, SPECIES_ID_MINIOR, slot(SPECIES_ID_MINIOR, 0, ABILITY_ID_SHIELDS_DOWN), 0, PR_DEV_FUSION_ABILITIES);
    CHECK(p->ability == ABILITY_ID_SHIELDS_DOWN);
    int32_t def = p->stats[STAT_DEF];
    set_hp_percent(p, 40);
    end_turn();
    CHECK(p->fusion_form == 7 && p->form == 0);
    CHECK(p->stats[STAT_DEF] < def);                                      /* the core form has the lower Defense */

    /* Schooling (Wishiwashi, level 20 and 25% or more): the school form, solo below that */
    p = start(SPECIES_ID_SQUIRTLE, SPECIES_ID_WISHIWASHI, slot(SPECIES_ID_WISHIWASHI, 0, ABILITY_ID_SCHOOLING), 0, PR_DEV_FUSION_ABILITIES);
    CHECK(p->ability == ABILITY_ID_SCHOOLING);
    atk = p->stats[STAT_ATK];
    end_turn();
    CHECK(p->fusion_form == 1 && p->form == 0 && p->stats[STAT_ATK] > atk);
    set_hp_percent(p, 20);
    end_turn();
    CHECK(p->fusion_form == 0 && p->stats[STAT_ATK] == atk);
}

/* Hunger Switch (Morpeko): the form swaps at the end of each turn, and Aura Wheel's type follows the fusion's form. Zero to Hero (Palafin): the
 * Hero form on leaving the field */
static void toggles(void)
{
    pr_pokemon *p = start(SPECIES_ID_SQUIRTLE, SPECIES_ID_MORPEKO, slot(SPECIES_ID_MORPEKO, 0, ABILITY_ID_HUNGER_SWITCH), 0, PR_DEV_FUSION_ABILITIES);
    CHECK(p->ability == ABILITY_ID_HUNGER_SWITCH);
    int type = -1;
    pr_apply_variable_move_type(&run.bt, p, MOVE_ID_AURA_WHEEL, &type);
    CHECK(type == POKEMON_TYPE_ELECTRIC);
    end_turn();
    CHECK(p->fusion_form == 1 && p->form == 0);
    type = POKEMON_TYPE_NORMAL;
    pr_apply_variable_move_type(&run.bt, p, MOVE_ID_AURA_WHEEL, &type);
    CHECK(type == POKEMON_TYPE_DARK);
    end_turn();
    CHECK(p->fusion_form == 0);

    p = start(SPECIES_ID_SQUIRTLE, SPECIES_ID_PALAFIN, slot(SPECIES_ID_PALAFIN, 0, ABILITY_ID_ZERO_TO_HERO), 0, PR_DEV_FUSION_ABILITIES);
    CHECK(p->ability == ABILITY_ID_ZERO_TO_HERO);
    int32_t atk = p->stats[STAT_ATK];
    choose(GC_CH_POKEMON, 1);
    CHECK(p->fusion_form == 1 && p->form == 0 && p->stats[STAT_ATK] > atk);
}

/* Stance Change (Aegislash): an attack takes the blade form, King's Shield the shield form again; Attack and Defense follow */
static void stance_change(void)
{
    pr_pokemon *p = start(SPECIES_ID_SQUIRTLE, SPECIES_ID_AEGISLASH, slot(SPECIES_ID_AEGISLASH, 0, ABILITY_ID_STANCE_CHANGE), 0, PR_DEV_FUSION_ABILITIES);
    CHECK(p->ability == ABILITY_ID_STANCE_CHANGE);
    const int32_t atk = p->stats[STAT_ATK], def = p->stats[STAT_DEF];
    set_only_move(p, MOVE_ID_TACKLE);
    end_turn();
    CHECK(p->fusion_form == 1 && p->form == 0);
    CHECK(p->stats[STAT_ATK] > atk && p->stats[STAT_DEF] < def);
    set_only_move(p, MOVE_ID_KINGS_SHIELD);
    end_turn();
    CHECK(p->fusion_form == 0 && p->stats[STAT_ATK] == atk && p->stats[STAT_DEF] == def);

    p = start(SPECIES_ID_SQUIRTLE, SPECIES_ID_AEGISLASH, slot(SPECIES_ID_AEGISLASH, 0, ABILITY_ID_STANCE_CHANGE), 0, 0);
    set_only_move(p, MOVE_ID_TACKLE);
    end_turn();
    CHECK(p->fusion_form == 0);                                           /* the pin */
}

/* Disguise (Mimikyu): the first damaging hit is blocked and busts the disguise, which does not come back during the battle */
static void disguise(void)
{
    pr_pokemon *p = start(SPECIES_ID_SQUIRTLE, SPECIES_ID_MIMIKYU, slot(SPECIES_ID_MIMIKYU, 0, ABILITY_ID_DISGUISE), 0, PR_DEV_FUSION_ABILITIES);
    CHECK(p->ability == ABILITY_ID_DISGUISE);
    const int32_t max_hp = p->hp;
    set_only_move(run.bt.field[2], MOVE_ID_TACKLE);
    end_turn();
    CHECK(p->fusion_form == 1 && p->form == 0);                           /* busted */
    CHECK(p->hp == max_hp - max_hp / 8);                                  /* a bust costs an eighth of the HP, the hit itself nothing */
    end_turn();
    CHECK(p->fusion_form == 1 && p->hp < max_hp - max_hp / 8);            /* the second hit lands in full */

    p = start(SPECIES_ID_SQUIRTLE, SPECIES_ID_MIMIKYU, slot(SPECIES_ID_MIMIKYU, 0, ABILITY_ID_DISGUISE), 0, 0);
    set_only_move(run.bt.field[2], MOVE_ID_TACKLE);
    end_turn();
    CHECK(p->fusion_form == 0 && p->hp < p->stats[STAT_HP]);               /* the pin: no disguise, the hit lands */
}

/* Ice Face (Eiscue): a physical hit is blocked and takes the ice face off, snow brings it back */
static void ice_face(void)
{
    pr_pokemon *p = start(SPECIES_ID_SQUIRTLE, SPECIES_ID_EISCUE, slot(SPECIES_ID_EISCUE, 0, ABILITY_ID_ICE_FACE), 0, PR_DEV_FUSION_ABILITIES);
    CHECK(p->ability == ABILITY_ID_ICE_FACE);
    const int32_t max_hp = p->hp, def = p->stats[STAT_DEF];
    set_only_move(run.bt.field[2], MOVE_ID_TACKLE);
    end_turn();
    CHECK(p->fusion_form == 1 && p->form == 0 && p->hp == max_hp);       /* the hit did nothing, the face is off */
    CHECK(p->stats[STAT_DEF] != def);                                     /* Noice Face has its own stats */
    set_only_move(run.bt.field[2], MOVE_ID_SPLASH);
    set_only_move(p, MOVE_ID_SNOWSCAPE);
    end_turn();
    CHECK(run.bt.weather == WEATHER_TYPE_SNOW && p->fusion_form == 0 && p->stats[STAT_DEF] == def);

    p = start(SPECIES_ID_SQUIRTLE, SPECIES_ID_EISCUE, slot(SPECIES_ID_EISCUE, 0, ABILITY_ID_ICE_FACE), 0, 0);
    set_only_move(run.bt.field[2], MOVE_ID_TACKLE);
    end_turn();
    CHECK(p->fusion_form == 0 && p->hp < p->stats[STAT_HP]);               /* the pin */
}

/* Gulp Missile (Cramorant): Surf loads an Arrokuda (HP above half) or a Pikachu (below), the next hit taken sets it off at the attacker */
static void gulp_missile(void)
{
    pr_pokemon *p = start(SPECIES_ID_SQUIRTLE, SPECIES_ID_CRAMORANT, slot(SPECIES_ID_CRAMORANT, 0, ABILITY_ID_GULP_MISSILE), 0, PR_DEV_FUSION_ABILITIES);
    CHECK(p->ability == ABILITY_ID_GULP_MISSILE);
    pr_pokemon *foe = run.bt.field[2];
    set_only_move(p, MOVE_ID_SURF);
    end_turn();
    CHECK(p->fusion_form == 1 && p->form == 0);                           /* gulping */
    const int32_t foe_hp = foe->hp;
    set_only_move(foe, MOVE_ID_TACKLE);
    set_only_move(p, MOVE_ID_SPLASH);
    end_turn();
    CHECK(p->fusion_form == 0 && foe->hp < foe_hp);                       /* the missile hit the attacker, the beak is empty again */

    p = start(SPECIES_ID_SQUIRTLE, SPECIES_ID_CRAMORANT, slot(SPECIES_ID_CRAMORANT, 0, ABILITY_ID_GULP_MISSILE), 0, PR_DEV_FUSION_ABILITIES);
    set_hp_percent(p, 40);
    set_only_move(p, MOVE_ID_SURF);
    end_turn();
    CHECK(p->fusion_form == 2 && p->form == 0);                           /* gorging */

    p = start(SPECIES_ID_SQUIRTLE, SPECIES_ID_CRAMORANT, slot(SPECIES_ID_CRAMORANT, 0, ABILITY_ID_GULP_MISSILE), 0, 0);
    set_only_move(p, MOVE_ID_SURF);
    end_turn();
    CHECK(p->fusion_form == 0);                                           /* the pin */
}

/* Battle Bond (Greninja): knocking a Pokemon out takes the Ash form */
static void battle_bond(void)
{
    pr_pokemon *p = start(SPECIES_ID_SQUIRTLE, SPECIES_ID_BATTLE_BOND_GRENINJA, slot(SPECIES_ID_BATTLE_BOND_GRENINJA, 0, ABILITY_ID_BATTLE_BOND), 0,
                          PR_DEV_FUSION_ABILITIES);
    CHECK(p->ability == ABILITY_ID_BATTLE_BOND);
    pr_pokemon *foe = run.bt.field[2];
    foe->hp = 1;
    set_only_move(p, MOVE_ID_TACKLE);
    end_turn();
    CHECK(foe->hp == 0);
    CHECK(p->fusion_form == 1 && p->form == 0);

    p = start(SPECIES_ID_SQUIRTLE, SPECIES_ID_BATTLE_BOND_GRENINJA, slot(SPECIES_ID_BATTLE_BOND_GRENINJA, 0, ABILITY_ID_BATTLE_BOND), 0, 0);
    foe = run.bt.field[2];
    foe->hp = 1;
    set_only_move(p, MOVE_ID_TACKLE);
    end_turn();
    CHECK(foe->hp == 0 && p->fusion_form == 0);                           /* the pin */
}

int main(void)
{
    weather_forms();
    hp_forms();
    toggles();
    stance_change();
    disguise();
    ice_face();
    gulp_missile();
    battle_bond();
    printf("fusion abilities: %u checks\n", checks);
    return 0;
}
