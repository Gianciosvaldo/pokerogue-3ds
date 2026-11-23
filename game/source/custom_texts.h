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
  "\"description\":\"\\\"One hand of Soul Poker, trainer. Your Pokemon against my cards. Win, and I pay handsomely. Lose, and the souls you wagered are mine... until you beat me at my own game.\\\"\"," \
  "\"query\":\"Will you play?\"," \
  "\"option\":{" \
  "\"1\":{\"label\":\"Wager one soul\"," \
  "\"tooltip\":\"(-) If you lose, 1 Pokemon is sealed until you beat him later (+) 2 Rarer Candy and 2 vitamins of your choice\"}," \
  "\"2\":{\"label\":\"Go all in\"," \
  "\"tooltip\":\"(-) If you lose, 3 Pokemon are sealed until you beat him later (+) A Big Nugget, a Master-tier and a Rogue-tier reward screen\"}," \
  "\"3\":{\"label\":\"Walk away\"," \
  "\"tooltip\":\"(-) Nothing happens\"," \
  "\"selected\":\"You leave D'Arby to his cards.\"}}," \
  "\"soul\":{\"query\":\"Choose a soul to wager. ({{soulsLeft}} left)\"}," \
  "\"card\":{\"description\":\"D'Arby lays six cards face up: {{cards}}, all at Lv. {{cardLevel}}. One of them is his. Ties are broken by base Speed.\"," \
  "\"query\":\"Choose the Pokemon you will play.\"}," \
  "\"accuse\":{\"query\":\"What will you do?\"," \
  "\"option\":{" \
  "\"1\":{\"label\":\"Accept the Deal\",\"tooltip\":\"(-) D'Arby seals the souls you wagered in his chips (+) He will challenge you again later: beat him to win them back\"}," \
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
  "\"soulSealed\":\"{{pokemonName}}'s soul is sealed in one of D'Arby's poker chips. Beat him when he returns to win it back.\"}," \
  "\"rematch\":{" \
  "\"query\":\"D'Arby's chip holds {{pokemonName}}. Take it back?\"," \
  "\"option\":{" \
  "\"1\":{\"label\":\"Take it back\",\"tooltip\":\"(+) {{pokemonName}} rejoins your party at the level it fought at\"}," \
  "\"2\":{\"label\":\"Leave it\",\"tooltip\":\"(-) {{pokemonName}} is gone for the rest of the run\"}}," \
  "\"back\":\"{{pokemonName}} is back in your party!\"," \
  "\"gone\":\"{{pokemonName}} stays with D'Arby.\"}," \
  "\"outro\":\"D'Arby gathers his cards and is gone.\"" \
  "}"

/* Jobin's Gentleman's Bet (game-core gc_jobin.c). lorenzo's own: the three Speed King telegraph lines, his tissue line (caught.line) and the achievement titles; everything else is the
 * wording proposed to him 2026-11-23 for his review (generated from the table in the PR; each line is one text key under "mysteryEncounters/jobin:"). Tokens: yourBeetle and jobinBeetle
 * (the two beetles' names), jobinBeetle on the first screen, gripYou/gripJobin/suspicion on the round screen, youDelta/himDelta/suspDelta (the round's changes), pokemonName. */
