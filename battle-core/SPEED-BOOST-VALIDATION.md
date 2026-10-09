# Speed Boost actual post-turn proof (Card 106, 2026-10-09)

TurnEndPhase already dispatched post-turn ability attributes, but Speed Boost's
leaf was missing. The bounded repair implements `SpeedBoostAbAttr.canApply`
and queues the existing Speed +1 stat-change phase through the existing ability
dispatcher. The ordinary ability gates retain suppression and faint checks;
existing ability display, passive handling, stat clamping, Simple, Contrary and
text paths supply their behavior. No new timer or stat formula is introduced.

Source is `pagefaultgames/pokerogue` pinned at
`8555c08c823b856cbec4eb99ca84ea52a955836d`, Node 22.23.3. Source
`ab-attrs.ts:4348` defines canApply as simulated OR neither switched-in-this-turn
nor failed-run-away. There is **no minimum tempSummonData.turnCount condition**.
Its apply method always queues `StatStageChangePhase`, Speed +1, on the holder.
`turn-end-phase.ts:57` supplies the real post-turn call before incrementing the
holder's temporary turn counters. Suppression and faint eligibility belong to
`Pokemon.canApplyAbility` and the field iteration, rather than an invented leaf
condition. The Speed Boost factory registers this existing attribute directly.

## Actual source turns

`tests/speed-boost.producer.test.ts` runs actual complete turns through the pinned
TurnEndPhase, ability dispatcher and StatStageChangePhase. Observers delegate
to the original SpeedBoostAbAttr canApply/apply methods. Fresh actual
getEffectiveStat values, complete Pokemon state/stages/PP/history, field/arena,
commands, phase and pending queue, temporary turn counters, RNG and exact texts
are compared.

The new `speed-boost-replay.inc` copies the established comparator with its
existing additive EFFECTIVE stat transport. It additionally reads/writes the
**already existing** C `failed_run_away` field, which the original transport did
not expose. No old producer/comparator or comparison rule changes. This
additive field makes the source refusal boundary observable instead of silently
losing it during input loading.

Four seeds **777,9001,4242,31337**, each **2000 requested cases**:

| Evidence per seed | Before | Final |
|---|---:|---:|
| Covered / exact turns | 398 / 398 | 2000 / 2000 |
| Unsupported Speed Boost turns | 1602 | 0 |
| Parse / numerical input mismatches | 0 / 0 | 0 / 0 |
| Full turn failures / text differences | 0 / 0 | 0 / 0 |

All **8000 final turns/texts are exact**, with **1000 player / 1000 enemy
holders per seed**. Each seed observes **1332 actual apply invocations** and
**336 actual leaf refusals** from switched-in/failed-run flags. Apply invocation
counts include two holders in 66 cases and cap cases, and are distinct from
visible stat-phase counts. Suppressed/fainted/no-ability controls do not reach
the leaf, so they are not mislabeled as leaf canApply refusals.

Thirty bounded loaded states exercise ordinary activation; switched-in,
failed-run-away and both flags; suppression of primary and passive abilities;
primary/passive Speed Boost alongside Simple or Contrary; Speed +5/+6 and -6,
including Contrary at the floor; temporary turn counts 0/1/100; Protect without
and with an incoming Tackle; an already fainted holder; an actual Tackle KO
before the post-turn callback; both active holders and their ordering; and a
no-Speed-Boost control. Each state occurs 66 or 68 times per seed. Loaded
failed-run-away flags prove that existing flag's caller boundary; these are not
a proof of the entire run-away command/chance policy. Passive combinations are
loaded canonical ability states, not a claim about acquisition or natural
species passive eligibility.

This proves ordinary singles inputs and the actual post-turn caller, including
bounded faint/suppression/protection cases. It does not claim all abilities,
doubles, fusion, challenge policy, timer overflow, broader interlude/whole-run
upstream parity, or hardware playability.

## Runtime mutations and strict contracts

Six copied-object mutations replace actual C runtime code and replay unchanged
seed777 with the unchanged new comparator:

| Actual caller mutation | Covered / full turn failures | Exit |
|---|---:|---:|
| Remove Speed Boost dispatch | 398 / 0;1602 unsupported | 1 |
| Remove switched-in gate | 2000 / 134 | 1 |
| Remove failed-run-away gate | 2000 / 134 | 1 |
| Queue Attack instead of Speed | 2000 / 1266 | 1 |
| Queue +2 instead of +1 | 2000 / 1068 | 1 |
| Invent temp-turn-count >0 gate | 2000 / 66 | 1 |

The missing handler fails requested complete coverage. Other mutations retain
2000 covered turns and fail actual state/text comparisons. No production code
is changed while these separate copied-object mutants run.

The owning wrapper rejects incomplete requested coverage, state/numerical/text
failures and unsupported messages. Integer counts use checked strtol rather
than an overflowing scanf narrowing conversion. **17 malformed traces exit 2**:
missing header/footer, wrong request/version/footer, fractional/extra header,
extra footer, records after completion, missing END, duplicate case, unexpected
MOD, missing command, invalid RUN, and overflowed header/footer/case counts.
Fractional producer request `2000.5` exits 1 before trace creation.

