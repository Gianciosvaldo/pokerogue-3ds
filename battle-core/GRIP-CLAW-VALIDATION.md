# Grip Claw attack callback and inventory

Reference: upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22.23.3.

Grip Claw now runs from MoveEffectPhase after post-attack abilities and enemy
status tokens, using the native attack check. The item's class name does not
restrict it to contact moves. Its filtered scene dispatch checks the holder and
HP; the raw modifier override bypasses those shouldApply gates. A missing target
returns before RNG use. Choosing its sole target calls battle RNG with range 1,
which returns immediately without advancing the stream. The chance then uses
global RNG, including virtual stacks and the inclusive `<=` threshold. Only a
successful roll enumerates transferable target stock and chooses it using battle
RNG. Sticky Hold, recipient capacity, cloning, source loss/Unburden, list ordering,
stat refresh and localized messages use the shared transfer boundary from #360.

The new class carries the canonical 0.1 chance, cap 5 and class-only matching.
Its actual native tier is 3 in all three inference pools. Game-core projects and
reimports the stock, including newly stolen Grip Claws, transfer flags, stacks
and cached tiers. Existing named-field saves reconstruct the canonical class;
no save schema field was added. Raw duplicate matching Grip Claw objects are
explicitly unsupported until shared object identity is represented.

## Native validation

Both producers capture native `BattleScene.prototype.randBattleSeedInt` before
constructing GameManager, then restore it after natural battle startup. They call
the actual modifier override, scene dispatch and owning turn phases. Gameplay
implementations and expectations are not replaced. UI/phase advancement is
intercepted as in the established turn driver.

- 16,384 callback/dispatch cases at each seed 777/2034, all exact. Direct raw
  successes: 1,055/1,001. All 18 currently represented transferable keys fire;
  both sides, same-side and cross-side transfers, absent targets, fainted raw
  users, live zero IDs, locks, caps and virtual stacks are included. Public native
  RNG state control verifies an exact 0.5 roll with five Grip Claws, retaining
  the actual inclusive-boundary behavior.
- The strict reader checks ordered complete inventory, metadata and tier caches;
  all actor HP/stats/Unburden tags; all queued phase levels and exact text; both
  RNG streams; fixed sequential IDs and complete transport boundaries. Native
  tier observations are compared with C inference.
- 2,114 owning doubles turns: all covered, all state/trace/text/RNG comparisons
  exact, no native exceptions. The original 2,048 matrix cases are retained,
  followed by two targeting controls and 64 immunity/Protect/miss/Substitute
  cases. Existing doubles coverage floors remain unchanged. This includes
  physical, special, contact, noncontact, status and spread moves, fainting,
  switches, Sticky Hold and Unburden.
- Fourteen compiled callback/owner faults and 17 malformed captures are rejected.
  The controls include using the wrong chance stream, losing virtual stacks,
  changing `<=` to `<`, losing the callback hook, and restricting it to physical
  attacks. The singleton target call itself has no RNG effect; its control
  instead detects changing the range to 2.
- Real game command, inventory, repeated synchronization and save/load tests:
  11,535 checks, including successful contact/noncontact steals on both sides,
  status-move exclusion, blocked/full/locked donor stock and restored commands.
- Retained source comparisons: all 65,536 theft/Mini Black Hole cases and all
  3,608 shared scene cases remain exact. The 17 theft, nine Mini Black Hole and
  twelve scene faults plus all 55 malformed controls are retained. Only the
  theft mutation scope was narrowed to exclude the new independent callback;
  no assertion or coverage floor was dropped.
- Three established doubles corpora retain every merged covered ID, unchanged
  counts 1,994/1,996/1,998 and zero mismatches.
- Full game-core suite passes; paired saved/unsaved digest remains `b7cf2078`.
  Full normal app/custom/save suite passes. The 10,000-run digest changes to
  `1283d6de` as Grip Claw becomes active in ordinary runs; all five smaller
  save/custom digests stay unchanged. Exact-final-head CI remains a merge gate.
- The real 3DS app builds with its strong 524,288-byte stack override. All 2,251
  ARM frame reports pass the existing bound; largest frame remains 45,056 bytes.

Reproduce on Node 22:

```sh
battle-core/tools/grip-claw-diff.sh /path/to/pinned-upstream 16384 777
TURNTEST_LOCALES=/path/to/pinned-upstream/locales/en \
  python3 battle-core/tools/check-grip-claw-proof.py \
  battle-core/build/grip-777.txt battle-core/build/grip-turns-777.txt
make -C game-core grip-claw-host-test
```

## Remaining scope

These are bounded canonical-stock observations. They do not establish all item,
fusion, ability, save or hardware interactions. Unknown subclasses/parameters,
raw duplicate Grip Claw aliases, self-object transfers, broader transferable
classes and engine capacity limits retain explicit unsupported reporting. The
legacy game-core live-zero-ID sentinel boundary remains unchanged. Pickup's
ordered loot identity/cache and owning post-battle dispatch, Eviolite clone
semantics, other stock families and Baton remain separate work. This is not a
full battle-core completion claim.
