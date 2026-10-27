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
- [ ] Reconcile all doubles paths and interactions against the source inventory.
  Retain Helping Hand, Acupressure, After You, suppression, Flame Burst, partner
  cures/healing, attention/redirection and Commander/Order Up proofs; close any
  additional enabled callbacks and unresolved owning-caller interactions.
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

Merged evidence includes Helping Hand, Acupressure, the subsequent doubles and
wide-level slices, Transform/form fixes, Keldeo/Mimicry (#342), Hospitality and
Curious Medicine (#343), and Costar (#345). Preserve their proofs and retained
covered IDs when changing shared callbacks; do not recreate already merged work.
Poison Puppeteer (#353) adds the actual status callback and complete-turn proofs;
see [its validation record](POISON-PUPPETEER-VALIDATION.md). Biome callbacks and
their stock-mode reachability merged in #354; see
[their validation record](BIOME-CALLBACK-VALIDATION.md). Double-battle encounter
chance and its actual wild/trainer owners merged in #355; see
[its validation record](DOUBLE-CHANCE-VALIDATION.md). Synchronize encounter
nature and its raw/custom nature setter merged in #356; see
[its validation record](SYNC-NATURE-VALIDATION.md). Honey Gather and shared wide
money handling merged in #357; see [their validation record](HONEY-MONEY-VALIDATION.md).

1. Close the remaining enabled callbacks against their actual owners:
   Synchronize encounter nature merged in #356. Honey Gather and shared wide-money handling merged in #357. Terminal Endless, canonical transfer stock and the Mini Black Hole/Grip Claw/Baton transfer prerequisites are merged. Pickup's callback owner and 50 item identities now have source comparisons; see [the Pickup record](PICKUP-VALIDATION.md).
   The [stat-booster follow-up](STAT-LOOT-VALIDATION.md) adds their canonical
   clone/save paths. The [fused held-item follow-up](FUSED-HELD-VALIDATION.md)
   adds original Eviolite/species-booster/Leek fusion eligibility through native
   full turns and the doubles command/save owner. The [inventory-capacity
   follow-up](INVENTORY-CAPACITY-VALIDATION.md) fixes canonical stock truncation
   and expands battle/reward/save storage. Transient inventory history and the
   remaining source paths still need review.
   Existing helper arithmetic alone does not
   establish that a public owner invokes it.
   The Pokérus/trapping owning-caller audit is recorded in [the validation record](../game-core/POKERUS-TRAP-VALIDATION.md). It does not close all switch interactions.
   The [received-hit history proof](RECEIVED-HISTORY-VALIDATION.md) covers the
   enabled whole-list Counter/Revenge/Focus Punch/Enigma/Rage queries, a naturally
   ordered forty-hit turn and actual turn/save boundaries. Unknown imported
   prefixes and identities outside the two-party budget remain explicit.
   The [turn-order follow-up](TURN-ORDER-VALIDATION.md) covers complete Fusion
   Flare/Bolt queries beyond the first eight movers, live actor histories,
   shared-target alias lifetime and a native nineteen-action turn.
   The [actor-reference follow-up](TURN-ACTOR-VALIDATION.md) preserves native
   turn-order objects across ID collisions, ID changes and party removal.
   The [current held dispatch inventory](HELD-ITEM-INVENTORY.md) reconciles all
   33 pinned subclasses with C mappings, owner references and bounded proof
   records. Its class mapping count is not a semantic completion count.
   The [event handoff follow-up](EVENT-DELIVERY-VALIDATION.md) drains full core
   presentation buffers through the real controller without changing their
   capacities. Downstream and other engine limits remain explicit.
   The [diagnostic trace follow-up](TRACE-CAPACITY-VALIDATION.md) lets a native
   607-phase turn finish after its 512-entry diagnostic prefix fills; trace
   truncation is explicit and separate from genuine engine-storage failures.
   The [controller storage follow-up](../game-core/CONTROLLER-EVENT-STORAGE-VALIDATION.md)
   delivers that turn's 555 events through the app with optional caller-owned
   growth. Default inline clients and other engine/app capacities remain bounded.
2. Finish the source inventory, including AI moveset attributes, constructor
   closures/conditions, tags, modifiers, phases and unsupported/capacity paths.
   The [closure identity audit](CLOSURE-IDENTITY-VALIDATION.md) keeps helper
   hashes aligned with captured constructor arguments and the generator.
   Generic registry misses require owning-caller review, not automatic rework.
3. Reconcile identity/form/copy and persistent interaction evidence with later
   merged slices; retain exact state, queues, text, RNG and all prior covered IDs.
   [Last Resort history](LAST-RESORT-HISTORY-VALIDATION.md) adds full non-virtual
   membership, summon reset and compatible saves. The [nonvirtual query
   follow-up](NONVIRTUAL-HISTORY-VALIDATION.md) adds all four last-real-move
   selectors, their shared-target identities and optional save state. The
   [Protect follow-up](PROTECT-HISTORY-VALIDATION.md) adds complete protection
   streaks, Number-sized seeded ranges and compatible saves. Other all-history
   queries now have [Conversion 2 and Lock-On/Mind Reader evidence](REMAINING-HISTORY-VALIDATION.md). [Repeated faint history](FAINT-HISTORY-VALIDATION.md) now preserves its ordered tail and complete party membership for Retaliate and trainer Tera; numeric/party boundaries remain explicit.
4. Finish wide-level Endless acceptance across generation, arithmetic,
   progression, display and compatible saves, then publish the final evidence
   audit and explicitly list any remaining external prerequisites.
   The [Endless wave sweep](../run-core/ENDLESS-WAVE-SWEEP-VALIDATION.md)
   retains wave-rule/level/RNG observations at every wave through 5,850 in
   both modes. It does not close complete encounter generation or whole runs.

The order can change when an actual source mismatch reveals a prerequisite.
Each slice retains its own reproducible validation and exact-head CI record.
Source proofs must observe actual pinned methods, including native RNG captured
before GameManager construction when the framework would replace it.

The user authorized cross-core fixes during unattended continuation on
2026-10-17. Another agent handles mystery encounters and rival fights as of
2026-10-26. Follow existing contracts and validate
any run/game/AI changes required to close battle-core callers.
Hardware/emulator acceptance is separate from source parity and a passing build.

Gimmighoul canonical EvoTracker acquisition, class admission, primary evolution
and persistence now have [native owner evidence](../game-core/EVO-TRACKER-VALIDATION.md).
The [fusion level-up follow-up](../game-core/FUSION-EVOLUTION-VALIDATION.md)
adds primary/secondary selection, mutation, queued confirmation and optional
secondary-gender saves. Fusion acquisition, item evolution, special extra-party
evolutions and broader interactions remain open caller prerequisites.

[Constructor-lock transport](../game-core/HELD-LOCK-VALIDATION.md) now preserves
source locks in acquisition, projection and save reconstruction. Held-form
activation and party action owners also have [their own proof](../game-core/FORM-ITEM-OWNER-VALIDATION.md).
The [Macho Brace stat/Victory/capture owners](../game-core/MACHO-BRACE-VALIDATION.md)
are implemented; Winstrate reward selection and arbitrary cleanup histories
remain separate. Do not recreate these merged owners based on the historical
Card 104 table.
