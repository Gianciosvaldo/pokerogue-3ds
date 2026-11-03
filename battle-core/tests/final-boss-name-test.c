/* Final-boss name affixes follow getPokemonNameWithAffix in pinned upstream. */
#include "pr/engine.h"
#include <stdio.h>
#include <string.h>

static const char locale[] =
    "{\"test\":\"{{pokemonName}}\","
    "\"wildPokemonWithAffix\":\"Wild {{pokemonName}}\","
    "\"foePokemonWithAffix\":\"Foe {{pokemonName}}\"}";
static const char pokemon_locale[] = "{\"eternatus\":\"Eternatus\"}";

static const char *get_locale(void *ctx, const char *file, int fallback, size_t *len)
{
    (void)ctx;
    if (fallback) return NULL;
    if (!strcmp(file, "battle")) {
        *len = sizeof locale - 1;
        return locale;
    }
    if (!strcmp(file, "pokemon")) {
        *len = sizeof pokemon_locale - 1;
        return pokemon_locale;
    }
    return NULL;
}

static int check(int mode, int wave, int player, int trainer, const char *expected)
{
    pr_battle bt = {0};
    pr_pokemon p = {0};
    bt.game_mode = mode;
    bt.wave_index = wave;
    p.species = SPECIES_ID_ETERNATUS;
    p.is_player = player;
    p.has_trainer = trainer;
    pr_msg msg = pr_msg1("battle:test", pr_arg_pokemon(&bt, "pokemonName", &p));
    const pr_text_source src = {get_locale, NULL, "en"};
    char actual[128];
    size_t n = pr_msg_format(&msg, &src, actual, sizeof actual);
    if (n != strlen(expected) || strcmp(actual, expected)) {
        fprintf(stderr, "mode%d wave%d player%d trainer%d: expected [%s], got [%s]\n",
                mode, wave, player, trainer, expected, actual);
        return 1;
    }
    return 0;
}

int main(void)
{
    int failed = 0;
    failed += check(GAME_MODES_CLASSIC, 200, 0, 0, "Foe Eternatus");
    failed += check(GAME_MODES_CHALLENGE, 200, 0, 0, "Foe Eternatus");
    failed += check(GAME_MODES_CLASSIC, 199, 0, 0, "Wild Eternatus");
    failed += check(GAME_MODES_CHALLENGE, 199, 0, 0, "Wild Eternatus");
    failed += check(GAME_MODES_ENDLESS, 200, 0, 0, "Wild Eternatus");
    failed += check(GAME_MODES_CHALLENGE, 199, 0, 1, "Foe Eternatus");
    failed += check(GAME_MODES_CHALLENGE, 200, 1, 0, "Eternatus");
    printf("Final-boss name affixes: 7 cases, %d mismatches\n", failed);
    return failed != 0;
}
