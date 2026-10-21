# Shared held-item transfer boundary

Pinned source: `8555c08c823b856cbec4eb99ca84ea52a955836d`; locales:
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`; Node 22.23.3.

`pr_can_transfer_held_item` ports the scene's preflight check. It checks the
recipient's existing stock and actual source-object membership without consuming
stock or RNG. Cross-side Sticky Hold can still queue an ability display.
`pr_try_transfer_held_item` now serves both Thief/Covet and Mini Black Hole. It
supports a requested quantity, recipient capacity, detached loot, clone defaults,
source removal, recipient remove/reappend ordering, modifier updates and the
separate item-loss callback option. Pickup needs `item_lost=false`; that option
preserves the native absence of Unburden. Skipped updates preserve unrelated
virtual counts; ordinary updates clear virtual counts and recalculate stats.

Modifier cleanup now discards empty stacks before checking the supported item
effect context. Native `updateModifiers` removes those stacks before calculating
stats. This avoids reporting an unsupported item effect on an already removed
zero-count stock. Unsupported surviving item effects remain visible.

## Native observations

The producer calls the real scene methods on naturally created doubles actors.
It captures `BattleScene.prototype.randBattleSeedInt` before constructing
`GameManager`, then restores it after battle startup. Only phase shifting and
presentation are intercepted. Quantities include 0, 1, 2 and 99; raw stocks include
zero, full and over-cap stacks, virtual counts, locked stocks and detached
owners. Preflight also includes source objects absent from their owner's actual
inventory. These raw states test the helper boundary and are not all legal
reward or battle states.

- 3,608 observations across 44 identities from the existing 17 item keys:
  1,760 preflight and 1,848 transfer calls, 2,321 successful results, zero
  mismatches or unsupported/deferred results. The original 3,520 cases are
  retained; 88 supplemental cases cover same-side source removal before an
  existing recipient stack, for every identity on both sides.
- The strict C reader compares return values; complete ordered inventory
  class/owner/count/virtual/transferability/parameter state; every actor's
  HP/stats/Unburden tags; all queued phase levels and localized text; both RNG
  streams; sequential IDs, fixed row counts and the complete footer.
- Twelve compiled faults and 22 malformed captures must be rejected. Existing
  Thief/Covet's 17 faults/19 malformed controls and Mini Black Hole's nine
  faults/14 malformed controls are retained, with mutation locations moved to
  the shared implementation. No old assertions or coverage floors were removed.
- Retained source captures: 32,768 Thief/Covet cases and 32,768 Mini Black Hole
  API/owning TurnEnd cases at seeds 777/2034 all remain exact. This retains the
  existing zero-ID source observations and their RNG/callback behavior.
- Full game-core tests pass; 200-run saved/unsaved digests remain `b7cf2078`.
  The 3DS app builds; its actual ELF has a strong 524,288-byte stack override,
  and all 2,249 compiler frame reports pass the existing bound (largest 45,056).
  Normal app/custom/save and exact-head CI remain required merge checks.

Reproduce the new comparison with:

```sh
. /workspace/shared/activate-parity.sh
battle-core/tools/scene-transfer-diff.sh /path/to/pinned-upstream
TURNTEST_LOCALES=/path/to/pinned-upstream/locales/en \
  python3 battle-core/tools/check-scene-transfer-proof.py \
  battle-core/build/scene-transfer.txt
```

## Remaining work

This is a prerequisite for Pickup, not its implementation. The other canonical
transferable item classes, Eviolite's clone-to-base-class asymmetry, ordered loot
object identity, cached selection/global RNG, per-actor Honey/Pickup dispatch,
Faint/BattleEnd ownership and game-core inventory/save integration still need
implementation and native validation. The transfer API requires inventory-owned
stock; an apply on an orphan cached object is not yet represented. Raw transfers
to the same object/holder and unrepresented item families remain explicit
unsupported paths. This does not establish complete battle-core coverage.
