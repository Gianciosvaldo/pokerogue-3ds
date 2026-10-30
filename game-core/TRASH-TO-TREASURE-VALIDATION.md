# Trash to Treasure

Reference: source `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node22.

The old dispatcher required three inventory entries, consumed the first three
and gave a Golden Poke Ball for investigating. Digging did nothing. Replace both
placeholder choices with the actual encounter:

- Investigate starts Gigantamax Garbodor, with an extra boss segment, the source
  level scaling, configured moves and held items. Toxic targets the player's
  lead, then Stockpile targets Garbodor. Fleeing and capture stay disabled. A won
  fight gives Leftovers plus Rogue/Ultra/Great guaranteed reward tiers, filling
  any remaining normal reward slots. Generic victory transport now carries the
  tiers and clears them for the next encounter.
- Dig grants Leftovers and Shell Bell to the first party member whose first
  matching stock is below its cap. Full parties still receive the source reward
  messages. Black Sludge persists, applies the 2.5x shop-price multiplier and the
  encounter leaves through its heal-only shop. Existing items are not consumed.
- Retain the `costMultiplier` dialogue token and Garbodor reveal page. The UI test
  now includes this encounter's pinned locale rather than testing its sprite
  against missing dialogue data.

The boss item configuration rolls on the wave offset used by native `onInit`.
Both awaited presentation steps restore the option offset before the actual
constructor: constructor and subsequent threshold rolls use the ambient RNG.
Configured duplicate item stocks follow `PersistentModifier.add` order/caps;
zero stacks disappear on the modifier update. The port retains the constructor's
variant/luck metadata when the config subsequently forces shininess off.

## Verification

192 native API cases cover both options, three wave contexts, eight seeds, two
party orders and four existing-stock states: empty, first member full, everybody
full and partial stock. Twenty-four cases start with no inventory at all. Some
source duplicate berries exceed their cap, so uncapped merging is distinguishable.
There are 23,004 comparisons with no mismatches. Observe source requirements,
rolled config, ordered inventory/owners, boss identity/IVs/stats/HP/moves/PP,
ambient RNG, exact digging messages, start-effect requests and reward settings.
The game-core replay also runs the actual victory-request transport.

Native battle RNG is captured before GameManager and restored after natural
startup. Actual `onInit`, both option methods and `initBattleWithEnemyConfig`
execute. Text/audio/intro transition playback and final phase delivery are
presentation boundaries; callbacks record their real arguments. Original source,
framework and helper files stay clean. Wave100/180 are controlled API boundary
inputs, not a claim that boss waves naturally select the event.

Fifteen compiled faults, twelve malformed captures and five legal forged
observations are rejected. Fresh native JSON/C records reproduce byte for byte.
Scoped ASan/UBSan/LSan covers the event, dispatcher, encounter-item and reward
request owners; supporting archives are unsanitized. The real mystery UI sanitizer
test passes all 423 checks, including source tooltip interpolation and both reveal
pages. An additional played C flow executes Toxic/Stockpile, battles through
victory (handling replacements), obtains the guaranteed reward, advances to wave124
without stale reward tiers, and separately checks the digging heal shop. That
functional flow is not a whole-battle native differential.

```sh
python3 game-core/tools/check-trash-proof.py
bash game-core/tools/reproduce-trash.sh PINNED_UPSTREAM OUTPUT_DIR
TRASH_SANITIZE=1 python3 game-core/tools/check-trash-proof.py
make -C game/tests me_screen_test
# Run game/tests/build/me_screen_test outside ptrace with leak checks enabled.
```

Normal core/app/custom/save/football and actual ARM/stack checks remain required;
only dedicated Task63 is excluded. No Windows builder is produced. This event
follow-up does not establish parity for the other reported mystery encounters,
shiny playback or the remaining battle-core/Endless inventory.
