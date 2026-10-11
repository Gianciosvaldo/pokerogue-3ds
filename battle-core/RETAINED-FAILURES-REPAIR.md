# Retained ITEMS text and trainer queue repairs — 2026-10-11

This follow-up implements the user's request to fix the failures retained by
D14/#196 and D15/#216. Their earlier investigation/probe contracts deliberately
left established runtime/comparator behavior unchanged; those records are
historical evidence, not a restriction on this new repair.

Source pin `8555c08c823b856cbec4eb99ca84ea52a955836d`, locale pin
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22.23.3, GCC 14.2.0.

## Changes and independent source contract

`PreMoveMessageAttr.apply` calls `attackedByItemMessageFunc` for Poltergeist.
`Pokemon.getHeldItems` selects actual held modifiers on the target side with
**exact owner IDs**, retains list order, and the message function then filters
transferable items and chooses the first. The port now follows that selection,
queues `moveTriggers:attackedByItem`, and supplies the target name/affix and
translated item name. It shares the existing independently mapped six named
battle-item/eleven-berry identities with item removal. No removal, RNG draw,
stack weighting, or attack-result condition was added. The existing full-turn
cases cover announcements before misses, ordinary damage, and KOs.
Unsupported/custom item identities remain visible missing-text failures; there
is no invented generic name or new exclusion to conceal them. Source null/type
name fallbacks and arbitrary custom item names are outside this bounded map.

The original queue comparator now uses the D15 independent source-inheritance/C
addressing classification when projecting legacy `join` output. A phase whose
source object has no `battlerIndex` emits an empty slot; an indexed phase with
actual integer `-1` still emits `-1`. This repairs pending SwitchSummonPhase in
trainer seed 4242 CASE 1051. It changes **test transport only**. Production phase
indices, addressing, queue order, gameplay, source producer and state/RNG records
are unchanged. The separate versioned D15 A/U/N/I probe still tests absence,
undefined, null and genuine negative indices distinctly; its builder now anchors
the updated legacy loop before substituting the typed projection.

`turntest` now returns failure for covered differing/missing texts, stat
mismatches and skipped/unread records, as well as state/parse/condition failures.
Previously all five missing announcements incorrectly returned exit 0 because
only state comparisons contributed to its exit status. Unsupported turns still
receive no pass credit.

## Validation

Fresh source runs: 2000 cases per seed, seeds 777/9001/4242/31337. Each source run
completed all requested cases; replay skipped/parse/stat mismatches are zero.

| Mode | Seed | Covered, all passing | Explicitly uncovered | Covered text identical / differing / missing |
|---|---:|---:|---:|---:|
| ITEMS | 777 | 1939 | 61 | 1939 / 0 / 0 |
| ITEMS | 9001 | 1945 | 55 | 1945 / 0 / 0 |
| ITEMS | 4242 | 1940 | 60 | 1940 / 0 / 0 |
| ITEMS | 31337 | 1956 | 44 | 1956 / 0 / 0 |
| TRAINER | 777 | 1946 | 54 | 1946 / 0 / 0 |
| TRAINER | 9001 | 1943 | 57 | 1943 / 0 / 0 |
| TRAINER | 4242 | 1945 | 55 | 1945 / 0 / 0 |
| TRAINER | 31337 | 1953 | 47 | 1953 / 0 / 0 |

Baseline at main `76a9f5c`: ITEMS had 1/1/2/1 missing texts with the same
covered-state counts, and TRAINER 4242 had 1944 pass/1 fail. Replaying the saved
pre-repair traces also passes now: no expected output was changed to obtain a
pass. Fresh IDs may differ because source constructor IDs are not seeded.

- 25 independently produced actual PreMoveMessageAttr method fixtures: six named
  items, all eleven berries, first-of-multiple order, nontransferable first,
  another owner, wildcard owner, no items, null target, enemy side and wrong
  side. All exact rendered texts/return values and state/RNG controls pass.
- Restore the actual runtime null-key announcement in temporary compiled C:
  the intact fixture driver and four ITEMS replays return 0; every mutant returns
  1. The whole-turn mutant retains matching state and restores precisely the
  original 1/1/2/1 missing texts, proving the new text failure gate.
- D15 typed trainer 2000: 1945 covered exact, 55 explicitly excluded; six
  deterministic full-queue fixtures exact. Its 46 malformed parse refusals and
  seven valid-but-wrong typed semantic refusals remain effective. Removing
  SwitchSummonPhase from the independent C absence mapping fails CASE 1051 and
  three deterministic fixtures; genuine `I-1` cannot be substituted with `A`.
- Dedicated trainer switches and berries: four retained 2000-case source traces
  each still replay exactly. Item removal: four fresh 2000-case source runs exact,
  validating the shared naming helper's existing caller.
- `make -j2 -C battle-core`, `make -C battle-core test`, and
  `make -j2 -C game-core test build/gcplay` pass. Paired host test digest
  `019412a9`; paired 1000-run normal/save digest `874370f1`, 49966 turns,
  14629 round trips, largest save 11472 bytes. Each run ended; zero stuck,
  crashed or timed out, **zero gameplay clears**.

These results repair the two named retained failures, not full game parity,
natural doubles, every custom item name, or hardware rendering.

## Reproduce

Use a clean pinned upstream with its pinned locales, installed dependencies and
Node 22. Run the fixtures/probes from the repository root:

```sh
battle-core/tools/poltergeist-message-diff.sh "$UP"
battle-core/tools/optional-phase-diff.sh "$UP" 2000 4242
```

For each seed, copy `battle-core/tests/turntest.producer.test.ts` into upstream
`test/tests/zz-dump/`, generate using `TURN_ITEMS=1` (or `TURN_TRAINER=1`),
`TURN_N=2000`, `TURN_SEED`, and `TURN_OUT`. Execute upstream vitest with
`--maxWorkers=1`, then run `battle-core/build/turntest` on that source trace with
`TURNTEST_LOCALES="$UP/locales/en"`. Check the covered and text summaries; all
three kinds of differences must be zero. Preserve uncovered counts separately.

`poltergeist-message-mutations.py` takes the source message fixture trace and four
ITEMS traces, with the same locale environment. The D15 malformed/mutation tools
retain their documented arguments in OPTIONAL-PHASE-METADATA-VALIDATION.md.
