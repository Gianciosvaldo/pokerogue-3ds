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
static pr_pokemon *start(uint16_t first, uint16_t second, int fusion_ability_index, unsigned deviations)
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
    p->fusion_species = second; p->fusion_form = 0; p->fusion_ability_index = (int8_t)fusion_ability_index;
    p->fusion_custom_ability = -1; p->fusion_gender_known = true; p->ability_lookup_known = true;
    pr_refresh_ability(p);
    pr_calculate_stats(&run.bt, p);
    p->hp = p->stats[STAT_HP];
    p->on_field = true; run.bt.field[0] = p; run.bt.party[0][0] = p;

    pr_pokemon *enemy = run.bt.field[2];
    set_only_move(enemy, MOVE_ID_SPLASH);
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
    pr_pokemon *p = start(SPECIES_ID_SQUIRTLE, SPECIES_ID_CASTFORM, 0, PR_DEV_FUSION_ABILITIES);
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
    p = start(SPECIES_ID_SQUIRTLE, SPECIES_ID_CHERRIM, 0, PR_DEV_FUSION_ABILITIES);
    CHECK(p->ability == ABILITY_ID_FLOWER_GIFT);
    set_only_move(p, MOVE_ID_SUNNY_DAY);
    choose(GC_CH_FIGHT, 0);
    CHECK(run.bt.weather == WEATHER_TYPE_SUNNY && p->fusion_form == 1 && p->form == 0);
    CHECK(has_types(p, POKEMON_TYPE_WATER, POKEMON_TYPE_GRASS));

    /* Castform as the FIRST species: the fused Pokemon's ability is the second species' (Torrent), so Forecast is not in play and the
     * weather changes nothing, as in the pin */
    p = start(SPECIES_ID_CASTFORM, SPECIES_ID_SQUIRTLE, 0, PR_DEV_FUSION_ABILITIES);
    CHECK(p->ability != ABILITY_ID_FORECAST);
    set_only_move(p, MOVE_ID_SUNNY_DAY);
    choose(GC_CH_FIGHT, 0);
    CHECK(run.bt.weather == WEATHER_TYPE_SUNNY && p->form == 0 && p->fusion_form == 0);

    /* the pin: the flag cleared, Forecast does nothing on a fusion */
    p = start(SPECIES_ID_SQUIRTLE, SPECIES_ID_CASTFORM, 0, 0);
    CHECK(p->ability == ABILITY_ID_FORECAST);
    set_only_move(p, MOVE_ID_SUNNY_DAY);
    choose(GC_CH_FIGHT, 0);
    CHECK(run.bt.weather == WEATHER_TYPE_SUNNY && p->fusion_form == 0 && p->form == 0);
}

int main(void)
{
    weather_forms();
    printf("fusion abilities: %u checks\n", checks);
    return 0;
}
