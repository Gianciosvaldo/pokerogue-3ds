# After You callback and queued mover identity

Base: Acupressure merge `dd819ece9eb18607bd90857cdc6d1d14fc5f7f74` (#286).
Reference: source `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node22.23.3.

The pinned `AfterYouAttr.apply` always queues `moveTriggers:afterYou` with the
target's affixed name, then calls `forceMoveNext` for the first queued
MovePhase whose cached `pokemon` is that target. It does not call the parent
HP gate or check `acted`. The move's separate conditions already reject
singles and an acted target; those conditions remain unchanged. The C leaf
now uses the existing `pr_force_move_next` helper and the exact message.
No new persistent state or save fields are introduced.

## Actual source API

`tests/after-you.producer.test.ts` captures native BattleScene RNG before
GameManager construction and restores it after battle startup. It creates
real MovePhases through the native phase manager and calls the registered
AfterYouAttr. No gameplay method or expected result is replaced. Phase
advancement is stalled so actual queue observations can be transported.

The 8,192 complete cases /15,936 calls cover both sides and all user/target
positions, absent movers, duplicate owners, all initial timing modifiers,
repeated/alternating targets, fainted and acted actors. Party positions are
also swapped after phase construction; cached Pokemon identity, rather than
the current field slot, must select the first mover. The comparison checks
every phase-tree level and marker/message, exact localized message text,
dynamic queue membership/order/timing, unchanged HP/acted state and both RNG
streams. There are zero mismatches and no unsupported cases in this API proof.

The runtime faults include missing handler/message/force, added HP/acted
gates, wrong message target/affix, forcing the user, current-slot identity,
forcing last, and changing every duplicate instead of the first. The guard
tool compiles eleven real runtime faults and rejects twenty-one malformed
captures. Removing the force call also causes nine covered complete-turn
failures in the plain seed777 capture.

## Owning turns and retained coverage

The three actual doubles captures from the preceding slice are replayed
unchanged on the exact #286 baseline and this implementation. Each contains
all 2,000 source-written turns with zero source exceptions. Source producers,
choosers, comparators, seeds/counts and exclusions are unchanged.

| Sample | Covered before | Covered after | Gained IDs | Lost IDs |
| --- | ---: | ---: | ---: | ---: |
| Plain seed777 | 1,888 | 1,905 | 17 | 0 |
| Rich seed777 | 1,884 | 1,905 | 21 | 0 |
| Plain seed2034 | 1,889 | 1,911 | 22 | 0 |

All 5,721 compared states and exact localized messages match. There are still
279 explicitly unsupported turns among the 6,000 observations. Covered-ID
sets prove zero loss, independently of the aggregate gains.

The complete 8,194-turn Acupressure matrix remains fully covered and exact.
The retained 8,000 mixed seed777 turns preserve every covered ID, with counts
plain1,971, rich1,965, summon1,991 and switch1,994. Applicable game-core,
app/custom/save and 3DS checks are required on the exact PR head before merge;
the PR records their results. The dedicated custom-only Task63 workflow stays
excluded, while normal custom/save checks remain enabled.

## Boundaries

This proof exercises ordinary non-fused wild-double actors and native cached
move identity. The broader rich sample adds other ability contexts without
establishing complete doubles parity. Command/save UI support for playable
app/run doubles, suppression callbacks, attention redirection, other remaining
partner interactions and wide-level Endless remain open. No run-core changes,
legal-clear or emulator/hardware acceptance are included.

Reproduce with `tools/after-you-diff.sh UPSTREAM`, then
`TURNTEST_LOCALES=UPSTREAM/locales/en python3 tools/check-after-you-proof.py
build/after-you.txt DOUBLES_TRACE`. The actual complete-turn comparison uses
the unchanged `build/doublestest` runner.
