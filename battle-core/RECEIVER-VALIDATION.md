# Receiver / Power of Alchemy

Base: main `35f29c438013aabce72dd60e42d6841f01cbeaef` (#262).
Source: `8555c08c823b856cbec4eb99ca84ea52a955836d`; locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`; Node 22.

## Source and change

`FaintPhase.start` resets the victim's summon data before `doFaint` calls
`applyAbAttrs("PostKnockOutAbAttr", { pokemon: p, victim: pokemon })` for each
active Pokémon in speed order. The C caller now supplies that victim explicitly.
It does not reuse the attacker/defender parameter or infer a missing victim.

`CopyFaintedAllyAbilityAbAttr.canApply` checks only matching sides and the
victim's current primary ability's `copiable` flag. It does not copy the passive
or add an invented HP/adjacency check. `apply` skips mutation during simulation, calls the real
temporary-ability setter, then queues the victim's name and current ability.
Calling it as a passive still changes the primary ability. Source gain/loss
callbacks and applied-ability sets remain part of the shared setter/dispatcher.

The source getter ignores a zero `summonData.ability`. The shared C setter now
uses the permanent ability for ordinary actors when given NONE, including that
ability's gain effects. A transformed actor given NONE still needs the source
ability-index/custom-override lookup model; that branch explicitly reports
unsupported rather than claiming the permanent fallback is always correct.

## Owning differential

`tests/receiver.producer.test.ts` initializes a real four-Pokémon double battle
and executes either the actual PostKnockOut dispatcher (normal or simulated),
or the actual FaintPhase. Only rendering/assets/cry/tween timing are stubbed.
No gameplay method, canApply condition or expected formula is replaced.

The fixed 125-number actor projection includes raw/effective identity, moves,
HP/stats/stages, passives, temporary overrides, IVs, friendship, suppression,
applied-ability sets and turn counters. The world projection checks weather,
terrain, score, faint counts, loot, ability-bar state and participants. Queued
phases, dynamic stat payloads, rendered English messages and both RNG states
are compared exactly. Reset observations retain the permanent ability/passive.

```
tools/receiver-diff.sh /path/to/pinned-upstream 2000 777
```

Four seeds (777, 9001, 4242, 31337), 2,000 complete boundary cases each. Thirty-two
state/side combinations include primary/passive/both dispatch, suppression,
fainted holders, an alive victim at the direct API, current versus permanent
victim ability, NONE, same-side uncopyable abilities, Intimidate/Download/weather
and stat gains, and reset behavior. These are **phase/API boundary cases**, not
8,000 complete battle turns or a claim that all doubles behavior is finished.

Mutation checks prompted a stronger same-side temporary-override case: direct
dispatch copies the override, while the actual faint caller resets it first.
The original expected values, comparator and established source fixtures are
unchanged. The owning reader refuses incomplete or malformed input. Distinct caller and
victim species also make the victim-name message check independently observable.

## Separate checks

`game-core/tests/receiver_save_host_test.c` checks legal command save/load of a
copied primary, applied-ability state, passive activation, permanent identity
and summon reset. Core/application host tests, retained source suites, mutation
and malformed-input checks and exact-head host/3DS CI are required before merge.
No hardware or full battle-core completion claim is made.

Core/application host suites pass, including 15 command/save/reset checks and
paired 200-run save/no-save digest `9a82f439`. Retained 24,000 sampled turns have
no covered mismatch or coverage loss: plain 1,964; rich 1,940 -> 1,944; switch
1,891; summon 1,893; item seeds 1,945 / 1,948 / 1,945 / 1,958; trainer seeds
1,951 / 1,950 / 1,947 / 1,959. Nine dedicated ability/move suites retain all
72,000 owning turns; all 8,000 Ditto phases and 8,000 Ditto turns remain exact.
All 19 compiled runtime mutants are caught. All 17 malformed fixtures exit 2;
the intact owning fixture passes. Exact-head CI is recorded on the PR before
merge. The conditional custom-content check is excluded by the user's instruction;
ordinary core and application tests remain required.
