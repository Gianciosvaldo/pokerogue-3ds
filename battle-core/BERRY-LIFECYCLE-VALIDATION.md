# Harvest and Cud Chew lifecycle

Base: main `756c40d0f8dc49fa7a9fb65558135b1d942c23e2` (Ditto #261).
Source: `8555c08c823b856cbec4eb99ca84ea52a955836d`; locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`; Node 22.

## Owning source boundaries

- `Pokemon.recordEatenBerry`: Belch flag, recoverable battle history only for
  actually consumed berries, and current-turn history even when Berry Pouch
  preserves the item.
- `BerryModifier.apply` / `BerryPhase.eatBerries`: actual effect, item-loss
  callbacks and consumption bookkeeping; ordinary berry stat changes aggregate.
- `CudChewConsumeBerryAbAttr`: previous-turn effects, individually queued stat
  phases, no second consumption, history entry, Unburden or Cheek Pouch trigger.
- `CudChewRecordBerryAbAttr`: end-of-turn recording, replacing the previous list.
- `PostTurnRestoreBerryAbAttr`: actual eligible history filter, chance roll even
  under sun, random choice, restoration to the correct owner and held-item cap.
- `BattleScene.updateModifiers`: virtual stack cleanup, orphan/depleted item
  removal and party stat refresh at ordinary consumption/restoration boundaries.

The source filters capped berries, then splices the chosen **filtered index**
from the original battle history. The port deliberately preserves this quirk.
Leppa's source fallback chooses the first move whose `ppUsed < getMovePp`, even
if that move has full PP; it is not replaced with a mainline-Pokémon rule.
Source Cud Chew methods do not respect `simulated` consistently; the port keeps
that behavior rather than adding an invented simulation guard.

## Differential

`tools/build-berry-turn-proof.py` adds berry histories and virtual stacks to the
unchanged Speed Boost full-turn scaffold/comparator. It invokes the actual
source methods; it does not replace gameplay methods with expected formulas.
All added parser/output projections have exact anchors. Existing producer
seeds, case counts, tolerances and coverage floors remain unchanged.

Run with the pinned source checkout and Node 22:

```
tools/berry-lifecycle-diff.sh /path/to/upstream 2000 777
```

Four seeds (777, 9001, 4242, 31337), 2,000 complete turns each: all 8,000 covered
and exact, including phase trace, both RNG states, HP/PP/stats, modifier lists,
all three histories and rendered English text. No unsupported, skipped or
parse/stat-mismatch cases. Forty state/side combinations cover all berry
identities, primary/passive dispatch, suppression, faints, sun/harsh sun/weather
suppression, caps (including virtual stacks), filtered-index removal, existing
stacks, preservation, Unnerve, Ripen, Cheek Pouch, Lum/Leppa and stage bounds.
Following-turn cases include fresh consumption, preservation and repeat expiry.
An additional 2,000 first-turn cases at seed 777 prove recording when the new
berry identities differ from the previous history; run the wrapper with a final
`first` argument. All 10,000 owning turns are exact.

An initial virtual-stack extension exposed a missing input projection in the
owning replay reader. Once that reader was complete, it exposed the missing
modifier refresh. Both were fixed; no expected value or comparison rule changed.

## Application state

Per-battle histories survive switches and reset at the existing arena/trainer
transition boundary. Current-turn history resets with turn data; last-turn
history resets with summon data. Legal command saves retain battle and summon
histories. Loading resets ephemeral turn data after validating its bounds.

The hashed save format appends six fields. Older saves with none of the fields
load with empty histories; history absent from old saves cannot be reconstructed.
Partial new schemas, unknown berry values and oversized counts are rejected.
The inventory bridge persists restored berries on both sides, distinguishes
owners/types, and makes repeated synchronization idempotent. Restored entries
join the represented prefix without removing unrepresented overflow items.
Berry Pouch maps its actual side-wide class and physical/virtual stack count.

`game-core/tests/berry_history_host_test.c`: 49 command/save/reset/inventory and
refusal checks. Bounds are explicit: 128 berries per history, the existing
modifier/item capacities, and ordinary legal command save points. Overflow is
reported unsupported rather than silently described as complete parity.

The application soak's strongest-move policy had one remaining permanent
moveset read after Transform. It now reads the effective moveset. The policy,
seed generator, run count and coverage thresholds otherwise remain unchanged.

## Regression validation

The unchanged retained turn suite matches all 24,000 cases without a covered
mismatch or coverage loss: plain 1,964; rich 1,932 -> 1,940; switch 1,891;
summon 1,891 -> 1,893. Item seeds cover 1,945 / 1,948 / 1,945 / 1,958; trainer
seeds 1,951 / 1,950 / 1,947 / 1,959. Nine retained dedicated ability/move suites
match all 72,000 owning turns; Ditto's 8,000 phase cases and 8,000 full turns
also remain exact.

The 10,000-run application soak passes: 119,732 waves, 485,844 commands, zero
stalls, display mismatches or model violations. Core host checks pass, including
paired 200-run save/no-save digest `4e62468c`. The corrected fake move-selection
policy reads copied moves without changing its algorithm or coverage floors.

All 26 compiled runtime mutants are caught; recording-copy removal is caught
by the additional first-turn source proof. All 26 malformed fixtures exit 2;
the intact owning fixture passes. The complete application host suite and
exact-head host/3DS CI are recorded on the PR before merge. No hardware or full doubles-battle
claim is made. Broader copying, forms, partner mechanics, wide levels and final
boss/run prerequisites remain separate continuation gates.
