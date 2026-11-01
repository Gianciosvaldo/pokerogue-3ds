# Ordinary trainer constructor continuation and TM pools

Pinned upstream: `8555c08c823b856cbec4eb99ca84ea52a955836d`.
Pinned locales: `270ed0a2938b2c3e2e34e7de940e84aecadf548b`.

PR #508 moved ordinary Classic trainers below level 25 onto weighted native
movesets, with moveset generation and constructor finish inside each member's
`executeWithSeedOffset` stream. Its optional run-core continuation preserves the
prefix-only API and rival owner, and stages completed members until the entire
party prefix succeeds. It also ports Young Couple's paired factories.

The follow-up extends that owner through common/great/ultra TM pools and
relearner moves. It preserves source pool order, duplicates, forbidden TMs,
superseded moves across TM/level pools, tier weights and fill caps. Before this
extension, higher-level members still used the last-four-level-moves stand-in
and finished on the ambient RNG. Formerly guarded native cases now receive full
moves/PP/finish/RNG comparisons.

**Egg-enabled trainers remain outside this owner.** All 63 ordinary trainer
configurations disable egg moves in the pinned source. A compiled fault check
caught that distinction during development; exploratory egg code was removed
before publication. Level-80/170 samples alone do not prove egg generation.
Boss/static trainers that enable eggs need their own source-backed owner,
including their other constructor, passive and signature policies.

## Source boundaries

Read directly in the pinned checkout:

- `src/field/trainer.ts`, `Trainer.genPartyMember`: base, moveset and finish run
  inside the member seed offset; ambient RNG is restored afterward. Both
  same-seed and member-specific offsets matter.
- `src/data/trainers/trainer-config.ts`: exact paired species pools/partner
  slots, same-seed configuration and `allowEggMoves`. Ordinary configs leave it
  false. `setBoss` and `setEggMovesAllowed` enable it; `setStaticParty` alone
  does not. Runtime custom configs are not assumed equivalent to these defaults.
- `src/ai/ai-moveset-gen.ts`: trainers include relearner rows; level moves are
  superseded before constructing the TM pool; TMs exclude level/egg duplicates
  and forbidden TMs, and consider level moves when filtering superseded TMs.
  Master pool insertion order is TMs, eggs, level moves. The admitted ordinary
  domain has an empty egg pool. Forced signature/STAB picks count toward TM
  usage but bypass the remaining-slot cap. A subsequently removed useless move
  does not refund its count. Trainer OHKO exclusion, buff/multiturn weights and
  per-slot type diversity remain shared with the retained low-level proof.
- `src/data/balance/moves/moveset-generation.ts`: TM tiers begin at 25/40/55;
  relearner moves become weighted at 40; TM fill caps increase at 25/41/71/101.
  Thresholds and weights are generated from the native module. Private cap
  thresholds are observed through its real public helper over 0–70,000, with
  the source's final branch also checked at signed-32-bit maximum.
- `src/field/pokemon.ts`: initial constructor IVs/stats determine moveset
  weights. Trainer IVs are rerolled after moveset generation. A later no-item
  `BattleScene.updateModifiers(false)` refresh uses the final IVs.

TM compatibility/order comes from the retained generated species/form
`getTms(formKey)` tables, including prevolutions. The new generated facts are
forbidden TM IDs, tier levels/weights, cap thresholds and ordinary trainer egg
policy. Existing wild rules and power tables remain shared.

## Actual native observations

`tests/trainer-constructor.producer.test.ts` captures native
`BattleScene.prototype.randBattleSeedInt` **before** `GameManager`, restores it
after battle startup, and verifies real moveset/effective-power methods.
Wrappers observe and invoke real `Trainer.genPartyMember`, `addEnemyPokemon` and
`generateAndPopulateMoveset`. No replacement generation algorithm produces the
expected moves or RNG.

The compressed fixture retains all 1,880 observations from #508 unchanged:
696 first members and 504 complete parties / 1,184 members. Both legacy
partitions have immutable canonical hashes in the strict transport. Added
first-member and whole-party cases exercise Ace Trainer, Breeder, Baker,
Mysterious Sisters, Twins, Veteran, Young Couple and Youngster around the tier,
relearner, denylist and cap thresholds, at waves 11/99 and levels through 201.
Prior source members are placed in the actual party before generating each next
member; source template metadata, roles, constructor states, moves/PP, all four
RNG fields and refreshed stats/HP are copied into the C projection.

Controlled source context: trainer IDs 12345/54321, Town, no weather/terrain,
recorded wave-cycle offsets, inactive timed events and default trainer configs.

Production comparison results:

- 1,400 first-member cases plus 856 complete parties / 2,174 members.
- All 3,574 base prefixes and complete moves/PP/constructor-finish streams match.
- All complete parties match member order, partner slots, identity, ability,
  IVs, friendship, shiny metadata, refreshed stats and HP; 180 are doubles.
- 247,371 assertions pass. The original 128 higher-level rejection cases are
  retained and upgraded to complete constructor comparisons.
- Twenty atomic rejection cases retain unsupported roles, invalid levels,
  modes/classes, challenge/fusion/boss/passive/Tera/tag states, occupied
  field/moveset and form callbacks, including an injected unsupported stat
  operation. A second-member constructor failure leaves the live party unchanged
  and restores ambient RNG. Prefix-only and declining/error callback contracts
  remain covered.
- Scoped ASan/UBSan/LeakSanitizer passes the same 247,371 assertions with the
  changed generator, constructor continuation and runner instrumented; other
  linked libraries remain unsanitized.
- Fresh actual source capture and C projection are byte-identical. Fourteen
  malformed/forged captures are rejected. Three pristine owner controls pass;
  22 compiled production faults are rejected, including missing/reordered TM
  pools, tier weights, forbidden TMs, cross-pool superseding, caps, TM counts
  and relearner weights.

Reproduce with Node 22 and the prepared pinned checkout:

```sh
make -C game-core trainer-constructor-test trainer-constructor-capture trainer-constructor-faults
bash game-core/tools/reproduce-trainer-constructors.sh /path/to/pinned-upstream /tmp/trainer-native
```

The source's assets **gitlink** is pinned; headless observations do not establish
hardware asset decoding. Normal battle/app/custom/save tests, retained wild and
boss comparisons, and actual 3DS/ARM stack checks remain required before merging.
Only the dedicated custom Task63 workflow is skipped.

## Still open

This is bounded constructor evidence, not battle-core completion. Egg-enabled
boss/static trainers, rival/Gym/E4/evil-team full movesets and signatures, instant
Tera, trainer fusions, challenges/Daily/custom config contexts and post-moveset
form callbacks remain outside this owner. Their already-merged encounter
selection/species work must be retained. Pool storage still has explicit atomic
capacity guards; the sampled constructor matrix is not a whole-catalogue bound.
Full encounter item rolls/played battles, broader closure classification and the
integrated wide-level Endless acceptance chain retain separate obligations.
No Windows builder is produced by this change.
