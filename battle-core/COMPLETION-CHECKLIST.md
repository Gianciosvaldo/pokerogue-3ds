# Battle-core completion checklist

Target: enabled battle behavior at upstream
`8555c08c823b856cbec4eb99ca84ea52a955836d`, with the pinned locales and Node22.
This checklist describes the finish line. A checked-in implementation or a high
passing percentage in a bounded random sample does not close a whole category.

## Required before claiming completion

- [ ] Finish the source inventory: enabled move/ability attributes, constructor
  closures and conditions, battler/arena tags, held modifiers, phases and public
  callers. Give every enabled path an implementation/proof reference or a
  documented prerequisite owned outside battle-core. Check the source's disabled
  registrations and unimplemented mechanics explicitly; do not enable them to
  manufacture parity cases.
- [ ] Review every explicit unsupported/deferred path and fixed-size overflow
  boundary against that inventory. Distinguish an unreachable native branch,
  source exception, unsupported valid input and missing state. Keep real missing
  paths visible instead of replacing their flags with no-ops.
- [ ] Close remaining doubles behavior: active-partner party cures, Pollen Puff,
  attention/redirection, Commander/Order Up, and any additional enabled callbacks
  found by the inventory. Complete Flame Burst's actual damage/ability/faint/
  shield callers and then retain those proofs in later slices.
- [ ] Close remaining identity/form/copy and phase-transition gaps identified by
  current source audit, including their gain/loss/reset and serialization paths.
  Historical inventory rows must be reconciled with later merged slices.
- [ ] Complete wide-level Endless: EXP/candy and display callers, generated
  encounters, damage and other level-dependent arithmetic, existing save
  compatibility, and source-backed high-wave progression. The storage foundation
  in #289 is already merged; its bounded numeric proofs do not close this row.
- [ ] Expand interaction evidence beyond ordinary random turns: all legal target
  arrangements, simultaneous faints/switches, fusion/Transform, held inventory,
  suppression/bypass/passives, boss shields and persistent state across turns.
  Use source-defined boundaries and counterexamples to choose these cases.
- [ ] Verify all retained covered IDs and exact states, queues, text and RNG.
  Keep source exceptions and unsupported cases reported. A required producer
  case must not disappear, be normalized away or become a weaker comparison.
- [ ] Keep normal game-core/app/custom/save tests and the 3DS build passing on
  every merged PR head. Only the dedicated custom-only Task63 checks are skipped.
- [ ] Publish a final source inventory and evidence audit listing any externally
  owned prerequisites and remaining limits. No full completion claim while an
  enabled in-scope battle path remains unsupported or has unresolved mismatches.

## Current sequence

1. Flame Burst and its downstream indirect damage/shield/RNG behavior.
2. Remaining partner cures/healing, attention/redirection and Commander/Order Up.
3. Wide-level progression and combat; preserve the wide save migration.
4. Reconcile all remaining unsupported paths with the complete source inventory,
   implement them in reviewable slices and run the final interaction audit.

The order can change when an actual source mismatch reveals a prerequisite.
Each slice retains its own reproducible validation and exact-head CI record.
Source proofs must observe actual pinned methods, including native RNG captured
before GameManager construction when the framework would replace it.

Run-core remains owned separately. Read its contracts and use its APIs; do not
silently change that owner's implementation while closing battle-core callers.
Hardware/emulator acceptance is separate from source parity and a passing build.
