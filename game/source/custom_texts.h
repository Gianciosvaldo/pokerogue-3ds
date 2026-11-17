/* The texts of lorenzo's own mystery encounters (Bizarre runs). The locale pack holds upstream's dialogue files only, so these are compiled in: English
 * for every language, in upstream's own schema (intro, title, description, query, option.N.label / tooltip / selected, then the encounter's messages), under
 * the file name pr_text_ns_file gives the namespace ("mysteryEncounters/standArrow" -> "mystery-encounters/stand-arrow-dialogue"). battle_events.c's text
 * source answers for these files before it reads the pack, so the encounter screens and game-core's messages use them like any other dialogue.
 *
 * Header-only on purpose: battle_events.c is linked by many test programs, and a header costs none of them a new source file. A "$" separates pages of one
 * text on the encounter screen (the messages game-core says have one sentence each); "(+)" and "(-)" colour the tooltip lines like upstream's. */
#ifndef CUSTOM_TEXTS_H
#define CUSTOM_TEXTS_H

#include <stddef.h>
#include <string.h>

/* The Stand Arrow (game-core gc_arrow.c). Tokens: selectedPokemon (the pierced one, from the party screen), awakenPercent and losePercent (the odds, from
 * the core's constants), pokemonName, stat1, stat2 (the result messages). */
#define CUSTOM_STAND_ARROW_JSON \
  "{" \
  "\"intro\":\"A strange arrowhead lies in the dust, glowing faintly.\"," \
  "\"title\":\"The Stand Arrow\"," \
  "\"description\":\"It is said to awaken a hidden power in whatever it pierces, and to destroy what cannot bear it.\"," \
  "\"query\":\"What will you do?\"," \
  "\"option\":{" \
  "\"1\":{\"label\":\"Pierce your Pokemon\"," \
  "\"tooltip\":\"(+) {{awakenPercent}}% to awaken a Stand and raise two IVs (-) {{losePercent}}% to lose the Pokemon for good\"," \
  "\"disabledTooltip\":\"You need a second Pokemon that can fight on.\"," \
  "\"selected\":\"The Arrow pierces {{selectedPokemon}}!\"}," \
  "\"2\":{\"label\":\"Pierce a wild Pokemon\"," \
  "\"tooltip\":\"(-) A hard boss fight (+) Ultra and Great rewards\"," \
  "\"selected\":\"The Arrow flies into the grass and something howls!\"}," \
  "\"3\":{\"label\":\"Leave it\"," \
  "\"tooltip\":\"(-) Nothing happens\"," \
  "\"selected\":\"You leave the Arrow where it lies.\"}}," \
  "\"result\":{" \
  "\"awakens\":\"{{pokemonName}} awakens a Stand!\"," \
  "\"stats\":\"Its {{stat1}} and {{stat2}} rose!\"," \
  "\"statsOne\":\"Its {{stat1}} rose!\"," \
  "\"statsNone\":\"But there is nothing left to grow.\"," \
  "\"nothing\":\"The Arrow glances off {{pokemonName}}. Nothing happens.\"," \
  "\"rejects\":\"{{pokemonName}} cannot bear the Arrow's power!\"," \
  "\"gone\":\"{{pokemonName}} is gone for good.\"," \
  "\"wild\":\"The wild Pokemon rages with the Arrow's power!\"" \
  "}}"

/* The dialogue file of a custom encounter by the name the text source asks for, or NULL (an upstream file: the locale pack answers). */
static inline const char* custom_text_file(const char* file, size_t* len) {
  static const struct {
    const char* file;
    const char* json;
  } FILES[] = {
      {"mystery-encounters/stand-arrow-dialogue", CUSTOM_STAND_ARROW_JSON},
  };
  for (size_t i = 0; i < sizeof FILES / sizeof FILES[0]; i++)
    if (!strcmp(file, FILES[i].file)) {
      *len = strlen(FILES[i].json);
      return FILES[i].json;
    }
  return NULL;
}

#endif
