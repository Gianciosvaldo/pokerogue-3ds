# Turn-order actor identity

Source pin: `8555c08c823b856cbec4eb99ca84ea52a955836d`; locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`; assets
`87426a79611f9d212c4dc8af58e2834a05b93725`.

## Source counterexample

`MovePhasePriorityQueue.pop` stores the actual Pokemon object. Fusion Flare/Bolt's
`LastMoveDoublePowerAttr.apply` reads those objects' current histories directly.
It does not resolve them through `globalScene.getPokemonById`. The original port,
including the complete-history summary, reduced these references to numeric IDs.

The native Pokemon constructor calls `randSeedInt(4294967296)` without a uniqueness
check. The producer finds ID **957185811** at RNG draw indices **34337 and 52608**
from a fixed seed, then restores both recorded states and calls two actual
`addPlayerPokemon` constructors. Both produce that ID for separate objects.
The capture retains those RNG states; the C command test verifies the ID draw
from each. This is a controlled constructor witness, not an estimate of collision
frequency in played runs.

A separate four-command turn gives its two player Blissey the witnessed shared ID.
Fusion Bolt acts first, Fusion Flare second, then two enemy Splash commands.
The native target ends at **4645 HP**. Before the fix, C fails the HP comparison
while matching PP, histories and RNG and reporting no unsupported path. The
numeric-ID lookup had selected the currently acting player's pending move instead
of its partner's completed Fusion Bolt.

## Implementation and compatibility

The normal dynamic-pop owner now records the acting Pokemon reference. Latest
unique entries compare object identity, preserving different objects with equal
IDs and reading the same object after its ID changes or it leaves the party.
The existing two-party object budget and conservative missing-suffix flags stay.

The first-eight numeric transport remains available. The reference cache applies
at short lengths too, and validates a copy of that transport prefix so explicit
raw edits at an unchanged count remain authoritative. Legacy ID-only inputs
resolve only unambiguous scene objects; an ambiguous identity reports
`PR_FEAT_TURN_ORDER` unless a known newer terminal entry already answers the query.
Turn and encounter clearing removes references. Saves do not serialize this
transient cache, and no save schema changes.

This is scoped to turn-order object references. Other ID-based callbacks follow
their own source contracts and are not changed by this slice.

## Evidence

- **336 native API cases / 3360 consumer and RNG observations**: unique IDs,
  one colliding pair, twelve colliding objects; lengths 0, 1, 2, 8, 9, 19 and 256;
  repeated objects, current-history edits, subsequent ID changes, party removal
  and actual queue clearing. C builds the order through the real dynamic-pop
  owner. Array and party edits are explicit API inputs, not legal long-turn
  reachability claims.
- **70 checks** on the actual four-command C turn, including HP, stages, finite
  PP, retained move records, both RNG streams, constructor witness draws and
  normal turn clearing. All match the native capture after the fix.
- **22 raw/cache/ambiguity guards**, including an authoritative prefix edit at
  equal count and conservative ambiguous legacy IDs.
- **40 controller checks**: real doubles commands before and after a legal initial
  save/load with separate objects sharing the ID; matching HP, PP and RNG;
  normal next-command clearing and refusal to save after the played turn. This
  is controller consistency evidence, not a paired upstream whole-run proof.
- **8 compiled faults**, **13 malformed captures**, and **3 forged observations**
  (power, command HP, command RNG) are rejected. Compilation errors do not count
  as detected faults. The prior complete-turn-order checker retains all its
  cases and 12 faults; its field-only lookup injection targets the updated code.
- Scoped ASan/UBSan/LeakSanitizer runs instrument history, engine, move attributes
  and phases plus game-core's battle owner. Supporting archives use ordinary
  optimized builds; full-program instrumentation is not claimed.

Both new native captures and generated includes reproduced byte for byte with
Node 22. Each producer captures native `BattleScene.prototype.randBattleSeedInt`
before constructing GameManager and restores it after starting the battle.

```sh
bash battle-core/tools/reproduce-turn-actors.sh /path/to/pinned/upstream /path/to/output
python3 battle-core/tools/check-turn-actors-proof.py
python3 battle-core/tools/check-turn-order-proof.py
```

Normal core/app/custom/save/football and actual 3DS/stack checks remain required.
Only the dedicated Task63 check is skipped. Broader inventory, identity/fusion
interactions and wide-level acceptance remain in the
[completion checklist](COMPLETION-CHECKLIST.md).
