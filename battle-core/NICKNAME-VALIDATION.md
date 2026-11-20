# Nicknames in battle messages

Base: `ef47c60`. Source: `8555c08c823b856cbec4eb99ca84ea52a955836d`. Locales: `270ed0a2938b2c3e2e34e7de940e84aecadf548b`. Node 22.23.5, GCC 13.3.

Upstream's `Pokemon.nickname` (set from the party screen's Rename, or from the starter select's Rename) changes `getNameToRender()`, which
`getPokemonNameWithAffix()` and most messages use. battle-core had no nickname, so every message named the species. This change gives
`pr_pokemon` a `nickname` (plain UTF-8, at most 20 characters, `""` = none) and makes the messages that name a Pokemon through upstream's
`getNameToRender()` / `getPokemonNameWithAffix()` show it. Setting it is the app's and game-core's job; battle-core only reads it.

## What a message shows

- `pr_arg_pokemon` (getPokemonNameWithAffix) and `pr_arg_pokemon_name` (getNameToRender): the nickname when there is one, else the species
  name as before. The Wild/Foe affix of an enemy wraps the nickname like any name; a nickname replaces a fusion's blended name.
- A disguise (Illusion) hides the nickname, as `getNameToRender` checks the illusion first. The disguise's own nickname is not modelled
  (the header always said presentation metadata stays with the application), so a disguised Pokemon shows the disguise's species name.
- `pr_arg_pokemon_plain_name` (new, `pokemon.name`): the species name, never the nickname. Upstream builds a few messages from `.name`, not
  `getNameToRender()`, and they keep showing the species of a nicknamed Pokemon: the form-change messages (`battlePokemonForm:megaChange`,
  `gigantamaxChange`, `eternamaxChange`: `pokemonName`), `stealEatBerry` (user and target), Frisk (`opponentName`), `postSummonAllyHeal`
  (`pokemonName`), `postSummonTransform` (`targetName`, already so), the Revival Blessing phase's `revivalBlessing`, and game-core's
  `evolutionDone` (`evolvedPokemonName`, switched in the game-core change). Found by reading every `pr_arg_pokemon*` call against the
  pinned source's `i18next.t` argument for the same key.
- A message holds its nickname as an argument `s` pointing at a copy in a ring of 64 (`events.c`), made when the message is made, so a
  rename, a release or a party reorder afterwards does not change a message already made. A message can only lose its nickname if 64 other
  distinct nicknames were made after it; a batch of events names a dozen Pokemon at most.

## Not done

- **Wish, Healing Wish and Lunar Dance** store the name as `pokemon_name` / `fusion_name` words (and save them), so they still name the
  species. Upstream's `WishTag` stores `getPokemonNameWithAffix`, which has the nickname. Needs a stored string in two saved structs.
- The disguise's nickname (above).
- Messages game-core and the app build from a species rather than a Pokemon (a catch, the party/summary header, the battle info box) are
  the other two changes' business.

## Checks

`battle-core/tools/nickname-diff.sh PINNED_UPSTREAM [N [SEED]]` copies `tests/nickname-message.producer.test.ts` into the upstream checkout,
which builds random Pokemon in a real battle (species and forms of every species, fusions, enemy/trainer/final-boss, a disguise, a
transformed Pokemon), gives them a nickname the way the form stores it (`btoa(unescape(encodeURIComponent(text)))`: none, `""`, edge
cases such as accents, inner and outer spaces, 20 characters, emoji, `{{pokemonName}}`, `$t(x)`, and random text) and records the pinned
`getNameToRender()`, `getPokemonNameWithAffix()` and `pokemon.name`. `tests/nickname-message-test.c` makes the same Pokemon in C, formats
`pr_arg_pokemon_name`, `pr_arg_pokemon` and (no disguise) `pr_arg_pokemon_plain_name`, and compares the text byte for byte; it also
checks that a message does not change when its Pokemon does.

| Seed | Cases | With a nickname | Species-name checks | Failures |
| --- | --- | --- | --- | --- |
| 777 | 8,192 | 6,123 | 6,144 | 0 |
| 2034 | 8,192 | 6,308 | 6,144 | 0 |
| 4242 | 8,192 | 6,150 | 6,144 | 0 |
| 99 | 8,192 | 6,113 | 6,144 | 0 |

`tools/nickname-guards.py battle-core OUT LOCALES CAPTURE` plants ten faults (nickname ignored, dropped under the affix, fusion blend over
the nickname, nickname in `pokemon.name`, nickname under a disguise, name-alone without nickname, pointer into the Pokemon instead of a copy,
truncated copy, ring of eight, no sharing) and every one is caught; nine malformed captures (truncated, wrong version, wrong count, extra
line, bad hex, bad species, bad wave, duplicate index, over-long nickname) are all refused with exit 2.

`tests/nickname-test.c` (`make -C battle-core build/nickname-test && battle-core/build/nickname-test`; needs no upstream, it carries its own
five-string locale and runs in CI): the affix, Foe, final boss, fusion, disguise, blank and padded nicknames, the 20 four-byte
characters, no Pokemon / no battle, a message after a rename or a wipe of its Pokemon, shared copies, and the ring over 300 nicknames.

No existing check was changed. Re-run for regressions with this change: `fusion-name-diff.sh` modes `name`, `message`, `heal`, `wish`
(8,192 cases each, 0 failures) and `turntest` (2,000 cases, seed 777: covered 2000, pass 2000, texts identical 2000).
`sizeof(pr_pokemon)` 3,536 -> 3,624 bytes.
