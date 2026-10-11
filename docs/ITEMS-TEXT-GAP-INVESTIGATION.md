# D14: retained ITEMS text failures (2026-10-11)

**Follow-up:** the user subsequently requested a runtime repair. The five missing announcements are now fixed and the original comparator rejects missing text; see [current repair and validation](../battle-core/RETAINED-FAILURES-REPAIR.md). The investigation below records the pre-repair behavior.

**Investigation complete; strict ITEMS text completeness remains FAILED.** All five
missing texts are production placeholders for Poltergeist's pre-move held-item
announcement. They are not missing locales or comparator formatting errors.
This docs-only task does not repair or waive them.

Port runtime audited: `8489efa3be16236f897b1e210c5cc1fc69e7af13` (#194).
PR base also includes the test-only #195 repair at
`f486ae30e2a6ea21142b24cdf15d0de27e56567f`; its production/ITEMS producer/comparator
are identical. No competing root-cause report or open implementation was found.
Upstream: `8555c08c823b856cbec4eb99ca84ea52a955836d`; locales:
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`; Node 22.23.3, Vitest 4.1.11,
GCC 14.2.0. Implementation and established test files stayed read-only.

## Reproduction and separate state/text results

```sh
source /workspace/shared/activate-parity.sh
make -j2 -C battle-core
cp battle-core/tests/turntest.producer.test.ts <upstream>/test/tests/zz-dump/
cd <upstream>
TURN_ITEMS=1 TURN_N=2000 TURN_SEED=<seed> TURN_OUT=<evidence>/items-<seed>.txt \
  node node_modules/vitest/vitest.mjs run test/tests/zz-dump/turntest.producer.test.ts --maxWorkers=1
cd <port>
TURNTEST_LOCALES=<upstream>/locales/en battle-core/build/turntest <evidence>/items-<seed>.txt
```

| Seed | Requested/completed | State covered/identical | State uncovered | Text identical / differ / missing | Producer / replay exit |
|---|---|---|---|---|---|
| 777 | 2000/2000 | 1928/1928 | 72 | 1927 / 0 / 1 | 0 / 0 |
| 9001 | 2000/2000 | 1935/1935 | 65 | 1934 / 0 / 1 | 0 / 0 |
| 4242 | 2000/2000 | 1932/1932 | 68 | 1930 / 0 / 2 | 0 / 0 |
| 31337 | 2000/2000 | 1946/1946 | 54 | 1945 / 0 / 1 | 0 / 0 |

All four: skipped records 0, parse mismatches 0, stat mismatches 0, state failures
0. The five missing texts are not text passes; exit 0 is state-only. The existing
uncovered domains remain unchanged, not exclusions added by this investigation.

## Every missing text and first mismatch

Text positions below are zero-based within the emitted TEXTS array. C emits
`<?>` at that position for a `PR_EV_TEXT` with `msg.key=NULL`, no arguments,
battler/target=-1. `args[0].s` is also NULL, so the old TODO-site summary prints
`(null)` rather than the producing callsite. Full arrays and queues are retained
in the evidence below.

| Seed / CASE | Position | Actual upstream text | Source/queue context |
|---|---|---|---|
| 777 / 1066 | 2 | `Galvantula is about to be attacked by its White Herb!` | After Galvantula uses Expanding Force and Wild Spheal uses Poltergeist; before Galvantula avoids the attack |
| 9001 / 264 | 1 | `Emboar is about to be attacked by its Lum Berry!` | After Wild Great Tusk uses Poltergeist; before Emboar avoids it and uses Constrict |
| 4242 / 1033 | 1 | `Wild Regirock is about to be attacked by its Leftovers!` | After Fletchling uses Poltergeist; before Wild Regirock faints |
| 4242 / 1598 | 2 | `Budew is about to be attacked by its Petaya Berry!` | After Budew uses Shadow Punch and Wild Metapod uses Poltergeist |
| 31337 / 1171 | 1 | `Thwackey is about to be attacked by its Shell Bell!` | After Wild Kingler uses Poltergeist; before Thwackey faints and Cradily is sent out |

In each first differing string the source has the announcement above and C has
`<?>`. State, RNG and queue order otherwise match in the unchanged comparison.
This includes both a miss and KO: the announcement occurs before hit resolution,
so missing it cannot be excused because the attack misses or ends the battle.

## Source-to-C causal chain

| Pinned source symbol / lines | Observation / C boundary |
|---|---|
| `src/data/moves/move.ts:12137`, POLTERGEIST factory | Includes `PreMoveMessageAttr(attackedByItemMessageFunc)` |
| `move.ts:1876`, `PreMoveMessageAttr.apply` | Executes the real message function; nonempty result queues `queueMessage(message, 500)` |
| `move.ts:8988`, `attackedByItemMessageFunc` | Null target/no transferable held items returns empty; else first `target.getHeldItems().filter(i => i.isTransferable)` entry's `type.name`, fallback `item` |
| `src/field/pokemon.ts:1197`, `getHeldItems` | Actual held-item class AND exact Pokemon ID, on the target's side, in modifier-list order |
| `src/messages.ts:13`, `getPokemonNameWithAffix` | Uses actual rendered name and wild/foe role; no numeric or ability interpolation in this message |
| `locales/en/move-trigger.json:18`, `attackedByItem` | `{{pokemonName}} is about to be attacked by its {{itemName}}!`; White Herb/Leftovers/Shell Bell names come from modifier-type locale, Lum/Petaya names from berry locale |
| `battle-core/src/phases.c:670`, `pre_move_message` | POLTERGEIST checks `pr_held_item_count(..., true)>0`, then explicitly `*out=pr_msg0(NULL)` with comment “the item's name isn't ported yet” |
| `phases.c:1413`, real move caller | Invokes `pre_move_message` for the first active target and queues that null-key message |
| `battle-core/tests/turntest.c:1191`, `c_texts` | Null key sets known=false and emits `<?>`; locale formatting is not called for this event |

An external observer copied the unchanged producer into an untracked diagnostic
file outside the port and wrapped `PreMoveMessageAttr.apply`, delegating the
original. For Poltergeist only it logged the actual ordered transferable items
and wrapped `phaseManager.queueMessage`, also delegating the original, to log
its text/delay. Four N2000 source reruns independently observed all five CASEs
with the exact texts above and delay 500. Rendering/UI/storage stubs were the
unchanged producer's boundaries. No answers or fixtures came from C.

An external C diagnostic copy added logging only when `c_texts` returned false:
CASE, complete C/JS TEXTS arrays, and null-key event indices. It linked the same
production library. It did not change comparisons, coverage or exit behavior;
it identified event indices 4/1/1/4/1 respectively. It is not a repaired harness.

## Smallest future repair and regression

Owning lane: A, battle-core. Replace the explicit null-key pre-move message with
`moveTriggers:attackedByItem`, the target name and the first transferable held
record's independently mapped item-name locale key. Existing removal-name
mapping is private to `src/move_effects.c:740`; inspecting it does not authorize
sharing/refactoring it in this docs task. Unknown item identity needs its own
proved boundary; do not drop the announcement, use a regex ignore or make the
turn unported to hide a supported failure.

A separate production PR should retain these five whole-turn regressions and
add ordered multiple-item, other-holder, wildcard-owner, untransferable-first,
empty/no-target and unknown-name tests. It must compare exact rendered text,
unchanged state/queue/RNG, and detect mutation back to `pr_msg0(NULL)`. The
established producer/comparator must remain intact. No repair was made here.

## Evidence and hashes

Retained outside the repository: `/workspace/work/batch-2026-10-11/items/`.
`producer-<seed>.log`, `replay-<seed>.log`, `diagnostic-<seed>.log`,
`source-observer-<seed>.log`, original and diagnostic traces, and diagnostic
source are retained. Driver scripts are one directory above. Pokemon IDs are
not seeded: these hashes identify the exact retained traces, not byte-for-byte
regeneration guarantees.

```text
5663fe8d0d37acc4b90de0b299ba38d22122ac83364a46d65c2deba62a4f381f  battle-core/tests/turntest.c
625ff61d15b22dc6410eef6343369498041231edbf06a36abf23667c590a52c0  battle-core/tests/turntest.producer.test.ts
7a8ff9f0c6ad9e3b804b8c8eadd42cdc4266dc000f9dd02fcb9e2c13dcb889d0  items-777.txt
e37f16f7e8b5d44e1767326a7c3c9042cc72a7dc67fbfb59e2f1d4b3b2b5d05d  items-9001.txt
92836f694e0e138945968893a5b1a7d9a6422093631b1fc30cac8fd2df59b5c4  items-4242.txt
0159a2ed8f257ba6a5ff27dc9c566ec1a6e4e847578201243b968925d98acd96  items-31337.txt
```

`git diff --check` passes; implementation diff is empty. H is not required for
this docs-only card. Ordinary exact-head host/3DS CI remains the merge gate.
No hardware, whole-item or whole-run parity claim. Trainer/legacy damage
failures remain separate retained failures.
