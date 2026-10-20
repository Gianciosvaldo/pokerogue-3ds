# Biome weather and terrain callbacks

Pin: `8555c08c823b856cbec4eb99ca84ea52a955836d`; locales:
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`. Native battle RNG is captured before
GameManager construction and restored after natural startup; gameplay RNG is
not mocked.

## Source behavior

PostBiomeChangeWeatherChangeAbAttr requires the **existing** weather to be
immutable and the desired weather to differ. Arena.canSetWeather checks only
inequality. The later trySetWeather can reject immutable-to-ordinary weather,
but activation still shows the ability and records its history. The rejection
queues the old weather animation and continuation message. Reusing the
PostSummonWeatherChangeAbAttr replaceability gate would incorrectly skip this
activation. Valid primal weather replacements remain permanent.

PostBiomeChangeTerrainChangeAbAttr checks terrain inequality and calls the
normal setter with animations enabled and the ability owner. The shared setter
sets duration and runs speed-ordered terrain callbacks, including Mimicry.
Simulation produces no field change, queues, history or RNG effects. Existing
PostSummon weather/terrain simulations previously required an engine even when
they only checked gates; that unsupported report is corrected here too.

The exported pr_new_biome_encounter_callbacks implements the gameplay prefix of
NewBiomeEncounterPhase.doEncounter: reset every player's battle/wave data,
then apply biome callbacks to on-field players in **party order**. Summon data
is retained. The caller owns the visual tween and subsequent encounter phases.

## Owning reachability

All five pinned stock GameMode configs leave hasRandomBiomes unset. Stock
new-biome cleanup recalls outgoing players before NewBiomeEncounterPhase.
A native guard executes actual doPostBattleCleanup, ReturnPhase and
ShowTrainerPhase for five modes and single/double outgoing fields: ten cases,
fifteen returns and ten subsequent biome prefixes. The alive Electric Surge
user retains its eligible ability but is off the field, so no callback applies.
This is verified rather than assuming the registered attributes fire normally.
No additional game-core activation is inserted.

Direct public phase calls with active players still have defined behavior.
The owning prefix fixture executes the actual protected doEncounter method
through its pending visual continuation. Audio and presentation are replaced;
the continuation is retained without running the later encounter controller.
This is a prefix comparison, not complete encounter acceptance.

## Bounded evidence

Both seeds (777 and 2034) match all 53,760 raw dispatcher/gain cases and all
13,440 actual owning-prefix cases. Comparisons include eight full actor frames,
weather/terrain types and remaining/maximum durations, regular/dynamic queues,
cached ability names, translated messages and both RNG streams. Prefix cases
also compare consumed berry history, Endure state, wave history/counters and
retained summon history, with competing setters to verify party order.

Raw cases exercise all eight enabled weather registrations and all five terrain
registrations, every weather/terrain type, field/reserve roles, simulation,
gain, primary/passive/duplicate abilities, fainting, suppression, switched
actors and nested Mimicry, with levels 1, 50, 256 and 1,000,000. Prefix cases
include recalled fields and resets of inactive reserves. The actual old core
fails 14,160 of the first raw matrix's 26,880 cases. The initial new callbacks
also exposed 3,600 existing simulated-gain unsupported reports, now fixed.

Twenty-one compiled production faults are caught and fifteen malformed captures
are rejected. Forty-two cross matrices, twenty-six further raw/turn/learn-form
matrices, entry/context checks and both raw/complete-turn Poison Puppeteer
matrices pass. Seven historical corpora retain every previously covered ID.
Normal game-core tests pass with identical saved/unsaved digests; full app,
custom and save sanitizer tests and the 3DS build pass.

No completion claim follows from these bounded cases. Held field extenders,
every fusion/Transform, challenge and later encounter-controller interaction
are outside these matrices.
Post-battle abilities, encounter integration, the complete source inventory and
wide-Endless acceptance remain open.

## Reproduction

```sh
. /workspace/shared/activate-parity.sh
sh battle-core/tools/biome-callback-diff.sh /path/to/pinned-upstream 777
sh battle-core/tools/biome-callback-diff.sh /path/to/pinned-upstream 2034
```

See [the completion checklist](COMPLETION-CHECKLIST.md).
