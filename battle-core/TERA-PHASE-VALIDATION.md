# Tera phase and arena counter

Pinned source: `8555c08c823b856cbec4eb99ca84ea52a955836d`; locales:
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`. Producers use Node 22 and
capture native `BattleScene.prototype.randBattleSeedInt` before constructing
`GameManager`, restoring it after natural doubles startup.

## Implemented behavior

`TeraPhase.start` queues the translated message using the pre-end Tera type,
constructs the common animation with the actual Pokémon reference, then ends.
`end` sets Terastallization, clears the added type, requests the species form
change, increments the player's arena counter without a cap and requests the
normal/Stellar achievement checks. The phase itself has no HP, active-field,
already-Tera or eligibility gate. Ogerpon's four mask pairs and Terapagos's
terastal-to-stellar transition follow the registered quiet form triggers;
lapse triggers retain their registered inverse transitions.

TurnStart now preserves the Pokémon constructor reference when it queues Tera.
Animation event metadata also retains references for raw off-field phase calls.
The player counter is an explicit double input; missing counter/constructor
contexts remain unsupported. Achievement observers report validation requests;
this work does not implement the native profile's unlock persistence.

The game bridge preserves uses across encounter setup and ordinary trainer
cleanup. New arenas and PartyHeal reset uses. Owning cleanup resets uses when
any player party member has raw or fusion Terapagos, including an off-field
member. Named run saves preserve the counter. Historical saves without it load
zero without reporting lost state; malformed present fields and invalid numeric
values are rejected. The public game menu still has no Tera choice: the host
fixture supplies the existing internal pre-command explicitly.

## Bounded source evidence

Seeds 777 and 2034:

- 16,384 raw cases per seed: actual phase start/end and scene Tera/lapse triggers,
  eight real Pokémon including both benches, all eight Ogerpon forms and three
  Terapagos forms, zero HP, added types, already-Tera states and levels through
  1,000,000. All 32,768 match full 125-field snapshots, queued phases/context,
  translated text, animation constructor references, arena counters, achievement
  validation requests and both RNG streams. Counters reach `2^53` after the
  native increment. Each seed records 4,696 quiet changes, 1,432 Stellar check
  requests, 4,096 player increments and 4,096 animation constructions.
- 8,192 complete doubles turns per seed: 4,096 actual Tera pre-commands across
  all four field roles and 4,096 retained wide combat controls. All 16,384
  compare, including exact text/state/queue/RNG; none are excluded. Each seed
  executes 4,096 Tera and 1,152 quiet form phases. Existing wide, ally-target,
  spread-hit, simultaneous-faint and switch coverage floors remain enforced.
- 2,048 actual owning cleanup cases per seed: a separate counter-only projection
  checks retention/reset for normal, raw Terapagos and fusion Terapagos actors
  on either side. All 4,096 counter results match. Full source cleanup frames
  are saved for diagnosis; this supplemental comparison claims only counters.

The first owning replay exposed a missing constructor reference and the old
blanket unsupported marker; it compared only 4,096 controls. Those failures
were retained and both full matrices rerun after the fixes. An early raw source
fixture produced an invalid form for species with an empty forms array; that
capture was excluded and regenerated at the original full size. An early raw
comparison exposed the C added-type null sentinel (`0`, not UNKNOWN); production
was corrected. Expected output and required coverage were not relaxed.

## Reproduction

```sh
. /workspace/shared/activate-parity.sh
sh battle-core/tools/tera-phase-diff.sh /path/to/pinned-upstream 777
sh battle-core/tools/tera-phase-diff.sh /path/to/pinned-upstream 2034
make -C battle-core tera-phase-host-test
make -C game-core tera-host-test wide-level-host-test
```

The runner includes compiled production faults and malformed transport guards.
The host bridge fixture checks internal phase ownership, legal save boundaries,
wide counts, old saves, malformed fields and actual reset callers. Normal
app/custom/save tests and the 3DS build remain required before merge.

These are bounded proofs. They do not close the source inventory, broader
fusion/Transform interactions, public Tera menu eligibility, external profile
persistence, remaining callbacks or wide-level Endless acceptance. See
[the completion checklist](COMPLETION-CHECKLIST.md).
