# Complete turn-order queries

Reference: upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, assets
`87426a79611f9d212c4dc8af58e2834a05b93725`.

## Source behavior and representation

`MovePhasePriorityQueue.pop` appends the acting Pokemon to `lastTurnOrder`.
`LastMoveDoublePowerAttr.apply` (Fusion Flare/Bolt) scans
`getLastTurnOrder().slice(0, -1).reverse()`, reading each Pokemon's **current**
`getLastXMoves(1)`. FAIL continues; every other result stops, and only SUCCESS
with the partner move doubles power. These are the only enabled whole-order
consumers found in the pinned source search. Repeated occurrences of one actor
have the same predicate during this pure query, so the latest occurrence of each
identity suffices **after excluding the final occurrence**.

The [actor-reference follow-up](TURN-ACTOR-VALIDATION.md) preserves actual object
identity when numeric IDs collide, change, or no longer resolve through a party.
The legacy first-eight transport remains available. A separate summary stores
latest distinct identities, and their order immediately before the last append,
within the two six-member parties. It reads live histories at query time and
resolves identities across both parties, including switched-out/fainted members.
The summary continues updating after the transport's uint8 count saturates.
Turn/encounter clear owners clear both representations. No save format changes.

Raw inputs through eight entries remain authoritative. A raw nine-entry input
still supplies every entry except the excluded last occurrence. Longer raw
inputs without a summary have an unknown suffix. A subsequent known non-FAIL
actor can answer before that gap; otherwise the caller reports
`PR_FEAT_TURN_ORDER`. A thirteenth distinct identity likewise leaves an explicit
gap. Empty history mirrors a source exception as unsupported. These cases are
not silently converted to negative answers.

## Native proof and actual commands

`turn-order.producer.test.ts` observes the actual queue and Fusion attributes:
216 cases, five stages and both consumers, or **2,160 caller/RNG observations**.
Lengths are 0, 1, 7, 8, 9, 19, 255, 256 and 65,536, with repeated identities,
excluded-final cases, twelve actors, unsigned IDs, bench identities and blockers.
Stages mutate current histories, pop a real MovePhase, clear through the actual
queue owner and pop again. Complete order arrays and history edits are explicit
API inputs; a 65,536-entry array is **not** evidence of a legal turn that long.
The builder compresses supplied arrays losslessly and verifies reconstruction;
expected powers come from native calls.

`turn-order-command.producer.test.ts` separately reaches **nineteen MovePhases**
with four level-1000 Blissey, Dancer, Fiery Dance and three Instruct commands.
Natural speed order determines execution. Owned movesets retain real PP usage;
no midturn HP, PP, history or phase repairs are made. Both producers capture native
battle RNG before GameManager construction and restore it after battle startup.

The C engine executes the same nineteen-action command turn. A link observer
runs after the real BerryPhase and checks order, HP, stages, finite PP, every
retained move record, both RNG streams and actual Fusion queries: **102 checks**,
zero mismatches/unsupported flags, trace 150 and events 93. It also checks normal
next-turn clearing. The old eight-entry representation fails 888 observations
in the API matrix. With only the alias prerequisite fixed, the original engine
matches the command's physical state/RNG but flags both Fusion queries as
unsupported.

That turn exposed a separate prerequisite: the original sixteen-slot shared
history-target alias table filled before completion. Aliases now recycle entries
outside all observable selectors, preserving the eight-record tail, four
last-real selectors and Lock-On/Mind Reader selector for each party member.
The bound is `12 * (8 + 4 + 1) = 156`. Guards exercise every live selector,
recycling and summon epochs; this raw storage test does not assert that twelve
Pokemon can simultaneously occupy the field. Other phase/target/event capacities
still report overflow and are not declared universally complete.

The raw/legacy/reset/identity/alias guards add **246 checks**. The game-core test
uses ordinary player commands and enemy Splash moves to execute eleven
MovePhases, checks next-command clearing and save refusal after a played turn,
then escapes normally and saves/loads at the next encounter. It is controller
consistency evidence, not a paired upstream full-run comparison.

## Reproduction and retained checks

```sh
# Node 22, dependencies installed in the clean pinned checkout:
bash battle-core/tools/reproduce-turn-order.sh /path/to/upstream /path/to/output
python3 battle-core/tools/check-turn-order-proof.py
```

The gzip captures retain native observations. Reproduction checks raw captures
and generated includes byte for byte. Strict builders validate schemas, coverage,
records, RNG and duplicate keys. The checker also compiles deliberate production
faults and verifies that tests reject them; compilation failures do not count.
It rejects 12 compiled faults, 14 malformed captures and three well-formed forged
power/HP/RNG observations.
The standard workflow retains this checker and the normal game-core test target.
Scoped ASan/UBSan/LeakSanitizer runs instrument history, engine, move attributes
and phases, plus the game-core battle owner. Supporting archives use ordinary
optimized builds; this is not full-program sanitizer coverage.

This closes the eight-entry turn-order consumer gap. Full enabled-path inventory,
remaining identity/fusion interactions, wide-level acceptance and hardware/run
acceptance remain separate items in the [completion checklist](COMPLETION-CHECKLIST.md).
