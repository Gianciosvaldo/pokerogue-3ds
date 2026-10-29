# Arena storage for all public tag keys

Pinned source: `8555c08c823b856cbec4eb99ca84ea52a955836d`; locales:
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`; Node 22.

`Arena.addTag` consults `getTagOnSide` before constructing a new tag. For each
type, a BOTH tag excludes further side-specific copies; a PLAYER and ENEMY tag
can coexist. `getArenaTag` constructs 28 non-NONE types: eight force BOTH and
twenty retain the requested side. Thus the public API admits at most **48
distinct keys**. The native producer queries actual constructors and asserts
these counts, then creates all 48 through actual `Arena.addTag` calls. The old
24-entry C limit refused valid public API states.

Storage now holds all 48 keys. The overflow guard remains for input beyond the
public-API bound, with a separate C guard for an imported duplicate-tag array. Court Change retains only the constructor arguments and layer
count it actually consumes, avoiding large copies of unrelated pending-heal
payloads. Bulk side removal keeps ordered indices until callbacks finish, then
compacts the live objects forward. The original full list remains visible during
callbacks, matching native `leftoverTags` references.

## Evidence

The producer captures native battle RNG before GameManager construction and
restores it after battle startup. All **32 cases** combine four type orders,
two side orders and four subsequent actions: no-op, the actual registered Court
Change attribute, player-side removal and full removal. Each uses 54 public
additions: 48 distinct keys and six native hazard overlaps, yielding three-layer
Spikes and two-layer Toxic Spikes on both sides. Constructor durations and
identities are retained, not reconstructed as expected formulas.

The strict replay performs **77,264 checks, zero mismatches**: add results,
ordered tags before and after the action, projected actor state, battler tags,
both RNG streams and exact phase queues including rendered text, markers,
displayed abilities and source identities. Restoring the old limit produces
32 mismatching cases. Eleven compiled faults, eleven malformed captures and four
forged observations are rejected. Fresh native records and nine pinned English
locale namespaces reproduce byte-for-byte.

The legal-command save transport test uses a literal projection of native
constructor records, checked against the capture by the proof tool. It replaces
the fixture's source IDs with the scaffold's actual party IDs. Its **1,016
checks** retain ordered tag fields at counts 0, 1, 24, 25 and 48, with zero
dropped fields. The named, variable-count save schema is unchanged; previous
counts remain admitted. This transport test does not claim a naturally reached
48-tag command screen or an old-binary roundtrip. Existing arena cleanup's 78
save/encounter checks remain required.

Scoped ASan/UBSan with leak detection passes on the changed arena and move-effect
units plus the native replay; supporting archive units remain unsanitized.
Normal core/app/custom/save/football tests and the actual ARM build remain merge
gates. The retained Tailwind source proof remains enabled with all its cases
and failure controls, and passes locally. The retained 8,192-case standard
bulk-removal replay also passes exactly.

```sh
python3 battle-core/tools/check-arena-capacity-proof.py
ARENA_CAPACITY_SANITIZE=1 python3 battle-core/tools/check-arena-capacity-proof.py --replay-only
battle-core/tools/reproduce-arena-capacity.sh /path/to/pinned/upstream /tmp/arena-capacity
make -C game-core arena-capacity-host-test arena-cleanup-host-test
```

The maximum is a public add/remove API bound, not a proof that all 48 effects
coexist during an ordinary legal turn. The basis uses six unfused level-30
Rattata with no abilities. It observes queued phases without executing them.
Arbitrary imported duplicate tags, payload mutations, whole doubles turns,
ability recovery combinations, suppressed/fused identities, other languages,
wide levels and hardware acceptance remain separate. Pending-heal payload
behavior retains its own proof. No coverage flag is cleared to hide overflow.