#define CUSTOM_JOBIN_JSON \
  "{\"intro\":\"A man in a white suit crouches beside a hollow drum, a beetle in each hand. He's been waiting for you.\"," \
  "\"title\":\"Jobin's Gentleman's Bet\",\"description\":\"\\\"One round of beetle fighting, young man. Whoever loses their grip gets flipped. Win," \
  " and I'll pay up. Lose, and a bet is a bet.\\\" He has picked {{jobinBeetle}}.\",\"query\":\"Will you take the bet?\"," \
  "\"option\":{\"1\":{\"label\":\"Use your own beetle\",\"labelDisabled\":\"No beetle to use\",\"tooltip\":\"(-) If it loses," \
  " it faints and Jobin takes half your money (+) If it wins, it gets a Grip Claw, a King's Rock and 2 Silver Powder that stay with it\"," \
  "\"tooltipDisabled\":\"None of your Pokemon is a beetle Jobin can face\"},\"2\":{\"label\":\"Borrow a beetle\"," \
  "\"tooltip\":\"(-) If it loses, Jobin takes all your money (+) If it wins: a Relic Gold, a Charcoal, and you may keep the beetle holding a Silver Powder\"}" \
  ",\"3\":{\"label\":\"Walk away\",\"tooltip\":\"(-) Nothing happens\",\"selected\":\"You leave Jobin to his beetles.\"}" \
  "},\"start\":{\"jobin\":\"Jobin picks {{jobinBeetle}}.\",\"own\":\"You send out {{yourBeetle}}.\",\"rental\":\"Jobin tosses you a {{yourBeetle}" \
  "}. \\\"Don't scratch it.\\\"\"},\"round\":{\"query\":\"Choose your move.\",\"description\":\"Grip: you {{gripYou}}" \
  "%, Jobin {{gripJobin}}%. Suspicion: {{suspicion}}%.\",\"option\":{\"1\":{\"label\":\"Horn Lock\",\"tooltip\":\"Beats Flank," \
  " loses to Dig In\"},\"2\":{\"label\":\"Dig In\",\"tooltip\":\"Beats Horn Lock, loses to Flank\"},\"3\":{\"label\":\"Flank\"," \
  "\"tooltip\":\"Beats Dig In, loses to Horn Lock\"},\"4\":{\"label\":\"Stand Trick\",\"tooltip\":\"(+) Takes 35% of his Grip," \
  " whatever he does (-) Suspicion rises 35%, then 50%, then 70% when repeated\"}},\"result\":\"Grip: you {{youDelta}" \
  "}, Jobin {{himDelta}}. Suspicion: {{suspDelta}}.\",\"hornLock\":{\"hornLock\":\"{{yourBeetle}} and {{jobinBeetle}" \
  "} lock horns! Neither gives an inch.\",\"digIn\":\"{{jobinBeetle}} digs in and hooks {{yourBeetle}} back as it charges!\"," \
  "\"flank\":\"{{yourBeetle}} turns and meets the flank with a Horn Lock! {{jobinBeetle}} loses its grip!\"}" \
  ",\"digIn\":{\"hornLock\":\"{{yourBeetle}} digs in against the Horn Lock and counters!\",\"digIn\":\"Both beetles dig in and stare each other down. Jobin's eyes narrow.\"," \
  "\"flank\":\"{{jobinBeetle}} flanks the dug-in {{yourBeetle}} and pries it loose!\"},\"flank\":{\"hornLock\":\"{{jobinBeetle}" \
  "} meets {{yourBeetle}}'s flank with a Horn Lock! {{yourBeetle}} loses its grip!\",\"digIn\":\"{{yourBeetle}" \
  "} flanks the dug-in {{jobinBeetle}} and pries it loose!\",\"flank\":\"Both beetles flank at once and collide side to side!\"}" \
  ",\"standTrick\":\"Something unseen shoves {{jobinBeetle}} off balance. {{yourBeetle}} never moved.\",\"superheatedHorn\":{\"hornLock\":\"{{jobinBeetle}" \
  "}'s scorching horn overpowers {{yourBeetle}}'s Horn Lock!\",\"digIn\":\"{{yourBeetle}} digs in and the burning horn skids off. A heavy counter!\"," \
  "\"flank\":\"{{yourBeetle}} flanks straight into the white-hot horn!\",\"standTrick\":\"The heat is negated by something unseen," \
  " and {{jobinBeetle}} is knocked off balance.\"},\"thermalDash\":{\"hornLock\":\"{{yourBeetle}} meets the dash with a Horn Lock and stuffs it!\"," \
  "\"digIn\":\"{{jobinBeetle}} dashes through the dug-in {{yourBeetle}} like a flame!\",\"flank\":\"Both beetles flank," \
  " but the heated dash overpowers {{yourBeetle}}!\",\"standTrick\":\"The heat is negated by something unseen," \
  " and {{jobinBeetle}} is knocked off balance.\"},\"bubblingSap\":{\"hornLock\":\"The sap burns {{yourBeetle}" \
  "}'s legs as {{jobinBeetle}} hooks it back!\",\"digIn\":\"{{yourBeetle}} digs in, right into the burning sap!\"," \
  "\"flank\":\"{{yourBeetle}} leaps clear of the sap and flanks!\",\"standTrick\":\"The sap is neutralized by something unseen," \
  " and {{jobinBeetle}} is knocked off balance.\"}},\"bluff\":{\"reveal\":\"It was a bluff! {{jobinBeetle}} never heated up at all.\"}" \
  ",\"telegraph\":{\"superheatedHorn\":\"A faint heat shimmer rises from Jobin's beetle's horn...\",\"thermalDash\":\"Heat waves ripple behind Jobin's beetle as it coils...\"," \
  "\"bubblingSap\":\"The sap under your beetle's legs starts to smoke...\"},\"end\":{\"youFlipped\":\"{{yourBeetle}" \
  "} is flipped onto its back!\",\"jobinFlipped\":\"{{jobinBeetle}} is flipped onto its back!\",\"rematch\":\"Both beetles are flipped at once! A rematch: both Grips are back to 50%.\"}" \
  ",\"win\":{\"line\":\"Jobin grumbles. \\\"Well fought. A bet is a bet.\\\"\"},\"give\":{\"own\":\"Jobin hands {{pokemonName}" \
  "} a Grip Claw, a King's Rock and two Silver Powder. They won't come off.\",\"charcoal\":\"{{pokemonName}" \
  "} receives a Charcoal.\"},\"charcoal\":{\"query\":\"Choose a Pokemon to carry the Charcoal.\"},\"keep\":{\"loan\":{\"query\":\"Jobin lets you keep {{pokemonName}" \
  "}. Take it?\",\"option\":{\"1\":{\"label\":\"Keep it\",\"tooltip\":\"(+) {{pokemonName}} joins your party holding a Silver Powder\"}" \
  ",\"2\":{\"label\":\"Give it back\",\"tooltip\":\"(-) Jobin takes {{pokemonName}} home\"}}},\"bonus\":{\"query\":\"A shiny {{pokemonName}" \
  "} from his collection. Take it?\",\"option\":{\"1\":{\"label\":\"Take it\",\"tooltip\":\"(+) A shiny {{pokemonName}" \
  "} joins your party\"},\"2\":{\"label\":\"Leave it\",\"tooltip\":\"(-) Jobin keeps {{pokemonName}}\"}}},\"replace\":\"Choose the Pokemon it replaces.\"," \
  "\"farewell\":\"Goodbye, {{pokemonName}}!\",\"yes\":\"{{pokemonName}} joins your party!\",\"no\":\"You leave {{pokemonName}" \
  "} with Jobin.\"},\"bonus\":{\"line\":\"\\\"A clean fight, no tricks! Take one from my collection.\\\" Jobin holds out a shiny {{pokemonName}" \
  "}.\"},\"lose\":{\"own\":\"Jobin smirks. \\\"Bragging rights are mine.\\\" He takes half your money, and {{yourBeetle}" \
  "} faints.\",\"rental\":\"Jobin glares. \\\"You flipped one of my precious beetles! I'll take all of it.\\\"\"}" \
  ",\"caught\":{\"void\":\"The match is void! Jobin calls off the bet in disgust, and no money changes hands.\"," \
  "\"line\":\"\\\"You're sweating quite a lot, are you sure you don't need a tissue?\\\"\"},\"hud\":{\"grip\":\"Grip\"," \
  "\"suspicion\":\"Suspicion\"},\"outro\":{\"win\":\"Jobin walks off, already looking for his next bet.\",\"lose\":\"Jobin whistles and carries his beetles away.\"}" \
  "}"

/* The dialogue file of a custom encounter by the name the text source asks for, or NULL (an upstream file: the locale pack answers). */
static inline const char* custom_text_file(const char* file, size_t* len) {
  static const struct {
    const char* file;
    const char* json;
  } FILES[] = {
      {"mystery-encounters/darby-dialogue", CUSTOM_DARBY_JSON},
      {"mystery-encounters/jobin-dialogue", CUSTOM_JOBIN_JSON},
  };
  for (size_t i = 0; i < sizeof FILES / sizeof FILES[0]; i++)
    if (!strcmp(file, FILES[i].file)) {
      *len = strlen(FILES[i].json);
      return FILES[i].json;
    }
  return NULL;
}

#endif
