/*
 * i18n: upstream's locale strings (the pokerogue-locales JSON files in data.pak) looked up the
 * way upstream's i18next setup does (src/i18n.ts):
 *   - keys are "namespace:path.to.key"; the namespace is the file name in camelCase
 *     ("commandUiHandler" -> locales/<lang>/command-ui-handler.json), with upstream's
 *     namespaceMap exceptions ("titles" -> trainer-titles.json, "biome" -> biomes.json...);
 *     no namespace means "menu" (upstream's defaultNS);
 *   - {{name}} interpolation (no HTML escaping), $t(ns:key) nesting, plurals (key_one /
 *     key_other with a "count" argument, English rules), context (key_male / key_female with
 *     a "context" argument);
 *   - a missing key falls back to English, then to the key itself (as i18next shows it).
 * Files load from data.pak on first use and stay in memory (jansson objects).
 */
#ifndef GM_I18N_H
#define GM_I18N_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Language code as the locales folders name them ("en", "it", "es-ES", "ja"...). */
void i18n_set_language(const char* lang);
const char* i18n_language(void);

/* Arguments: pairs of name, value strings, then NULL. Values are copied during the call.
 *   i18n_t(buf, sizeof buf, "battle:singleWildAppeared", "pokemonName", "Garchomp", NULL);
 * Writes the result into out (always NUL-terminated) and returns out. */
const char* i18n_t(char* out, int n, const char* key, ...);
/* Returns a pointer to the raw string for a key (no interpolation), or NULL when missing in
 * every language. Valid until i18n_set_language. */
const char* i18n_raw(const char* key);
bool i18n_exists(const char* key);

/* Convenience for the common names, into a static rotating buffer (8 slots, 96 bytes each):
 * fine for building a message, not for keeping. */
const char* i18n_species_name(int species_id);   /* pokemon:<camelCase SpeciesId> */
const char* i18n_move_name(int move_id);         /* move:<camelCase MoveId>.name */
const char* i18n_ability_name(int ability_id);   /* ability:<key>.name */
const char* i18n_type_name(int type);            /* pokemonInfo:type.<camelCase PokemonType> */
const char* i18n_stat_name(int stat);            /* pokemonInfo:stat.<camelCase Stat> */

/* Drops every loaded file (e.g. before changing language). */
void i18n_unload(void);

#ifdef __cplusplus
}
#endif
#endif
