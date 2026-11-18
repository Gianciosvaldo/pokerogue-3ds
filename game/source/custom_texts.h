/* The texts of lorenzo's own mystery encounters (Bizarre runs). The locale pack holds upstream's dialogue files only, so these are compiled in: English
 * for every language, in upstream's own schema (intro, title, description, query, option.N.label / tooltip / selected, then the encounter's messages), under
 * the file name pr_text_ns_file gives the namespace ("mysteryEncounters/darby" -> "mystery-encounters/darby-dialogue"). battle_events.c's text
 * source answers for these files before it reads the pack, so the encounter screens and game-core's messages use them like any other dialogue.
 *
 * Header-only on purpose: battle_events.c is linked by many test programs, and a header costs none of them a new source file. A "$" separates pages of one
 * text on the encounter screen (the messages game-core says have one sentence each); "(+)" and "(-)" colour the tooltip lines like upstream's. */
#ifndef CUSTOM_TEXTS_H
#define CUSTOM_TEXTS_H

#include <stddef.h>
#include <string.h>

/* D'Arby the Gambler (game-core gc_darby.c). lorenzo's own lines: line.1, line.2 and the two detector lines; the rest is the wording proposed to him
 * 2026-11-17 for his review. Tokens: soulsLeft, cards (the app joins the six names: a message holds five tokens) and cardLevel, vitaminsLeft, pokemonName, multiplier ("2x"), source (an ability or a
 * move). */
#define CUSTOM_DARBY_JSON \
  "{" \
  "\"intro\":\"A well-dressed man sits at a small table, shuffling a deck of cards. He's been waiting for you.\"," \
  "\"title\":\"D'Arby the Gambler\"," \
  "\"description\":\"\\\"One hand of Soul Poker, trainer. Your Pokemon against my cards. Win, and I pay handsomely. Lose, and the souls you wagered are mine.\\\"\"," \
  "\"query\":\"Will you play?\"," \
  "\"option\":{" \
  "\"1\":{\"label\":\"Wager one soul\"," \
  "\"tooltip\":\"(-) Lose 1 Pokemon if you lose (+) 2 Rarer Candy and 2 vitamins of your choice\"}," \
  "\"2\":{\"label\":\"Go all in\"," \
  "\"tooltip\":\"(-) Lose 3 Pokemon if you lose (+) A Big Nugget, a Master-tier and a Rogue-tier reward screen\"}," \
  "\"3\":{\"label\":\"Walk away\"," \
  "\"tooltip\":\"(-) Nothing happens\"," \
  "\"selected\":\"You leave D'Arby to his cards.\"}}," \
  "\"soul\":{\"query\":\"Choose a soul to wager. ({{soulsLeft}} left)\"}," \
  "\"card\":{\"description\":\"D'Arby lays six cards face up: {{cards}}, all at Lv. {{cardLevel}}. One of them is his. Ties are broken by base Speed.\"," \
  "\"query\":\"Choose the Pokemon you will play.\"}," \
  "\"accuse\":{\"query\":\"What will you do?\"," \
  "\"option\":{" \
  "\"1\":{\"label\":\"Accept the Deal\",\"tooltip\":\"(-) D'Arby seals the souls you wagered in his chips (+) Beat him when he comes back to win them back\"}," \
  "\"2\":{\"label\":\"You Cheated!\",\"tooltip\":\"(+) If he cheated, you win the full reward (-) If he didn't, the souls are lost for good\"}}}," \
  "\"vitamin\":{\"query\":\"Choose a vitamin to give. ({{vitaminsLeft}} left)\"}," \
  "\"line\":{\"1\":\"\\\"I've already made my move. Show me yours.\\\"\",\"2\":\"\\\"Fate has already decided this hand.\\\"\"}," \
  "\"reveal\":{" \
  "\"player\":\"You play {{pokemonName}}: {{multiplier}} against his card.\"," \
  "\"darby\":\"D'Arby turns over {{pokemonName}}: {{multiplier}} against yours.\"," \
  "\"speedTie\":\"The odds are even! Base Speed decides.\"," \
  "\"coin\":\"Even their Speed is tied! A coin decides...\"}," \
  "\"detector\":{" \
  "\"caught\":\"Your {{pokemonName}}'s {{source}} caught D'Arby palming a different card!\"," \
  "\"clean\":\"Your {{pokemonName}} watched his hands closely\xe2\x80\x94he didn't swap a thing.\"}," \
  "\"result\":{" \
  "\"win\":\"You win the hand!\"," \
  "\"handLost\":\"D'Arby wins the hand.\"," \
  "\"caught\":\"The swapped card slips from D'Arby's sleeve!\"," \
  "\"falseCall\":\"D'Arby smiles. \\\"An honest hand, trainer. Those souls are mine for good.\\\"\"," \
  "\"soulTaken\":\"{{pokemonName}}'s soul is lost to D'Arby for good.\"," \
  "\"soulSealed\":\"{{pokemonName}}'s soul is sealed in one of D'Arby's poker chips.\"}," \
  "\"rematch\":{" \
  "\"query\":\"D'Arby's chip holds {{pokemonName}}. Take it back?\"," \
  "\"option\":{" \
  "\"1\":{\"label\":\"Take it back\",\"tooltip\":\"(+) {{pokemonName}} rejoins your party at the level it fought at\"}," \
  "\"2\":{\"label\":\"Leave it\",\"tooltip\":\"(-) {{pokemonName}} is gone for the rest of the run\"}}," \
  "\"back\":\"{{pokemonName}} is back in your party!\"," \
  "\"gone\":\"{{pokemonName}} stays with D'Arby.\"}," \
  "\"outro\":\"D'Arby gathers his cards and is gone.\"" \
  "}"

/* The dialogue file of a custom encounter by the name the text source asks for, or NULL (an upstream file: the locale pack answers). */
static inline const char* custom_text_file(const char* file, size_t* len) {
  static const struct {
    const char* file;
    const char* json;
  } FILES[] = {
      {"mystery-encounters/darby-dialogue", CUSTOM_DARBY_JSON},
  };
  for (size_t i = 0; i < sizeof FILES / sizeof FILES[0]; i++)
    if (!strcmp(file, FILES[i].file)) {
      *len = strlen(FILES[i].json);
      return FILES[i].json;
    }
  return NULL;
}

#endif
