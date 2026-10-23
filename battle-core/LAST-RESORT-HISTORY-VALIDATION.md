# Last Resort across long summon histories

Pinned source `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22.

`lastResortCondition` in `src/data/moves/move-condition.ts` requires Last Resort
and at least one other distinct move in the current moveset. Every other move
must occur in the entire summon move history with a non-virtual use mode. The
condition ignores the entry's result. It reads the current moveset, including a
temporary collection. The owning move registers this condition in sequence 3.
The source's annotated overwritten/relearned-move edge case is preserved.

The old C implementation searched only its eight retained entries, reporting
MOVE_HISTORY when a needed earlier entry was no longer available. A native
16-case audit reproduced ten unresolved cases once the length exceeded eight.
Those original cases now all compare exactly and remain in the expanded matrix.

## Implementation and saves

A 921-bit membership set records every non-virtual move passed to the existing
history push function. It is independent of the retained tail and the current
moveset. Actual summon reset clears it. Raw short histories can initialize a
complete set from their retained entries. Legacy long histories retain known
membership and explicitly report unknown older entries when needed. A subsequent
known use can satisfy a condition without inventing the missing history.

The save format and existing Pokémon field schema are unchanged. Separate
optional party/enemy move-use sections persist the set and its completeness
flag. New sections require exact names, sizes, record counts and a valid boolean;
duplicate sections and unused high bits are rejected. Older saves lacking them
rebuild known membership from the saved tail with zero dropped data. A legacy
history longer than that tail remains incomplete. The other history consumers
continue to use their existing explicit boundaries.

## Source observations and caller checks

The producer starts a real battle, captures native BattleScene.randBattleSeedInt
before constructing GameManager and restores it after startup. Moveset overrides
are cleared after startup, and each active collection is asserted before querying
the actual move.applyConditions sequence-3 owner. A preliminary diagnostic had
left the Splash override active; that capture is preserved separately and is not
used as evidence for the corrected matrix.

The required matrix is 432 independent histories: nine lengths (0, 1, 7, 8, 9,
10, 64, 256 and 1,024), all six use modes and eight current-moveset arrangements.
These include duplicate moves, no Last Resort, Last Resort alone, missing other
moves, a high move ID and a temporary moveset. Five observations per history
compare the initial condition, a changed moveset requiring Splash, another
changed moveset requiring the earlier first move, actual summon reset and a new
normal use after reset: **2,160 native owner-condition observations**.

The original four observations per history remain; the fifth checks an earlier
move that was outside the former current moveset and has left the retained tail.
Both RNG streams remain unchanged by the condition. All 2,160 C observations
match with zero uncovered cases. The fixture losslessly run-length-encodes the
observed move/use-mode/result/target tuples; every entry is replayed. Expected
conditions are transported directly from source, not calculated by the builder.
Native TurnMove.turn is outside this condition contract and the C history type.

Game-core compares all 2,160 conditions again after save/load, with zero dropped
data (12,053 checks including schema and command checks). Thirty actual doubles command turns additionally verify invocation of the
condition, damage versus refusal, and identical HP/RNG continuation after reload.
These command checks are C owner/continuity checks, not a claim of complete
native turn-state/queue comparison for the new long-history cases.

Malformed summary boolean, size, record count, high bits and duplicate sections
are rejected. Section removal tests legacy short/long saves and a later known
use while keeping the older unknown portion explicit. Seven compiled faults
are detected (aging loss, virtual counting, high-ID omission, current-moveset-only
tracking, reset leakage, saved-set loss and invented legacy completeness).
Eight malformed captures are rejected. The full local game-core suite passes,
including the historical save fixture; saved/unsaved digests remain `5073f991`.
Normal app/custom/save and
actual ARM build/stack checks remain required before merge; only dedicated
custom-only Task63 is skipped. No SD-card builder is produced.

All retained fused-item 5,120, ordinary stat 2,000, critical-item 2,000, Pickup
4,400, transferred-stat-item 960, theft 32,768, Grip Claw 32,768, Baton 256 and
scene transfer/Mini Black Hole 3,608 comparisons pass against the changed state.

Reproduce:

```sh
. /workspace/shared/activate-parity.sh
sh battle-core/tools/last-resort-history-diff.sh /path/to/pinned-upstream
python3 battle-core/tools/check-last-resort-history-proof.py
make -C game-core test
```

## Remaining audit

This closes Last Resort's full-history membership query for newly recorded
histories and preserves explicit uncertainty in legacy partial histories. It
does not close the eight-entry boundary for other consumers, the 16-bit tracked
history length, long Protect streak arithmetic, last-non-virtual searches, faint
histories or transient inventory histories. Broader identity/interaction and
wide-Endless acceptance remain open. Battle core is not declared complete.
