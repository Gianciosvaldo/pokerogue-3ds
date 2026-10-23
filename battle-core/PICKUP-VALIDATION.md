# Pickup and its BattleEnd owner

Source pin: `8555c08c823b856cbec4eb99ca84ea52a955836d`; locales:
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`; Node 22.

`PostBattleLootAbAttr` (`src/data/abilities/ab-attrs.ts:4853`) caches a
globally seeded selection in `canApply`, including a selection whose transfer
preflight fails. Simulation, defeat and empty loot skip selection. A singleton
selection consumes no RNG. `apply` uses that shared cached object, transfers one
stack without an item-loss callback, removes its entire loot-list entry on
success, queues the localized message, then clears the cache. The native
`splice(indexOf(item), 1)` also removes the last entry when a raw cached item is
absent from a nonempty list. This behavior is retained.

`Battle.addPostBattleLoot` (`src/battle.ts:188`) detaches actual transferable
objects in faint/inventory order. It leaves those objects in enemy inventory,
including zero stacks after a transfer. The port records transient object
references separately from shared ModifierType references. Clones get a new
object identity. These references are not save data.

`BattleEndPhase.start` calls `PostBattleAbAttr` per eligible party member, in
party order, before collecting scattered money and clearing enemy items.
The existing money callback API now dispatches this parent class, preserving
primary/passive ordering between Honey Gather and Pickup. The game adapter
synchronizes the resulting inventory before enemy cleanup. Positive EXP and
friendship effects remain owned by the existing game/run/reward paths; they
are not applied again by turn callbacks.

## Transferable-family source inventory

Following inheritance in pinned `src/modifier/modifier.ts` gives 33 held-item
classes: two abstract classes and 31 concrete classes. Six concrete classes
are nontransferable, leaving 25 transferable classes. The native flag excludes
these six from Pickup:

| Nontransferable class | Native declaration / inheritance |
| --- | --- |
| BaseStatModifier | `isTransferable = false`, line 817 |
| EvoTrackerModifier | `isTransferable = false`, line 872 |
| PokemonBaseStatTotalModifier | `isTransferable = false`, line 931 |
| PokemonBaseStatFlatModifier | `isTransferable = false`, line 991 |
| PokemonIncrementingStatModifier | `isTransferable = false`, line 1060 |
| PokemonFormChangeItemModifier | `isTransferable = false`, line 2790 |

The earlier working note that StatBoosterModifier was nontransferable was
incorrect. It inherits the default `true`. EvolutionStatBoosterModifier and
SpeciesStatBoosterModifier therefore are eligible loot too. These three classes
remain explicit unsupported transfer paths in this slice. Eviolite inherits
the base clone, producing StatBoosterModifier; species boosters preserve their
class and species list. Their transfer/clone/save bridge is the next required
item slice. Existing unsupported flags for these paths are retained.

The 22 transferable families represented by this slice preserve their class in `clone`:

| Class | Canonical key(s) | Cap |
| --- | --- | --- |
| CritBoosterModifier | SCOPE_LENS | 1 |
| SpeciesCritBoosterModifier | LEEK | 1 |
| AttackTypeBoosterModifier | ATTACK_TYPE_BOOSTER (18 types) | 99 |
| SurviveDamageModifier | FOCUS_BAND | 5 |
| BypassSpeedChanceModifier | QUICK_CLAW | 3 |
| FlinchChanceModifier | KINGS_ROCK | 3 |
| TurnHealModifier | LEFTOVERS | 4 |
| TurnStatusEffectModifier | TOXIC_ORB, FLAME_ORB | 1 |
| HitHealModifier | SHELL_BELL | 4 |
| BerryModifier | BERRY (11 types) | 2 or 3 |
| PokemonInstantReviveModifier | REVIVER_SEED | 1 |
| ResetNegativeStatStageModifier | WHITE_HERB | 2 |
| FieldEffectModifier | MYSTICAL_ROCK | 2 |
| PokemonExpBoosterModifier | LUCKY_EGG, GOLDEN_EGG | 99 |
| PokemonFriendshipBoosterModifier | SOOTHE_BELL | 3 |
| PokemonNatureWeightModifier | SOUL_DEW | 10 |
| PokemonMoveAccuracyBoosterModifier | WIDE_LENS | 3 |
| PokemonMultiHitModifier | MULTI_LENS | 2 |
| DamageMoneyRewardModifier | GOLDEN_PUNCH | 5 |
| SwitchEffectTransferModifier | BATON | 1 |
| ContactHeldItemTransferChanceModifier | GRIP_CLAW | 5 |
| TurnHeldItemTransferModifier | MINI_BLACK_HOLE | 1 |

The Pickup capture covers 50 identities, including Baton and all five newly
represented factory keys. It does not cover the seven Eviolite/species-booster
factory identities. Grip Claw's class/transfer contract is retained by its separate
source proof. EXP matching includes the boost multiplier, so Lucky and Golden
Egg are separate stocks. SpeciesCrit cloning preserves its species list and
stage parameter. Source `getOrInferTier` is observed for both wild and trainer
pool lookup, including the fallback pool search for stolen player-only items.

## Reproduction and scope

```sh
. /workspace/shared/activate-parity.sh
sh battle-core/tools/pickup-diff.sh /path/to/pinned-upstream
TURNTEST_LOCALES=/path/to/pinned-upstream/locales/en \
  python3 battle-core/tools/check-pickup-proof.py battle-core/build/pickup.native.txt
make -C game-core pickup-host-test
```

The native producer captures battle RNG before constructing GameManager and
restores it after natural doubles startup. It calls actual preflight, raw apply,
ability dispatch and BattleEndPhase. Presentation shifting and subsequent enemy
teardown are intercepted. Honey interactions are captured at the actual boundary
before money collection; the real money method runs afterwards. That money
continuation has its own retained source proof.

Version 1's 4,000 required cases remain accepted and unchanged. Version 2 retains
them and adds 400 cases across eight owner arrangements: bench holders, passive
Pickup, primary/passive overlap, suppression, both Honey/Pickup orders, reversed
party order and a fainted holder. The strict comparison includes ordered item
objects and parameters, loot order/cache, actor HP/stats/ability history, every
queued phase and rendered message, both RNG streams and money at the boundary.
Unsupported/deferred/overflow results fail the comparison.

Raw cached objects outside both inventories remain explicitly unsupported.
Cross-battle raw apply without a fresh gate does not have a persistent native
attribute-object representation. The ordinary owner always runs the gate before
applying. Capacity failures remain reported. This slice does not close the
remaining source inventory, all interaction arrangements, or battle core.

## Validation for this slice

- Version 1: all 4,000 native cases exact. Version 2: all 4,400 native cases
  exact, including the original cases; no unsupported/deferred/overflow results.
- Eight deliberately compiled Pickup faults are caught; twelve malformed or
  truncated captures are rejected. Fault runners stop at their first observed
  mismatch; the ordinary comparison always consumes every required row.
- Retained theft 32,768, Grip Claw 32,768, Baton 256 and scene/Mini Black Hole
  3,608 comparisons remain exact.
- Actual game-core turn -> faint loot -> bench Pickup -> next wave -> save/load:
  22 checks pass, including one-stack transfer and full-stack refusal.
- Full game-core suite passes. Saved and unsaved 200-run digests both
  `5073f991`. Normal app/custom/save and ARM/stack checks remain merge gates.