Four additive actual-dispatch guards preserve the queue boundary: without an
engine, eligible actual and simulated applications remain explicitly flagged;
actual switched-in and failed-run refusals do not flag or queue. The source
apply method queues even when simulated, so queue-less simulated parity is
**not** promoted. These guards run after the full source contract, keeping
runtime mutation evidence attributable to actual turn comparisons first.

## Retained proof and historical failures

Fresh baseline main is `a6d175705206930c92e50bf975fcb3d1cdcb4cbd` (Growth and
Classic win included). All **76 retained battle pairs** replay unchanged traces
and unchanged owning drivers: the prior 72 plus the four Growth suites. These
retain plain/rich/switch/summon/ITEMS/COND, held-item families, four legacy raw
damage seeds, WEATHER/ARENA/forms/Field Effect, temporary boosters, trainer
switches and Growth.

**74 battle outputs are byte-identical.** Two suites gain **8 newly covered
exact turns** as formerly flagged Speed Boost becomes active:

| Suite / seed | Covered before → after | New state failures |
|---|---:|---:|
| rich / 777 | 1832 → 1836 | 0 |
| summon / 777 | 1863 → 1867 | 0 |

Coverage never decreases and all covered state comparisons pass.

Historical ITEMS text-completeness checks remain explicitly **FAILED**, with
unchanged missing-text counts777=1,9001=1,4242=2,31337=1. They receive no pass
credit, even though the old comparator accepts their covered state checks.
The original broad trainer comparator is also rerun independently on its four
unchanged traces: seed4242 remains **FAILED** at CASE1051 due to the previously
documented pending SwitchSummonPhase optional-index transport mismatch. The
other three seeds pass. No source producer, comparator, tolerance, count,
exclusion or guard is edited to hide these historical failures.

All **60 retained game source outputs are byte-identical** (prior 56 plus four actual Classic
GameOverPhase traces) replay unchanged.

## Host integration

Fresh baseline/final `make -j2 -C game-core test` pass. The unchanged original
host chooser runs 1000 normal and 1000 saved simulations, jobs 8:

| Host measure | Before | After |
|---|---:|---:|
| Ended / cleared / stuck / crashed / timed out, each path | 1000 / 0 / 0 / 0 / 0 | 1000 / 0 / 0 / 0 / 0 |
| Turns, each path | 50356 | 50410 |
| Normal and saved digest | e789ef81 / e789ef81 | 75bb4db1 / 75bb4db1 |
| Save round trips / largest bytes | 14686 / 11472 | 14702 / 11472 |
| Original 200-run normal / saved test digest | 6741e0a8 / 6741e0a8 | 6cd5fc16 / 6cd5fc16 |

Baseline reports SpeedBoostAbAttr **181 times in 35 host runs**. Its flag
disappears with the only runtime change, the source-proved ability handler.
Existing source-backed examples above independently prove Speed +1,
Simple/Contrary, caps, refusal and turn-count behavior; later host outcomes can
therefore change. Neither the original chooser nor the digest is overridden.

| Host stand-in, runs / occurrences | Before | After |
|---|---:|---:|
| Last four level moves | 1000 / 15096 | 1000 / 15112 |
| Doubles played as singles | 767 / 1583 | 767 / 1582 |
| Evolution condition unchecked | 2 / 3 | 2 / 3 |
| Biome weather roll | 1000 / 1901 | 1000 / 1903 |
| Enemy generation stand-in | 1000 / 2305 | 1000 / 2308 |
| Post-battle abilities / Pay Day / item lapse | 166 / 1375 | 166 / 1398 |
| Trainer title omitted | 311 / 421 | 311 / 421 |
| Reward without effect | 145 / 181 | 145 / 181 |

These are original host chooser/digest and completion checks, not upstream run
parity. Source turns independently prove the new post-turn effect; differences
in subsequent host trajectories are expected when formerly unsupported Speed
Boost becomes active. Existing host stand-ins and deferred source boundaries
remain visible in the complete saved host outputs.

## Reproduction

```sh
. /workspace/shared/activate-parity.sh
for seed in 777 9001 4242 31337; do
  battle-core/tools/speedboostdiff.sh /path/to/pinned/upstream 2000 "$seed"
done
make -j2 -C game-core test
game-core/build/gcplay --auto 1000 --jobs 8
game-core/build/gcplay --auto 1000 --jobs 8 --save-test
```

The tool rejects a wrong source pin or Node version. Trace data, source logs,
before/final replays, copied-object mutants, malformed/queue guards, retained
pair summaries, historical failure logs, host outputs and PR/CI metadata are
saved under `/workspace/validation/lane-a/eight/speed-boost/`. Dumps and binaries
are not committed. Final host CI and 3DS cross-build remain separate gates.
