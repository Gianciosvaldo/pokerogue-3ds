# Bounded vitamin application (Task 96)

`BASE_STAT_BOOSTER` now calls reward-core's validated vitamin API (PR #150).
The adapter maps storage indexes to party order, enforces IV-derived caps, adds
or stacks the persistent item, writes every party member's permanent stats/HP,
and refreshes battle modifiers so later candy/evolution recalculations retain
the vitamin. Other modeled Pokemon fields, profile and global RNG are preserved.

Pinned upstream has an unusual purchase result: persistent `addModifier` can add
the vitamin and update stats but return false. The shop reports an error, keeps
the screen open and deducts no money. This adapter preserves that mutation and
false result, with an empty reason. Free picks finish normally. `gc_offer` now
has `is_purchase`, set by SHOP, so price-zero purchases differ from free offers
(whose cost is normally zero). Presentation of the error sound remains the
caller's responsibility; frontend dialog/audio parity is not established here.

Unsupported configurations retain generic storage and an explicit reward-apply
stand-in count: challenges, Soul Dew/mystery stat items, unknown or unmapped
inventory owners, invalid stock, unfused Spliced Endless and inventory exceeding
the battle modifier boundary. At capacity, a new entry is guarded; an existing
representable entry can still stack. This does not complete Task 96. The earlier
Mint adapter still guards target/wildcard vitamin inventory; vitamin-after-Mint
preserves the original/custom nature, but Mint-after-vitamin is not unlocked.

## Reproduce

Use Node 22 (tested 22.23.3) and prepared external upstream at
`8555c08c823b856cbec4eb99ca84ea52a955836d`, with frozen dependencies, locales and
headless assets. No upstream files or generated dumps are committed.

```sh
make -C game-core test
game-core/tools/vitamindiff.sh /external/pinned-upstream 2000 777
game-core/tools/vitamindiff.sh /external/pinned-upstream 2000 9001
game-core/tools/vitamindiff.sh /external/pinned-upstream 2000 4242
game-core/tools/vitamindiff.sh /external/pinned-upstream 2000 31337
game-core/build/gcplay --auto 1000 --jobs 8
game-core/build/gcplay --auto 1000 --jobs 8 --save-test
```

`BUILD` and `GC_VIT_OUT` select external build/trace paths. The new additive
producer uses real filters, factories, persistent addition, stat updates and
`SelectModifierPhase.applyModifier`, capturing its error/end boundary. The
comparator replays direct application and public free/shop choices with rotated
storage, comparing all 54 modeled party fields, full fixture inventory, money,
result/screen state, filter reasons, unchanged profile and RNG. All input/output
records and fields are consumed, with a strict completion footer. Unexpected
queued phases throw. Established producers, comparators and exclusions are intact.

## Results

Baseline main `0a10696` (previous six-task batch); replay also verified after
rebasing onto the documentation/money-API merges.

- Four seeds x 2,000 source cases: 16,000 direct/public comparisons, zero
  mismatches or skipped/thrown cases. Every stat covers application, zero-IV,
  full stock, paid and free choices. Per seed: 1,200 applied, 400 zero-IV and
  400 full-stock refusals; 799 applied purchases report errors and 401 free
  picks end. Each seed has 191 price-zero purchases and 286 level-255 targets.
  Forms, fusions, prior custom natures, fainted members and Wonder Guard are
  positively covered by source fixtures.
- 187 host checks: all stats/caps, stat recalculation after application, repeated
  purchases without payment, zero-price shop versus free pick, affordability,
  typed filter refusals, transient field/profile/RNG preservation, prior Mint,
  guarded stock/owners/challenges, both capacity paths and legal save/load.
- Actual adapter-source mutations: accepting the erroneous purchase rejects
  1,598/4,000 comparisons; omitting stat writeback rejects 2,240/4,000 (exit 1).
  Missing completion footer exits 2. Mutants are external temporary copies.
- Established healing/candy/ball and reward-options traces for all four seeds
  replay byte-identically before/after. Their existing Hardcore/other-item
  exclusions and constructor-stat diagnostics remain uncredited. All four Mint
  traces (16,000 comparisons) and Orb traces (8,000 full turns/texts) still pass.
- H passes: 32,719 resume checks, 39 existing item-bridge checks, 641,335 existing
  application checks. Paired 200-run digest changes from `a6662b64` to
  `803ad8cf` with real vitamin application.
- Both 1,000-run modes finish every run, with no stuck/crashed/timed-out runs.
  Before: 49,293 turns, paired digest `b72f38a9`; after: 49,284 turns, paired
  digest `403b442f`, 14,302 save round trips, maximum save 11,582 bytes. Chooser
  and digest code are unchanged. External observation of that same soak counts
  135 real vitamin applications and 49 unsupported stored stand-ins, with two
  filter refusals. The 184 taken tally therefore does not mean 184 real effects.
  Aggregate reward stand-ins rise from 418 to 467 because unsupported held
  vitamins were previously omitted from that counter; no zero-stand-in claim.

Required host and 3DS build CI must pass on the actual PR head before merge.
Console hardware and complete upstream run/frontend parity remain unrun.
